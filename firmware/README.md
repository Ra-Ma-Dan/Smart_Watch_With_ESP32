# ESP-IDF Smart Watch Firmware Application : ⌚ Smart Watch In C (ESPressif-IoT Development Framework Ecosystem) 

A feature-rich, high-performance smartwatch firmware built entirely in C using the official **Espressif IoT Development Framework (ESP-IDF)**. This repository houses low-level hardware abstraction layers (HAL), peripheral drivers, power management state machines, and graphical porting files tailored for a wearable environment. This subdirectory contains the primary C codebase built for the Espressif **ESP-IDF v6.x** framework. The firmware follows a highly decoupled component design, assigning isolated drivers to separate system modules to ensure long-term stability, thread safety, and straightforward testing.

---

---

## 🛠️🏗️ Architectural Principles & Core Components

The project's modular firmware design isolates custom interface drivers from peripheral chips, ensuring optimal reusability and clean RTOS task management.

### 🔌 Custom Interface Buses & Communication
* **`my_gpio`:** Configured hardware interrupts, sleep wakeup triggers, and general state pins.
* **`my_i2c` & `my_spi`:** Optimized hardware-level communication blocks driving the peripheral array and high-speed display transmission.
* **`my_uart`:** Debug logging bridge and modular communication bus.

### 🩺 Biometric & Ambient Sensors (`/peripherals`)
* **`MAX30102`:** High-sensitivity pulse oximetry and heart-rate monitoring sensor.
* **`BMI270`:** Ultra-low-power IMU for advanced step counting, motion profiles, and lift-to-wake logic.
* **`TMP117`:** High-precision clinical-grade temperature tracking.
* **`VEML7700`:** Ambient light sensor utilized for automated display brightness scaling.

### 🔋 System Infrastructure & Power
* **`AXP2101`:** Highly integrated Advanced Power Management Unit (PMU) routing voltage rails dynamically to maximize battery life.
* **`MAX17048`:** Precise fuel gauge mapping state-of-charge (SoC) percentages via algorithmic tracking.
* **`PFC85063A`:** Ultra-low-power Real-Time Clock (RTC) maintaining accurate wall-clock time independent of system state.
* **`DRV2605L`:** Haptic driver providing localized tactile patterns and notification vibrations.

### 🧠 OS & Control Managers (`/components`)
* **`ble_driver`:** Handles Bluetooth Low Energy profiles for smart notification syncing and clock anchoring.
* **`sleep_manager`:** Orchestrates transition states between Light Sleep, Deep Sleep, and high-frequency operating modes.
* **`ota_manager`:** Secure over-the-air firmware updates via Wi-Fi/BLE routines.
* **`lvgl_driver`:** Custom display port framework utilizing **LVGL (Light and Versatile Graphics Library)** for the circular `gc9a01` panel and `CST8165` capacitive touch interface.

### 🚌 Multi-Device Shared I2C Bus Management
To eliminate bus collision risks across the extensive array of peripheral sensors, the custom `my_i2c` component wraps standard I2C reads and writes inside FreeRTOS Mutex semaphores. This ensures safe multi-tasking access across the `AXP2101`, `BMI270`, `CST8165`, and other devices sharing the exact same physical trace pins.

### 🧠 FreeRTOS Task Architecture
The firmware divides operations across two processing cores to protect high-frequency sensory tasks from display rendering overhead:
1. **Core 0 (Sensory & Bus Loop):** Asynchronously services I2C sensor frames, registers interrupt handlers, and processes Bluetooth events.
2. **Core 1 (Graphics Pipeline):** Executes the LVGL visual runtime processing loop and updates the frame buffers.

---

---

## 🚀 Environment Setup & Compilation

### 🐋 Method A: Docker DevContainer (Highly Recommended)
An isolated compilation pipeline is configured under `.devcontainer/`. This completely bypasses local host toolchain installation debugging.
1. Open the `/firmware` directory inside **VS Code**.
2. Click the prompt **"Reopen in Container"** when requested.
3. Once the environment completes initialization, open the built-in terminal. It is pre-configured with active ESP-IDF paths.

### 💻 Method B: Manual Native Setup
1. Ensure you have the stable release of **ESP-IDF v5.1+** installed natively.
2. Open your terminal shell initialized with variables matching the target profile:
   ```bash
   . \$HOME/export.sh # Linux/macOS path wrapper
   ```
3. Change into the application root directory:
   ```bash
   cd Smart_Watch_with_esp_idf/firmware
   ```

---

## 🛠️ Step-by-Step Compilation Commands

### 1. Define the Hardware Target
Configure the project explicitly to map against the ESP32-S3 SOC core:
```bash
idf.py set-target esp32s3
```

### 2. Adjust System Configurations (Menuconfig)
Open the interactive terminal overlay menu to toggle specific device drivers, adjust CPU clocking speeds, or reconfigure custom smart watch parameters:
```bash
idf.py menuconfig
```

### 3. Build the Binaries
Compile the main binary partition layouts, bootloaders, and external file distributions:
```bash
idf.py build
```

### 4. Flash and Monitor Real-Time Debug Logs
Connect your smart watch hardware module through its built-in UART/USB line, update the template serial parameter match below, and run:
```bash
idf.py -p [YOUR_SERIAL_PORT] flash monitor
```
*(To exit the live console tracking display terminal later, press `Ctrl + ]`)*.

---

## 🔄 OTA (Over-The-Air) Deployments
The internal `ota_manager` handles wireless platform updates over a secure Wi-Fi connection. It utilizes dual flash partition banks (`ota_0` and `ota_1`) along with an anti-rollback enforcement system to ensure safe recovery if a new build encounters runtime validation failures.

## 📄 License
This architecture framework is made open-source and free to distribute under the terms of the **MIT License**.
