#include "storage.h"
#include "esp_littlefs.h"
#include "esp_log.h"
#include "esp_err.h"
#include "unistd.h"

esp_err_t storage::load_config(zb_app_config_t& config) {
    FILE* f = fopen(CONFIG_FILE, "rb");
    if (!f) {
        ESP_LOGD(TAG, "File '%s' not found. Creating default...", CONFIG_FILE);
        esp_err_t ret = save_config(config);
        if (ret != ESP_OK) return ret;
        f = fopen(CONFIG_FILE, "rb");
    }
    if (!f) return ESP_ERR_NOT_FOUND;
    size_t readed = fread(&config, 1, sizeof(zb_app_config_t), f);
    fclose(f);
    if (readed != sizeof(zb_app_config_t)) {
        ESP_LOGE(TAG, "Read size mismatch: expected %d, got %d", sizeof(zb_app_config_t), readed);
        return ESP_ERR_INVALID_SIZE;
    }
    return ESP_OK;
}

esp_err_t storage::save_config(const zb_app_config_t& config) {
    FILE* f = fopen(CONFIG_FILE, "wb");
    if (!f) {
        ESP_LOGE(TAG, "Failed to open file '%s' for writing", CONFIG_FILE);
        return ESP_ERR_NOT_FOUND;
    }
    size_t written = fwrite(&config, 1, sizeof(zb_app_config_t), f);
    fclose(f);
    if (written != sizeof(zb_app_config_t)) {
        ESP_LOGE(TAG, "Write size mismatch: expected %d, got %d", sizeof(zb_app_config_t), written);
        return ESP_ERR_INVALID_SIZE;
    }
    return ESP_OK;
}

esp_err_t storage::write_param(size_t offset, const void* src, size_t size) {
    if (src == nullptr) return ESP_ERR_INVALID_ARG;
    FILE* f = fopen(CONFIG_FILE, "rb+");
    if (!f) return ESP_ERR_NOT_FOUND;
    if (fseek(f, offset, SEEK_SET) != 0) {
        fclose(f);
        return ESP_ERR_INVALID_ARG;
    }
    size_t written = fwrite(src, 1, size, f);
    fclose(f);
    return (written == size) ? ESP_OK : ESP_ERR_INVALID_SIZE;
}

esp_err_t storage::read_param(size_t offset, void* dst, size_t size) {
    if (dst == nullptr) return ESP_ERR_INVALID_ARG;
    FILE* f = fopen(CONFIG_FILE, "rb");
    if (!f) return ESP_ERR_NOT_FOUND;
    if (fseek(f, offset, SEEK_SET) != 0) {
        fclose(f);
        return ESP_ERR_INVALID_ARG;
    }
    size_t read = fread(dst, 1, size, f);
    fclose(f);
    return (read == size) ? ESP_OK : ESP_ERR_INVALID_SIZE;
}

esp_err_t storage::delete_config() {
    if (unlink(CONFIG_FILE) == 0) {
        ESP_LOGD(TAG, "File '%s' has been deleted", CONFIG_FILE);
        return ESP_OK;
    }
    return ESP_ERR_NOT_FOUND;
}

esp_err_t storage::format() {
    if (_is_mounted) {
        esp_vfs_littlefs_unregister(PARTITION);
        _is_mounted = false;
    }
    esp_err_t ret = esp_littlefs_format(PARTITION);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to format Partition '%s' (error: %s)", PARTITION, esp_err_to_name(ret));
    }
    else {
        ESP_LOGD(TAG, "Partition '%s' formatted successfully", PARTITION);
    }
    return ret;
}

esp_err_t storage::init() {
    if (_is_mounted) return ESP_OK;
    esp_vfs_littlefs_conf_t conf = {
        .base_path = BASE_PATH,
        .partition_label = PARTITION,
        .partition = nullptr,
        .format_if_mount_failed = false,
        .read_only = false,
        .dont_mount = false,
        .grow_on_mount = false
    };
    esp_err_t ret = esp_vfs_littlefs_register(&conf);
    if (ret != ESP_OK) {
        switch (ret) {
        case ESP_ERR_INVALID_STATE:
            _is_mounted = true;
            ESP_LOGD(TAG, "Partition '%s' already mounted", PARTITION);
            return ESP_OK;
        case ESP_ERR_NOT_FOUND:
            ESP_LOGE(TAG, "Partition '%s' is not found in partition table", PARTITION);
            return ret;
        case ESP_ERR_NO_MEM:
            ESP_LOGE(TAG, "Out of memory");
            return ret;
        case ESP_FAIL:
        default:
            ESP_LOGW(TAG, "Error: %s. Attempting to recover '%s' by formatting...", esp_err_to_name(ret), PARTITION);
            if ((ret = storage::format()) != ESP_OK) {
                return ret;
            }
            ret = esp_vfs_littlefs_register(&conf);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "Failed to mount even after format: %s", esp_err_to_name(ret));
                return ret;
            }
        }
    }
    _is_mounted = true;
    size_t total = 0, used = 0;
    if ((ret = esp_littlefs_info(PARTITION, &total, &used)) == ESP_OK) {
        ESP_LOGD(TAG, "Partition '%s' mounted. Total: %d bytes, Used: %d bytes", PARTITION, total, used);
    }
    return ret;
}
