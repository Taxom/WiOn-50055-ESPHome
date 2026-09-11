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

> Hardware photos, PCB photos and the programming-pad pinout will be added later.

### USB output power caveat

The development unit was observed to reset with `Power On` as the reset reason when
an approximately 3 A load was drawn from its USB charging output. This indicates that
the ESP8266/control electronics are not sufficiently isolated from a heavy USB load.
The USB output is therefore not recommended for high-current charging when reliable
outlet operation matters. This is a stock hardware/power-distribution limitation,
not an ESPHome reset policy.

## How the stock power-monitor bridge works

The ESP8266 does **not** measure mains voltage/current directly and does not talk
directly to the metering IC. The original WiOn design contains a separate stock
power-monitor bridge between the mains metering circuitry and the ESP8266.

The ESP8266 clocks the bridge on GPIO0 and reads data on GPIO12. A valid measurement
frame contains four 32-bit words in this order:

```text
48xxxxxx  49xxxxxx  57xxxxxx  56xxxxxx
    H         I         W         V
```

The upper byte is a tag; the lower 24 bits are the measurement payload:

| Tag | Field | Meaning |
|---|---|---|
| `0x48` | `H` | Energy increment / accumulation input |
| `0x49` | `I` | Current |
| `0x57` | `W` | Active power |
| `0x56` | `V` | Voltage |

The firmware keeps the original bridge intact and converts these raw values instead
of bypassing the bridge or rewiring the metering section.

### Bridge timing and ESP8266 Wi‑Fi stability

An earlier bridge reader disabled interrupts for the entire 128-bit transaction.
With a 35 us low phase and 35 us high phase per bit, that blocked ESP8266 interrupt
servicing for at least:

```text
128 × (35 us + 35 us) = 8.96 ms
```

That implementation was associated with intermittent ESP8266 exceptions/watchdog
resets and occasional local web-UI stalls during extended testing.

The current `wion_power_bridge.h` intentionally leaves interrupts enabled while the
frame is read. The ESP8266 supplies the clock on GPIO0, so a Wi‑Fi/SDK interrupt may
stretch an individual clock phase but does not advance the bridge to the next bit.
The reader still validates every frame by checking the expected `0x48`, `0x49`,
`0x57`, `0x56` tags.

On the development unit this change preserved valid power-monitor frames, improved
web-UI responsiveness, and completed more than 24 hours of continuous operation
without the unexplained exception/watchdog resets seen with the long interrupt lock.
A deliberate power interruption during that period was separately identified by a
`Power On` reset reason and was not a firmware crash.

The stock conversion equations recovered during reverse engineering are:

```text
Voltage [V] = 100000000000 / (RawV × V_OFFSET)

Current_factory [A] = 1000000000 / (RawI × A_OFFSET)

Power [W] = 1000000000000 / (RawW × W_OFFSET)

Energy_uncalibrated [kWh] =
    sum(RawH) × 100 / HW_OFFSET
```

ESPHome then applies the user calibration. Current additionally uses the calibrated
no-load offset:

```text
I_final = max(0, (I_factory - I_zero) × current_gain)
```

There is no wide low-current deadband or hysteresis.

## Factory calibration data — back up the original flash first

> **Important: make a full backup of the original 1 MiB flash before erasing or
> flashing ESPHome.**
>
> The stock firmware contains the factory conversion constants used by the
> power-monitor bridge. It is not yet known whether these constants are identical
> across every WiOn 50055 or individually calibrated at the factory. Once the stock
> flash has been erased, the original values cannot be recovered unless a backup
> was made first.

Create **two** independent dumps before changing the device:

```powershell
python -m esptool --chip esp8266 --port COM19 read-flash 0x000000 0x100000 wion_original_1.bin
python -m esptool --chip esp8266 --port COM19 read-flash 0x000000 0x100000 wion_original_2.bin

Get-FileHash .\wion_original_1.bin -Algorithm SHA256
Get-FileHash .\wion_original_2.bin -Algorithm SHA256
```

Replace `COM19` with the actual serial port. The two SHA256 hashes should match.

For the development unit, both original dumps were identical:

