# Architektura sprzętowa (Hardware)

## 1. Główne komponenty

| Element | Model | Opis |
|---------|-------|------|
| Mikrokontroler | STM32F411CEU6 (WeAct Black Pill) | ARM Cortex-M4F @ 100 MHz, FPU, 512 KB Flash, 128 KB SRAM |
| Gate driver | 3× EG2113D | Half-bridge driver, bootstrap, do 600V — jeden na fazę |
| MOSFET (implementacja) | 6× HY4008 | N-Channel, 80 V / 180 A, R₍ds(on)₎ 2.9 mΩ, TO-220 |
| MOSFET (symbol na schemacie) | IRFB4110 | Symbol biblioteczny KiCad, pin-compatible z HY4008 |
| Current sense | 3× INA240A2D | Wzmacniacz różnicowy 50V/V, Enhanced PWM Rejection, SOIC-8 |
| Zasilanie DC-DC | Moduł 4-pin DC/DC 5..80V | Przetwornica step-down 5..80V → 5V |
| LDO 3.3V | LD1117S33TR | LDO 3.3V, 800 mA, SOT-223 |
| Komparator | LM393 | Podwójny komparator (overcurrent) |
| Translator | TXB0102DCU | 2-bitowy translator poziomów 3.3V ↔ 5V |

## 2. Bloki funkcjonalne

### 2.1 Zasilanie (Power Supply)

```
Wejście DC (5-80V) ── J10 (VBAT) / J12 (GNDPWR)
    │
    │
    ├── DC-DC (U10: 5..80V → 5V) ──┬── +5V (EG2113D, INA240, Hall, throttle)
    │                               └── LD1117S33TR (U9: 5V → 3.3V) ── +3.3V (STM32, TXB0102)
    │
    └── Dzielnik napięcia ── ADC STM32 (pomiar VBAT)
```

- **Zakres napięcia wejściowego:** 5-80V DC
- **+5V:** zasilanie driverów EG2113D, wzmacniaczy INA240, czujników Halla
- **+3.3V:** zasilanie STM32 Black Pill i translatora TXB0102

### 2.2 Stopień mocy (Inverter)

```
BUS DC+ (VBAT)
    ├── HY4008 (Q1 - High side A) ──┐
    ├── HY4008 (Q2 - Low side A)  ──┤── Faza A (MOTOR)
    │                                │
    ├── HY4008 (Q3 - High side B) ──┤
    ├── HY4008 (Q4 - Low side B)  ──┤── Faza B (MOTOR)
    │                                │
    ├── HY4008 (Q5 - High side C) ──┤
    └── HY4008 (Q6 - Low side C)  ──┘── Faza C (MOTOR)
         │
         └── Shunty prądowe → INA240A2D
```

- 6× **HY4008** w konfiguracji 3-fazowego mostka H
- Napięcie dren-źródło: 80V
- Prąd ciągły: 180A (przy odpowiednim chłodzeniu)
- Rezystancja włączenia: 2.9 mΩ (typ.)
- Obudowa: TO-220
- **Uwaga:** Na schemacie KiCad użyto symbolu **IRFB4110** (pin-compatible, ten sam footprint TO-220)

### 2.3 Gate Driver EG2113D (×3)

Każda faza ma osobny driver **EG2113D**. W sumie 3 sztuki (U2, U3, U4).

| Pin EG2113D | Funkcja | Podłączenie |
|-------------|---------|-------------|
| 1 (LO) | Low-side output | Bramka MOSFET niska strona (przez RG 15Ω) |
| 2 (COM) | Masa power | GNDPWR |
| 3 (VCC) | Zasilanie drivera | +12V (napięcie bootstrap) |
| 6 (VS) | Phase output | Punkt środkowy half-bridge → silnik |
| 7 (VB) | Bootstrap | Dioda 1N4148 + kondensator do VS |
| 8 (HO) | High-side output | Bramka MOSFET wysoka strona (przez RG 15Ω) |
| 11 (VDD) | Logika | +3.3V (z MCU) |
| 12 (HIN) | PWM high-side in | PWMAHIGH (PA8) / PWMBHIGH (PA9) / PWMCHIGH (PA10) |
| 13 (SD) | Shutdown | SD (PB4) — wspólny sygnał (z LM393 lub STM32) |
| 14 (LIN) | PWM low-side in | PWMALOW (PB13) / PWMBLOW (PB14) / PWMCLOW (PB15) |
| 15 (VSS) | GND logiki | GND |

