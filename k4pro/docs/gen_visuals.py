"""Generate README visuals for the K4 Pro community firmware.
Usage: python gen_visuals.py <repo_root>
Writes SVGs into <repo_root>/k4pro/docs/.
"""
import colorsys
import json
import os
import sys

ROOT = sys.argv[1]
OUT = os.path.join(ROOT, "k4pro", "docs")
os.makedirs(OUT, exist_ok=True)

FONT = "-apple-system, 'Segoe UI', Helvetica, Arial, sans-serif"
MONO = "ui-monospace, 'SF Mono', Consolas, monospace"
BG0, BG1 = "#0b0f19", "#151b2c"
CARD, CARD_STROKE = "#121829", "#263049"
TXT, MUTED = "#e6edf3", "#8b98b0"
CYAN, VIOLET, PINK, GREEN, AMBER, GREY = "#22d3ee", "#a78bfa", "#f472b6", "#34d399", "#fbbf24", "#3b455c"


def esc(s):
    return s.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")


def defs_common():
    return f"""
  <linearGradient id="bg" x1="0" y1="0" x2="1" y2="1">
    <stop offset="0" stop-color="{BG0}"/><stop offset="1" stop-color="{BG1}"/>
  </linearGradient>
  <linearGradient id="accent" x1="0" y1="0" x2="1" y2="0">
    <stop offset="0" stop-color="{CYAN}"/><stop offset=".5" stop-color="{VIOLET}"/><stop offset="1" stop-color="{PINK}"/>
  </linearGradient>"""


def svg(w, h, body, extra_defs=""):
    return (f'<svg xmlns="http://www.w3.org/2000/svg" width="{w}" height="{h}" viewBox="0 0 {w} {h}" '
            f'font-family="{FONT}"><defs>{defs_common()}{extra_defs}</defs>'
            f'<rect width="{w}" height="{h}" rx="18" fill="url(#bg)"/>{body}</svg>\n')


def write(name, content):
    with open(os.path.join(OUT, name), "w", encoding="utf-8", newline="\n") as f:
        f.write(content)
    print("wrote", name)


def hue_color(t, s=0.75, l=0.62):
    # t in [0,1] across the keyboard: cyan -> violet -> pink
    h = 0.52 + t * 0.40
    r, g, b = colorsys.hls_to_rgb(h % 1.0, l, s)
    return "#%02x%02x%02x" % (int(r * 255), int(g * 255), int(b * 255))


# ---------------------------------------------------------------- keyboard
def keyboard(x0, y0, u, layout="iso", glow=True, mono=False):
    info = json.load(open(os.path.join(ROOT, "keyboards/keychron/k4_pro/info.json"), encoding="utf-8"))
    keys = info["layouts"]["LAYOUT_iso_101" if layout == "iso" else "LAYOUT_ansi_100"]["layout"]
    max_x = max(k["x"] + k.get("w", 1) for k in keys)
    max_y = max(k["y"] + k.get("h", 1) for k in keys)
    pad = 0.35
    W, H = (max_x + 2 * pad) * u, (max_y + 2 * pad) * u
    g = [f'<rect x="{x0}" y="{y0}" width="{W:.1f}" height="{H:.1f}" rx="{u*0.35:.1f}" fill="#1a2033" stroke="#2c3550" stroke-width="2"/>']
    glow_rects, caps = [], []
    for k in keys:
        x, y, w, h = k["x"], k["y"], k.get("w", 1), k.get("h", 1)
        px, py = x0 + (x + pad) * u, y0 + (y + pad) * u
        c = "#e9edf5" if mono else hue_color(x / max_x)
        ins = u * 0.06
        iso_enter = layout == "iso" and k["matrix"] == [2, 13]
        if iso_enter:
            # L-shaped ISO Enter: top part 1.5u wide on row 2, lower part 1.25u wide
            ax = x0 + (13.5 + pad) * u
            pts = [(ax + ins, py + ins), (px + w * u - ins, py + ins), (px + w * u - ins, py + h * u - ins),
                   (px + ins, py + h * u - ins), (px + ins, py + u - ins), (ax + ins, py + u - ins)]
            d = "M" + " L".join(f"{a:.1f},{b:.1f}" for a, b in pts) + " Z"
            glow_rects.append(f'<path d="{d}" fill="{c}"/>')
            caps.append(f'<path d="{d}" fill="#232a40" stroke="#323c5a" stroke-width="1.2" stroke-linejoin="round"/>')
            caps.append(f'<rect x="{px+u*0.16:.1f}" y="{py+u*0.12:.1f}" width="{w*u-u*0.32:.1f}" height="{h*u-u*0.32:.1f}" rx="{u*0.1:.1f}" fill="#2a3250"/>')
            continue
        rx, ry, rw, rh = px + ins, py + ins, w * u - 2 * ins, h * u - 2 * ins
        glow_rects.append(f'<rect x="{rx:.1f}" y="{ry:.1f}" width="{rw:.1f}" height="{rh:.1f}" rx="{u*0.12:.1f}" fill="{c}"/>')
        caps.append(f'<rect x="{rx:.1f}" y="{ry:.1f}" width="{rw:.1f}" height="{rh:.1f}" rx="{u*0.12:.1f}" fill="#232a40" stroke="#323c5a" stroke-width="1.2"/>')
        caps.append(f'<rect x="{rx+u*0.1:.1f}" y="{ry+u*0.06:.1f}" width="{rw-u*0.2:.1f}" height="{rh-u*0.26:.1f}" rx="{u*0.09:.1f}" fill="#2a3250"/>')
        # underglow line at the bottom of each key
        caps.append(f'<rect x="{rx+u*0.14:.1f}" y="{ry+rh-u*0.12:.1f}" width="{rw-u*0.28:.1f}" height="{u*0.05:.1f}" rx="1" fill="{c}" opacity=".9"/>')
    if glow:
        g.append(f'<g filter="url(#glow)" opacity=".55">{"".join(glow_rects)}</g>')
    g.extend(caps)
    return "".join(g), W, H


