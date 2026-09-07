# v1.0.0 — 2026-09-07

First production release for the Woods WiOn 50055 ESPHome replacement firmware.

## Highlights

- Retains the stock power-monitor bridge instead of replacing the metering hardware.
- Home Assistant encrypted native API and local authenticated web UI.
- Configurable outlet restore policy: Always Off, Always On, Restore Last State.
- Protected fallback AP/captive portal and 30-second physical-button recovery action.
- Calibrated voltage/current/power/energy measurement.
- Current zero implemented as a real offset, with no wide low-current deadband.
- Added 60-second H-derived average active power for loads below the instantaneous
  W-channel floor.
- Energy is RAM-only; no periodic energy writes to SPI flash.
- Uptime, reset reason, RSSI and raw bridge diagnostics.
- OTA and ESPHome safe-mode recovery.

## Final development-unit calibration

- Current gain: 1.041415
- Energy gain: 1.138468
- Current zero raw: 443876 (~0.0163 A equivalent)
- Power gain: 1.031000
- Voltage gain: 1.000000

## Known limitation

The stock bridge's instantaneous W channel can return zero at very low active
power. The firmware exposes `Average Power` derived from the H/energy channel to
cover this range.

The normal Wi‑Fi infrastructure AP loss/rejoin path was not explicitly
stress-tested before tagging v1.0.0.
