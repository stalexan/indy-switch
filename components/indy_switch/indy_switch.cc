#include "indy_switch.h"

#include <time.h>

#include <esp_log.h>
#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <FreeRTOSConfig.h>
#include <soc/clk_tree_defs.h>

#include <string>
#include <vector>

#include "cJSON.h"
#include "indy_config.h"
#include "indy_json.h"
#include "indy_scheduler.h"
#include "indy_task_manager.h"
#include "indy_util.h"

namespace {
  const char *TAG = "indy_switch";
  const char *NVS_KEY_IS_ON = "is_on";
  const char *NVS_KEY_CONFIG_TIMEZONE = "timezone";
  const char *NVS_KEY_CONFIG_RANDOM_OFFSET_RANGE = "offset";
  const char *NVS_KEY_CONFIG_SUNTIMES = "suntimes";
}

// Initial configuration, from the file main/initial_config.json
extern const uint8_t initial_config_start[]  asm("_binary_initial_config_json_start");
extern const uint8_t initial_config_end[]    asm("_binary_initial_config_json_end");

void IndySwitch::Setup() {
  // Create the is_on mutex, to control access to is_on
  is_on_mutex = xSemaphoreCreateMutex();
  if (is_on_mutex == nullptr) {
    ESP_LOGE(TAG, "Create is on mutex failed");
    abort();
  }

  // Setup the ESP32
  nvs.Setup();
  wifi.Setup();
  mdns.Setup();
  mqtt.Setup();
  time.Setup([this]() { HandleTimeSynced(); });

  // Setup peripherals
  led.Setup();
  button.Setup();
  relay.Setup();

  // Load configuration
  LoadInitialConfig();  // Configuration flashed to device
  LoadSavedConfig();    // Configuration set at runtime and saved to NVS

  // Restore state
  ESP_LOGI(TAG, "Restoring is on state");
  SetSwitch(nvs.ReadBool(NVS_KEY_IS_ON));

  // Register button press handler
  button.RegisterButtonPressHandler([this]() {
    ESP_LOGI(TAG, "Button press handler called");
    ToggleSwitch();
  });

  // Register next action handler
  scheduler.RegisterNextActionHandler([this](bool on) {
    ESP_LOGI(TAG, "Register next action handler called");
    SetSwitch(on);
  });

  // Create  MQTT topics
  control_topic = FormatString("indy-switch/%s/control", HOSTNAME);
  config_topic = FormatString("indy-switch/%s/config", HOSTNAME);
  status_topic = FormatString("indy-switch/%s/status/get", HOSTNAME);
  restart_topic = FormatString("indy-switch/%s/restart", HOSTNAME);
  reset_topic = FormatString("indy-switch/%s/reset", HOSTNAME);

  // Register MQTT connected handler
  mqtt.RegisterConnectedHandler([this]() { HandleMqttConnected(); });

  ESP_LOGI(TAG, "Setup completed");
}

const char* SwitchStateAsStr(bool on) {
  return on ? "ON" : "OFF";
}

// Toggles switch on and off
void IndySwitch::ToggleSwitch() {
    ESP_LOGI(TAG, "Toggling switch");
    SetSwitch(!is_on);
}

// Sets switch on and off
void IndySwitch::SetSwitch(bool on) {
  // Acquire the mutex needed to change is on state
  if (xSemaphoreTake(is_on_mutex, MAX_WAIT) != pdTRUE) {
    ESP_LOGE(TAG, "Failed to acquire is on mutex");
    return;
  }

  // Is this a change?
  const char* on_str = SwitchStateAsStr(on);
  if (is_on == on) {
    // No change is needed
    ESP_LOGI(TAG, "Switch is %s", on_str);

    // Release mutex
    if (xSemaphoreGive(is_on_mutex) != pdTRUE)
      ESP_LOGE(TAG, "Failed to release is on mutex");

    return;
  }

  // Update peripherals
  if (on) {
    led.TurnOn();
    relay.TurnOn();
  } else {
    led.TurnOff();
    relay.TurnOff();
  }
  ESP_LOGI(TAG, "Turned switch %s", on_str);

  // Store new state
  is_on = on;
  nvs.WriteBool(NVS_KEY_IS_ON, on);
  nvs.Commit();

  // Release mutex
  if (xSemaphoreGive(is_on_mutex) != pdTRUE)
    ESP_LOGE(TAG, "Failed to release is on mutex");
}

// Subscribes to MQTT topics
void IndySwitch::HandleMqttConnected() {
  // Subscribe to control topic
  mqtt.SubscribeToTopic(
    control_topic.c_str(),
    [this](const cJSON* content, JsonParser* parser) -> MqttResponse {
      return HandleControlMessage(content, parser); });

  // Subscribe to config topic
  mqtt.SubscribeToTopic(
    config_topic.c_str(),
    [this](const cJSON* content, JsonParser* parser) -> MqttResponse {
      return HandleConfigMessage(content, parser); });

  // Subscribe to status topic
  mqtt.SubscribeToTopic(
    status_topic.c_str(),
    [this](const cJSON* content, JsonParser* parser) -> MqttResponse {
      return HandleStatusMessage(content, parser); });

  // Subscribe to reboot topic
  mqtt.SubscribeToTopic(
    restart_topic.c_str(),
    [this](const cJSON* content, JsonParser* parser) -> MqttResponse {
      return HandleRestartMessage(content, parser); });
}

