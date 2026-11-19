#!/usr/bin/env python3
"""k4tool - diagnostics and measurements for the Keychron K4 Pro community firmware.

Talks to the keyboard over the USB cable (raw HID, the same channel VIA uses), so the
cable must be plugged in. The Cable/BT switch can be on either side: in Bluetooth mode
the keys still go out over Bluetooth while the cable carries the diagnostics.

    py k4tool.py info              settings, battery, connection, counters
    py k4tool.py watch             live dashboard + chatter (double-type) detector
    py k4tool.py latency [-n 300]  measure key latency of the active connection
    py k4tool.py reset             clear the counters

Requires: Windows, Python 3.9+, `py -m pip install hidapi`.
Close VIA / Keychron Launcher first: they hold the same raw HID channel.
"""

import argparse
import ctypes
import json
import queue
import random
import statistics
import sys
import threading
import time
from ctypes import wintypes

try:
    import hid
except ImportError:
    sys.exit("Missing dependency: run  py -m pip install hidapi")

VID = 0x3434
PIDS = {0x0240: "ANSI RGB", 0x0241: "ISO RGB", 0x0243: "ANSI white", 0x0244: "ISO white"}
RAW_USAGE_PAGE, RAW_USAGE = 0xFF60, 0x61
DIAG_ID = 0xC4
CMD_INFO, CMD_COUNTERS, CMD_RESET, CMD_PROBE, CMD_OPTION, CMD_PROFILE = 1, 2, 3, 4, 5, 6
SECTIONS = ["Bluetooth UART task", "Bluetooth events", "report buffer", "indicators", "battery",
            "low-power checks", "key matrix scan", "LED flush step", "Bluetooth send (wake + UART)"]
OPTIONS = {"led-flush-incremental": 1}
PACKET = 32
VK_F24 = 0x87

TRANSPORTS = {0: "none", 1: "USB", 2: "Bluetooth"}
BT_STATES = {0: "reset", 1: "initialized", 2: "disconnected", 3: "connected",
             4: "pairing", 5: "reconnecting", 6: "suspended"}
COUNTERS = ["bt_reports", "bt_retries", "bt_fifo_full", "bt_connects",
            "bt_disconnects", "wake_keys", "sleeps"]


# --------------------------------------------------------------------------- raw HID

class Keyboard:
    def __init__(self):
        path = None
        for d in hid.enumerate(VID, 0):
            if d["product_id"] in PIDS and d["usage_page"] == RAW_USAGE_PAGE and d["usage"] == RAW_USAGE:
                path, self.model = d["path"], PIDS[d["product_id"]]
                break
        if path is None:
            sys.exit("K4 Pro not found over USB. Plug the cable in (any switch position) "
                     "and close VIA / Keychron Launcher.")
        self.dev = hid.device()
        self.dev.open_path(path)

    def request(self, cmd, timeout_ms=500):
        pkt = bytes([DIAG_ID, cmd]) + bytes(PACKET - 2)
        self.dev.write(b"\x00" + pkt)  # leading 0 = report ID
        deadline = time.monotonic() + timeout_ms / 1000
        while time.monotonic() < deadline:
            r = self.dev.read(PACKET, 50)
            if r and r[0] == DIAG_ID and r[1] == cmd:
                if r[2] == 0xFF:
                    sys.exit("This firmware has no diagnostics. Flash v0.6.0 or newer.")
                return bytes(r)
        if cmd == CMD_INFO:
            sys.exit("No answer. Flash v0.6.0 or newer, and close VIA / Keychron Launcher.")
        return None

    def option(self, name, value=0xFF):
        """Read (value 0xFF) or set a run-time firmware option. Returns its value."""
        pkt = bytes([DIAG_ID, CMD_OPTION, 0, OPTIONS[name], value]) + bytes(PACKET - 5)
        self.dev.write(b"\x00" + pkt)
        deadline = time.monotonic() + 0.5
        while time.monotonic() < deadline:
            r = self.dev.read(PACKET, 50)
            if r and r[0] == DIAG_ID and r[1] == CMD_OPTION:
                if r[2] != 0:
                    sys.exit(f"Option {name} is not supported by this firmware.")
                return r[3]
        sys.exit("No answer to the option command.")

    def send_probe(self):
        self.dev.write(b"\x00" + bytes([DIAG_ID, CMD_PROBE]) + bytes(PACKET - 2))

    def drain(self):
        while self.dev.read(PACKET, 1):  # timeout 0 would block in hidapi
            pass

    def info(self):
        r = self.request(CMD_INFO)
        p = r[3:]
        u16 = lambda o: int.from_bytes(p[o:o + 2], "little")
        u32 = lambda o: int.from_bytes(p[o:o + 4], "little")
        flags = p[16]
        return {
            "model": self.model,
            "protocol": p[0],
            "transport": TRANSPORTS.get(p[1], p[1]),
            "bluetooth": BT_STATES.get(p[2], p[2]),
            "battery_percent": p[3],
            "battery_mv": u16(4),
            "usb_power": bool(p[6]),
            "uptime_s": u32(7) // 1000,
            "scan_rate_hz": u16(11),
            "bt_report_rate_hz": u16(13),
            "debounce_ms": p[15],
            "smart_wake": bool(flags & 1),
            "battery_to_host": bool(flags & 2),
            "bt_nkro": bool(flags & 4),
            "idle_ms": u32(17),
            "loop_max_ms": p[21],
            "slow_loops_per_s": u16(22),
        }

    def counters(self):
        r = self.request(CMD_COUNTERS)
        p = r[3:]
        return {name: int.from_bytes(p[4 * i:4 * i + 4], "little") for i, name in enumerate(COUNTERS)}

    def reset(self):
        self.request(CMD_RESET)


