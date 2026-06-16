# Definicja pinoutu

## MCU → Funkcja

| MCU Pin | Funkcja | Opis |
|---------|---------|------|
| PA0  | PHASE_A_CURRENT | Prąd fazy A (INA240 #1) |
| PA1  | PHASE_B_CURRENT | Prąd fazy B (INA240 #2) |
| PA2  | CONFIG_TX | UART konfiguracyjny TX |
| PA3  | CONFIG_RX | UART konfiguracyjny RX |
| PA4  | THROTLE | Manetka/throttle (ADC) |
| PA5  | BATTERY_V | Napięcie baterii (dzielnik) |
| PA6  | NC | Niepodłączony |
| PA7  | NC | Niepodłączony |
| PA8  | PWMAHIGH | PWM High faza A → EG2113D |
| PA9  | PWMBHIGH | PWM High faza B → EG2113D |
| PA10 | PWMCHIGH | PWM High faza C → EG2113D |
| PA11 | NC | Niepodłączony |
| PA12 | NC | Niepodłączony |
| PA13 | (SWDIO) | Programowanie/debug (moduł) |
| PA14 | (SWCLK) | Programowanie/debug (moduł) |
| PA15 | SPEED | Sygnał prędkości |
| PB0  | TORQUE | Sygnał momentu obrotowego (ADC) |
| PB1  | FET_TEMP | Temperatura MOSFETów (NTC) |
| PB2  | NC | Niepodłączony |
| PB3  | PAS | Pedal Assist Sensor |
| PB4  | SD | Shutdown driverów EG2113D |
| PB5  | HALLA | Czujnik Halla A |
| PB6  | DISPLAY_TX | UART TX do wyświetlacza / ESP |
| PB7  | DISPLAY_RX | UART RX z wyświetlacza / ESP |
| PB8  | HALLB | Czujnik Halla B |
| PB9  | HALLC | Czujnik Halla C |
| PB10 | BREAK_LOW | Sygnał hamulca |
| PB11 | NC | Niepodłączony |
| PB12 | OVERCURRENT | Sygnał overcurrent z LM393 |
| PB13 | PWMALOW | PWM Low faza A → EG2113D |
| PB14 | PWMBLOW | PWM Low faza B → EG2113D |
| PB15 | PWMCLOW | PWM Low faza C → EG2113D |
| PC0  | NC | Niepodłączony |
| PC1  | NC | Niepodłączony |
| PC2  | NC | Niepodłączony |
| PC3  | NC | Niepodłączony |
| PC13 | (LED) | LED na płytce Black Pill |
| PC14 | (OSC32_IN) | Zegar RTC |
| PC15 | (OSC32_OUT) | Zegar RTC |
| VBat | NC | Niepodłączony |
| RES  | NC | Niepodłączony |

## Zestawienie grup

| Grupa | Piny |
|-------|------|
| **PWM** (6) | PA8 (HIGH_A), PA9 (HIGH_B), PA10 (HIGH_C), PB13 (LOW_A), PB14 (LOW_B), PB15 (LOW_C) |
| **Hall** (3) | PB5 (A), PB8 (B), PB9 (C) |
| **ADC pomiar** (6) | PA0 (I_A), PA1 (I_B), PA4 (throttle), PA5 (VBAT), PB0 (torque), PB1 (temp) |
| **UART** (4) | PA2 (CONFIG_TX), PA3 (CONFIG_RX), PB6 (DISPLAY_TX), PB7 (DISPLAY_RX) |
| **GPIO ster.** (5) | PA15 (SPEED), PB3 (PAS), PB4 (SD), PB10 (BRAKE), PB12 (OVERCURRENT) |
| **Onboard / RTC** (3) | PC13 (LED), PC14 (OSC32_IN), PC15 (OSC32_OUT) |
| **NC** (12) | PA6, PA7, PA11, PA12, PB2, PB11, PC0, PC1, PC2, PC3, VBat, RES |
| **SWD** (2) | PA13, PA14 (wbudowane w moduł) |

## Uwagi

- Pinout PWM jest zgodny ze sprzętowym sterowaniem 3-fazowym przez **TIM1** w STM32F411.
- Mapowanie kanałów TIM1 jest następujące: **PA8 = TIM1_CH1**, **PA9 = TIM1_CH2**, **PA10 = TIM1_CH3**, **PB13 = TIM1_CH1N**, **PB14 = TIM1_CH2N**, **PB15 = TIM1_CH3N**.
- Oznacza to możliwość użycia **3 par wyjść komplementarnych**, sprzętowego **dead-time**, preload i synchronizacji ADC do sterowania BLDC w trybach BLOCK, SINUS i FOC.
- Wejście hamulca **BREAK_LOW (PB10)** nie jest tym samym co wejście **TIM1_BKIN**; w obecnym schemacie wyłączenie stopnia mocy realizuje sygnał **SD (PB4)** do EG2113D oraz sygnał **OVERCURRENT (PB12)** z LM393.
- Nazwa sygnału **THROTLE** jest zachowana zgodnie z obecną dokumentacją projektu.
- Wejście hamulca na MCU to **BREAK_LOW (PB10)**; na złączu zewnętrznym może być opisane jako **BRAKE**.
- W mapowaniu ADC występują tylko dwa pomiary prądu fazowego: **PHASE_A_CURRENT** i **PHASE_B_CURRENT**.
