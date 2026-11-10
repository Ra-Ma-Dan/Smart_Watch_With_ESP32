# ESP32-S3 Extensible Smart Watch Project

A feature-rich, low-power open-source smart watch built entirely on the **ESP-IDF (Espressif IoT Development Framework)** framework. This repository contains a modular firmware application supporting high-performance graphics via LVGL, advanced power routing, step tracking, and an array of environmental and biometric monitoring sensors.

---

## 📂 Project Structure

Below is the layout of the repository. It separates the firmware workspace, visual design presets, hardware schematics, and Git operational footprints:

```text
Smart_Watch_with_esp_idf/
├── .vscode/                     # Workspace settings and configurations
├── firmware/                    # Main ESP-IDF build application
│   ├── .devcontainer/           # Isolated Docker container build configs
│   ├── .vscode/                 # Compilation paths, C/C++ properties, and debug launch options
│   ├── components/              # Modular, isolated hardware peripheral abstractions
│   │   ├── ble_driver/          # NIMBLE/Bluedroid Bluetooth transport layer
│   │   ├── gc9a01/              # SPI Display driver implementation
│   │   ├── my_gpio/             # ISR and GPIO hardware macro bindings
│   │   ├── my_i2c/              # Shared I2C master bus communication controller
│   │   ├── my_spi/              # Multi-device SPI bus abstraction
│   │   ├── my_uart/             # Debugging log console interface configuration
│   │   ├── ota_manager/         # Secure Over-The-Air firmware updater
│   │   └── sleep_manager/       # Light/Deep sleep power orchestration logic
│   ├── lvgl_driver/             # Graphics engine routing port layer
│   ├── main/                    # FreeRTOS core entry points (`main.c` / task loops)
│   └── peripherals/             # Sensor definitions and registry maps
│       ├── AXP2101/             # Multi-channel programmable Power Management Unit (PMU)
│       ├── BMI270/              # 6-axis IMU (Accelerometer / Gyroscope)
│       ├── CST8165/             # I2C Capacitive Multi-touch screen interface
│       ├── DRV2605L/            # ERM/LRA Haptic feedback vibration motor engine
│       ├── MAX17048/            # Ultra-compact ModelGauge battery fuel gauge
│       ├── MAX30102/            # Pulse Oximetry and Heart-rate monitoring sensor
│       ├── PFC85063A/           # Ultra-low power highly precise Real-Time Clock
│       ├── TMP117/              # High-precision digital temperature monitor
│       └── VEML7700/            # High-accuracy ambient light sensor
├── hardware/                    # [Placeholder] PCB layout, schematics, and 3D enclosures
├── .gitignore                   # Specified operational and compile file exclusions
├── LICENSE                      # Project license terms
└── README.md                    # Primary repository overview
```

---

## ⚡ Main System Core Features

### 🖥️ Display & Interactive Graphics
* **GC9A01 Driver Integrated:** Controls the 1.28" round color display via a dedicated high-speed SPI bus interface.
* **LVGL Processing Port:** Runs smooth user interfaces leveraging optimized buffer swapping configurations.
* **Capacitive Touch:** Uses a `CST8165` driver supporting responsive gesture navigation and wake-up events.

### 🔋 Micro-Power Management & Fueling
* **Orchestrated PMU Engine:** The `AXP2101` dynamically regulates distinct software-controlled LDO/DC-DC power rails.
* **Precise SoC Tracking:** The `MAX17048` fuel gauge calculates strict battery percentages and voltage degradation using a proprietary algorithm.
* **Haptic Vibration Management:** The `DRV2605L` handles rich tactile patterns for crisp user alarms and operational notifications.

### 🩺 Biometrics & Environmental Metrics
* **Heart Rate & SpO2:** Driven by the `MAX30102` sensor suite via an asynchronous task matrix.
* **Precision Thermal Readout:** `TMP117` tracking records skin/ambient body temperature down to fractions of a degree Celsius.
* **Illuminance Detection:** The `VEML7700` captures ambient light levels to dynamically scale display backlight brightness.
* **Activity & Motion Profiling:** The `BMI270` IMU detects acceleration trends, enabling gesture activation and custom step tracking.

---

## 🛠️ Hardware Subsystem (Upcoming)
The `/hardware` directory is currently reserved for the upcoming hardware release phase. Future iterations will contain:
* **KiCAD PCB Layouts:** Double/Multi-layer dense circular smartwatch layout.
* **Schematics:** Detailed mapping of the ESP32-S3 system block with sensor power domains.
* **3D Enclosures:** STEP and STL files for custom 3D-printable cases and bezels.

---

## 📄 License
This project is licensed under the conditions specified within the root [LICENSE] using **MIT License**.
