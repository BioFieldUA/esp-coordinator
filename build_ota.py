import os
import sys
import re
import json

try:
    from image_builder_tool import generate_ota_file
except ImportError:
    print("ERROR: The original packer script image_builder_tool.py was not found in this folder.")
    sys.exit(1)

MANUFACTURER_ID   = 0x131B      # Espressif
IMAGE_TYPE        = 0x0001      # A custom Device Type - ZB_Coordinator
HEADER_STRING     = "ESP32-ZBDongle"

CURRENT_DIR   = os.path.dirname(os.path.abspath(__file__))
CMAKE_FILE    = os.path.abspath(os.path.join(CURRENT_DIR, "CMakeLists.txt"))
CONFIG_FILE   = os.path.abspath(os.path.join(CURRENT_DIR, "sdkconfig"))
INPUT_BIN     = os.path.abspath(os.path.join(CURRENT_DIR, "build", "esp-coordinator.bin"))
OUTPUT_DIR    = CURRENT_DIR

def get_version_from_cmake():
    if not os.path.exists(CMAKE_FILE):
        print(f"ERROR: CMake settings file not found at: {CMAKE_FILE}")
        sys.exit(1)
    with open(CMAKE_FILE, "r", encoding="utf-8") as f:
        content = f.read()
    match = re.search(r'set\s*\(\s*PROJECT_VER\s*"([^"]+)"\s*\)', content)
    if not match:
        print("ERROR: Could not find variable PROJECT_VER in file CMakeLists.txt")
        sys.exit(1)
    ver_str = match.group(1)
    try:
        parts = [int(p) for p in ver_str.split(".")]
        if len(parts) != 4:
            raise ValueError("The version must consist of exactly 4 digits (Major.Minor.Patch.Build)")
        ver_hex = (parts[0] << 24) | (parts[1] << 16) | (parts[2] << 8) | parts[3]
        return ver_str, ver_hex
    except Exception as e:
        print(f"ERROR: Version parsing get version: '{ver_str}': {e}")
        sys.exit(1)

def get_hv_from_sdkconfig():
    if not os.path.exists(CONFIG_FILE):
        print(f"ERROR: Configuration file not found at: {CONFIG_FILE}")
        sys.exit(1)
    flags = {
        "CONFIG_IDF_TARGET_ESP32C5": False,
        "CONFIG_IDF_TARGET_ESP32C6": False,
        "CONFIG_NCP_EXTERNAL_ANTENNA": False,
        "CONFIG_NCP_BUS_MODE_UART": False,
        "CONFIG_NCP_BUS_MODE_USB": False
    }
    HW_CHIP_ESP32C5         = 0x0500
    HW_CHIP_ESP32C6_CERAMIC = 0x0600
    HW_CHIP_ESP32C6_EXT     = 0x0610
    HW_BUS_UART             = 0x0001
    HW_BUS_USB              = 0x0002
    with open(CONFIG_FILE, "r", encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            match = re.match(r'^(CONFIG_[A-Z0-9_]+)=y$', line)
            if match:
                config_name = match.group(1)
                if config_name in flags:
                    flags[config_name] = True
    if flags["CONFIG_IDF_TARGET_ESP32C5"]:
        chip_mask = HW_CHIP_ESP32C5
    elif flags["CONFIG_IDF_TARGET_ESP32C6"]:
        if flags["CONFIG_NCP_EXTERNAL_ANTENNA"]:
            chip_mask = HW_CHIP_ESP32C6_EXT
        else:
            chip_mask = HW_CHIP_ESP32C6_CERAMIC
    else:
        print("ERROR: Unsupported or missing CONFIG_IDF_TARGET in sdkconfig")
        sys.exit(1)
    if flags["CONFIG_NCP_BUS_MODE_UART"]:
        bus_mask = HW_BUS_UART
    elif flags["CONFIG_NCP_BUS_MODE_USB"]:
        bus_mask = HW_BUS_USB
    else:
        print("ERROR: No valid NCP bus mode (UART/USB) defined in sdkconfig")
        sys.exit(1)
    return (chip_mask | bus_mask)

def main():
    ver_text, file_version_uint32 = get_version_from_cmake()
    hw_version_uint16 = get_hv_from_sdkconfig()
    if not os.path.exists(INPUT_BIN):
        print(f"ERROR: The assembly file was not found at: {INPUT_BIN}")
        sys.exit(1)
    print(f"[i] Firmware version: {ver_text} (32-bit HEX: 0x{file_version_uint32:08X}), Hardware version: {hw_version_uint16} (16-bit HEX: 0x{hw_version_uint16:04X})")
    if not os.path.exists(OUTPUT_DIR):
        os.makedirs(OUTPUT_DIR)
        print(f"[✓] Created target directory: {OUTPUT_DIR}")
    tag_element = {
        "tag_id": 0x0000,
        "tag_length": None,
        "tag_file": INPUT_BIN
    }
    ota_kwargs = {
        "manuf_id": MANUFACTURER_ID,
        "image_type": IMAGE_TYPE,
        "file_version": file_version_uint32,
        "header_string": HEADER_STRING,
        "tags": [tag_element],
        "min_hw_ver": hw_version_uint16,
        "max_hw_ver": hw_version_uint16,
        "file_id": 0x0BEEF11E,
        "header_version": 0x0100,
        "stack_version": 0x0002,
        "security_credentials": None,
        "upgrade_dest": None
    }
    try:
        ota_data = generate_ota_file(**ota_kwargs)
        output_name = f"esp-coordinator-v{ver_text}-hw{hw_version_uint16:04X}-ota.zigbee"
        output_path = os.path.join(OUTPUT_DIR, output_name)
        with open(output_path, "wb") as f:
            f.write(ota_data)
        print(f"[✓] SUCCESS! The OTA file has been generated: {output_name}")
    except Exception as e:
        print(f"[X] Critical OTA build Error: {e}")
        sys.exit(1)

if __name__ == "__main__":
    main()
