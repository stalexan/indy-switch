#ifndef COMPONENTS_INDY_SWITCH_INDY_SWITCH_H_
#define COMPONENTS_INDY_SWITCH_INDY_SWITCH_H_

#include <cJSON.h>
#include <driver/gpio.h>

#include <string>

#include "indy_button.h"
#include "indy_config.h"
#include "indy_json.h"
#include "indy_mdns.h"
#include "indy_mqtt.h"
#include "indy_nvs.h"
#include "indy_output_pin.h"
#include "indy_scheduler.h"
#include "indy_time.h"
#include "indy_wifi.h"

// The top-level class for managing an IndySwitch
class IndySwitch {
 public:
  IndySwitch() : led(LED_GPIO), relay(RELAY_GPIO) {}

  void Setup();

  void SetSwitch(bool on);
  void ToggleSwitch();

 private:
  // ESP32
  IndyWifi wifi;
  IndyNvs nvs;
  IndyMdns mdns;
  IndyMqtt mqtt;
  IndyTime time;

  // Peripherals
  IndyButton button;
  IndyDigitalOutputPin led;
  IndyDigitalOutputPin relay;

  // Scheduler
  IndyScheduler scheduler;

  // "Is on" state
  bool is_on = false;
  SemaphoreHandle_t is_on_mutex;

  // MQTT topics
  std::string control_topic;
  std::string config_topic;
  std::string status_topic;
  std::string restart_topic;
  std::string reset_topic;

  // Configure
  void SetTimezone(const std::string& timezone);
  void SetOffset(uint offset);
  void SetSuntimes(const JsonParser& parser, cJSON* suntimes);

  // Configure with JSON
  void LoadInitialConfig();
  std::string ApplySettings(const JsonParser& parser, cJSON* settings, bool save);

  // Configure with values saved to NVS
  void LoadSavedConfig();

  // MQTT event handlers
  void HandleMqttConnected();
  MqttResponse HandleControlMessage(const cJSON* content, JsonParser* parser);
  MqttResponse HandleConfigMessage(const cJSON* content, JsonParser* parser);
  MqttResponse HandleStatusMessage(const cJSON* content, JsonParser* parser);
  MqttResponse HandleRestartMessage(const cJSON* content, JsonParser* parser);

  // Other event handlers
  void HandleTimeSynced();
};

#endif  // COMPONENTS_INDY_SWITCH_INDY_SWITCH_H_
