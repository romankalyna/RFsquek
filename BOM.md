# Bill of Materials (BOM) — RFSqueak-MK1

This list reflects the parts used/tested for the ESP32 + CC1101 + ST7789 build.

## Core Electronics
| Qty | Item | Example Part/Notes |
| --- | ---- | ------------------ |
| 1 | ESP32 Dev Module | ESP32-WROOM-32 DevKit (e.g., DOIT, AI-Thinker, Espressif DevKitC) |
| 1 | CC1101 433 MHz module | Must expose SPI + GDO0 + GDO2 pins |
| 1 | 1.14" ST7789 TFT (135×240) | 3.3V logic, SPI. Common pin names: VCC, GND, SCL/SCK, SDA/MOSI, DC, RST, CS |
| 1 | 4×AA battery holder | With leads |
| 1 | HW-441 3.3V regulator | 3.3V step-down module; ensure sufficient current (>500 mA recommended) |
| 1 | SMA 433 MHz antenna | Tuned for 433 MHz ISM band |
| 2 | Tactile pushbuttons | 6×6 mm suggested (SELECT/BACK) |
| 1 | Potentiometer | 10 kΩ linear (panel-mount or trimmer) |
| 1 | Power switch (SPST or SPDT) | Slide or toggle; ≥1 A at low voltage. Wire battery + through the switch into regulator VIN |

## Passives and Power
| Qty | Item | Notes |
| --- | ---- | ----- |
| 2+ | 0.1 µF ceramic capacitors | Decoupling: near CC1101 VCC and near POT wiper to GND |
| 1 | Power LED + resistor (optional) | LED with ~1 kΩ series resistor on 3.3V if you want a power indicator |
| — | Hook-up wire / headers | For perfboard/prototyping |
| — | Perfboard or PCB (optional) | For robust build |

## Cables and Tools
- USB cable for ESP32 programming
- Soldering iron and consumables
- Multimeter for continuity and power checks

## Notes
- Power the ST7789 with 3.3V logic only. If your display is 5V-tolerant, verify onboard level shifting (many are not).
- The SPI bus is shared between CC1101 and TFT. Use separate CS lines.
- Install 100 nF decoupling caps close to the CC1101 VCC and near the POT to reduce noise injection into the ADC.