GLOW = '<filter id="glow" x="-10%" y="-10%" width="120%" height="120%"><feGaussianBlur stdDeviation="7"/></filter>'


def pill(x, y, label, value, color):
    w = 34 + 9.2 * max(len(label), len(value) * 1.45)
    return (f'<g transform="translate({x},{y})"><rect width="{w:.0f}" height="74" rx="14" fill="{CARD}" stroke="{CARD_STROKE}"/>'
            f'<rect x="0" y="16" width="4" height="42" rx="2" fill="{color}"/>'
            f'<text x="20" y="38" font-size="24" font-weight="800" fill="{TXT}">{esc(value)}</text>'
            f'<text x="20" y="60" font-size="13" fill="{MUTED}">{esc(label)}</text></g>'), w


# ---------------------------------------------------------------- hero
def hero():
    kb, kw, kh = keyboard(0, 0, 48)
    W, H = 1280, int(330 + kh)
    scale = 1.0
    kx = (W - kw) / 2
    body = [f'<circle cx="{W*0.82}" cy="40" r="260" fill="{VIOLET}" opacity=".07"/>',
            f'<circle cx="{W*0.12}" cy="{H}" r="240" fill="{CYAN}" opacity=".06"/>',
            f'<text x="64" y="86" font-size="15" font-weight="700" letter-spacing="3" fill="url(#accent)">UNOFFICIAL · OPEN SOURCE · QMK</text>',
            f'<text x="62" y="138" font-size="50" font-weight="800" fill="{TXT}">Keychron K4 Pro</text>',
            f'<text x="64" y="176" font-size="22" fill="{MUTED}">Community firmware — faster, fixed, longer-lasting.</text>']
    x = 64
    for label, value, color in [("faster key press (USB &amp; BT)", "−5 ms", CYAN),
                                ("Bluetooth report rate", "133 Hz", VIOLET),
                                ("bugs fixed", "8", PINK),
                                ("ISO · ANSI · RGB · White", "4 variants", GREEN)]:
        p, w = pill(x, 206, label.replace("&amp;", "&"), value, color)
        body.append(p)
        x += w + 16
    body.append(f'<g transform="translate({kx:.1f},{H - kh - 28:.1f}) scale({scale})">{kb}</g>')
    write("hero.svg", svg(W, H, "".join(body), GLOW))


