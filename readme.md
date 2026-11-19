<img src="k4pro/docs/banner.png" alt="Keychron K4 Pro community firmware" width="100%">

<p align="center">
  <a href="https://github.com/alban-minana/keychron-k4-pro-firmware/releases"><img src="https://img.shields.io/github/v/release/alban-minana/keychron-k4-pro-firmware?include_prereleases&label=release&style=flat-square&color=18181b" alt="Release"></a>
  <a href="https://github.com/alban-minana/keychron-k4-pro-firmware/actions/workflows/build.yml"><img src="https://img.shields.io/github/actions/workflow/status/alban-minana/keychron-k4-pro-firmware/build.yml?branch=main&label=build&style=flat-square" alt="Build"></a>
  <a href="license_GPLv2.md"><img src="https://img.shields.io/badge/license-GPL--2.0%2B-18181b?style=flat-square" alt="License"></a>
</p>

Unofficial firmware for the **Keychron K4 Pro**: ISO and ANSI, RGB and white. It is built on Keychron's open-source code and QMK 0.34, stays compatible with VIA, and you can go back to stock at any time.

#### Why

- **Lower latency**: keys are sent on first contact, and Bluetooth runs at 133 Hz.
- **Bluetooth fixes**: no more stuck or repeating keys, and no lost key on wake-up.
- **Better battery**: the backlight turns off sooner and the battery percentage is accurate.

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="k4pro/docs/latency-dark.svg">
  <img src="k4pro/docs/latency-light.svg" alt="Worst-case delay per key press: USB 6.5 ms to 1.3 ms, Bluetooth 19.6 ms to 7.8 ms" width="100%">
</picture>

#### Install

