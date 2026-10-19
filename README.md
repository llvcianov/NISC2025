# Smart Plant Probe

Final project from **NISC2025** (Neapolis Innovation Summer Camp — STMicroelectronics-affiliated embedded systems training on ChibiOS/STM32), University of Naples Federico II.

A smart probe for monitoring plant care: an STM32G474RE (ChibiOS RTOS) reads sensors (temperature/humidity, soil moisture, light), scores them against a plant's ideal profile, and communicates over serial with an ESP8266 that bridges to a small web app for managing plants and viewing measurement history.

> **Status: work in progress.** This project is not finished — it still needs completion and a review pass before it's reliable.

## Structure

- [`STM32_Firmware/`](STM32_Firmware) — the ChibiOS firmware (sensor modules, finite-state machine, JSON-over-serial communication, OLED display).
- [`espSondaSondina/`](espSondaSondina) — the ESP8266 Arduino sketch and the web app it serves (including a simulator mode for development without the physical sensors).
- [`IN-OUT.txt`](IN-OUT.txt) — design notes for the serial communication protocol between the two sides.
