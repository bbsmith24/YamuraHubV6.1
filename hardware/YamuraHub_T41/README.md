# YamuraLog Hub carrier board, Teensy 4.1 (rev B)

A 2-layer, all through-hole board, 122 × 68 mm, built in KiCad 10. It carries the same parts as the YamuraHubV6.1 hub, with the Teensy 4.0 replaced by a Teensy 4.1.

**The display goes on the front; every other part goes on the back.**
- **Front:** the 4″ display (MSP4030/4031, 108 × 60.88 mm) plugs into the J4 socket, with its long side along the board. It's held by M3 standoffs at its own four mounting holes (H5–H8).
- **Back:** the Teensy, AirLift, CAN board, power section, connectors and headers.
- **Board size:** the display outline plus a margin at each end for the four corner mounting holes (H1–H4).

| File | Contents |
|---|---|
| `YamuraHub_T41.kicad_pro / .kicad_sch / .kicad_pcb` | KiCad project, schematic and board |
| `YamuraHub.pretty`, `YamuraHub.kicad_sym` | Project footprint (Teensy 4.1) and symbols |
| `fab/YamuraHub_T41_gerbers.zip` | Gerbers and Excellon drill files, ready to upload to a board house |
| `fab/YamuraHub_T41_BOM.csv` | Bill of materials |
| `docs/` | Schematic PDF, 3D renders (top = display side, bottom = parts side) and layer plots |
| `gen/` | Scripts that generate the whole design from `gen/design.py` (see *Regenerating*) |

## Changes from rev A
- **Two DB9 CAN connectors** (J3, J8), wired in parallel, so the CAN bus can pass through the hub.
- **No on-board buttons or LEDs.** The buttons are panel-mounted and connect to J6. The panel LEDs connect to J7, with their 330 Ω resistors (R1/R2) still on the board.
- **The display plugs straight onto the board** (J4 socket on the front) instead of connecting by cable, and everything else moved to the back.
- **Board size:** 122 × 68 mm, up from 120 × 80 mm, with M3 holes at all four corners.

## What changed from the Teensy 4.0 hub
- **The SD card moved to the Teensy 4.1's built-in microSD slot**, which uses the fast 4-bit SDIO interface. The old SD chip select (pin 3) and the display's SD slot are no longer used, and the display's `SD_CS` pin is left unconnected.
- **The display's SDO (MISO) line is cut off by default with JP1.** The display controller doesn't release its SDO line when it isn't selected, so on a shared MISO line it would interfere with the AirLift. The firmware never reads from the display, so leave JP1 open.

## Pin map (from `src/main.h`)
| Function | Teensy pin |
|---|---|
| SPI: MOSI / MISO / SCK | 11 / 12 / 13 (shared by AirLift and display) |
| AirLift: CS / RESET / BUSY | 5 / 6 / 9 |
| Display: CS / DC (RS) / RST | 15 / 2 / 4 |
| CAN3 (FlexCAN_T4FD): CTX3 / CRX3 | 31 / 30 → SN65HVD230 board |
| Buttons (J6): SELECT / UP / DOWN | 16 / 17 / 18 (to GND, internal pull-ups) |
| LEDs (J7, via R1/R2): recording / GPS | 21 / 22 (330 Ω) |
| I2C (Wire2): SDA2 / SCL2 | 25 / 24 → J5, spare for an on-board GPS |

## Power and CAN
- **J1 power input:** 12 V comes in on J1 and passes through D1 (1N5822, reverse-polarity protection) to the +12 V rail. D4 (1.5KE20A TVS diode) clamps voltage spikes on that rail.
- **5 V supply:** U1 (RECOM R-78E5.0-1.0, a 1 A switching regulator) makes +5 V for the Teensy VIN pin, the AirLift VIN pin, the display VCC pin and the display backlight (LED pin).
- **3.3 V supply:** the Teensy's own 3.3 V output powers the CAN board and J5.
- **J3 and J8 (DB9 male, CAN to the nodes), wired identically:**
  - pin 2 = CAN_L
  - pin 7 = CAN_H
  - pins 3 and 6 = GND
  - pin 9 = +12 V to the nodes, through F1 (1.1 A resettable fuse, shared by both connectors)
  - shell = GND
