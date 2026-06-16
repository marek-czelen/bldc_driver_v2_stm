# Architektura oprogramowania

## 1. Warstwy oprogramowania

```
┌──────────────────────────────────────────────────────────┐
│                    Aplikacja (App)                        │
│  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌────────────┐  │
│  │   CLI    │ │  OLED    │ │  CAN     │ │  Safety    │  │
│  └──────────┘ └──────────┘ └──────────┘ └────────────┘  │
├──────────────────────────────────────────────────────────┤
│              Logika sterowania (Control)                  │
│  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌────────────┐  │
│  │  BLOCK   │ │  SINUS   │ │   FOC    │ │  PID       │  │
│  │  6-step  │ │  SVPWM   │ │ dq/Clarke│ │  Regulator │  │
│  └──────────┘ └──────────┘ └──────────┘ └────────────┘  │
├──────────────────────────────────────────────────────────┤
│              HAL (Hardware Abstraction Layer)             │
│  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌────────────┐  │
│  │  PWM     │ │  ADC     │ │  Encoder │ │  GPIO      │  │
│  │  TIM1    │ │  ADC1    │ │  TIM2    │ │  Exti      │  │
│  └──────────┘ └──────────┘ └──────────┘ └────────────┘  │
├──────────────────────────────────────────────────────────┤
│              CMSIS / LL / HAL (ST)                        │
│  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌────────────┐  │
│  │  Startup │ │  LL      │ │  DSP     │ │  FPU       │  │
│  └──────────┘ └──────────┘ └──────────┘ └────────────┘  │
└──────────────────────────────────────────────────────────┘
```

## 2. Moduły szczegółowo

### 2.1 CMSIS / LL (Low-Layer)

Bezpośrednia praca na rejestrach przez **STM32 LL Library** (lub CMSIS):
- Szybsze i mniejsze niż HAL
- Pełna kontrola nad peryferiami
- Mniejsze opóźnienia w pętlach regulacji

### 2.2 HAL — PWM (TIM1, 6 kanałów)

**Plik:** `pwm.c / pwm.h`

Konfiguracja **TIM1** — PWM dla 6 kanałów (3 pary komplementarne):

Mapowanie sprzętowe kanałów:
- **TIM1_CH1** → **PA8** → `PWMAHIGH`
- **TIM1_CH2** → **PA9** → `PWMBHIGH`
- **TIM1_CH3** → **PA10** → `PWMCHIGH`
- **TIM1_CH1N** → **PB13** → `PWMALOW`
- **TIM1_CH2N** → **PB14** → `PWMBLOW`
- **TIM1_CH3N** → **PB15** → `PWMCLOW`

Ten układ pozwala użyć zaawansowanych funkcji timera **TIM1** bez programowego generowania par high/low:
- wyjścia komplementarne dla 3 faz,
- sprzętowy dead-time,
- synchroniczny update wszystkich kanałów,
- wyzwalanie ADC zsynchronizowane z PWM.

```c
void pwm_init(uint32_t freq_khz);             // PWM 20 kHz
void pwm_set_duty(phase_t phase, float duty); // 0.0 - 1.0
void pwm_set_sv(float u_alpha, float u_beta, float vbus); // SVPWM dla FOC
```

**Sygnały PWM (na podstawie schematu):**
- `PWMAHIGH` (**PA8**), `PWMALOW` (**PB13**) — faza A
- `PWMBHIGH` (**PA9**), `PWMBLOW` (**PB14**) — faza B
- `PWMCHIGH` (**PA10**), `PWMCLOW` (**PB15**) — faza C

### 2.3 HAL — ADC (pomiar prądu przez INA240A2D)

**Plik:** `adc.c / adc.h`

Pomiar prądu przez **INA240A2D** (wzmocnienie 50V/V) z synchronizacją PWM:

```c
void adc_init(void);
void adc_start_conversion(void);
float adc_get_current(phase_t phase);   // Prąd fazy w amperach (z INA240)
float adc_get_temperature(void);        // Temperatura z NTC (termistor TH1)
float adc_get_bus_voltage(void);        // Napięcie szyny VBAT (dzielnik)
float adc_get_throttle(void);           // Pozycja manetki (throttle)
```

