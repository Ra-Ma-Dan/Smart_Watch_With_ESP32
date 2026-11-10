# ⌚ Smart Watch In C (ESP-IDF Ecosystem)

A feature-rich, high-performance smartwatch firmware built entirely in C using the official **Espressif IoT Development Framework (ESP-IDF)**. This repository houses low-level hardware abstraction layers (HAL), peripheral drivers, power management state machines, and graphical porting files tailored for a wearable environment.

---

## 🛠️ Architecture & Core Components

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

---

## 📂 Project Structure

```text
Smart_Watch_In_C_ESP-IDF/
├── .devcontainer/           # Isolated development environment settings
├── .vscode/                 # Pre-configured C/C++ workspace properties and launch profiles
├── components/              # System-level drivers and communication interfaces
│   ├── ble_driver/          # BLE stack and service setup
│   ├── ota_manager/         # Seamless network update routines
│   ├── sleep_manager/       # Wearable energy-conservation states
│   ├── gc9a01/              # Circular TFT display initialization
│   ├── my_gpio / my_i2c / my_spi / my_uart  # Specialized low-level hardware abstraction
│   └── sleep_manager/       # Active-to-standby power profiling
├── lvgl_driver/             # Graphics engine orchestration layer
│   └── lvgl_port/           # Touch panel mapping and memory buffer configuration
├── main/                    # Application entryway, hardware setup, and task loops
│   ├── CMakeLists.txt
│   ├── Kconfig.projbuild    # Project configuration menu definitions
│   └── main.c               # System initialization sequence
└── peripherals/             # Dedicated chip drivers for the onboard sensor suite
    ├── AXP2101/             # Multi-rail Power Management
    ├── BMI270/              # Pedometer & Motion IMU
    ├── CST8165/             # Multi-touch capacitive screen mapping
    ├── DRV2605L/            # Advanced ERM/LRA haptic feedback
    ├── MAX17048/            # Battery fuel gauge monitoring
    ├── MAX30102/            # Photoplethysmogram (PPG) sensor
    ├── PFC85063A/           # Accurate I2C standalone RTC
    └── TMP117 / VEML7700    # Environmental temperature & luminosity tracking
```

---

## 💻 Technical Highlights

* **Native ESP-IDF v5.x Compilation:** Avoids bloated wrappers to leverage true FreeRTOS task prioritization, queue passing, and Event Groups.
* **Deep Sleep Profiling:** Leverages the `sleep_manager` alongside the external `PFC85063A` RTC and `BMI270` motion tracking to put the ESP32 chip into an ultra-low-current state until shaken or scheduled to wake up.
* **Optimized Display Pipeline:** Directly interacts with the `gc9a01` circular screen over a tuned, high-speed SPI bus via DMA (Direct Memory Access) allocations inside the LVGL porting layer.

---

## 🚀 Setting Up the Repository

### Prerequisites
* Espressif **ESP-IDF v6.1.x** environment fully configured (via command line or the VS Code Extension).
* Target Microcontroller: ESP32 variants matching your custom PCB.

### Building and Flashing
1. Clone the repository and update necessary submodules (such as `lvgl` inside components):
   ```bash
   git clone --recursive https://github.com
   cd Smart_Watch_In_C_ESP-IDF
   ```
2. Configure project parameters (such as display pins, BLE parameters, or battery targets) using the structural configuration terminal:
   ```bash
   idf.py menuconfig
   ```
3. Compile, flash to your device, and spin up the debugging monitor:
   ```bash
   idf.py build
   idf.py -p [YOUR_COM_PORT] flash monitor
   ```

---

## 📄 License
This architecture framework is made open-source and free to distribute under the terms of the **MIT License**.
