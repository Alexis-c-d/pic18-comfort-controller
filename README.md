# PIC18 Environmental Comfort Controller

Comfort-zone controller on **PIC18F45K22** (XC8) that monitors **light (LDR)** and **temperature (LM35)** and drives a **lamp**, **fan**, and **heater** to keep the environment in the comfort zone.

---

## Overview
| Sensor | Channel | Range | Comfort Zone |
|--------|---------|-------|--------------|
| LM35 (temp) | AN0 | 0–50 °C | 20–26 °C |
| LDR (light) | AN1 | 0–1023 ADC | 300–800 lux (approx) |

| Actuator | Pin | Control |
|----------|-----|---------|
| Lamp (supplemental light) | RC2/CCP2 | PWM – on when lux < 300 |
| Fan (cooling) | RC1/CCP1 | PWM – on when temp > 26 °C |
| Heater | RC0 | Digital – on when temp < 20 °C |

**Indicators** (LEDs on PORTB):
- 🟢 Green – comfort zone OK
- 🟡 Yellow – one parameter out of range
- 🔴 Red – both parameters out of range

---

## Repository Layout
```
pic18-comfort-controller/
├── src/
│   └── main.c          # Complete controller + drivers
├── sim/                # Proteus simulation (to be added)
├── docs/
│   └── Documento_Tecnico_Panel_Solar_PIC18F45K22.pdf  # Related project report
└── README.md
```

---

## Building
1. Open in **MPLAB X IDE** with **XC8**.
2. Device: **PIC18F45K22**, 8 MHz INTIO.
3. Build → `main.hex`.
4. Simulate in **Proteus** or flash to hardware.

---

## Key Formulas
- **Temperature**: `T(°C) = (ADC × 5.0 / 1024) / 0.01`  (LM35: 10 mV/°C)
- **Light (approx)**: `lux ≈ V_LDR × 200`  (calibrate for your LDR)
- **PWM duty (10-bit)**: `CCPRxL = duty >> 2; CCPxCONbits.DCxB = duty & 0x03`

---

## UART Commands
| Char | Action |
|------|--------|
| `s` / `S` | Request status report |
| `r` / `R` | Software reset |

**Status report format**:
```
STATUS: T=23.5C, Lux=450, Lamp=12%, Fan=0%, Heat=OFF
```

---

## Extracted From
Project report: *Documento_Tecnico_Panel_Solar_PIC18F45K22.pdf* (see `docs/`). The comfort controller shares the same PIC18F45K22 platform and peripherals (ADC, PWM, UART, LCD).

---

## License
MIT