- **Termination:** the Waveshare SN65HVD230 board has a fixed 120 Ω terminator.
  - **One DB9 used:** the hub is one end of the bus, so leave the terminator in place.
  - **Both DB9s used:** the hub sits mid-bus, so remove the terminator from the Waveshare board, and terminate the two far ends of the bus instead.

## Before ordering, check against your actual parts
1. **AirLift (A2), 12-pin socket**, pin order assumed to be VIN, 3Vo, GND, SCK, MISO, MOSI, CS, BUSY, RST, GP0, RXI, TXO.
2. **Waveshare CAN board (A3), 4-pin socket**, pin order assumed to be 3V3, GND, CTX, CRX. Its CANH/CANL terminals connect to J2 with two short wires.
3. **Display (J4)** follows the MSP4030/4031 14-pin header: VCC, GND, LCD_CS, LCD_RST, LCD_RS, SDI, SCK, LED, SDO, CTP ×4, SD_CS.
4. **Display header position:** J4 assumes the display's header is 2.0 mm in from its left short edge, centred vertically (13.93 mm from top and bottom), with pin 1 (VCC) nearest the bottom edge, front view. These figures come from the MSP4030/4031 outline drawing. Measure your display before ordering; if they're off, the display won't line up with H5–H8.

Each socket's signal names are printed on the silkscreen. If a module's order differs, edit `pins` and `pinnames` for that part in `gen/design.py` and regenerate.

## Assembly notes
- **Cut the Teensy's VUSB-VIN pad** on its underside. Otherwise, with USB plugged in, USB and the board's 5 V supply are tied together.
- **Back-side parts:** solder every part except J4 on the back of the board. Mount the Teensy on female headers, with its USB port at the top edge and its microSD slot at the bottom edge so the card can be swapped.
- **Trim leads under the display:** cut the back-side parts' leads flush on the front. The display's underside (SMD parts up to 2.2 mm, plus its SD slot) sits over them, held at the J4 socket height by the standoffs.
- **Leave JP1 open** (see above).
- **Panel controls:** J6 (SELECT, UP, DOWN, GND) and J7 (REC, GPS, GND) go to the panel buttons and LEDs.

## Firmware
The `teensy41` branch of YamuraHubV6.1 already builds for this board: `env:teensy41`, with `SD.begin(BUILTIN_SDCARD)`. Every other pin is unchanged from the Teensy 4.0 hub.

## Regenerating
Everything in this folder is generated from `gen/design.py`:
```
python gen/build_sch.py                                      # schematic and symbol library
kicad-cli sch export netlist -o gen/out/YamuraHub_T41.net YamuraHub_T41.kicad_sch
"C:/Program Files/KiCad/10.0/bin/python.exe" gen/build_pcb.py  # placement; pad nets come from the netlist
"C:/Program Files/KiCad/10.0/bin/python.exe" gen/route.py      # grid router and GND pours
```
- **Back-side parts:** parts marked `side="B"` in `design.py` are flipped onto the back, and their labels go on the back silkscreen.
- **Routing retries:** the router routes one net at a time. If a net fails, it starts again with the failed nets routed first, for up to 8 attempts.
- **Hand edits:** rebuilding this way discards any hand edits made in KiCad. If you route by hand in KiCad, stop running these scripts.

## Check results (rev B)
- **ERC:** 0 errors, 0 warnings.
- **DRC with schematic parity:** 0 unconnected pads, 0 board/schematic mismatches, and no courtyard or clearance errors.
- **Remaining DRC warnings, not errors:** two ground-pour connections are partial thermal reliefs. Both pads are also connected by tracks.