# ---------------------------------------------------------------- feature cards
ICONS = {
    "bolt": f'<path d="M26 4 L10 30 H22 L18 52 L38 22 H26 Z" fill="{CYAN}"/>',
    "shield": f'<path d="M28 4 L48 12 V28 C48 41 39 50 28 54 C17 50 8 41 8 28 V12 Z" fill="none" stroke="{PINK}" stroke-width="4" stroke-linejoin="round"/><path d="M18 29 L25 36 L39 21" fill="none" stroke="{PINK}" stroke-width="4" stroke-linecap="round" stroke-linejoin="round"/>',
    "battery": f'<rect x="6" y="16" width="40" height="24" rx="5" fill="none" stroke="{GREEN}" stroke-width="4"/><rect x="47" y="23" width="5" height="10" rx="2" fill="{GREEN}"/><rect x="12" y="22" width="22" height="12" rx="2" fill="{GREEN}"/>',
}


def features():
    W, H = 1280, 360
    cards = [
        ("bolt", CYAN, "Responsiveness", ["Eager debounce: press sent on first contact", "No 3 ms Bluetooth wake stall per report", "Bluetooth at 133 Hz instead of 90 Hz", "Matrix scanned ~2× faster"]),
        ("shield", PINK, "Reliability", ["Fixes stuck / repeating keys over Bluetooth", "Fixes media keys that kept repeating", "Pairing no longer runs code in an interrupt", "Hardened Bluetooth packet parsing"]),
        ("battery", GREEN, "Battery life", ["Backlight off after 3 min idle (was 10)", "MCU sleeps sooner and works less", "Battery level shown on your computer", "Accurate Li-ion percentage curve"]),
    ]
    cw, gap, x0 = 384, 24, 40
    body = []
    for i, (icon, color, title, items) in enumerate(cards):
        x = x0 + i * (cw + gap)
        body.append(f'<g transform="translate({x},32)"><rect width="{cw}" height="296" rx="16" fill="{CARD}" stroke="{CARD_STROKE}"/>'
                    f'<rect x="24" y="0" width="{cw-48}" height="3" rx="1.5" fill="{color}"/>'
                    f'<g transform="translate(28,30)">{ICONS[icon]}</g>'
                    f'<text x="98" y="68" font-size="24" font-weight="800" fill="{TXT}">{title}</text>')
        for j, it in enumerate(items):
            yy = 128 + j * 42
            body.append(f'<circle cx="34" cy="{yy-5}" r="4" fill="{color}"/><text x="50" y="{yy}" font-size="15.5" fill="{TXT}">{esc(it)}</text>')
        body.append("</g>")
    write("features.svg", svg(W, H, "".join(body)))


# ---------------------------------------------------------------- latency
def latency():
    W, H = 1280, 400
    s = 44  # px per ms
    x0 = 300
    rows = [("USB", "Stock", 6.5, GREY), ("USB", "This firmware", 1.3, CYAN),
            ("Bluetooth", "Stock", 19.6, GREY), ("Bluetooth", "This firmware", 7.8, VIOLET)]
    body = [f'<text x="48" y="64" font-size="28" font-weight="800" fill="{TXT}">Worst-case firmware delay per key press</text>',
            f'<text x="48" y="94" font-size="15" fill="{MUTED}">Debounce + Bluetooth wake + report interval + matrix scan, estimated from the source code (lower is better).</text>']
    for ms in range(0, 21, 5):
        gx = x0 + ms * s
        body.append(f'<line x1="{gx}" y1="124" x2="{gx}" y2="338" stroke="#232b40"/><text x="{gx}" y="360" font-size="13" fill="{MUTED}" text-anchor="middle">{ms} ms</text>')
    y = 140
    for i, (grp, name, v, c) in enumerate(rows):
        if i in (0, 2):
            body.append(f'<text x="48" y="{y+30}" font-size="18" font-weight="700" fill="{TXT}">{grp}</text>')
        body.append(f'<text x="{x0-16}" y="{y+22}" font-size="15" fill="{MUTED}" text-anchor="end">{name}</text>')
        grad = 'url(#accent)' if c != GREY else GREY
        body.append(f'<rect x="{x0}" y="{y+6}" width="{v*s:.1f}" height="24" rx="6" fill="{grad}"/>')
        body.append(f'<text x="{x0+v*s+12:.1f}" y="{y+24}" font-size="16" font-weight="800" fill="{TXT if c!=GREY else MUTED}">{v} ms</text>')
        y += 46 if i in (0, 2) else 56
    body.append(f'<text x="{W-48}" y="{H-24}" font-size="13" fill="{MUTED}" text-anchor="end">USB −80 % · Bluetooth −60 %</text>')
    write("latency.svg", svg(W, H, "".join(body)))


