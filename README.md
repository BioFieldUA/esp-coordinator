# ESP32-Coordinator (Zigbee NCP Firmware)

[![GitHub Release](https://img.shields.io/github/v/release/BioFieldUA/esp-coordinator)](https://github.com/BioFieldUA/esp-coordinator/releases/latest)
[![License: MIT](https://img.shields.io/badge/License-PolyForm_Strict_1.0.0-yellow.svg)](./LICENSE.txt)

A high-performance, professional Zigbee Network Co-Processor (NCP) firmware designed for modern Espressif chips (**ESP32-C5, ESP32-C6, ESP32-H2**). This project transforms your ESP32-board into a robust Zigbee Coordinator tailored for seamless integration with **Zigbee2MQTT (Z2M)**.

---

## ⚡ Flashing Methods

Choose **one** of the two methods below to flash your device. 

### Method 1: Web Installer (Easiest)
1. Connect your ESP32-board to your computer via a USB cable.
2. Open Google Chrome, Mozilla Firefox or Microsoft Edge and navigate to: **[biofieldua.github.io/esp-coordinator](https://biofieldua.github.io/esp-coordinator)**
3. Follow the on-screen instructions to flash your device directly from the browser.

### Method 2: Manual Flashing via CLI
Ensure you have the Espressif tool installed (`esptool.py`, included in ESP-IDF v5.5+ or available via `pip install esptool`).

#### ➡️ First-Time Flash (or Factory Reset)
Use the `*-factory.bin` image. This writes the entire partition layout (including the bootloader) to the chip.
```bash
esptool.py --chip esp32c5 -p COM6 -b 460800 --before=default_reset --after=hard_reset write_flash --flash_mode dio --flash_freq 80m --flash_size detect 0x0 esp-coordinator-esp32c5-usb-ceramic-v1.0.2.7-factory.bin
```
*(Adjust the `--chip` target and serial `-p` port according to your hardware)*.

#### 🔄 Firmware Update (Keep Your Zigbee Network Configuration)
Use the `*-update.bin` image. This overwrites the application code without resetting your Zigbee network data. **You must flash it to both OTA slots:**
```bash
esptool.py --chip esp32c5 -p COM6 -b 460800 write_flash 0x40000 esp-coordinator-esp32c5-usb-ceramic-v1.0.2.7-update.bin 0x180000 esp-coordinator-esp32c5-usb-ceramic-v1.0.2.7-update.bin
```

---

## 🎛️ Understanding Firmware Naming Conventions

The release page contains multiple binaries named by target parameters:
`esp-coordinator-[CHIP]-[INTERFACE]-[ANTENNA]-[VERSION]-[TYPE].bin`

* **CHIP**: `esp32c5`, `esp32c6`, or `esp32h2`.
* **INTERFACE**: `usb` (direct USB-JTAG/Serial) or `uart` (default ESP32-board RX/TX pins without rts/cts).
* **ANTENNA Configurations**:
  * `ceramic` (**Default Antenna**): Select this for standard boards. It uses whatever antenna is physically hardwired (PCB trace or Ceramic antenna). No software RF-switching is executed.
  * `external` (**Software RF-Switch Enabled**): Select this **only** if your board features a dedicated software-controlled RF switch (e.g., *Seeed Studio XIAO ESP32-C6* or *Waveshare ESP32-C5-Zero*). It forces the firmware to shift RF output to the external **IPEX/u.FL** connector, while keeping the onboard antenna as default at startup.
* **TYPE**: `factory` (for Factory Reset) or `update` (for Keep Existing Zigbee Network).

---

## ⚙️ Zigbee2MQTT Configuration

Add the following block to your Zigbee2MQTT `configuration.yaml` file. 

```yaml
serial:
  port: /dev/serial/by-id/usb-Espressif_USB_JTAG_serial_debug_unit_... (Persistent ID)
  adapter: zboss
  baudrate: 500000
  rtscts: false
advanced:
  pan_id: GENERATE
  ext_pan_id: GENERATE
  network_key: GENERATE
```

### 🔍 How to Find Your **Persistent ID** for USB connection on Linux
To find the exact string for the `/dev/serial/by-id/` path, connect your ESP32 via USB and run the following command in your Linux terminal:
```bash
ls -l /dev/serial/by-id/
```
**Example Output:**
```text
lrwxrwxrwx 1 root root 13 Jun 14 18:31 usb-Espressif_USB_JTAG_serial_debug_unit_38:8D:B3:A1:56:DC-if00 -> ../../ttyACM0
```
Just copy the entire filename (e.g., `usb-Espressif_USB_JTAG_serial_debug_unit_38:8D:B3:A1:56:DC-if00`) and append it to `/dev/serial/by-id/` inside your `configuration.yaml`.

### 📋 Selecting the Correct Serial Port (`port:`)
The correct port depends entirely on how you connected the ESP32 chip to your host system (e.g., Raspberry Pi, Orange Pi, or PC):

| Connection Type | Port Example Syntax | Description |
| :--- | :--- | :--- |
| **Direct USB (Native)** | `/dev/ttyACM0` | Standard virtual COM port when using the native ESP32 USB-JTAG/Serial interface. |
| **Direct USB (Persistent ID)** | `/dev/serial/by-id/usb-Espressif_USB_JTAG_serial_debug_unit_XXXXXX` | **Recommended for USB!** Keeps the port identical even after host reboots. Replace `XXXXXX` with your chip's specific MAC-suffix. |
| **Direct Hardware UART** | `/dev/ttyS2` (or `/dev/ttyS1`, `/dev/ttyAMA0`) | Used when connecting the host's hardware UART lines directly to the ESP32-board RX/TX pins. |
| **Windows Users** | `COM3` or `COM5` or `COM6` | Check your Device Manager to locate the exact COM index. |

### 📌 Hardware UART Pinout (Default ESP32-board RX/TX pins)
If you connect ESP32-Coordinator via **Direct Hardware UART** (e.g., `/dev/ttyS2` without flow control), use the following default pin map pre-configured in the firmware:
* **ESP32-C5**:
  * **TX**: `GPIO 11` ➡️ *(Connect to Host RX)*
  * **RX**: `GPIO 12` ➡️ *(Connect to Host TX)*
* **ESP32-C6**:
  * **TX**: `GPIO 16` ➡️ *(Connect to Host RX)*
  * **RX**: `GPIO 17` ➡️ *(Connect to Host TX)*
* **ESP32-H2**:
  * **TX**: `GPIO 24` ➡️ *(Connect to Host RX)*
  * **RX**: `GPIO 23` ➡️ *(Connect to Host TX)*
> ⚠️ **Note:** Remember that **TX goes to RX** and **RX goes to TX**. A common ground (GND) connection between the ESP32-Coordinator and your host board is strictly required.

---

## 🔄 Zigbee2MQTT OTA Updates Integration

ESP32-Coordinator natively supports OTA (Over-The-Air) self-updates driven directly from the Zigbee2MQTT dashboard. To activate this feature, you must install an **External Extension**.

1. Open your Zigbee2MQTT Frontend Dashboard.
2. Navigate to: **Settings** ➡️ **Dev console** ➡️ **External Extensions**.
3. Create new extension with the following details:
   * **Name**: `coordinator_ota_ext.mjs`
   * **Code**: Copy and paste the full script block below:

```javascript
import fs from "fs";
import os from "os";
import path from "path";
import https from "https";
import { Zcl } from "zigbee-herdsman";

export default class OTACoordinatorExtension {
    constructor(zigbee, mqtt, state, publishEntityState, eventBus, enableDisableExtension, restartCallback, addExtension, settings, logger) {
        this.zigbee = zigbee;
        this.logger = logger;
        this.otaSettings = settings.get().ota || {};
        this.onMessageBound = this.onMessage.bind(this);
        this.githubRepo = "BioFieldUA/esp-coordinator";
        this.userAgent = "Zigbee2MQTT-OTA-Client";
        this.tempFirmwarePath = path.join(os.tmpdir(), "coordinator-update.zigbee");
        this.isUpdating = false;
    }

    parseVersionToUint32(verStr) {
        const parts = verStr.replace(/^v/, '').split('.').map(p => parseInt(p, 10));
        if (parts.length !== 4 || parts.some(isNaN)) {
            throw new Error(`Invalid version format: ${verStr}`);
        }
        return (parts[0] << 24) | (parts[1] << 16) | (parts[2] << 8) | parts[3];
    }

    fetchJson(url) {
        return new Promise((resolve, reject) => {
            const options = {headers: {"User-Agent": this.userAgent, "Accept": "application/vnd.github+json"}};
            https.get(url, options, (res) => {
                let data = "";
                res.on("data", (chunk) => data += chunk);
                res.on("end", () => {
                    try { resolve(JSON.parse(data)); }
                    catch (e) { reject(e); }
                });
            }).on("error", reject);
        });
    }

    downloadFile(version, url, destPath) {
        return new Promise((resolve, reject) => {
            const request = (targetUrl) => {
                const options = {headers: {"User-Agent": this.userAgent}};
                https.get(targetUrl, options, (response) => {
                    if (response.statusCode >= 300 && response.statusCode < 400 && response.headers.location) {
                        return request(response.headers.location);
                    }
                    if (response.statusCode !== 200) {
                        return reject(new Error(`Failed to download file. Status Code: ${response.statusCode}`));
                    }
                    const file = fs.createWriteStream(destPath);
                    response.pipe(file);
                    file.on("finish", () => file.close((err) => err ? reject(err) : resolve(version)));
                    file.on("error", (err) => { fs.unlink(destPath, () => {}); reject(err); });
                }).on("error", reject);
            };
            request(url);
        });
    }

    onMessage(msg) {
        if (this.isUpdating || msg?.device.type !== "Coordinator" || msg.cluster !== "genOta" || msg.type !== "attributeReport" || !msg.data?.currentFileVersion || !msg.data.manufacturerId || !msg.data.imageTypeId || !msg.data.currentZigbeeStackVersion || !msg.data.minimumBlockReqDelay) return;
        this.isUpdating = true;
        const extLogger = this.logger;
        this.otaSettings.default_maximum_data_size = msg.data.minimumBlockReqDelay;
        const source = {
            downgrade: false,
            url: this.tempFirmwarePath
        };
        const dataSettings = {
            requestTimeout: this.otaSettings.image_block_request_timeout ?? 150000,
            responseDelay: this.otaSettings.image_block_response_delay ?? 250,
            baseSize: this.otaSettings.default_maximum_data_size ?? 50,
        };
        const queryNextImagePayload = {
            fieldControl: 1,
            manufacturerCode: msg.data.manufacturerId,
            imageType: msg.data.imageTypeId,
            fileVersion: msg.data.currentFileVersion,
            hardwareVersion: msg.data.currentZigbeeStackVersion
        };
        const tsn = msg.meta?.zclTransactionSequenceNumber || undefined;
        const originalFindMatchingOtaImage = msg.device.findMatchingOtaImage;
        this.fetchJson(`https://api.github.com/repos/${this.githubRepo}/releases/latest`).then((latestRelease) => {
            if (!latestRelease?.tag_name || !latestRelease.assets) {
                throw new Error("Invalid GitHub API response");
            }
            const onlineVersion = this.parseVersionToUint32(latestRelease.tag_name);
            extLogger.info(`[Automation] Latest Coordinator firmware is ${latestRelease.tag_name} (${onlineVersion}).`);
            if (onlineVersion <= msg.data.currentFileVersion) {
                throw { type: "NO_UPDATE" };
            }
            const asset = latestRelease.assets.find(a => a.name?.endsWith('.zigbee') && parseInt(a.name.match(/-hw(?<hw>[0-9a-fA-F]{4})/)?.groups?.hw, 16) === msg.data.currentZigbeeStackVersion);
            if (!asset?.browser_download_url || !asset.name) {
                throw new Error(`Coordinator Release ${latestRelease.tag_name} is missing a compiled *.zigbee file for Hardware Version 0x${msg.data.currentZigbeeStackVersion.toString(16).toUpperCase().padStart(4, '0')}.`);
            }
            return this.downloadFile(onlineVersion, asset.browser_download_url, source.url);
        }).then((onlineVersion) => {
            msg.device.findMatchingOtaImage = async function (src, curr, extra) {
                if (this.type !== 'Coordinator') return originalFindMatchingOtaImage.call(this, src, curr, extra);
                return { url: source.url, imageType: queryNextImagePayload.imageType, manufacturerCode: queryNextImagePayload.manufacturerCode, fileVersion: onlineVersion, force: true };
            };
            return msg.device.updateOta(
                source,
                queryNextImagePayload,
                tsn,
                {},
                (progress, remaining) => extLogger.info(`[Automation] Coordinator updating progress: ${progress.toFixed(1)}%`),
                dataSettings,
                msg?.endpoint
            );
        }).then(([from, to]) => {
            extLogger.info(`[Automation] Coordinator successfully updated (v.${from.fileVersion} => v.${to.fileVersion})`);
        }).catch((err) => {
            if (err && err.type === "NO_UPDATE") {
                extLogger.info("[Automation] Coordinator firmware is up to date.");
            } else {
                extLogger.error(`[Automation] Coordinator OTA Update Error: ${err.message}`);
            }
            if (!fs.existsSync(source.url)) {
                msg.endpoint.commandResponse("genOta", "queryNextImageResponse", { status: Zcl.Status.NO_IMAGE_AVAILABLE }, undefined, tsn).catch(() => {});
            }
        }).finally(() => {
            if (fs.existsSync(source.url)) fs.unlink(source.url, () => {});
            msg.device.findMatchingOtaImage = originalFindMatchingOtaImage;
            this.isUpdating = false;
        });
    }

    start() {
        const controller = this.zigbee.zhController;
        if (!controller) {
            return;
        }
        try {
            controller.off("message", this.onMessageBound);
            controller.on("message", this.onMessageBound);
            const port = controller.adapter?.driver?.port;
            if (port && typeof port.onPortClose === "function" && !port.__patched) {
                port.__patched = true;
                const extLogger = this.logger;
                const wait = (ms) => new Promise((resolve) => setTimeout(resolve, ms));
                const activePort = port.serialPort;
                const rawPath = port.portOptions?.path;
                if (activePort && rawPath) {
                    activePort.removeAllListeners("close");
                    const oldOnPortClose = port.onPortClose;
                    const portCloseCallback = async (err) => {
                        oldOnPortClose.call(port, err);
                        if (err) {
                            await port.stop();
                            (async () => {
                                while (true) {
                                    await wait(3000);
                                    extLogger.info("[Automation] Checking if Coordinator is reconnected...");
                                    try {
                                        if (fs.existsSync(rawPath)) {
                                            extLogger.info(`[Automation] Coordinator detected at ${rawPath}. Restarting Zigbee2MQTT...`);
                                            await wait(2000);
                                            process.exit(1);
                                        }
                                    } catch (checkError) {
                                        extLogger.error(`[Automation] Coordinator still unavailable: ${checkError.message}`);
                                    }
                                }
                            })();
                        }
                    };
                    port.onPortClose = portCloseCallback;
                    activePort.once("close", portCloseCallback);
                } else {
                    extLogger.info("[Automation] Failed to initialize Serial Port at extension startup");
                }
            }
            this.logger.info("[Automation] OTA Engine for Coordinator Initialized");
        } catch (error) {
            this.logger.error(`[Automation] Failed to initialize OTA Engine for Coordinator: ${error.message}`);
        }
    }

    stop() {
        const controller = this.zigbee.zhController;
        if (controller) {
            controller.off("message", this.onMessageBound);
        }
    }
};
```
4. Save changes and **restart** Zigbee2MQTT.

---

## 🔋 Green Power Devices (Kinetic Switches)

This firmware includes full support for battery-free **Green Power** energy-harvesting hardware (such as the *Moes Green Power kinetic 1/2/3-gang wireless switch*).
To bind and declare these custom switches, add the following handlers:

### Step 1: External Converter (Declaration)
1. Open your Zigbee2MQTT Frontend Dashboard.
2. Go to **Settings** ➡️ **Dev console** ➡️ **External Converters**.
3. Create new converter:
   * **Name**: `gp_moes_switch_ext.mjs`
   * **Code**:
```javascript
import {genericGreenPower} from "zigbee-herdsman-converters/lib/modernExtend";
import {presets, access} from "zigbee-herdsman-converters/lib/exposes";
import {hasAlreadyProcessedMessage} from "zigbee-herdsman-converters/lib/utils";

const GP_COMMANDS = {
    16: "toggle_1",
    17: "toggle_2",
    18: "toggle_3",
    24: "toggle_1",
    25: "toggle_2",
    26: "toggle_3",
    32: "toggle_1",
    33: "toggle_2",
    34: "toggle_3",
};
const baseExtend = genericGreenPower();
baseExtend.exposes = [
    presets.action(["toggle_1", "toggle_2", "toggle_3"]),
    presets.list("payload", access.STATE, presets.numeric("payload", access.STATE).withDescription("Byte")).withDescription("Payload of the command"),
];
baseExtend.options = [
    presets.text("target_toggle_1", access.SET).withLabel("Object or Group").withDescription("Friendly Name for Button 1 (toggle_1)").withCategory("config"),
    presets.text("target_toggle_2", access.SET).withLabel("Object or Group").withDescription("Friendly Name for Button 2 (toggle_2)").withCategory("config"),
    presets.text("target_toggle_3", access.SET).withLabel("Object or Group").withDescription("Friendly Name for Button 3 (toggle_3)").withCategory("config"),
];
baseExtend.fromZigbee[0].convert = (model, msg, publish, options, meta) => {
    const commandID = msg.data?.commandID;
    if (!commandID || commandID >= 0xe0 || hasAlreadyProcessedMessage(msg, model, msg.data.frameCounter, `${msg.device?.ieeeAddr}_${commandID}`)) return;
    const gpdfCommandStr = GP_COMMANDS[commandID];
    const payloadBuf = "raw" in msg.data.commandFrame ? msg.data.commandFrame.raw : undefined;
    return {
        action: gpdfCommandStr ?? `unknown_${commandID}`,
        payload: payloadBuf?.length > 0 ? Array.from(payloadBuf) : [],
    };
};
export default {
    fingerprint: [{modelID: "GreenPower_2", ieeeAddr: /^0x00000000........$/}],
    model: "ZT-B-EU2",
    vendor: "Moes",
    description: "Green Power kinetic 1/2/3-gang wireless switch",
    extend: [baseExtend],
};
```
4. Save changes.

### Step 2: External Extension (Control Logic)
1. Open your Zigbee2MQTT Frontend Dashboard.
2. Go to **Settings** ➡️ **Dev console** ➡️ **External Extensions**.
3. Create new extension:
   * **Name**: `gp_switch_light_ext.mjs`
   * **Code**:
```javascript
export default class MoesSwitchLightExtension {
    constructor(zigbee, mqtt, state, publishEntityState, eventBus, enableDisableExtension, restartCallback, addExtension, settings, logger) {
        this.mqtt = mqtt;
        this.eventBus = eventBus;
        this.logger = logger;
        this.allowedActions = new Set(["toggle_1", "toggle_2", "toggle_3"]);
        this.baseTopic = settings.get()?.mqtt?.base_topic || "zigbee2mqtt";
    }

    start() {
        this.eventBus.on("stateChange", this.onStateChange.bind(this), this);
        this.logger.info("[Automation] Moes GreenPower Multi-Switch Linker Initialized");
    }

    async onStateChange(data) {
        if (!data.update?.action || !this.allowedActions.has(data.update.action) || !data.entity?.options) {
            return;
        }
        const target = data.entity.options[`target_${data.update.action}`];
        if (target) {
            this.mqtt.onMessage(`${this.baseTopic}/${target.trim()}/set`, JSON.stringify({ state: "TOGGLE" }));
        }
    }

    stop() {
        this.eventBus.removeListeners(this);
    }
};
```
4. Save changes and **restart** Zigbee2MQTT to load both scripts.

---

## 🤝 Contributing & Support
If you encounter bugs, missing chip parameters, or want to suggest improvements, feel free to open an [Issue](https://github.com/BioFieldUA/esp-coordinator/issues/new) or submit a Pull Request!

---

## 📄 License
This project is licensed under the **[PolyForm Strict License 1.0.0](./LICENSE.txt)**.
* Non-commercial use, testing, and personal research are permitted.
* Commercial production, integration into commercial platforms, or sales require a dedicated license.

For commercial inquiries, please contact: **biofield.com.ua@gmail.com**
