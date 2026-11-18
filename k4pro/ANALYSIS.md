# Technical analysis of the stock K4 Pro firmware

Source reviewed: [Keychron/qmk_firmware](https://github.com/Keychron/qmk_firmware), branch `bluetooth_playground`
(last K4 Pro commit: 2024-12-07), on a QMK base from early 2023.

## Hardware, as seen from the code

- **MCU**: STM32L432 (Cortex-M4, ultra-low-power), clocked at 48 MHz (`mcuconf.h`).
- **Bluetooth**: separate CKBT51 module (Bluetooth Classic only), driven over UART at 460 800 baud. Its firmware is closed: the MCU can only send commands and read events.
- **Matrix**: 6 rows × 18 columns; 2 columns on GPIOs, 16 through a 74HC595 shift register (`matrix.c`).
- **Lighting**: two CKLED2001 drivers over I²C (101 RGB LEDs on ISO RGB).
- **Switches**: Mac/Win on `A8`, Cable/BT on `A10`.

## Bugs

| # | Severity | File | Problem | Status |
|---|---|---|---|---|
| B1 | High | `bluetooth/bluetooth.c` | 6KRO fast path did not record the sent report. On `ACK_FIFO_FULL`, `report_buffer_task()` re-sent the previous buffered report and the rejected one was lost — typically a key release, giving a stuck/repeating key over BT. A pending retry could also re-send an old report after a newer one. | Fixed |
| B2 | Medium | `bluetooth/bluetooth.c` | Same for consumer (media) reports. | Fixed |
| B3 | Medium | `k4_pro/k4_pro.c` | Long-press pairing started from a ChibiOS virtual-timer callback (ISR context): 3 ms busy-wait + `sdWrite()` (not ISR-safe) + race on `bt_state`. Now timed in `matrix_scan_kb()`. | Fixed |
| B4 | Medium | `bluetooth/ckbt51.c` | RX packet length from the module was only checked against its complement; `len > 32` overflowed a 32-byte stack buffer, `len < 3` underflowed. | Fixed |
| B5 | Low | `bluetooth/indicator.c` | `last_host_index` starts at `0xFF` → `host_led_matrix_list[254]` read out of bounds. | Fixed |
| B6 | Low | `bluetooth/bluetooth.c` | Leaving PIN-code entry forced `keymap_config.nkro = true`, ignoring the user's BT NKRO setting. | Fixed |
| B7 | Latent | `bluetooth/bluetooth.c` | `bluetooth_hid_set_protocol()` ignored its argument (event never emitted by CKBT51 today). | Fixed |
| B8 | Cosmetic | `bluetooth/bat_level_animation.c` | `<=` loop bound (harmless, driver bounds-checks). | Fixed |
| B9 | Info | `k4_pro/k4_pro.c` | Cable/BT switch read twice back-to-back, no real debounce. | Open |
| B10 | Info | `bluetooth/lpm_stm32l432.c` | `LOW_POWER_RUN_MODE_ENABLE` code does not compile (dead code). | Open |

## Latency

| Stage | Stock | This firmware |
|---|---|---|
| Debounce | `sym_defer_g` 5 ms on every press | `sym_eager_pk`: 0 ms on press |
| BT wake pulse | 3 ms blocking before every UART packet | Skipped for HID reports if a packet was sent < 1.5 s ago (module sleeps its UART after 3 s); reset on boot, MCU wake-up and connection events |
| BT report rate | 90 Hz | 133 Hz (documented module value) |
| Matrix scan | 18 × (2 × 16-bit shift + 2 × 100-cycle delay) | 18 × (1 × 16-bit shift + 1 × delay) |
| USB polling | 1 ms | 1 ms (full-speed USB minimum) |

## Battery

Main consumers: the RGB backlight (by far), the CKBT51 radio, then the MCU.
`lpm_task()` only lets the MCU enter STOP1 once the LED drivers are shut down, so the backlight timeout directly controls MCU sleep.

| Lever | Status |
|---|---|
| Backlight timeout on BT 600 s → 180 s | Done |
| No 3 ms busy-wait per report | Done |
| Faster matrix scan | Done |
| BT idle disconnect (`connected_idle_timeout` 7200 s) | Open — trade-off with first-keystroke latency |
| STOP2 instead of STOP1 | Not planned — µA-level gain, negligible vs. the radio |
| Non-linear Li-ion voltage → % curve | Planned |
| Report battery level to the host OS | Planned |

## Not fixable here

- CKBT51 firmware (radio stability, range, reconnection speed) is closed.
- The QMK base is from early 2023; porting Keychron's Bluetooth stack to current QMK is a larger project.
