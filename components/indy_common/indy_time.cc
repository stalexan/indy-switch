#include "indy_time.h"

#include <string.h>

#include <esp_log.h>
#include <esp_sntp.h>
#include <esp_netif_sntp.h>

#include "indy_config.h"

namespace {
  const char *TAG = "indy_time";
  IndyTime *indy_time;  // This is a global since esp_sntp_time_cb_t doesn't have callback data.
}

// SNTP code is based on example code from
// [Example: using LwIP SNTP module and time functions](https://github.com/espressif/esp-idf/tree/release/v5.1/examples/protocols/sntp)

// Notifies IndyTime that the system time has synced with SNTP
void time_sync_notification_cb(struct timeval *tv) {
  ESP_LOGI(TAG, "SNTP time has synced");
  indy_time->HandleSntpTimeSync();
}

// Notifies listeners that the system time has synced with SNTP, and stops the SNTP service
void IndyTime::HandleSntpTimeSync() {
  // Notify handlers that time has been synced
  for (const TimeSyncedHandler& handler : handlers) {
    handler();
  }

  // Stop SNTP
  ESP_LOGI(TAG, "Stopping SNTP");
  esp_netif_sntp_deinit();
}

// Sets up this IndyTime. The `handler` will called when time has been synced with the SNTP server.
void IndyTime::Setup(const TimeSyncedHandler& handler) {
  indy_time = this;

  // Register handler
  RegisterTimeInitializedHandler(handler);

  // Start the SNTP service
  if (USE_SNTP) {
    ESP_LOGI(TAG, "Starting SNTP");
    esp_sntp_config_t config = {
      .smooth_sync = false,
      .server_from_dhcp = false,
      .wait_for_sync = true,
      .start = true,
      .sync_cb = time_sync_notification_cb,
      .renew_servers_after_new_IP = false,
      .ip_event_to_renew = (ip_event_t) 0,
      .index_of_first_server = 0,
      .num_of_servers = 1,
      .servers = { SNTP_TIME_SERVER },
    };
    esp_netif_sntp_init(&config);
  }

  ESP_LOGI(TAG, "Setup completed");
}

// Returns `time` formatted as a string, using the format "Wed Sep 27 19:18:11 2023 CST"
std::string IndyTime::FormatTime(time_t time) {
  struct tm timeinfo;
  localtime_r(&time, &timeinfo);
  char strftime_buf[64];
  strftime(strftime_buf, sizeof(strftime_buf), "%c %Z", &timeinfo);
  return std::string(strftime_buf);
}

// Returns the current formatted as a string, using the format "Wed Sep 27 19:18:11 2023 CST"
std::string IndyTime::FormatCurrentTime() {
  time_t now;
  ::time(&now);
  return FormatTime(now);
}
