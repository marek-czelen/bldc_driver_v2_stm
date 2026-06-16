# BLDC Controller STM32

Regulator silnika bezszczotkowego (BLDC) oparty na **STM32F411CEU6 (WeAct Black Pill)** z 3× driverami bramek **EG2113D** i tranzystorami **HY4008** (pin-compatible z IRFB4110 ze schematu). Stopień mocy jest sterowany sprzętowo przez **TIM1** w konfiguracji **6PWM** (3 pary komplementarne).

## Tryby sterowania

- **BLOCK (6-step)** — sterowanie trapezoidalne z komutacją halla/sensorless
- **SINUS** — sterowanie sinusoidalne z modulacją PWM
- **FOC** — sterowanie polowo-zorientowane (Field Oriented Control) z pomiarem prądu faz A/B przez INA240A2D i wyliczaniem trzeciej fazy

## Właściwości

- **MCU:** STM32F411CEU6 (WeAct Black Pill) – Cortex-M4F @ 100 MHz, FPU
- **Gate driver:** 3× EG2113D (jeden na fazę) — half-bridge, bootstrap, do 600V
- **MOSFET:** 6× HY4008 — N-Channel 80V/180A, R₍ds(on)₎ 2.9 mΩ, TO-220  
  *(Schemat: IRFB4110 — symbol biblioteczny, pin-compatible)*
- **Current sensing:** 3× INA240A2D — wzmacniacz różnicowy 50V/V z PWM rejection; do MCU podłączone są 2 kanały prądowe (A/B)
- **Zasilanie:** DC-DC 5..80V → 5V + LDO LD1117S33TR 3.3V
- **Overcurrent:** LM393 (komparator) + INA240 → zatrzymanie PWM (SD)
- **Czujniki:** Hall (6-pin), throttle, PAS, brake
- **Komunikacja:** UART 5V przez TXB0102, UART konfiguracyjny, USB-C (wbudowany w Black Pill), SWD
- **Translator poziomów:** TXB0102DCU (3.3V ↔ 5V dla UART)
- **Zabezpieczenia:** overcurrent (HW + SW), overtemperature (NTC), under-voltage

## Struktura dokumentacji

```
docs/
├── hardware.md               # Architektura sprzętowa
├── pinout.md                 # Pinout i podłączenia
├── control-modes.md          # Tryby sterowania (block, sinus, FOC)
├── software-architecture.md  # Architektura oprogramowania
└── roadmap.md                # Plan rozwoju
```

## Status

Projekt w fazie projektowania i dokumentacji. Schemat KiCad gotowy.
