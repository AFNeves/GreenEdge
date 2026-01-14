# GreenEdge – Arduino Firmware

This Arduino sketch is part of the **GreenEdge IoT project**.
It reads environmental sensors and controls a smart greenhouse door via Bluetooth, acting as the embedded sensing and control layer of the system.

## Hardware used

* **Main board:** Arduino (Uno / compatible)
* **Bluetooth module:** Serial Bluetooth board (HC-05 / HC-06 compatible)
* **Sensors board:** **HY-M302** (integrated sensor breakout used in the project)

## Features

* Reads sensors:
  * DHT11 (temperature & humidity)
  * LDR (luminosity via ADC)
  * LM35 (temperature via ADC)
  * Soil moisture sensor (via ADC)
* Door/lock control (logical state + status LEDs)
* Simple request/response protocol over Bluetooth serial
* Rate-limited DHT reads with NaN-safe updates
* Auto-lock safety timeout after unlocking

## Wiring / Pin Mapping

| Function               | Arduino Pin           |
| ---------------------- | --------------------- |
| DHT11                  | `D4`                  |
| LDR (analog)           | `A1`                  |
| LM35 (analog)          | `A2`                  |
| Soil moisture (analog) | `A3`                  |
| Ventilation LED        | `D9` / `D10` / `D11`  |
| Door LEDs              | `D12` / `D13`         |
| Bluetooth RX/TX        | `D2` (RX) / `D3` (TX) |

**Bluetooth baud rate:** `9600`

## Sensor handling details

### LM35 temperature calculation

The **LM35** is an analog temperature sensor that outputs a voltage linearly proportional to temperature:

* **10 mV per °C**
* Example:

  * 250 mV → 25 °C
  * 300 mV → 30 °C

On an Arduino Uno:

* ADC resolution: **10 bits**
* ADC range: **0–1023**
* Reference voltage: **5.0 V**

The temperature calculation follows these steps:

1. Read the raw ADC value:

   ```cpp
   int lm35 = analogRead(LM35_Temp);
   ```

2. Convert ADC value to voltage:

   ```cpp
   voltage = (lm35 * 5.0) / 1023.0;
   ```

3. Convert voltage to temperature:

   ```cpp
   temperature = voltage / 0.01;
   ```

This explicit calculation makes the code:

* Easy to audit
* Easy to adapt to different reference voltages (e.g. 3.3 V boards)
* Clear for anyone reviewing the firmware

### DHT11 handling and the `readDHT()` function

The **DHT11** sensor has important limitations:

* It is **slow**
* It must not be read too frequently
* It may occasionally return **invalid values** (`NaN`)

To handle this safely, all DHT access is centralized in the `readDHT()` function.

#### Why `readDHT()` exists

* Enforces a **minimum interval** between reads (e.g. 2 seconds)
* Prevents unnecessary blocking calls
* Avoids corrupting stored values with failed readings
* Ensures control logic (LEDs, ventilation) always uses the **latest valid data**

#### How it works

* The function checks if enough time has passed since the last read
* If so, it attempts a new read
* Values are only updated if the result is valid (not `NaN`)
* If a read fails, the previous valid values are preserved

This approach ensures:

* Stable behavior
* Predictable timing
* No sensor spam
* No propagation of invalid data into control logic

The function is called on every `loop()` iteration, but internally rate-limits itself.

## Protocol (Bluetooth serial)

* The Arduino expects **single-character commands**
* Replies are **one line per command**
* Format is consistent and easy to parse:

  * `KEY=VALUE`
  * `Door=Locked` / `Door=Unlocked`
  * `ERR=UnknownCmd`

> Newline characters (`\n`, `\r`) are ignored to ensure compatibility with different clients.

### Commands

| Command | Meaning                      | Example response  |
| ------- | ---------------------------- | ----------------- |
| `d`     | Read DHT11 temperature       | `DHT11_Temp=23.0` |
| `h`     | Read DHT11 humidity          | `DHT11_Hum=48.0`  |
| `t`     | Read LM35 temperature        | `LM35_Temp=24.1`  |
| `s`     | Read soil moisture (raw ADC) | `Soil_Hum=612`    |
| `l`     | Read luminosity (raw ADC)    | `LDR_Lum=723`     |
| `o`     | Unlock door                  | `Door=Unlocked`   |
| `c`     | Lock door                    | `Door=Locked`     |

## Request lifecycle and `loop()` logic

The Arduino follows a **non-blocking request/response model**.

### High-level loop behavior

Each iteration of `loop()` performs three main tasks:

1. **Sensor maintenance**

   * `readDHT()` is called to keep DHT values up to date (rate-limited internally)

2. **Command processing**

   * All available Bluetooth bytes are read
   * Each valid command triggers exactly one response
   * Unknown commands return a clear error message

3. **State-based control**

   * LED states are updated based on current temperature
   * Door auto-lock logic is evaluated using timestamps (no `delay()`)

### Why this design was chosen

* No blocking delays → responsive system
* Commands are handled immediately when received
* Control logic runs continuously, independent of requests
* Safe interaction with a multithreaded Raspberry Pi client

Each command has a **short, deterministic execution path**, making the firmware reliable even under frequent polling.

## Uploading (Arduino IDE)

1. Install the **DHT sensor library**:

   * Arduino IDE → **Sketch** → **Include Library** → **Manage Libraries**
   * Install **“DHT sensor library” by Adafruit**
2. Open `smart_greenhouse.ino`
3. Select the correct **Board** and **Port**
4. Click **Upload**

### Final note

This firmware is intentionally simple, deterministic, and robust, prioritizing:

* Reliability over complexity
* Clear protocol behavior
* Safe sensor interaction
* Easy integration with edge and cloud components
