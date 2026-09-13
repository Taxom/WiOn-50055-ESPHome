# WiOn 50055 ESPHome v1.1.0

## Highlights

- Replaced the factory-offset × gain calibration chain with direct raw-domain calibration.
- Factory calibration constants from the stock firmware are no longer required.
- Added local reference-value calibration for voltage, current, power and energy.
- Added persistent no-load current-zero calibration.
- Improved low-power measurement using an H-channel 60-second average-power estimator.
- Improved measurement filtering and raw diagnostics.
- Fixed ESP8266 Wi-Fi stability by keeping interrupts enabled during the stock bridge read.
- Retains encrypted Home Assistant API, authenticated web UI, OTA, fallback AP, restore modes and outlet/backlight control.

## Calibration

Fresh installs use reference defaults measured on the development unit. Individual plugs can be refined from the local web UI.

A backup of the original 1 MiB flash is still strongly recommended for recovery, but it is no longer required for ESPHome power calibration.

## Upgrade note

v1.1.0 uses a different calibration model from v1.0.0. After upgrading, verify the measurements and recalibrate if necessary.

## Requirements

- Woods WiOn 50055
- ESP8266EX / 1 MiB flash
- ESPHome 2026.8.2 or newer
- `wion_power_bridge.h` from the same release