**Ważne:** EG2113D akceptuje sygnały logiczne 3.3V — bezpośrednio z STM32. Zastosowany pinout pozwala użyć TIM1 z trzema parami wyjść komplementarnych: PA8/PA9/PA10 jako CH1/CH2/CH3 oraz PB13/PB14/PB15 jako CH1N/CH2N/CH3N.

### 2.4 Obwody bootstrapu

Dla każdego EG2113D:
- **Dioda bootstrap:** D4, D9 (1N4148) — szybka dioda przełączająca
- **Kondensator bootstrap:** nF zakres (na schemacie C25 = 1nF)
- Rezystor bramkowy: **RG1** = 15Ω (każdy MOSFET)

### 2.5 Pomiar prądu (Current Sensing) — INA240A2D ×3

Każda faza ma osobny wzmacniacz **INA240A2D**.

| Pin INA240A2D | Funkcja | Podłączenie |
|---------------|---------|-------------|
| 1 (IN-) | Wejście odwracające | Shunt fazy (strona wysoka/niska) |
| 2 (GND) | Masa | GND |
| 3 (REF2) | Referencja 2 | Dzielnik napięcia (offset 1/2 VCC) |
| 4 (GND) | Masa | GND |
| 5 (OUT) | Wyjście | Do ADC STM32 (PHASExCURRENT) |
| 6 (V+) | Zasilanie | +5V |
| 7 (REF1) | Referencja 1 | Dzielnik napięcia (offset 1/2 VCC) |
| 8 (IN+) | Wejście nieodwracające | Shunt fazy |

- Wzmocnienie: **50 V/V** (A2D = A=50)
- Zaawansowane odrzucenie zakłóceń PWM (Enhanced PWM Rejection)
- Pomiar dwukierunkowy (bidirectional)
- Offset wyjścia: VREF/2 (2.5V dla VCC=5V)
- Do MCU są obecnie doprowadzone dwa kanały pomiarowe: **PHASE_A_CURRENT → PA0** oraz **PHASE_B_CURRENT → PA1**.
- Trzeci wzmacniacz prądowy nie jest obecnie podłączony do wejścia ADC STM32; dla FOC prąd trzeciej fazy jest wyliczany z zależności $I_C = -I_A - I_B$.

**Synchronizacja ADC:** Pomiar w środku okresu PWM (próbkowanie z triggerem TIM1).

### 2.6 Zabezpieczenie overcurrent — LM393

**LM393** (U7) — podwójny komparator z otwartym kolektorem.

- **Komparator 1:** Porównanie napięcia z INA240 (PHASEACURRENT) z progiem referencyjnym
- **Wyjście:** Sygnał `OVERCURRENT` → SD (shutdown) wszystkich EG2113D + przerwanie do STM32
- **Działanie:** Natychmiastowe wyłączenie wszystkich driverów przez pin SD

### 2.7 Translator poziomów — TXB0102DCU

**TXB0102DCU** (U8) — 2-bitowy translator poziomów napięć.

- Strona 3.3V: UART STM32 (PB6-TX, PB7-RX)
- Strona 5V: UART (J7)
- Umożliwia komunikację z urządzeniami 5V (np. FTDI, adapter USB-UART)

### 2.8 Czujniki i wejścia sterujące

