# Pinout — RFSqueak-MK1

Central bus: VSPI shared between CC1101 and ST7789 TFT.

## SPI Bus (VSPI)
| Signal | ESP32 GPIO | Connected To | Notes |
| ------ | ----------- | ------------ | ----- |
| SCK | 18 | TFT SCK, CC1101 SCK | Shared |
| MOSI | 23 | TFT MOSI, CC1101 MOSI | Shared |
| MISO | 19 | CC1101 MISO | TFT does NOT use MISO |

## Chip Selects and Control
| Function | ESP32 GPIO | Device | Notes |
| -------- | ---------- | ------ | ----- |
| TFT CS | 5 | ST7789 | Chip Select for TFT |
| TFT DC | 16 | ST7789 | Data/Command |
| TFT RST | 17 | ST7789 | Reset |
| CC1101 CS | 27 | CC1101 | Chip Select for CC1101 |
| CC1101 GDO0 | 26 | CC1101 | Interrupt/Status pin |
| CC1101 GDO2 | 25 | CC1101 | Interrupt/Status pin |

## User Controls
| Function | ESP32 GPIO | Notes |
| -------- | ---------- | ----- |
| BTN_SELECT | 32 | Configure as INPUT_PULLUP |
| BTN_BACK | 33 | Configure as INPUT_PULLUP |
| POT (ADC1) | 34 | Analog input (use 0.1 µF to GND near pin) |

## Power and Misc
- All logic: 3.3V
- TFT BLK (backlight): hard-wired ON (or route to a GPIO + transistor if you want brightness control)
- Use a common GND for all modules
