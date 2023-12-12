#ifndef COMPONENTS_INDY_COMMON_INDY_CONFIG_H_
#define COMPONENTS_INDY_COMMON_INDY_CONFIG_H_

#include <driver/gpio.h>

#define VERSION_MAJOR 1
#define VERSION_MINOR 2
#define VERSION_PATCH 1

extern const char* const WIFI_SSID;
extern const char* const WIFI_PASSWORD;

extern const char* const HOSTNAME;

extern const gpio_num_t BUTTON_GPIO;
extern const gpio_num_t LED_GPIO;
extern const gpio_num_t RELAY_GPIO;

extern const char* const MQTT_BROKER;

extern const char* const MQTT_USER;
extern const char* const MQTT_PASSWORD;

extern const bool USE_SNTP;
extern const char* const SNTP_TIME_SERVER;

extern const int MAX_WAIT;

#endif  //  COMPONENTS_INDY_COMMON_INDY_CONFIG_H_