| Złącze | Typ | Sygnały |
|--------|-----|---------|
| J5 (HALL) | 6-pin | HALL_A, HALL_B, HALL_C, +5V, GND, NC |
| J4 (THROTLE) | 3-pin | THROTLE (ADC), +5V, GND |
| J6 (PASS) | 4-pin | PAS, TORQUE, +5V, GND |
| J11 (BREAK) | 2-pin | BRAKE (przycisk) |

### 2.9 Komunikacja

| Interfejs | Zastosowanie |
|-----------|-------------|
| **USB-C** | Wbudowany w WeAct Black Pill (USB_FS) — programowanie, serial, debug |
| **UART** (PB6-TX, PB7-RX → J7 przez TXB0102) | Komunikacja z wyświetlaczem / ESP / adapterem 5V |
| **UART_CONFIG** (J8) | Dodatkowe złącze konfiguracyjne na PA2/PA3 |
| **SWD** | Programowanie i debug przez piny modułu Black Pill |

### 2.10 Zabezpieczenia

| Zabezpieczenie | Element | Działanie |
|----------------|---------|-----------|
| **Overcurrent** | LM393 + INA240 | HW: SD pin EG2113D (natychmiastowe wyłączenie) |
| **Overtemperature** | NTC (termistor TH1) | ADC STM32 → SW: ograniczenie prądu / wyłączenie |
| **Under-voltage** | Dzielnik VBAT → ADC | SW: zatrzymanie przy niskim napięciu |
| **Shoot-through** | EG2113D (dead-time) | Wbudowany dead-time + RG 15Ω |

## 3. Schemat blokowy

```
                         ┌─────────────────────────┐
                         │   WeAct Black Pill       │
                         │   STM32F411CEU6          │
                         │  ┌──────┐ ┌──────────┐  │
                         │  │ TIM1 │ │ ADC      │  │
                         │  │ 6ch  │ │ IN0..IN5 │  │
                         │  ┌───┴───┐ ┌────┴─────┐  │
                         │  │ UART  │ │ GPIO     │  │
                         │  │PB6/7  │ │(PB4=SD)  │  │
                         │  │PA2/3  │ │(PB5,8,9  │  │
                         │  └──┬───┘ │ =Hall)    │  │
                         │     │     └──────────┘  │
                         └─────┼────────────────────┘
                               │
          ┌─────────────────────┼─────────────────────────┐
          │                     │                         │
    ┌─────┴─────┐        ┌─────┴─────┐           ┌───────┴───────┐
    │ TXB0102   │        │ 3× INA240 │           │  LM393        │
    │ UART 5V ↔ │        │ A2D 50V/V │           │  Komparator   │
    │ 3.3V      │        │ cur.sense │           │  Overcurrent  │
    └─────┬─────┘        └─────┬─────┘           └───────┬───────┘
          │                    │                         │
          │              ┌─────┴──────┐                  │
          │              │ 3× EG2113D │◄─────────────────┘ (SD)
          │              │ Gate Drive │
          │              └─────┬──────┘
          │                    │
          │              ┌─────┴──────┐
          │              │ 6× HY4008  │
          │              │ Inverter   │
          │              └─────┬──────┘
          │                    │
          │              ┌─────┴──────┐
          │              │   SILNIK   │
          │              │   BLDC     │
          │              └────────────┘
          │
    ┌─────┴─────┐   ┌─────────┐   ┌────────┐   ┌──────────┐
    │ UART 5V   │   │ HALL    │   │THROTLE │   │ PASS/BRAKE│
    │ J7 / FTDI │   │ J5 (6p) │   │J4 (3p) │   │ J6,J11    │
    └───────────┘   └─────────┘   └────────┘   └──────────┘
```

### 3.1 Złącza mocy i silnika

Na schemacie połączenia dużej mocy nie są zebrane w jedno wielopinowe złącze:

- J1, J2, J3 — osobne wyprowadzenia faz silnika A/B/C
- J10 — VBAT
- J12 — GNDPWR
