# Tac Photo 🖼️

**A "Calm Tech" Digital Photo Frame with Screenless NFC Configuration.**

Tac Photo is a proof-of-concept IoT device that rethinks the user experience of connected hardware. Instead of clunky touchscreens or complex on-device menus, it uses **NFC cards** as physical keys to configure Wi-Fi, sync accounts, and change settings.

Built with **ESP32** and **FreeRTOS**, focused on minimalism, security, and tangibility.

## 🧘 Philosophy

> **"Lateral Thinking with Withered Technology"** — Gunpei Yokoi

We believe smart devices should be quiet. They shouldn't demand your attention with settings menus. Tac Photo separates the **Configuration** (done comfortably on your phone) from the **Appreciation** (done purely on the device).

## ✨ Features

* **Screenless Configuration:** No buttons, no touch digitizer required. Just tap an NFC card to set it up.
* **Tangible UI:** Physical cards represent contexts. Insert a "Travel Card" to show travel photos; insert a "Settings Card" to update Wi-Fi.
* **Secure Provisioning:** Wi-Fi credentials and OAuth tokens are transferred via NFC using **AES-128 GCM** encryption.
* **Headless OAuth:** Innovative "Token Teleportation" mechanism to authenticate Google Photos without a browser on the device.
* **Instant On:** Powered by FreeRTOS for immediate startup compared to Linux/Android frames.

## 🛠️ Hardware Architecture

The system is designed to be minimal and reproducible.

* **MCU:** Espressif ESP32-S3 (Dual Core, Wi-Fi/BLE)
* **Display:** IPS LCD / e-Paper (Supported via LVGL)
* **Sub-Display:** Segment LCD (TM1622) for minimal status indication (e.g., "F-Number" style metadata)
* **NFC Reader:** WS1850S (compatible with M5Stack Unit RFID 2)
* **Sensors:**
    * **BH1750:** Ambient Light Sensor for auto-brightness.
    * **AM312:** PIR Motion Sensor for auto-wake/sleep.

## 🚀 Getting Started

### Prerequisites

* [ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/)
* An ESP32-S3 development board
* WS1850S NFC Reader connected via I2C

### Build & Flash

1.  **Clone the repository:**
    ```bash
    git clone [https://github.com/your-username/tac-photo.git](https://github.com/your-username/tac-photo.git)
    cd tac-photo
    ```

2.  **Set up the target:**
    ```bash
    idf.py set-target esp32s3
    ```

3.  **Configuration (Optional):**
    ```bash
    idf.py menuconfig
    ```
    * Configure Pin assignments under `Tac Photo Hardware Config`.

4.  **Build and Flash:**
    ```bash
    idf.py build flash monitor
    ```

## 📱 Usage Flow

1.  **Configure on Phone:** Open the **Tac Photo Companion App** (PWA).
2.  **Write to Card:** Enter your Wi-Fi credentials or log in to Google Photos on your phone. The app encrypts this data and writes it to a standard NFC tag (NTAG213/215).
3.  **Touch to Device:** Tap the card on Tac Photo's reader.
4.  **Enjoy:** The device decrypts the config, connects to the cloud, and starts displaying your photos autonomously.

## 📂 Project Structure

* `main/` - Core firmware source code (FreeRTOS tasks, App logic).
* `components/` - Custom drivers and libraries.
    * `ws1850s`: NFC Reader driver.
    * `tm1622`: Segment LCD driver.
    * `lvgl_port`: Display interface integration.
* `assets/` - Static resources and default images.

## 📄 License

This project is open source and available under the **MIT License**. See the [LICENSE](LICENSE) file for details.

Hardware schematics and PCB data will be released under **CERN-OHL**.

## 🙌 Credits

* Built with [ESP-IDF](https://github.com/espressif/esp-idf)
* UI powered by [LVGL](https://lvgl.io/)
* NFC logic inspired by industrial provisioning standards.
