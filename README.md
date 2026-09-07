# Woods WiOn 50055 — ESPHome firmware v1.0.0

Replacement ESPHome firmware for the Woods WiOn 50055 Wi‑Fi outlet while retaining
the stock power-monitor bridge and the original relay, button, LEDs/backlight and
power-measurement hardware.

## Target hardware

Validated on the development unit with:

- ESP8266EX
- 1 MiB SPI flash
- Relay: GPIO15
- Outlet backlight: GPIO14
- Physical button: GPIO13
- Wi‑Fi status LED: GPIO2
- Stock power-monitor bridge clock: GPIO0
- Stock power-monitor bridge data: GPIO12

ESPHome 2026.8.2 or newer is required.

## Main features

- Home Assistant native encrypted API
- Local authenticated ESPHome web UI
- OTA updates
- Password-protected fallback AP + captive portal
- 30-second physical-button hold to start Wi‑Fi setup AP
- Short physical-button press toggles the outlet
- Configurable power restore mode:
  - Always Off
  - Always On
  - Restore Last State
- Outlet backlight control
- Voltage, current, active power, apparent power and power factor
- Energy (`total_increasing`) for Home Assistant
- 60-second average active power derived from the energy/H channel for very small loads
- Wi‑Fi RSSI, uptime and reset reason diagnostics
- Local calibration controls and raw bridge diagnostics

## Persistence policy

Flash is used only for infrequently changed preferences such as calibration,
restore mode and restorable switch state. Energy and average-power accumulators
are RAM-only and are not periodically checkpointed to flash.

`Energy` therefore restarts from zero when the ESP8266 reboots or loses power.
Home Assistant receives it as `state_class: total_increasing`, so long-term
statistics can account for counter resets.

## Calibration

The calibration UI is intended for setup/service, not routine use.

1. Turn `Outlet` OFF and run `Calibration - Zero Calibrate`.
2. For current calibration, use a high-power, near-resistive load and enter the
   external reference meter reading in `Calibration - Current Reference`, then
   press `Calibration - Current Calibrate`.
3. Voltage and power can be calibrated the same way if required.
4. For energy, reset the energy calibration run, accumulate a useful amount
   (for example 0.1–0.2 kWh), enter the external meter result, then calibrate energy.

Important: an external plug-in wattmeter can add its own current, especially if
it uses a capacitive-dropper supply. This can materially distort low-current
calibration. Prefer a high-power resistive load for current/power calibration.

### Reference-unit defaults in v1.0.0

These are the final measured defaults for the development unit:

- Voltage gain: 1.000000
- Current gain: 1.041415
- Power gain: 1.031000
- Energy gain: 1.138468
- Raw current zero: 443876 (about 0.0163 A factory-equivalent offset)

Other physical units may require individual calibration.

## Low-power behavior

The stock bridge can report `Raw W = 0` at very low active power even while the
energy/H channel continues to accumulate. This is upstream of the ESPHome
calculation. `Average Power` derives active power from the H-channel over about
60 seconds so standby loads can still be measured.

Current uses a calibrated offset:

`I_final = max(0, (I_factory - I_zero) * current_gain)`

There is no wide current deadband or hysteresis, so small real loads are not
intentionally discarded.

## Build

Keep these files together:

- `wion_50055_v1.0.0.yaml`
- `wion_power_bridge.h`
- `secrets.yaml`

Validate and compile:

```powershell
python -m esphome config .\wion_50055_v1.0.0.yaml
python -m esphome compile .\wion_50055_v1.0.0.yaml
```

OTA update:

```powershell
python -m esphome upload .\wion_50055_v1.0.0.yaml --device wion-50055.local
```

## Validation completed before v1.0.0

Validated on the development unit:

- zero-offset calibration
- calibrated high-current resistive load measurement
- energy calibration over 0.200 kWh
- low-power/standby current and H-derived average active power
- Home Assistant encrypted API
- local web UI
- OTA
- protected setup AP / captive portal
- 30-second setup-AP button action
- short-press outlet control
- Always Off / Always On / Restore Last State
- cold power-cycle persistence of restore mode, outlet state and calibration
- reset-reason and uptime diagnostics

Not explicitly stress-tested before v1.0.0: prolonged loss and recovery of the
normal 2.4 GHz infrastructure AP while the outlet remains powered. Wi‑Fi and API
`reboot_timeout` are both disabled, so the firmware is not intentionally
configured to reboot on connectivity loss.
