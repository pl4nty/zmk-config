# zmk-config
Firmware for my custom mechanical keyboards. Built with [ZMK Firmware](https://zmk.dev), graphics by [keymap-drawer](https://github.com/caksoylar/keymap-drawer).

## [Sofle](https://github.com/josefadamcik/SofleKeyboard) v1.0 + [nice!nano](https://nicekeyboards.com/nice-nano) v2.0

[![sofle-keymap](docs/sofle.svg)](https://keymap-drawer.streamlit.app/?zmk_url=https%3A%2F%2Fgithub.com%2Fpl4nty%2Fzmk-config%2Fblob%2Fmain%2Fconfig%2Fsofle.keymap)

## [Gboard Yunomi](https://github.com/google/mozc-devices/tree/main/mozc-yunomi) (Pro Micro base PCB) + nice!nano v2.0

Replaces the Pro Micro with a nice!nano. Each key types `U+XXXX`, Space, Space, Enter, like the stock firmware, so the host must run Google Japanese Input (Mozc). For a JIS host, set `UPLUS` to `LS(SEMI)` in [`yunomi.keymap`](config/boards/shields/yunomi/yunomi.keymap). Hold the top-left two keys (`@` `+`) for Bluetooth profiles, USB/BLE toggle, bootloader and reset.
