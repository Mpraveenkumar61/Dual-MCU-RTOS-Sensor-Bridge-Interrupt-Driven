# Dual-MCU RTOS Sensor Bridge
### STM32 & ESP32 — Interrupt-Driven UART Data Pipeline

> An embedded system using FreeRTOS and hardware UART interrupts to stream
> live analog sensor data from an ESP32 to an STM32F103, displayed in
> real-time on an SSD1306 OLED.

---

## Table of Contents

- [Overview](#overview)
- [System Architecture](#system-architecture)
- [How It Works](#how-it-works)
- [Communication Protocol](#communication-protocol)
- [Hardware Requirements](#hardware-requirements)
- [Wiring](#wiring)
- [Software Stack](#software-stack)
- [How to Build](#how-to-build)

---

## Overview

Two microcontrollers work together over a UART link to read a potentiometer
and display its value live on an OLED screen.

The STM32 receives data using `HAL_UART_Receive_IT()` — a fully
**interrupt-driven** approach. This eliminates the buffer overflow problem
that caused runaway / merged values in earlier polling-based designs.
The CPU is free between bytes; no busy-waiting, no dropped newlines.

---

## System Architecture

```
┌─────────────────────────────┐   UART 115200 baud    ┌─────────────────────────────┐
│        ESP32                │ ───────────────────►  │        STM32F103            │
│      (Producer)             │  GPIO17 TX → PA10 RX  │       (Consumer)            │
│                             │                        │                             │
│  ADC Task                   │                        │  UART RX Interrupt          │
│  └─ 12-bit, 16-sample avg   │                        │  HAL_UART_Receive_IT()      │
│  └─ Dead-band filter        │                        │  └─ 1 byte at a time        │
│                             │                        │  └─ State machine parser    │
│  UART TX Task               │                        │  └─ Re-arms after each byte │
│  └─ Format: value + \n      │                        │                             │
│  └─ 50 ms pacing delay      │                        │  OLED Display Task          │
│                             │                        │  └─ Reads from queue        │
└─────────────────────────────┘                        │  └─ Renders on SSD1306      │
                                                        └──────────────┬──────────────┘
                                                                       │ I2C
                                                             ┌─────────▼─────────┐
                                                             │  SSD1306 OLED     │
                                                             │  128 × 64         │
                                                             └───────────────────┘
```

---

## How It Works

### Node A — ESP32 (Producer)

**ADC Sampling Task**
- Reads the potentiometer at 12-bit resolution (0 – 4095)
- Averages 16 samples per burst to smooth noise
- Dead-band filter: only sends a new value when the change exceeds a set threshold

**UART TX Task**
- Pulls values from an internal FreeRTOS queue
- Transmits as ASCII: `<value>\n`
- 50 ms pacing delay to match the STM32 display refresh rate

---

### Node B — STM32 (Consumer)

**UART RX — Interrupt Driven**
- `HAL_UART_Receive_IT()` arms the hardware to fire `HAL_UART_RxCpltCallback` on every single received byte
- The callback runs a simple state machine:
  - Digit character → append to buffer
  - `\n` or `\r` → parse buffer with `atoi()`, push to queue, reset buffer
  - Non-numeric character → discard
  - Buffer full without terminator → force reset (overflow guard)
- Interrupt is re-armed immediately after each byte

**OLED Display Task**
- Blocks on `uart_to_oled_queue` — wakes only when new data arrives
- Clears the screen, renders the sensor value and a status line
- Drives the SSD1306 over I2C

---

## Communication Protocol

| Parameter  | Value         |
|------------|---------------|
| Baud Rate  | 115200        |
| Data Bits  | 8             |
| Parity     | None          |
| Stop Bits  | 1             |
| Encoding   | ASCII text    |
| Terminator | `\n`  (LF)    |

**Example frame:**
```
2048\n
```

---

## Hardware Requirements

| Component        | Part                       |
|------------------|----------------------------|
| MCU — Producer   | ESP32-WROOM-32             |
| MCU — Consumer   | STM32F103RB  (NUCLEO)  |
| Display          | SSD1306 128×64 I2C OLED    |
| Sensor           | 10 kΩ Potentiometer        |
| Programmer       | ST-Link V2                 |

---

## Wiring

```
ESP32                            STM32F103
─────                            ─────────
GPIO17  (TX)  ────────────────►  PA10  (UART1 RX)
GND           ────────────────►  GND              ← mandatory for signal integrity

STM32F103                        SSD1306 OLED
─────────                        ────────────
PB6  (I2C1 SCL)  ─────────────►  SCL
PB7  (I2C1 SDA)  ─────────────►  SDA
3.3V             ─────────────►  VCC
GND              ─────────────►  GND
```

---

## Software Stack

| Node      | Tools                                          |
|-----------|------------------------------------------------|
| ESP32     | ESP-IDF · VS Code · FreeRTOS Native API        |
| STM32     | STM32CubeIDE · HAL Drivers · FreeRTOS CMSIS-RTOS V1 |

**Required includes in STM32 `freertos.c`:**
```c
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "string.h"
#include "stdlib.h"
```

---

## How to Build

### ESP32

```bash
idf.py build
idf.py flash
idf.py monitor      # optional — view live serial output
```

### STM32

1. Open the project in **STM32CubeIDE**
2. Confirm FreeRTOS include paths are present under
   `Properties → C/C++ Build → Settings → MCU GCC Compiler → Include Paths`
3. `Project → Clean Project`
4. `Project → Build Project`
5. Flash using **ST-Link** via `Run → Debug`

---

## License

MIT License — free to use, modify, and distribute.
