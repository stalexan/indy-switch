#ifndef COMPONENTS_INDY_COMMON_INDY_MQTT_H_
#define COMPONENTS_INDY_COMMON_INDY_MQTT_H_

#include <cJSON.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <mqtt_client.h>

#include <functional>
#include <string>
#include <map>
#include <memory>
#include <vector>

#include "indy_json.h"
#include "indy_util.h"
#include "indy_task.h"

// Status codes returned with response
enum MqttStatusCode {
  MQTT_NULL = 0,
  MQTT_OK = 200,
  MQTT_BAD_REQUEST = 400,
  MQTT_SERVER_ERROR = 500,
};

// Response returned to publisher
class MqttResponse {
 public:
  MqttResponse() {}
  explicit MqttResponse(MqttStatusCode status_code, const std::string &message = "") :
    status_code(status_code), message(message) {}

  std::string GetId() { return id; }
  void SetId(const std::string& id) { this->id = id; }

  std::string GetMessage() { return message; }

  void SetContent(const std::string& content) { this->content = content; }

  bool GetSent() { return sent; }
  void SetSent(bool sent) { this->sent = sent; }

  std::string Marshal();

  bool IsOk() { return status_code == MQTT_OK; }

  std::string CreateErrorMessage(const std::string& prefix);

 private:
  std::string id = "";
  MqttStatusCode status_code = MQTT_NULL;
  std::string message = "";
  std::string content = "";

  bool sent = false;

  void AddContentToJson(cJSON *json);
};

// Manages the ESP32 MQTT service
class IndyMqtt {
 public:
  void Setup();

  // MQTT event handlers
  void HandleMqttConnected();
  void HandleMqttData(const std::string& topic, const std::string& data);

  // Connected handlers
  using ConnectedHandler = std::function<void()>;
  void RegisterConnectedHandler(const ConnectedHandler& handler) { connectedHandlers.push_back(handler); }

  // Subscriptions
  using DataHandler = std::function<MqttResponse(const cJSON*, JsonParser* parser)>;
  void SubscribeToTopic(const char* topic, const DataHandler& handler);

 private:
  // MQTT client handle
  esp_mqtt_client_handle_t client;

  // Handlers for MQTT_EVENT_DATA events
  std::map<std::string, std::vector<DataHandler>> dataHandlers;

  // Handlers for the connected event
  std::vector<ConnectedHandler> connectedHandlers;

  // Publish task
  IndyTask publish_task = IndyTask("PublishTask");
  static void PublishTaskFunction(void *arg);

  // Responses to send back to publisher
  SemaphoreHandle_t responses_mutex;
  std::vector<MqttResponse> responses;
  MqttResponse GenerateMqttResponse(const std::string& topic, const std::string& data);
  void PublishResponses();
};

#endif  // COMPONENTS_INDY_COMMON_INDY_MQTT_H_