# --------------------------------------------------------------- keyboard event hook

user32 = ctypes.WinDLL("user32", use_last_error=True)
kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
LRESULT = ctypes.c_ssize_t
HOOKPROC = ctypes.WINFUNCTYPE(LRESULT, ctypes.c_int, wintypes.WPARAM, wintypes.LPARAM)
user32.SetWindowsHookExW.argtypes = [ctypes.c_int, HOOKPROC, wintypes.HINSTANCE, wintypes.DWORD]
user32.SetWindowsHookExW.restype = wintypes.HHOOK
user32.CallNextHookEx.argtypes = [wintypes.HHOOK, ctypes.c_int, wintypes.WPARAM, wintypes.LPARAM]
user32.CallNextHookEx.restype = LRESULT
user32.GetMessageW.argtypes = [ctypes.POINTER(wintypes.MSG), wintypes.HWND, wintypes.UINT, wintypes.UINT]
kernel32.GetModuleHandleW.restype = wintypes.HMODULE


class KBDLLHOOKSTRUCT(ctypes.Structure):
    _fields_ = [("vkCode", wintypes.DWORD), ("scanCode", wintypes.DWORD), ("flags", wintypes.DWORD),
                ("time", wintypes.DWORD), ("dwExtraInfo", ctypes.c_size_t)]


class KeyHook:
    """Low-level keyboard hook. Every event goes to a queue as (t, vk, down).
    F24 (the probe key) is swallowed so it never reaches applications."""

    def __init__(self):
        self.events = queue.Queue()
        self._procs = []
        threading.Thread(target=self._run, daemon=True).start()

    def _run(self):
        def proc(code, wparam, lparam):
            t = time.perf_counter()
            if code == 0:
                k = ctypes.cast(lparam, ctypes.POINTER(KBDLLHOOKSTRUCT)).contents
                down = wparam in (0x100, 0x104)  # WM_KEYDOWN, WM_SYSKEYDOWN
                self.events.put((t, k.vkCode, down))
                if k.vkCode == VK_F24:
                    return 1
            return user32.CallNextHookEx(None, code, wparam, lparam)

        cb = HOOKPROC(proc)
        self._procs.append(cb)  # keep every callback alive, an old hook may still call it
        if not user32.SetWindowsHookExW(13, cb, kernel32.GetModuleHandleW(None), 0):
            raise OSError(ctypes.get_last_error(), "SetWindowsHookEx failed")
        msg = wintypes.MSG()
        while user32.GetMessageW(ctypes.byref(msg), None, 0, 0) > 0:
            pass

    def reinstall(self):
        """Windows silently drops a low-level hook that once answered too slowly
        (LowLevelHooksTimeout). Install a fresh one on the same queue."""
        threading.Thread(target=self._run, daemon=True).start()
        time.sleep(0.2)

    def get(self, timeout):
        try:
            return self.events.get(timeout=timeout)
        except queue.Empty:
            return None


# ------------------------------------------------------------------------- commands

