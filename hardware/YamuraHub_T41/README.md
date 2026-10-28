# YamuraLog Hub carrier board, Teensy 4.1 (rev A)

A 2-layer, all through-hole board, 120 × 80 mm, built in KiCad 10. It carries the same parts as the YamuraHubV6.1 hub, with the Teensy 4.0 replaced by a Teensy 4.1.

> **Status: rev B in progress.** The KiCad files, `fab/` and `docs/` are **rev A** (described below). `gen/design.py` already holds the rev B part list:
> - two DB9 CAN connectors
> - panel-mounted buttons only (J6)
> - the display plugging into J4 on the front, long side along the board, with every other part on the back
> - a 122 × 68 mm board with corner mounting holes
>
> `build_pcb.py` and `build_sch.py` haven't been updated for back-side parts yet. Don't regenerate until they are, or the output will be neither rev A nor a working rev B.
>
> With two DB9s the hub may sit mid-bus. If it does, remove the 120 Ω terminator on the Waveshare CAN board.

| File | Contents |
|---|---|
| `YamuraHub_T41.kicad_pro / .kicad_sch / .kicad_pcb` | KiCad project, schematic and board |
| `YamuraHub.pretty`, `YamuraHub.kicad_sym` | Project footprint (Teensy 4.1) and symbols |
| `fab/YamuraHub_T41_gerbers.zip` | Gerbers and Excellon drill files, ready to upload to a board house |
| `fab/YamuraHub_T41_BOM.csv` | Bill of materials |
| `docs/` | Schematic PDF, 3D renders and layer plots |
| `gen/` | Scripts that generate the whole design from `gen/design.py` (see *Regenerating*) |

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
| Buttons: SELECT / UP / DOWN | 16 / 17 / 18 (to GND, internal pull-ups) |
| LEDs: recording / GPS | 21 / 22 (330 Ω) |
| I2C (Wire2): SDA2 / SCL2 | 25 / 24 → J5, spare for an on-board GPS |

## Power and CAN
- **J1 power input:** 12 V comes in on J1 and passes through D1 (1N5822, reverse-polarity protection) to the +12 V rail. D4 (1.5KE20A TVS diode) clamps voltage spikes on that rail.
- **5 V supply:** U1 (RECOM R-78E5.0-1.0, a 1 A switching regulator) makes +5 V for the Teensy VIN pin, the AirLift VIN pin, the display VCC pin and the display backlight (LED pin).
- **3.3 V supply:** the Teensy's own 3.3 V output powers the CAN board and J5.
- **J3 (DB9 male, CAN to the nodes):**
  - pin 2 = CAN_L
  - pin 7 = CAN_H
  - pins 3 and 6 = GND
  - pin 9 = +12 V to the nodes, through F1 (1.1 A resettable fuse)
  - shell = GND
- **Termination:** the Waveshare SN65HVD230 board has a 120 Ω terminator built in. The hub is one end of the bus, so no other terminator is needed at this end.

## Before ordering, check against your actual parts
1. **AirLift (A2), 12-pin socket**, pin order assumed to be VIN, 3Vo, GND, SCK, MISO, MOSI, CS, BUSY, RST, GP0, RXI, TXO.
2. **Waveshare CAN board (A3), 4-pin socket**, pin order assumed to be 3V3, GND, CTX, CRX. Its CANH/CANL terminals connect to J2 with two short wires.
3. **Display (J4)** follows the MSP4030/4031 14-pin header: VCC, GND, LCD_CS, LCD_RST, LCD_RS, SDI, SCK, LED, SDO, CTP ×4, SD_CS. The display connects by cable.

Each socket's signal names are printed on the silkscreen. If a module's order differs, edit `pins` and `pinnames` for that part in `gen/design.py` and regenerate.

## Assembly notes
- **Cut the Teensy's VUSB-VIN pad** on its underside. Otherwise, with USB plugged in, USB and the board's 5 V supply are tied together.
- **Mount the Teensy on female headers.** Its microSD slot faces the bottom edge of the board, and its USB port faces the top.
- **Leave JP1 open** (see above).
- **Panel controls:** J6 and J7 bring the buttons and LEDs out to panel-mounted parts, wired in parallel with SW1–SW3 and D2/D3.

## Firmware changes needed
In `platformio.ini`, change the environment from `teensy40` to `teensy41`. In `main.cpp`, replace `SD.begin(SD_CS)` with `SD.begin(BUILTIN_SDCARD)`, and remove the `SD_CS` define. Nothing else changes: every other pin keeps its current number.

## Regenerating
Everything in this folder is generated from `gen/design.py`:
```
python gen/build_sch.py                                      # schematic and symbol library
kicad-cli sch export netlist -o gen/out/YamuraHub_T41.net YamuraHub_T41.kicad_sch
"C:/Program Files/KiCad/10.0/bin/python.exe" gen/build_pcb.py  # placement; pad nets come from the netlist
"C:/Program Files/KiCad/10.0/bin/python.exe" gen/route.py      # grid router and GND pours
```
Rebuilding the board this way discards any hand edits made in KiCad. If you route by hand in KiCad, stop running these scripts.

## Check results (rev A)
- **ERC:** 0 errors, 0 warnings.
- **DRC with schematic parity:** 0 unconnected pads and 0 board/schematic mismatches.
- **Remaining DRC warnings, none of them errors:**
  - Two ground-pour connections are partial thermal reliefs. Both pads are also connected by tracks.
  - Three silkscreen labels touch neighbouring outlines.