Kanały ADC:
- **PA0** — PHASE_A_CURRENT (INA240 #1)
- **PA1** — PHASE_B_CURRENT (INA240 #2)
- **PA4** — THROTLE (manetka)
- **PA5** — BATTERY_V (napięcie baterii)
- **PB0** — TORQUE (moment obrotowy)
- **PB1** — FET_TEMP (temperatura, NTC)

### 2.4 HAL — GPIO / EXTI (Halla, przyciski)

**Plik:** `gpio.c / gpio.h`

```c
void gpio_init(void);
void gpio_set_sd(bool enable);           // SD pin (PB4) — shutdown driverów
bool gpio_get_overcurrent(void);         // OVERCURRENT (PB12) z LM393
uint8_t gpio_get_hall_state(void);       // HALL A/B/C (PB5, PB8, PB9)
bool gpio_get_brake(void);               // BREAK_LOW (PB10)
bool gpio_get_pas(void);                 // PAS (PB3)
```

Czujniki Halla na **PB5, PB8, PB9**.
### 2.5 Control — BLOCK (6-step)

**Plik:** `block.c / block.h`

```c
void block_init(void);
void block_set_speed(float rpm);
void block_commutate(uint8_t step);     // Ręczna komutacja
void block_run(void);                   // Automatyczna pętla
```

Obsługa:
- Komutacja na podstawie Halla lub BEMF
- PWM modulation (duty cycle)
- Start: forced commutation → BEMF detection

### 2.6 Control — SINUS

**Plik:** `sinus.c / sinus.h`

```c
void sinus_init(void);
void sinus_set_speed(float rpm);
void sinus_set_voltage(float amplitude);
void sinus_run(void);
```

Realizacja:
- Generowanie sinusoidalnych wzorców PWM
- SVPWM (Space Vector PWM) dla lepszego wykorzystania napięcia
- Interpolacja pozycji z Halla lub enkodera

### 2.7 Control — FOC

**Plik:** `foc.c / foc.h`

```c
void foc_init(void);
void foc_set_speed(float rpm);
void foc_set_torque(float torque_nm);
void foc_run(void);
```

Algorytm:
1. Odczyt ADC (I_A, I_B)
2. Clarke transform: $I_\alpha, I_\beta = f(I_A, I_B, I_C)$
3. Park transform: $I_d, I_q = f(I_\alpha, I_\beta, \theta)$
4. PID regulators: $V_d^*, V_q^* = PID(I_d^*, I_d, I_q^*, I_q)$
5. Inverse Park: $V_\alpha, V_\beta = f(V_d^*, V_q^*, \theta)$
6. SVPWM → TIM1

### 2.8 PID Regulator

**Plik:** `pid.c / pid.h`

```c
typedef struct {
    float kp, ki, kd;
    float integral;
    float prev_error;
    float output_limit;
    float integral_limit;
} pid_t;

float pid_update(pid_t *pid, float setpoint, float measurement);
void pid_reset(pid_t *pid);
void pid_set_gains(pid_t *pid, float kp, float ki, float kd);
```

### 2.9 CLI (Command Line Interface)

**Plik:** `cli.c / cli.h`

Interfejs tekstowy przez UART (przez TXB0102 → 5V lub USB-C):

```
BLDC Controller CLI v1.0
> help
  help          - lista komend
  mode [block|sinus|foc] - ustaw tryb
  speed [rpm]   - zadana prędkość
  pid [kp ki kd] - nastawy PID
  status        - status systemu
  calib         - kalibracja ADC/pomiarów
  plot [var]    - zmienna do wykresu (telemetria)
```

**Komunikacja:**
- UART przez **TXB0102DCU** (translator 3.3V ↔ 5V) → złącze J7 (UART)
- UART konfiguracyjny na **PA2/PA3** → złącze J8 (UART_CONFIG)
- **USB-C** (wbudowany w Black Pill) — USB CDC ACM (Virtual COM Port)
- Oba interfejsy dostępne równolegle

### 2.10 Safety

**Plik:** `safety.c / safety.h`

```c
void safety_init(void);
void safety_check(void);          // Wywoływana cyklicznie
void safety_trigger(error_t err); // Natychmiastowe zatrzymanie
```

Kontrola:
- **Overcurrent (HW):** LM393 → sygnał OVERCURRENT (PB12) + SD (PB4) → natychmiastowe wyłączenie EG2113D
- **Overcurrent (SW):** Pomiar INA240 → ADC → próg w software
- **Overtemperature:** NTC (TH1) → ADC PB1
- **Under/Over voltage:** Dzielnik VBAT → ADC PA5
- **Watchdog (IWDG):** Niezależny watchdog STM32
- **Error state:** PWM off, SD high, LED (PC13) sygnalizacja błędu

## 3. Główna pętla programu

```c
int main(void) {
    system_init();            // Clock (100 MHz), GPIO, peryferia
    pwm_init(20);             // PWM 20 kHz
    adc_init();               // ADC (PA0/PA1-prąd, PA4-throttle, PA5-Vbat, PB0-torque, PB1-temp)
    gpio_init();              // SD (PB4), Hall (PB5/8/9), OVERCURRENT (PB12), itp.
    pid_init(&iq_pid);
    pid_init(&id_pid);
    cli_init(115200);         // UART przez TXB0102 + USB-C

    control_mode_t mode = CTRL_MODE_BLOCK;
    uint32_t tick = 0;

    while (1) {
        // Pętla sterowania — wywoływana z TIM1_IRQ (20 kHz)
        // W głównej pętli tylko:
        // - obsługa CLI
        // - aktualizacja OLED
        // - wolne pętle bezpieczeństwa

        safety_check();       // Overcurrent, temp, voltage

        cli_process();        // Obsługa komend (polling)

        if (tick++ % 100 == 0) {
            // Co 100 iteracji pętli głównej
            oled_update();    // Wyświetlanie parametrów
        }
    }
}
```

> **Uwaga:** Właściwa pętla regulacji (BLOCK/SINUS/FOC) jest wywoływana z przerwania TIM1 (20 kHz) dla deterministycznego czasu wykonania.

## 4. Przebieg przerwań (ISR)

```
TIM1_UP_IRQHandler (20 kHz — 50 µs)
  ├── Odczyt ADC (prądy z INA240, temp, throttle)
  ├── if (mode == FOC):
  │     ├── Clarke → Park → PID → Inverse Park → SVPWM
  │     └── Aktualizacja PWM CCR
  ├── if (mode == BLOCK/SINUS):
  │     └── Aktualizacja PWM CCR
  └── Flaga dla pętli prędkości (co N cykli)

EXTI_IRQHandler (przerwania GPIO)
  ├── OVERCURRENT (PB12) z LM393
  │     └── gpio_set_sd(true) — natychmiastowe wyłączenie
  ├── BREAK_LOW (PB10)
  │     └── Zatrzymanie silnika
  └── Czyszczenie flagi PR

USARTx_IRQHandler (CLI)
  ├── CONFIG UART (PA2/PA3) / DISPLAY UART (PB6/PB7)
  ├── Odebranie znaku
  └── Buforowanie do ring buffer
```

## 5. Pliki projektu

```
src/
├── main.c                 # Główna pętla programu
├── startup_stm32f411xe.s  # Plik startowy (CMSIS)
├── system_stm32f4xx.c     # Inicjalizacja systemu (clock)
├── stm32f411xe.h          # CMSIS header
│
├── pwm.c / pwm.h          # PWM TIM1 (6 kanałów, 3 pary komplementarne)
├── adc.c / adc.h          # ADC (prąd z INA240, temp NTC, VBAT)
├── gpio.c / gpio.h        # GPIO: SD, OVERCURRENT, Hall, BRAKE, PAS
│
├── block.c / block.h      # Sterowanie BLOCK (6-step, komutacja)
├── sinus.c / sinus.h      # Sterowanie SINUS (SPWM/SVPWM)
├── foc.c / foc.h          # Sterowanie FOC (Clarke, Park, SVPWM)
├── pid.c / pid.h          # Regulator PID (z anti-windup)
│
├── cli.c / cli.h          # Interfejs tekstowy (UART + USB-C)
├── oled.c / oled.h        # Wyświetlacz OLED (opcjonalnie)
├── safety.c / safety.h    # Zabezpieczenia (overcurrent, temp, voltage)
│
├── common.h               # Wspólne definicje (structy, stałe, enumy)
└── Makefile               # Kompilacja (ARM GCC)
```

## 6. Narzędzia i toolchain

| Element | Zalecenie |
|---------|-----------|
| IDE | STM32CubeIDE (z CubeMX) lub VS Code + Cortex-Debug |
| Toolchain | ARM GCC (gcc-arm-none-eabi) |
| Debugger | ST-Link V2 (SWD) |
| Biblioteki | CMSIS + LL (zewnętrzne), brak RTOS (na początek) |
| Kompilacja | Makefile lub CMake |