def cmd_info(kb, args):
    i, c = kb.info(), kb.counters()
    if args.json:
        print(json.dumps({"info": i, "counters": c}, indent=2))
        return
    print(f"Keychron K4 Pro {i['model']}  (diagnostics v{i['protocol']})")
    print(f"  Connection   {i['transport']}" + (f", Bluetooth {i['bluetooth']}" if i['transport'] == 'Bluetooth' else ""))
    print(f"  Battery      {i['battery_percent']} %  ({i['battery_mv']} mV)" + ("  charging" if i['usb_power'] else ""))
    print(f"  Scan rate    {i['scan_rate_hz']} Hz, longest pass {i['loop_max_ms']} ms, "
          f"{i['slow_loops_per_s']} passes >= 3 ms per second")
    print(f"  Settings     debounce {i['debounce_ms']} ms, Bluetooth {i['bt_report_rate_hz']} Hz, "
          f"smart wake {'on' if i['smart_wake'] else 'off'}, BT NKRO {'on' if i['bt_nkro'] else 'off'}")
    print(f"  Uptime       {i['uptime_s'] // 3600} h {i['uptime_s'] // 60 % 60} min")
    print("  Counters     " + ", ".join(f"{k} {v}" for k, v in c.items()))


def cmd_profile(kb, args):
    """Longest main-loop pass and longest run of each instrumented section, per second."""
    worst = {}
    pass_worst = 0
    print(f"Profiling for {args.seconds} s (type, or run a latency test at the same time)...")
    for _ in range(args.seconds):
        time.sleep(1)
        r = kb.request(CMD_PROFILE)
        if r is None or r[2] != 0:
            sys.exit("This firmware has no profiler (needs DIAG_PROFILE_ENABLE).")
        p = r[3:]
        pass_us = int.from_bytes(p[0:4], "little")
        pass_worst = max(pass_worst, pass_us)
        for s in range(min(p[4], len(SECTIONS))):
            us = int.from_bytes(p[5 + 2 * s:7 + 2 * s], "little")
            worst[SECTIONS[s]] = max(worst.get(SECTIONS[s], 0), us)
        if args.live:
            print(f"  pass {pass_us:>6} us  " + "  ".join(f"{k.split()[0]} {v}" for k, v in worst.items()))
    print(f"\nLongest main-loop pass: {pass_worst} us")
    for name, us in sorted(worst.items(), key=lambda kv: -kv[1]):
        print(f"  {name:<30} {us:>6} us")
    print("Sections not listed (RGB rendering, USB, QMK core) make up the rest of a pass.")


def cmd_reset(kb, args):
    kb.reset()
    print("Counters cleared.")


def cmd_watch(kb, args):
    hook = KeyHook()
    last_up = {}
    chatter = []
    keys = 0
    prev = kb.counters()
    start = time.monotonic()
    next_poll = 0
    print("Watching. Type normally; Ctrl+C to stop.\n")
    try:
        while True:
            ev = hook.get(0.05)
            if ev:
                t, vk, down = ev
                if down:
                    gap = (t - last_up.pop(vk)) * 1000 if vk in last_up else None
                    if gap is not None and gap < args.chatter_ms:
                        chatter.append((time.strftime("%H:%M:%S"), vk, gap))
                        print(f"\n  ! {chatter[-1][0]} possible double-type on key VK 0x{vk:02X} "
                              f"(pressed again {gap:.0f} ms after release)")
                    keys += 1
                else:
                    last_up[vk] = t
            if time.monotonic() >= next_poll:
                next_poll = time.monotonic() + 1
                i, c = kb.info(), kb.counters()
                d = {k: c[k] - prev[k] for k in c}
                prev = c
                line = (f"\r{i['transport']:<9} {i['bluetooth'] if i['transport'] == 'Bluetooth' else '':<12} "
                        f"bat {i['battery_percent']:>3}%  scan {i['scan_rate_hz']:>5} Hz (max {i['loop_max_ms']} ms)  "
                        f"BT reports/s {d['bt_reports']:>3}  retries {c['bt_retries']}  "
                        f"FIFO full {c['bt_fifo_full']}  reconnects {c['bt_connects']}  "
                        f"sleeps {c['sleeps']}  keys {keys}  double-types {len(chatter)}   ")
                print(line, end="", flush=True)
    except KeyboardInterrupt:
        mins = (time.monotonic() - start) / 60
        print(f"\n\n{keys} key presses in {mins:.1f} min, {len(chatter)} possible double-types "
              f"(same key again < {args.chatter_ms} ms after release).")


def cmd_latency(kb, args):
    hook = KeyHook()
    time.sleep(0.2)
    res = measure(kb, hook, args.n)
    print_result(res)
    print("Measured from the PC request to the key arriving in Windows: it includes about 1 ms of USB\n"
          "to reach the keyboard, but not the switch travel or debounce (those happen before).")
    if args.save:
        with open(args.save, "w") as f:
            json.dump(res, f, indent=2)
        print(f"Saved to {args.save}")