# ---------------------------------------------------------------- architecture
def box(x, y, w, h, title, sub, color, highlight=False):
    fill = "#16223b" if highlight else CARD
    return (f'<g transform="translate({x},{y})"><rect width="{w}" height="{h}" rx="14" fill="{fill}" stroke="{color}" stroke-width="{2 if highlight else 1.2}"/>'
            f'<text x="{w/2}" y="{h/2-6}" font-size="18" font-weight="800" fill="{TXT}" text-anchor="middle">{esc(title)}</text>'
            f'<text x="{w/2}" y="{h/2+18}" font-size="13.5" fill="{MUTED}" text-anchor="middle">{esc(sub)}</text></g>')


def architecture():
    W, H = 1280, 470
    ar = f'<marker id="ar" viewBox="0 0 10 10" refX="9" refY="5" markerWidth="8" markerHeight="8" orient="auto-start-reverse"><path d="M0,0 L10,5 L0,10 z" fill="{MUTED}"/></marker>'
    b = [f'<text x="48" y="64" font-size="28" font-weight="800" fill="{TXT}">Inside the K4 Pro</text>',
         f'<text x="48" y="94" font-size="15" fill="{MUTED}">What this firmware runs on, and what it controls.</text>',
         box(48, 140, 280, 100, "Key matrix", "6 × 18 · 74HC595 shift register", CARD_STROKE),
         box(48, 300, 280, 100, "2 × CKLED2001", "LED drivers over I²C", CARD_STROKE),
         box(480, 140, 320, 260, "", "", CYAN, True),
         box(952, 140, 280, 100, "USB-C", "Full-speed USB · 1000 Hz", CARD_STROKE),
         box(952, 300, 280, 100, "CKBT51", "Bluetooth Classic · closed firmware", AMBER)]
    b.append(f'<text x="640" y="196" font-size="22" font-weight="800" fill="{TXT}" text-anchor="middle">STM32L432</text>'
             f'<text x="640" y="222" font-size="14" fill="{MUTED}" text-anchor="middle">Cortex-M4 · 48 MHz · 256 KB flash</text>'
             f'<rect x="530" y="250" width="220" height="40" rx="20" fill="url(#accent)"/>'
             f'<text x="640" y="276" font-size="15" font-weight="800" fill="#0b0f19" text-anchor="middle">this firmware runs here</text>'
             f'<text x="640" y="330" font-size="13.5" fill="{MUTED}" text-anchor="middle">QMK + Keychron Bluetooth stack</text>'
             f'<text x="640" y="352" font-size="13.5" fill="{MUTED}" text-anchor="middle">sleeps in STOP mode when idle</text>')
    lines = [(328, 190, 478, 190, "scan", False), (478, 350, 330, 350, "I²C", False),
             (800, 190, 950, 190, "USB", True), (800, 350, 950, 350, "UART 460 kbaud", True)]
    for x1, y1, x2, y2, lab, both in lines:
        st = 'marker-start="url(#ar)" ' if both else ""
        b.append(f'<path d="M{x1},{y1} L{x2},{y2}" stroke="{MUTED}" stroke-width="2" fill="none" {st}marker-end="url(#ar)"/>'
                 f'<text x="{(x1+x2)/2}" y="{y1-12}" font-size="13" fill="{MUTED}" text-anchor="middle">{lab}</text>')
    b.append(f'<text x="48" y="{H-28}" font-size="13.5" fill="{MUTED}">The Bluetooth module runs its own closed firmware: radio range and stability can only be tuned through its settings.</text>')
    write("architecture.svg", svg(W, H, "".join(b), ar))


