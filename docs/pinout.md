# Pinout i podłączenia

## Moduł WeAct Black Pill (STM32F411CEU6)

Poniższa tabela porządkuje aktualnie używane sygnały MCU oraz usuwa sprzeczności z wcześniejszej wersji dokumentu.

## MCU → funkcja

| MCU Pin | Funkcja | Opis |
|---------|---------|------|
| PA0 | PHASE_A_CURRENT | Prąd fazy A (INA240 #1) |
| PA1 | PHASE_B_CURRENT | Prąd fazy B (INA240 #2) |
| PA2 | CONFIG_TX | UART konfiguracyjny TX |
| PA3 | CONFIG_RX | UART konfiguracyjny RX |
| PA4 | THROTLE | Manetka/throttle |
| PA5 | BATTERY_V | Napięcie baterii (dzielnik) |
| PA6 | NC | Niepodłączony |
| PA7 | NC | Niepodłączony |
| PA8 | PWMAHIGH | PWM High faza A → EG2113D |
| PA9 | PWMBHIGH | PWM High faza B → EG2113D |
| PA10 | PWMCHIGH | PWM High faza C → EG2113D |
| PA11 | NC | Niepodłączony |
| PA12 | NC | Niepodłączony |
| PA13 | SWDIO | Debugowanie |
| PA14 | SWCLK | Debugowanie |
| PA15 | SPEED | Sygnał prędkości |
| PB0 | TORQUE | Sygnał momentu obrotowego |
| PB1 | FET_TEMP | Temperatura MOSFET (NTC) |
| PB2 | NC | Niepodłączony |
| PB3 | PAS | Pedal Assist Sensor |
| PB4 | SD | Shutdown driverów EG2113D |
| PB5 | HALLA | Czujnik Halla A |
| PB6 | DISPLAY_TX | UART TX do wyświetlacza / ESP |
| PB7 | DISPLAY_RX | UART RX z wyświetlacza / ESP |
| PB8 | HALLB | Czujnik Halla B |
| PB9 | HALLC | Czujnik Halla C |
| PB10 | BREAK_LOW | Wejście hamulca |
| PB11 | NC | Niepodłączony |
| PB12 | OVERCURRENT | Sygnał overcurrent z LM393 |
| PB13 | PWMALOW | PWM Low faza A → EG2113D |
| PB14 | PWMBLOW | PWM Low faza B → EG2113D |
| PB15 | PWMCLOW | PWM Low faza C → EG2113D |
| PC0 | NC | Niepodłączony |
| PC1 | NC | Niepodłączony |
| PC2 | NC | Niepodłączony |
| PC3 | NC | Niepodłączony |
| PC13 | LED | LED na płytce Black Pill |
| PC14 | OSC32_IN | Zegar RTC |
| PC15 | OSC32_OUT | Zegar RTC |
| VBat | NC | Niepodłączony |
| RES | NC | Niepodłączony |

## Grupy funkcjonalne

### PWM

| Faza | High | Low |
|------|------|-----|
| A | PWMAHIGH (PA8) | PWMALOW (PB13) |
| B | PWMBHIGH (PA9) | PWMBLOW (PB14) |
| C | PWMCHIGH (PA10) | PWMCLOW (PB15) |

### Hall

| Czujnik | Pin |
|---------|-----|
| HALLA | PB5 |
| HALLB | PB8 |
| HALLC | PB9 |

### ADC

| Sygnał | Pin | Opis |
|--------|-----|------|
| PHASE_A_CURRENT | PA0 | INA240 #1 |
| PHASE_B_CURRENT | PA1 | INA240 #2 |
| THROTLE | PA4 | Manetka |
| BATTERY_V | PA5 | Napięcie baterii |
| TORQUE | PB0 | Moment obrotowy |
| FET_TEMP | PB1 | Temperatura (NTC) |

### UART

| Sygnał | Pin | Opis |
|--------|-----|------|
| DISPLAY_TX | PB6 | Do wyświetlacza / ESP |
| DISPLAY_RX | PB7 | Z wyświetlacza / ESP |
| CONFIG_TX | PA2 | UART konfiguracyjny |
| CONFIG_RX | PA3 | UART konfiguracyjny |

### GPIO

| Sygnał | Pin | Kierunek | Opis |
|--------|-----|----------|------|
| SD | PB4 | OUT | Shutdown EG2113D |
| OVERCURRENT | PB12 | IN | Wejście z LM393 |
| SPEED | PA15 | IN | Sygnał prędkości |
| PAS | PB3 | IN | Pedal Assist |
| BREAK_LOW | PB10 | IN | Wejście hamulca |

## Złącza

| Złącze | Typ | Sygnały |
|--------|-----|---------|
| J1 | 1-pin | PHASE A |
| J2 | 1-pin | PHASE B |
| J3 | 1-pin | PHASE C |
| J4 (THROTLE) | 3-pin | THROTLE, +5V, GND |
| J5 (HALL) | 6-pin | HALL_A, HALL_B, HALL_C, +5V, GND, NC |
| J6 (PASS) | 4-pin | PAS, TORQUE, +5V, GND |
| J7 (UART) | 4-pin | UARTTX_5V, UARTRX_5V, zasilanie, GND |
| J8 (UART_CONFIG) | 4-pin | CONFIG_TX, CONFIG_RX, +5V, GND |
| J10 | 1-pin | VBAT |
| J11 (BREAK) | 2-pin | BRAKE, GND |
| J12 | 1-pin | GNDPWR |

## Układy powiązane

### EG2113D

| Faza | HIN | LIN | HO | LO |
|------|-----|-----|----|----|
| A | PWMAHIGH (PA8) | PWMALOW (PB13) | Q1 (HS A) | Q2 (LS A) |
| B | PWMBHIGH (PA9) | PWMBLOW (PB14) | Q3 (HS B) | Q4 (LS B) |
| C | PWMCHIGH (PA10) | PWMCLOW (PB15) | Q5 (HS C) | Q6 (LS C) |

Wspólne sygnały i zasilanie:

- SD → PB4
- VDD → 3.3V
- VCC → 12V
- COM → GNDPWR

### INA240A2D

| INA240 | Faza A (U5) | Faza B (U6) | Faza C (U7) |
|--------|-------------|-------------|-------------|
| OUT | PA0 | PA1 | NC do MCU |
| V+ | +5V | +5V | +5V |
| REF1/2 | VREF/2 | VREF/2 | VREF/2 |

## Uwagi

- Dokumentacja została oczyszczona z duplikatów i wzajemnie sprzecznych wpisów.
- W obecnym mapowaniu MCU dostępne są dwa kanały pomiaru prądu faz: A i B.
- Nazwa sygnału THROTLE została zachowana zgodnie z istniejącym nazewnictwem projektu.
- Schemat używa kilku aliasów netów względem nazewnictwa logicznego w dokumentacji: PHASEACURRENT/PHASEBCURRENT, BATTERYVOLTAGE, FETTEMP, BREAK_L oraz HALLSENSORA/B/C.
- UART na PB6/PB7 jest wyprowadzony na złącze J7 przez translator TXB0102 jako UARTTX_5V/UARTRX_5V.
- Złącze J8 UART_CONFIG odpowiada bezpośrednio sygnałom CONFIG_TX/CONFIG_RX z PA2/PA3.