// Handles MQTT data received from control topic, to turn switch on and off
MqttResponse IndySwitch::HandleControlMessage(const cJSON* content, JsonParser* parser) {
  parser->SetTag(TAG);

  // Log message content
  char *content_str = cJSON_Print(content);
  ESP_LOGI(TAG, "Received MQTT control data:\n%s", content_str);
  free(content_str);

  // Get switch_on message parameter
  JsonResult<bool> switch_on = parser->GetBool(content, "header", "switch_on");
  if (switch_on.is_error)
    return MqttResponse(MQTT_BAD_REQUEST, switch_on.message);

  // Turn switch on/off
  ESP_LOGI(TAG, "HandleControlMessage is setting switch %s", SwitchStateAsStr(switch_on.value));
  SetSwitch(switch_on.value);

  return MqttResponse(MQTT_OK);
}

// Handles MQTT data received from config topic, to configure device
MqttResponse IndySwitch::HandleConfigMessage(const cJSON* content, JsonParser* parser) {
  parser->SetTag(TAG);

  // Log message content
  char *content_str = cJSON_Print(content);
  ESP_LOGI(TAG, "Received MQTT config data:\n%s", content_str);
  free(content_str);

  // Find message settings
  const char* SETTINGS = "settings";
  JsonResult<cJSON*> settings = parser->GetObject(content, "content", SETTINGS);
  if (settings.is_error)
    return MqttResponse(MQTT_BAD_REQUEST, settings.message);

  // Apply settings
  std::string error = ApplySettings(*parser, settings.value, true);
  if (error.size() > 0)
    return MqttResponse(MQTT_BAD_REQUEST, error);

  return MqttResponse(MQTT_OK);
}

// Handles MQTT data received from status topic, to get status
MqttResponse IndySwitch::HandleStatusMessage(const cJSON* message_content, JsonParser* parser) {
  parser->SetTag(TAG);

  // Log message content
  char *message_content_str = cJSON_Print(message_content);
  ESP_LOGI(TAG, "Received MQTT get status:\n%s", message_content_str);
  free(message_content_str);

  // Create status JSON
  cJSON *status_json = cJSON_CreateObject();
  cJSON_AddStringToObject(status_json, "device", HOSTNAME);
  cJSON_AddStringToObject(status_json, "firmware",
    FormatString("indy_switch_%d.%d.%d_esp32.bin", VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH).c_str());
  cJSON_AddStringToObject(status_json, "date", IndyTime::FormatCurrentTime().c_str());
  cJSON_AddBoolToObject(status_json, "is_on", is_on);
  if (scheduler.IsActive()) {
    SunTimes sun_times = scheduler.GetCurrentSunTimes();
    cJSON_AddStringToObject(status_json, "sunrise",
      IndyTime::FormatTime(sun_times.sunrise).c_str());
    cJSON_AddStringToObject(status_json, "sunset",
      IndyTime::FormatTime(sun_times.sunset).c_str());
    cJSON_AddNumberToObject(status_json, "offset", scheduler.GetRandomOffsetRange());
    cJSON_AddStringToObject(status_json, "next_action",
      IndyScheduler::NextActionAsStr(scheduler.GetNextAction()));
    cJSON_AddStringToObject(status_json, "next_action_time",
      IndyTime::FormatTime(scheduler.GetNextActionTime()).c_str());
  }
  cJSON_AddItemReferenceToObject(status_json, "suntimes", scheduler.GetSuntimesJson());

  // Create status JSON string
  char *status_json_str = cJSON_Print(status_json);
  std::string status(status_json_str);

  // Clean up
  cJSON_Delete(status_json);
  free(status_json_str);

  // Create response
  MqttResponse response = MqttResponse(MQTT_OK);
  response.SetContent(status);

  return response;
}

// Handles MQTT data received from restart topic, to restart device
MqttResponse IndySwitch::HandleRestartMessage(const cJSON* content, JsonParser* parser) {
  parser->SetTag(TAG);

  // Log message content
  char *contentStr = cJSON_Print(content);
  ESP_LOGI(TAG, "Received MQTT restart:\n%s", contentStr);
  free(contentStr);

  // Get message reset parameter
  JsonResult<bool> reset = parser->GetBool(content, "header", "reset");
  if (reset.is_error)
    return MqttResponse(MQTT_BAD_REQUEST, reset.message);

  // End tasks
  IndyTaskManager::GetInstance().Exit();

  // Reset
  if (reset.value)
    nvs.Reset();

  // Restart
  ESP_LOGI(TAG, "Restarting");
  esp_restart();
}

// Continues with device setup that needs current time
void IndySwitch::HandleTimeSynced() {
  ESP_LOGI(TAG, "Time has synced");

  // Print current time
  std::string time = IndyTime::FormatCurrentTime();
  ESP_LOGI(TAG, "The current time is: %s", time.c_str());

  // Configure scheduler
  scheduler.Setup(&nvs);
}