# ---------------------------------------------------------------- flashing steps
def flashing():
    W, H = 1280, 300
    steps = [("1", "Switch to Cable", "Flip the switch on the back to Cable, then unplug the keyboard."),
             ("2", "Hold Esc + plug in", "Keep Esc pressed while plugging the cable back in. The keyboard stays dark."),
             ("3", "Flash", "QMK Toolbox shows “STM32 DFU device connected”. Open the .bin, click Flash."),
             ("4", "Reset", "Hold Fn + J + Z for 3 s. Done — re-pair Bluetooth if needed.")]
    cw, gap, x0 = 284, 24, 40
    b = []
    colors = [CYAN, VIOLET, PINK, GREEN]
    for i, (n, title, text) in enumerate(steps):
        x = x0 + i * (cw + gap)
        b.append(f'<g transform="translate({x},36)"><rect width="{cw}" height="228" rx="16" fill="{CARD}" stroke="{CARD_STROKE}"/>'
                 f'<circle cx="46" cy="50" r="22" fill="{colors[i]}"/><text x="46" y="58" font-size="22" font-weight="800" fill="#0b0f19" text-anchor="middle">{n}</text>'
                 f'<text x="24" y="114" font-size="21" font-weight="800" fill="{TXT}">{esc(title)}</text>')
        # naive word wrap
        words, line, yy = text.split(), "", 148
        for w_ in words:
            if len(line) + len(w_) > 30:
                b.append(f'<text x="24" y="{yy}" font-size="15" fill="{MUTED}">{esc(line.strip())}</text>')
                line, yy = "", yy + 22
            line += w_ + " "
        b.append(f'<text x="24" y="{yy}" font-size="15" fill="{MUTED}">{esc(line.strip())}</text></g>')
        if i < 3:
            ax = x + cw + 4
            b.append(f'<path d="M{ax},150 l14,0 m-6,-6 l6,6 l-6,6" stroke="{MUTED}" stroke-width="2.5" fill="none" stroke-linecap="round"/>')
    write("flashing.svg", svg(W, H, "".join(b)))


# ---------------------------------------------------------------- variant picker
def variants():
    W, H = 1280, 330
    b = [f'<text x="48" y="58" font-size="26" font-weight="800" fill="{TXT}">Which file do I need?</text>',
         f'<text x="48" y="86" font-size="15" fill="{MUTED}">Look at the Enter key, then at the backlight colour.</text>']
    u = 46
    # ISO enter
    def enter_iso(x, y):
        return (f'<path d="M{x},{y} h{1.5*u} v{2*u} h{-1.25*u} v{-u} h{-0.25*u} Z" fill="#232a40" stroke="{CYAN}" stroke-width="2.5" stroke-linejoin="round"/>'
                f'<text x="{x+0.95*u}" y="{y+0.62*u}" font-size="14" fill="{TXT}" text-anchor="middle">Enter</text>')
    def enter_ansi(x, y):
        return (f'<rect x="{x}" y="{y+u*0.5}" width="{2.25*u}" height="{u}" rx="6" fill="#232a40" stroke="{VIOLET}" stroke-width="2.5"/>'
                f'<text x="{x+1.12*u}" y="{y+1.1*u}" font-size="14" fill="{TXT}" text-anchor="middle">Enter</text>')
    cards = [(48, "ISO", "Tall Enter · extra key by left Shift", enter_iso, CYAN, "iso"),
             (452, "ANSI", "Wide, single-row Enter key", enter_ansi, VIOLET, "ansi")]
    for x, name, sub, fn, c, slug in cards:
        b.append(f'<g transform="translate({x},112)"><rect width="380" height="186" rx="16" fill="{CARD}" stroke="{CARD_STROKE}"/>'
                 f'<text x="24" y="44" font-size="24" font-weight="800" fill="{c}">{name}</text>'
                 f'<text x="24" y="70" font-size="13.5" fill="{MUTED}">{esc(sub)}</text>{fn(262, 86)}'
                 f'<text x="24" y="118" font-size="13" fill="{TXT}" font-family="{MONO}">…_{slug}_rgb_…bin</text>'
                 f'<text x="24" y="142" font-size="13" fill="{TXT}" font-family="{MONO}">…_{slug}_white_…bin</text></g>')
    b.append(f'<g transform="translate(856,112)"><rect width="376" height="186" rx="16" fill="{CARD}" stroke="{CARD_STROKE}"/>'
             f'<text x="24" y="44" font-size="24" font-weight="800" fill="{TXT}">Backlight</text>'
             f'<rect x="24" y="72" width="150" height="40" rx="10" fill="url(#accent)"/><text x="99" y="98" font-size="15" font-weight="800" fill="#0b0f19" text-anchor="middle">RGB → _rgb_</text>'
             f'<rect x="196" y="72" width="156" height="40" rx="10" fill="#e9edf5"/><text x="274" y="98" font-size="15" font-weight="800" fill="#0b0f19" text-anchor="middle">White → _white_</text>'
             f'<text x="24" y="150" font-size="13.5" fill="{MUTED}">Wrong file? No permanent harm: flash</text>'
             f'<text x="24" y="170" font-size="13.5" fill="{MUTED}">the right one the same way.</text></g>')
    write("variants.svg", svg(W, H, "".join(b)))


hero()
features()
latency()
architecture()
flashing()
variants()
