#include "indy_wifi.h"

#include <string.h>

#include <esp_log.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>

#include "indy_config.h"
#include "indy_util.h"

// Code is based on example code from
// [Wi-Fi Station Example](https://github.com/espressif/esp-idf/tree/release/v5.1/examples/wifi/getting_started/station)

#pragma GCC diagnostic ignored "-Wmissing-field-initializers"

namespace {
  int connect_retry_num = 0;

  // FreeRTOS event group to signal when we are connected
  EventGroupHandle_t wifi_event_group;
  const EventBits_t WIFI_CONNECTED_BIT = BIT0;  // We are connected to the AP with an IP
  const EventBits_t WIFI_FAIL_BIT      = BIT1;  // We failed to connect after the maximum amount of retries

  const char *TAG = "indy_wifi";
}  // namespace

// Wifi settings
#define WIFI_MAXIMUM_RETRY  5
#define WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA2_PSK

// Handles wifi events
static void WifiEventHandler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
  if (event_base ==  WIFI_EVENT) {
    switch (event_id) {
      case WIFI_EVENT_STA_START:
        ESP_LOGI(TAG, "Connecting to wifi");
        esp_wifi_connect();
        break;
      case WIFI_EVENT_STA_DISCONNECTED:
        ESP_LOGI(TAG, "Wifi disconnected");
        if (connect_retry_num < WIFI_MAXIMUM_RETRY) {
          ESP_LOGI(TAG, "Attempting to reconnect to wifi");
          connect_retry_num++;
          esp_wifi_connect();
        } else {
          xEventGroupSetBits(wifi_event_group, WIFI_FAIL_BIT);
        }
        break;
    }
  } else if (event_base == IP_EVENT) {
    switch (event_id) {
      case IP_EVENT_STA_GOT_IP:
        ip_event_got_ip_t* event =  reinterpret_cast<ip_event_got_ip_t*>(event_data);
        ESP_LOGI(TAG, "Wifi connected and got IP " IPSTR, IP2STR(&event->ip_info.ip));
        connect_retry_num = 0;
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
        break;
    }
  }
}

// Copies string from `source` to `dest` of size `dest_size`.
static void CopyString(uint8_t *dest, uint dest_size, const char *source) {
  strncpy(reinterpret_cast<char *>(dest), source, dest_size - 1);
  dest[dest_size - 1] = '\0';  // Make sure dest is null-terminated
}

// Sets up wifi
void IndyWifi::Setup() {
  // Initialize the TCP/IP stack
  ESP_ERROR_CHECK(esp_netif_init());

  // Initialize event handling
  wifi_event_group = xEventGroupCreate();
  ESP_ERROR_CHECK(esp_event_loop_create_default());
  esp_event_handler_instance_t instance_any_id;
  esp_event_handler_instance_t instance_got_ip;
  ESP_ERROR_CHECK(esp_event_handler_instance_register(
    WIFI_EVENT, ESP_EVENT_ANY_ID, &WifiEventHandler, NULL, &instance_any_id));
  ESP_ERROR_CHECK(esp_event_handler_instance_register(
    IP_EVENT, IP_EVENT_STA_GOT_IP, &WifiEventHandler, NULL, &instance_got_ip));

  // Create the default STA
  esp_netif_create_default_wifi_sta();

  // Initialize wifi and start the wifi task
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));

  // Configure STA
  wifi_config_t wifi_config = {
    .sta = {
      .threshold = {
        .authmode = WIFI_SCAN_AUTH_MODE_THRESHOLD,
      },
    },
  };
  CopyString(wifi_config.sta.ssid, sizeof(wifi_config.sta.ssid), WIFI_SSID);
  CopyString(wifi_config.sta.password, sizeof(wifi_config.sta.password), WIFI_PASSWORD);
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));

  // Start wifi
  ESP_ERROR_CHECK(esp_wifi_start());

  // Wait until either the connection is established or the maximum number of
  // attempts tried
  EventBits_t bits = xEventGroupWaitBits(
    wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT, pdFALSE, pdFALSE, portMAX_DELAY);
  if (bits & WIFI_CONNECTED_BIT) {
    ESP_LOGI(TAG, "connected to ap SSID:%s", wifi_config.sta.ssid);
  } else if (bits & WIFI_FAIL_BIT) {
    ESP_LOGI(TAG, "Failed to connect to SSID:%s", wifi_config.sta.ssid);
  } else {
    ESP_LOGE(TAG, "Unexpected event");
  }

  ESP_LOGI(TAG, "Setup completed");
}