def print_result(res, label=None):
    print(f"\n{label or res['transport']}: median {res['median_ms']} ms, p95 {res['p95_ms']} ms, "
          f"min {res['min_ms']} ms, max {res['max_ms']} ms, jitter (stdev) {res['stdev_ms']} ms, "
          f"lost {res['lost']}, longest firmware pass {res['loop_max_ms']} ms")


def cmd_ab_flush(kb, args):
    """A/B test of the incremental LED flush: same keyboard, same session, toggled live."""
    hook = KeyHook()
    time.sleep(0.2)
    results = {}
    try:
        for rnd in range(args.rounds):
            for state in (0, 1):
                kb.option("led-flush-incremental", state)
                time.sleep(1.2)  # let the loop statistics refresh
                label = "incremental" if state else "stock (one block)"
                print(f"Round {rnd + 1}/{args.rounds}, LED flush {label}")
                r = measure(kb, hook, args.n, quiet=True)
                results.setdefault(label, []).append(r)
                print_result(r, label)
    finally:
        kb.option("led-flush-incremental", 1)
    print("\nSummary (median of the rounds):")
    for label, rs in results.items():
        med = lambda k: round(statistics.median(x[k] for x in rs), 2)
        print(f"  {label:<18} median {med('median_ms')} ms, p95 {med('p95_ms')} ms, max {med('max_ms')} ms, "
              f"longest pass {med('loop_max_ms')} ms")
    if args.save:
        with open(args.save, "w") as f:
            json.dump(results, f, indent=2)


def measure(kb, hook, count, quiet=False):
    i = kb.info()
    kb.drain()
    if i["transport"] == "Bluetooth" and i["bluetooth"] != "connected":
        sys.exit("Bluetooth is not connected.")
    if not quiet:
        print(f"Measuring {count} key taps over {i['transport']} "
              f"(the probe key is F24, it is hidden from applications)...")
    samples, lost, loop_max = [], 0, 0
    for n in range(count):
        while hook.get(0):
            pass
        t0 = time.perf_counter()
        kb.send_probe()
        got_down = False
        deadline = t0 + 0.5
        while time.perf_counter() < deadline:
            ev = hook.get(deadline - time.perf_counter())
            if not ev:
                break
            t, vk, down = ev
            if vk != VK_F24:
                continue
            if down and not got_down:
                samples.append((t - t0) * 1000)
                got_down = True
            elif not down and got_down:
                break
        if not got_down:
            lost += 1
            hook.reinstall()  # a missed probe is almost always a dropped hook on the PC side
        kb.drain()
        time.sleep(random.uniform(0.02, 0.05))  # random gap: no lock-step with the radio interval
        if (n + 1) % 100 == 0:
            loop_max = max(loop_max, kb.info()["loop_max_ms"])
            if not quiet:
                print(f"  {n + 1}/{count}")
    if not samples:
        sys.exit("No probe key received. Is the keyboard connected to this PC?")
    samples.sort()
    pct = lambda q: samples[min(len(samples) - 1, int(q * len(samples)))]
    return {"transport": i["transport"], "samples": len(samples), "lost": lost,
            "min_ms": round(samples[0], 2), "median_ms": round(statistics.median(samples), 2),
            "p95_ms": round(pct(0.95), 2), "max_ms": round(samples[-1], 2),
            "stdev_ms": round(statistics.pstdev(samples), 2), "loop_max_ms": loop_max,
            "settings": {k: i[k] for k in ("debounce_ms", "bt_report_rate_hz", "smart_wake", "bt_nkro")}}


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)
    p = sub.add_parser("info"); p.add_argument("--json", action="store_true")
    sub.add_parser("reset")
    p = sub.add_parser("watch"); p.add_argument("--chatter-ms", type=int, default=30)
    p = sub.add_parser("latency"); p.add_argument("-n", type=int, default=300); p.add_argument("--save")
    p = sub.add_parser("profile", help="longest run of each firmware section")
    p.add_argument("--seconds", type=int, default=20); p.add_argument("--live", action="store_true")
    p = sub.add_parser("ab-flush", help="A/B test: stock vs incremental LED flush")
    p.add_argument("-n", type=int, default=200); p.add_argument("--rounds", type=int, default=3); p.add_argument("--save")
    args = ap.parse_args()
    kb = Keyboard()
    {"info": cmd_info, "reset": cmd_reset, "watch": cmd_watch, "latency": cmd_latency,
     "ab-flush": cmd_ab_flush, "profile": cmd_profile}[args.cmd](kb, args)


if __name__ == "__main__":
    main()
