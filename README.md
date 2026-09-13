# Woods WiOn 50055 — ESPHome firmware v1.1.0

Replacement ESPHome firmware for the Woods WiOn 50055 Wi‑Fi outlet. It keeps the
stock power-monitor bridge and original relay, button, LEDs/backlight and metering
hardware.

## Target hardware

Validated on a Woods WiOn 50055 with:

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
- Authenticated local ESPHome web UI
- OTA updates
- Password-protected fallback AP + captive portal
- Short physical-button press toggles the outlet
- 30-second button hold starts the Wi‑Fi setup AP
- Configurable power restore mode: Always Off, Always On, Restore Last State
- Outlet backlight control
- Voltage, current, active power, apparent power and power factor
- Energy sensor with `total_increasing` state class
- 60-second H-channel average power for very small loads
- Wi‑Fi RSSI, uptime and reset-reason diagnostics
- Local calibration controls and raw bridge diagnostics

## Power-monitor bridge

The ESP8266 does not read the mains metering IC directly. The stock WiOn hardware
contains an intermediate bridge. The ESP8266 supplies its clock on GPIO0 and reads
its data on GPIO12.

A valid frame contains four 32-bit words:

```text
48xxxxxx  49xxxxxx  57xxxxxx  56xxxxxx
    H         I         W         V
```

| Tag | Field | Meaning |
|---|---|---|
| `0x48` | `H` | Energy increment |
| `0x49` | `I` | Current |
| `0x57` | `W` | Active power |
| `0x56` | `V` | Voltage |

`wion_power_bridge.h` intentionally leaves ESP8266 interrupts enabled while reading
the frame. Do not wrap the complete 128-bit read in `noInterrupts()`: the transaction
is about 9 ms long and blocking interrupts for the whole frame can interfere with the
ESP8266 Wi‑Fi/SDK stack. Because the ESP8266 supplies the clock, an interrupt may
stretch a clock phase without advancing the bridge to the next bit.

## Direct raw-domain calibration

v1.1.0 no longer requires the factory `HW_OFFSET`, `A_OFFSET`, `W_OFFSET` or
`V_OFFSET` values from the original firmware. A stock-flash backup is still strongly
recommended so the device can be restored, but it is not needed to configure the
ESPHome metering code.

The runtime conversion uses directly calibrated coefficients:

```text
Voltage [V] = K_V / RawV
Current [A] = max(0, K_I × (1 / RawI - 1 / RawI0))
Power [W]   = K_P / RawW
Energy [kWh] += RawH × K_E
```

Fresh-install reference defaults from the development unit are:

```text
K_V    = 322499.908463
K_I    = 7516.962365203
K_P    = 4618968.684198736
K_E    = 0.000001307873567336
RawI0  = 443876
```

These defaults should provide usable initial readings, but individual units can be
refined from the local web UI.

### Calibration procedure

1. Turn `Outlet` OFF and run `Calibration - Zero Calibrate`.
2. Apply a high-power, near-resistive load.
3. Enter the external meter value in `Calibration - Current Reference` and press
   `Calibration - Current Calibrate`.
4. Calibrate voltage and power the same way if required.
5. For energy, reset the calibration run, accumulate a useful amount such as
   0.1–0.2 kWh, enter the external meter result, then run energy calibration.

A plug-in reference wattmeter can contribute its own current. Use a reasonably large
resistive load for current/power calibration so that this error is insignificant.

## Low-power behavior

At very small loads the stock bridge can report `Raw W = 0` while the H/energy
channel continues to increment. `Average Power` derives active power from H over
about 60 seconds so standby consumption remains measurable below the useful range of
the instantaneous W channel.

## Persistence

Calibration values, restore mode and the restorable outlet state are stored as
preferences. Energy and average-power accumulators are RAM-only and are not
periodically written to flash.

`Energy` therefore restarts from zero after an ESP8266 reboot. Home Assistant sees it
as `state_class: total_increasing`, allowing long-term statistics to handle resets.

## Before flashing stock firmware

Make a full 1 MiB backup before erasing the original firmware. Two independent dumps
with matching SHA256 hashes are recommended:

```powershell
python -m esptool --chip esp8266 --port COM19 read-flash 0x000000 0x100000 wion_original_1.bin
python -m esptool --chip esp8266 --port COM19 read-flash 0x000000 0x100000 wion_original_2.bin

Get-FileHash .\wion_original_1.bin -Algorithm SHA256
Get-FileHash .\wion_original_2.bin -Algorithm SHA256
```

Replace `COM19` with the actual serial port.

## Build

Copy `secrets.example.yaml` to `secrets.yaml` and replace the example credentials.
Keep these files together:

- `wion_50055_v1.1.0.yaml`
- `wion_power_bridge.h`
- `secrets.yaml`

Validate and compile:

```powershell
python -m esphome config .\wion_50055_v1.1.0.yaml
python -m esphome compile .\wion_50055_v1.1.0.yaml
```

OTA update:

```powershell
python -m esphome upload .\wion_50055_v1.1.0.yaml --device wion-50055.local
```

## Upgrading from v1.0.0

v1.1.0 replaces the old factory-offset × gain conversion chain with direct
raw-domain coefficients. The old gain preferences are not imported automatically.
After upgrading from v1.0.0, verify the readings and run the local calibration steps
if necessary.

## Known hardware note

A heavy load on the built-in USB charging output can disturb the outlet's internal
power rail. The development unit reset when the USB output was loaded to roughly
3 A. Avoid using the USB ports for high-current charging if reliable outlet control
is required.

## Acknowledgements and prior work

This project builds on reverse-engineering work documented in:

- [Woods WiOn 50055 WiFi Plug Hardware Hacking / Tasmota Installation](https://www.tarball.ca/posts/woods-wion-50055-wifi-plug-hacking/)
- [Home Assistant Community: Decoding Power Bar with Energy Monitoring](https://community.home-assistant.io/t/decoding-power-bar-with-energy-monitoring/622841)

Those sources provided the hardware teardown, GPIO mapping and early documentation of
the GPIO0/GPIO12 bridge protocol. This project retains the stock bridge and adds a
complete ESPHome implementation, direct calibration, Home Assistant integration,
OTA/recovery features, restore modes and low-power energy-derived average power.
