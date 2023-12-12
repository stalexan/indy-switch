#include "indy_mdns.h"

#include <esp_log.h>
#include <mdns.h>

#include "indy_config.h"
#include "indy_util.h"

namespace {
  const char *TAG = "indy_mdns";
}

// Sets up this IndyMdns
void IndyMdns::Setup() {
  // Initialize the mDNS service
  esp_err_t err = mdns_init();
  if (err) {
    ESP_LOGE(TAG, "MDNS initialization failed: %s", esp_err_to_name(err));
    return;
  }

  // Set the hostname
  mdns_hostname_set(HOSTNAME);

  ESP_LOGI(TAG, "Setup completed");
}