// Loads initial configuration from flash
void IndySwitch::LoadInitialConfig() {
  ESP_LOGI(TAG, "Loading initial configuration");

  // Parse the embedded configuration file
  JsonParser parser((const char*) initial_config_start, TAG, FormatString("JSON parsing failed for initial config"));
  std::string error = parser.Parse();
  if (error.size() > 0) {
    ESP_LOGE(TAG, "failed to parse initial configuration: %s", error.c_str());
    return;
  }

  // Apply the initial configuration
  error = ApplySettings(parser, parser.GetRoot(), false);
  if (error.size() > 0) {
    ESP_LOGE(TAG, "failed to load initial configuration: %s", error.c_str());
  }
}

// Applies the configuration settings found in the JSON `settings`, which was
// parsed using `parser`. The settings are saved to NVS if `save` is true.
// Returns an error message if there was an error.
std::string IndySwitch::ApplySettings(const JsonParser &parser, cJSON *settings, bool save) {
  // Are there any settings?
  std::vector<std::string> keys = parser.LookupKeys(settings);
  if (keys.size() == 0)
    return "No settings found";

  // Process each setting
  const char* SETTINGS = "settings";
  const char* TIMEZONE = "timezone";
  const char* RANDOM_OFFSET_RANGE = "offset";
  const char* SUNTIMES = "suntimes";
  for (const std::string& key : keys) {
    if (key == TIMEZONE) {
      // Get timezone setting
      JsonResult<std::string> timezone = parser.GetString(settings, SETTINGS, TIMEZONE);
      if (timezone.is_error) {
        return timezone.message;
      }

      // Set timezone
      SetTimezone(timezone.value);

      // Save setting
      if (save) {
        nvs.WriteString(NVS_KEY_CONFIG_TIMEZONE, timezone.value.c_str());
        nvs.Commit();
      }
    } else if (key == RANDOM_OFFSET_RANGE) {
      // Get random offset setting
      JsonResult<int> offset = parser.GetInt(settings, SETTINGS, RANDOM_OFFSET_RANGE);
      if (offset.is_error) {
        return offset.message;
      } else if (offset.value <= 0) {
        return "Random offset range needs to be a greater than 0";
      }

      // Set offset
      SetOffset(offset.value);

      // Save setting
      if (save) {
        nvs.WriteInt(NVS_KEY_CONFIG_RANDOM_OFFSET_RANGE, offset.value);
        nvs.Commit();
      }
    } else if (key == SUNTIMES) {
      // Get suntimes
      JsonResult<cJSON*> suntimes = parser.GetObject(settings, SETTINGS, SUNTIMES);
      if (suntimes.is_error)
        return suntimes.message;

      // Save suntimes
      std::string error = scheduler.SetSuntimes(parser, suntimes.value);
      if (error.size() > 0) {
        return error;
      }

      // Save setting
      if (save) {
        char* json_str = cJSON_Print(suntimes.value);
        nvs.WriteString(NVS_KEY_CONFIG_SUNTIMES, json_str);
        nvs.Commit();
        free(json_str);
      }
    } else {
      return FormatString("Unrecognized setting %s", key.c_str());
    }
  }

  return "";
}

// Sets timezone on the ESP32
void IndySwitch::SetTimezone(const std::string& timezone) {
  ESP_LOGI(TAG, "Setting timezone to %s", timezone.c_str());
  setenv("TZ", timezone.c_str(), 1);
  tzset();
}

// Sets random offset range on the scheduler
void IndySwitch::SetOffset(uint offset) {
  ESP_LOGI(TAG, "Setting random offset range to %d", offset);
  scheduler.SetRandomOffsetRange(offset);
}

// Sets suntimes on the scheduler
void IndySwitch::SetSuntimes(const JsonParser& parser, cJSON* suntimes) {
  scheduler.SetSuntimes(parser, suntimes);
}

// Loads and configuration values that were saved to NVS
void IndySwitch::LoadSavedConfig() {
  ESP_LOGI(TAG, "Loading saved configuration");

  // Apply saved timezone
  std::string timezone;
  if (nvs.ReadString(NVS_KEY_CONFIG_TIMEZONE, &timezone))
    SetTimezone(timezone);

  // Apply saved offset
  int32_t offset;
  if (nvs.ReadInt(NVS_KEY_CONFIG_RANDOM_OFFSET_RANGE, &offset))
    SetOffset((uint32_t) offset);

  // Apply saved suntimes
  std::string suntimes_json;
  if (nvs.ReadString(NVS_KEY_CONFIG_SUNTIMES, &suntimes_json)) {
    JsonParser parser(suntimes_json.c_str(), TAG, "JSON parsing failed for load saved config");
    std::string message = parser.Parse();
    if (message.length() > 0)
      ESP_LOGE(TAG, "Unexpected error parsing saved suntimes: %s", message.c_str());
    else
      SetSuntimes(parser, parser.GetRoot());
  }
}
