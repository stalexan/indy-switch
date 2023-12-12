#include "indy_nvs.h"

#include <esp_log.h>
#include <nvs_flash.h>

#include <vector>

#include "indy_util.h"

namespace {
  const char* const NVS_NAMESPACE = "storage";
  const char *TAG = "indy_nvs";
}

// Sets up ESP32 NVS
void IndyNvs::Setup() {
  // Initialize NVS
  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());                   // NVS partition was truncated and needs to be erased
    err = nvs_flash_init();                               // Retry nvs_flash_init
  }
  ESP_ERROR_CHECK(err);

  // Open NVS
  err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Error opening NVS: %s", esp_err_to_name(err));
  }
}

IndyNvs::~IndyNvs() {
  // Close NVS
  nvs_close(handle);
}

// Commits changes
void IndyNvs::Commit() {
  ESP_LOGI(TAG, "Committing changes to NVS");
  esp_err_t err = nvs_commit(handle);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Error committing to NVS: %s", esp_err_to_name(err));
  }
}

// Erases all values from NVS
void IndyNvs::Reset() {
  ESP_LOGI(TAG, "Resetting");
  esp_err_t err = nvs_flash_erase();
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Error erasing flash: %s", esp_err_to_name(err));
  }
}

// Stores boolean `value` to `key`
void IndyNvs::WriteBool(const char* key, bool value) {
  ESP_LOGI(TAG, "Writing bool '%s': %d", key, value);
  esp_err_t err = nvs_set_i8(handle, key, value);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Error writing to NVS: %s", esp_err_to_name(err));
  }
}

// Reads boolean found at `key` and saves it to `result`. Returns `true` if
// the read was successful, or `false` otherwise. Default for `result` is
// `false` if there was an error or `key` was not found.
bool IndyNvs::ReadBool(const char* key, bool* result) {
  // Read the value
  int8_t value = false;
  esp_err_t err = nvs_get_i8(handle, key, &value);
  if (err != ESP_OK) {
    if (err == ESP_ERR_NVS_NOT_FOUND) {
      ESP_LOGI(TAG, "No value found for bool '%s'", key);
    } else {
      ESP_LOGE(TAG, "Error reading bool '%s': %s", key, esp_err_to_name(err));
    }
    *result = false;
    return false;
  } else {
    ESP_LOGI(TAG, "Read bool '%s': %" PRId8, key, value);
  }

  // Convert value to a bool
  if (value == true || value == false) {
    *result = value;
    return true;
  } else {
    ESP_LOGE(TAG, "Read bool '%s' is %" PRId8 " and not 0 or 1. Using 0 (false).", key, value);
    *result = false;
    return false;
  }
}

// Returns boolean found at `key`. Default is `false` if there was an
// error or `key` was not found.
bool IndyNvs::ReadBool(const char* key) {
  bool result;
  ReadBool(key, &result);
  return result;
}

// Stores `time` to `key`
void IndyNvs::WriteTime(const char* key, time_t time) {
  ESP_LOGI(TAG, "Writing time '%s': %" PRId64, key, (int64_t) time);
  esp_err_t err = nvs_set_i64(handle, key, (int64_t) time);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Error writing time: %s", esp_err_to_name(err));
  }
}

// Reads time found at `key` and saves it to `result`. Returns `true` if
// the read was successful, or `false` otherwise. Default for `result` is
// `NULL_TIME` if there was an error or `key` was not found.
bool IndyNvs::ReadTime(const char* key, time_t* result) {
  time_t time;
  esp_err_t err = nvs_get_i64(handle, key,  reinterpret_cast<int64_t*>(&time));
  if (err != ESP_OK) {
    if (err == ESP_ERR_NVS_NOT_FOUND) {
      ESP_LOGI(TAG, "No value found for time '%s'", key);
    } else {
      ESP_LOGE(TAG, "Error reading time '%s': %s", key, esp_err_to_name(err));
    }
    *result = NULL_TIME;
    return false;
  }
  ESP_LOGI(TAG, "Read time '%s': %" PRId64, key, time);
  *result = time;
  return true;
}

// Returns time found at `key`. Default is `NULL_TIME` if there was an error or
// `key` was not found.
time_t IndyNvs::ReadTime(const char* key) {
  time_t result;
  ReadTime(key, &result);
  return result;
}

// Stores integer `value` to `key`
void IndyNvs::WriteInt(const char* key, int32_t value) {
  ESP_LOGI(TAG, "Writing int '%s': %" PRIi32, key, value);
  esp_err_t err = nvs_set_i32(handle, key, value);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Error writing int: %s", esp_err_to_name(err));
  }
}

// Reads integer found at `key` and saves it to `result`. Returns `true` if
// the read was successful, or `false` otherwise. Default for `result` is
// `0` if there was an error or `key` was not found.
bool IndyNvs::ReadInt(const char* key, int32_t* result) {
  esp_err_t err = nvs_get_i32(handle, key, result);
  if (err != ESP_OK) {
    if (err == ESP_ERR_NVS_NOT_FOUND) {
      ESP_LOGI(TAG, "No value found for int '%s'", key);
    } else {
      ESP_LOGE(TAG, "Error reading int '%s': %s", key, esp_err_to_name(err));
    }
    *result = 0;
    return false;
  }
  ESP_LOGI(TAG, "Read int '%s': %" PRIi32, key, *result);
  return true;
}

// Returns integer found at `key`. Default is `0` if there was an error or
// `key` was not found.
int32_t IndyNvs::ReadInt(const char* key) {
  int32_t result;
  ReadInt(key, &result);
  return result;
}

// Stores string `value` to `key`
void IndyNvs::WriteString(const char* key, const char* value) {
  ESP_LOGI(TAG, "Writing string '%s': %s", key, value);
  esp_err_t err = nvs_set_str(handle, key, value);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Error writing string: %s", esp_err_to_name(err));
  }
}

// Reads string found at `key` and saves it to `result`. Returns `true` if
// the read was successful, or `false` otherwise. Default for `result` is
// `""` if there was an error or `key` was not found.
bool IndyNvs::ReadString(const char* key, std::string* result) {
  // Determine buffer length needed
  size_t length;
  esp_err_t err = nvs_get_str(handle, key, nullptr, &length);
  if (err != ESP_OK) {
    if (err == ESP_ERR_NVS_NOT_FOUND) {
      ESP_LOGI(TAG, "No value found for string '%s'", key);
    } else {
      ESP_LOGE(TAG, "Error reading length of string '%s': %s", key, esp_err_to_name(err));
    }
    *result = "";
    return false;
  }

  // Read string
  std::vector<char> buffer(length + 1);  // +1 for null terminator
  err = nvs_get_str(handle, key, buffer.data(), &length);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Error reading string '%s': %s", key, esp_err_to_name(err));
    *result = "";
    return false;
  }
  *result = std::string(buffer.data());
  ESP_LOGI(TAG, "Read string '%s': %s", key, result->c_str());
  return true;
}

// Returns string found at `key`. Default is `""` if there was an error or
// `key` was not found.
std::string IndyNvs::ReadString(const char* key) {
  std::string result;
  ReadString(key, &result);
  return result;
}