1. Download the `.bin` for your keyboard from the [latest release](https://github.com/alban-minana/keychron-k4-pro-firmware/releases).
2. Install [QMK Toolbox](https://github.com/qmk/qmk_toolbox/releases).
3. Set the switch to **Cable**, then plug the keyboard in while holding <kbd>Esc</kbd>.
4. When QMK Toolbox shows *STM32 DFU device connected*, open the `.bin` and click **Flash**.

| Keyboard | File |
|---|---|
| ISO (tall Enter), RGB | `keychron_k4_pro_iso_rgb_<version>.bin` |
| ISO (tall Enter), white | `keychron_k4_pro_iso_white_<version>.bin` |
| ANSI (wide Enter), RGB | `keychron_k4_pro_ansi_rgb_<version>.bin` |
| ANSI (wide Enter), white | `keychron_k4_pro_ansi_white_<version>.bin` |

> [!NOTE]
> Holding <kbd>Esc</kbd> also resets the keyboard settings, so redo your VIA remapping afterwards.
> To go back to stock, flash [Keychron's firmware](https://github.com/Keychron/qmk_firmware/tree/bluetooth_playground/keyboards/keychron/k4_pro/firmware) the same way.

#### Changes

| Area | Stock | This firmware |
|---|---|---|
| Debounce | Every press waits 5 ms | Sent on first contact |
| Bluetooth rate | 90 Hz | 133 Hz |
| Bluetooth send | 3 ms pause before each report | Pause only after 1.5 s idle |
| Matrix scan | 2 shift-register writes per column | 1 write per column |
| LED refresh | Sent in one block, keys not read for up to 9 ms | Sent in small pieces between scans |
| Battery read (Bluetooth) | 3 ms pause every few seconds | No pause while typing |
| Key pressed during reconnect | Lost | Sent once connected (≤ 3 s) |
| Backlight off on battery | 10 min, cuts out | 3 min, dims 10 s before |
| Battery % (<kbd>Fn</kbd>+<kbd>B</kbd>) | Linear estimate | Li-ion curve, faster animation |
| Caps Lock / Num Lock key (RGB) | White | Green |

**Fixed bugs**
- Stuck or repeating keys over Bluetooth after a full module buffer
- Media keys that kept repeating
- Bluetooth pairing code running inside an interrupt
- Possible memory corruption on a garbled Bluetooth packet
- NKRO setting ignored after PIN pairing
- Cable/Bluetooth switch not debounced (could flip modes while sliding)

Details: [k4pro/ANALYSIS.md](k4pro/ANALYSIS.md).

#### Settings

Options live at the top of [`keyboards/keychron/k4_pro/config.h`](keyboards/keychron/k4_pro/config.h).

| Option | Default | |
|---|---|---|
| `DEBOUNCE` | `5` | Raise to 7–8 if a switch double-types |
| `CKBT51_REPORT_RATE` | `133` | `90` = stock |
| `CKBT51_SMART_WAKE` | on | Disable if letters go missing over Bluetooth |
| `CONNECTED_BACKLIGHT_DISABLE_TIMEOUT` | `180` | Backlight off delay on battery (s) |
| `BACKLIGHT_DIM_PERCENT` | `30` | Brightness during the 10 s before it turns off |
| `CAPS_LOCK_COLOR` / `NUM_LOCK_COLOR` | `0, 255, 0` | Lock key colours on RGB boards (R, G, B) |
| `BT_WAKE_KEY_BUFFER_MS` | `3000` | Keep keys typed during reconnect (`0` = off) |
| `BT_REPORT_BATTERY_LEVEL` | on | Send the battery level to the Bluetooth module (experimental: not shown by Windows) |
| `BT_IDLE_DISCONNECT_S` | `7200` | Bluetooth disconnects after this idle time (s) |
| `SNLED27351_INCREMENTAL_FLUSH` | on | Send the LED frame in small pieces (remove to restore stock) |
| `SNLED27351_FLUSH_CHUNKS_PER_STEP` | `1` | LED transfers per main-loop pass (higher = smoother LEDs, longer pauses) |

#### Build

```sh
k4pro/build.sh            # all variants, needs Docker
k4pro/build.sh iso/rgb    # one variant
make -C k4pro/tests       # unit tests of the board logic, needs only gcc
```

GitHub Actions builds every push and pull request. Pushing a `v*` tag publishes a release.

#### Diagnostics

[`k4pro/tools/k4tool.py`](k4pro/tools/k4tool.py) reads the keyboard over the USB cable (Windows, Python 3, `py -m pip install hidapi`, VIA / Launcher closed). The switch can stay on Bluetooth: keys still go over the radio.

```sh
py k4pro/tools/k4tool.py info       # connection, battery, scan rate, settings, counters
py k4pro/tools/k4tool.py watch      # live dashboard + double-type detector
py k4pro/tools/k4tool.py latency    # 300 key taps, median / p95 / max latency
py k4pro/tools/k4tool.py ab-flush   # stock vs incremental LED flush, toggled live
py k4pro/tools/k4tool.py profile    # longest run of each firmware section, in microseconds
```

Measured on an ISO RGB board (v0.6.0, RGB effect on, 600 taps per row, PC request to key in Windows):

| | Median | 95 % under | Worst |
|---|---|---|---|
| USB, stock LED flush | 2.4 ms | 8.9 ms | 9.9 ms |
| USB, this firmware | 2.2 ms | 3.4 ms | 3.5 ms |
| Bluetooth, this firmware | 12.7 ms | 24 ms | 35 ms |

Longest pause of the main loop in Bluetooth mode (`profile`, 500 taps): 5.7 ms in v0.6.0, 1.8 ms in v0.7.0.

#### Hardware

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="k4pro/docs/hardware-dark.svg">
  <img src="k4pro/docs/hardware-light.svg" alt="Key matrix and LED drivers connected to the STM32L432, which talks to USB-C and to the CKBT51 Bluetooth module" width="100%">
</picture>

#### Contributing

Open an [issue](https://github.com/alban-minana/keychron-k4-pro-firmware/issues/new/choose) with your variant and connection mode. Pull requests are welcome; CI must pass. See the [roadmap](https://github.com/alban-minana/keychron-k4-pro-firmware/issues).

<sub>Based on [Keychron/qmk_firmware](https://github.com/Keychron/qmk_firmware) and [QMK](https://github.com/qmk/qmk_firmware). GPL-2.0-or-later. Not affiliated with Keychron.</sub>