```text
SHA256
7AB8DFA85F3D397D4D9EF0C00C8AE22F5BD2B287C7B2D5F9D11DD9D2D838029D
```

### Factory calibration table in the verified stock dump

In the verified 1 MiB stock image, a contiguous 16-byte little-endian table starts
at flash offset `0x07D228`:

| Flash offset | Name | 32-bit LE bytes | Decimal value |
|---:|---|---|---:|
| `0x07D228` | `HW_OFFSET` | `54 3C 30 05` | `87047252` |
| `0x07D22C` | `A_OFFSET` | `2E 1D 02 00` | `138542` |
| `0x07D230` | `W_OFFSET` | `EA 67 03 00` | `223210` |
| `0x07D234` | `V_OFFSET` | `14 B6 04 00` | `308756` |

These offsets are **verified for the stock firmware image above**. A different stock
firmware revision may place the table elsewhere, so keep the full dump even if the
values at these addresses look plausible.

`HW_OFFSET` also occurs elsewhere in the stock image at `0x00E124`; that occurrence
is not part of the contiguous calibration table. Use the four-value block beginning
at `0x07D228`.

You can extract the table directly from a backup with Python:

```powershell
@'
from pathlib import Path
import struct

data = Path("wion_original_1.bin").read_bytes()

for name, offset in {
    "HW_OFFSET": 0x07D228,
    "A_OFFSET":  0x07D22C,
    "W_OFFSET":  0x07D230,
    "V_OFFSET":  0x07D234,
}.items():
    value = struct.unpack_from("<I", data, offset)[0]
    print(f"{name:9s}  0x{offset:06X}  {value}")
'@ | python -
```

Expected output for the development unit:

```text
HW_OFFSET  0x07D228  87047252
A_OFFSET   0x07D22C  138542
W_OFFSET   0x07D230  223210
V_OFFSET   0x07D234  308756
```

### Using another WiOn 50055

Before flashing another unit:

1. Back up the entire original 1 MiB flash twice.
2. Compare the two SHA256 hashes.
3. Extract the four stock constants from the backup.
4. Compare them with the values above.
5. If they differ, edit the corresponding `HW_OFFSET`, `A_OFFSET`, `W_OFFSET` and
   `V_OFFSET` constants in `wion_50055_v1.0.0.yaml` **before flashing that unit**.
6. After ESPHome is running, perform the normal zero/current/power/energy calibration
   from the local web interface.

The current v1.0.0 firmware uses the factory constants recovered from the development
unit as its defaults. Until multiple original units have been compared, preserving
the stock dump is strongly recommended.

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

```text
I_final = max(0, (I_factory - I_zero) × current_gain)
```

There is no wide current deadband or hysteresis, so small real loads are not
intentionally discarded.

## Build

Copy `secrets.example.yaml` to `secrets.yaml` and replace all example credentials.

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

## Acknowledgements and prior work

This project builds on substantial reverse-engineering work done by others.

Special thanks to the author of:

- [Woods WiOn 50055 WiFi Plug Hardware Hacking / Tasmota Installation](https://www.tarball.ca/posts/woods-wion-50055-wifi-plug-hacking/)

That work provided the teardown, ESP8266 flashing pinout, GPIO mapping, PCB
documentation, identification of the stock HLW8012 metering hardware, and an
original factory-firmware backup. It made development of this firmware
considerably easier.

The following Home Assistant Community discussion was also especially useful:

- [Decoding Power Bar with Energy Monitoring](https://community.home-assistant.io/t/decoding-power-bar-with-energy-monitoring/622841)

That thread documented the stock bridge-to-ESP8266 data stream, including the
GPIO0 clock / GPIO12 data interface and the repeating four-word measurement
frame tagged as:

- `0x48` — H / energy accumulation
- `0x49` — current
- `0x57` — active power
- `0x56` — voltage

It also contained early proof-of-concept work for reading the bridge protocol.

This project extends that prior work by retaining the original power-monitor
bridge, implementing its protocol in ESPHome, recovering the stock conversion
formulas and factory calibration data, and adding calibration, Home Assistant
integration, OTA, recovery AP, restore modes, and low-power energy-derived
average power.

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
