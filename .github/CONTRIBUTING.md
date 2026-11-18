# Contributing

Thanks for helping make the K4 Pro better!

## Reporting a problem or an idea
Open an [issue](https://github.com/alban-minana/keychron-k4-pro-firmware/issues/new/choose) using the templates.
Always mention your **variant** (ISO/ANSI, RGB/White), the **firmware version** and the **connection mode** (USB / Bluetooth).

## Pull requests
1. Fork the repository and create a branch from `main`.
2. Keep changes focused: one fix or feature per PR.
3. Make sure it builds: `k4pro/build.sh` (Docker) — CI builds all 4 variants on every PR.
4. Say in the PR how you tested it, and on which variant. Untested hardware changes are welcome, but say so.
5. Only the maintainer can merge. Changes to Bluetooth / low-power code get extra scrutiny because they are hard to test.

## Code layout
- `keyboards/keychron/k4_pro/` — board code (matrix, config, tuning options)
- `keyboards/keychron/bluetooth/` — Keychron's Bluetooth / battery / low-power stack (CKBT51 module)
- `k4pro/` — build tooling, docs, analysis
- Comments tagged `FIX:` / `IMPROVEMENT:` mark changes from the stock Keychron code.
