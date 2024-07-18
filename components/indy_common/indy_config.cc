#include "indy_config.h"

#include <FreeRTOSConfig.h>
#include <driver/gpio.h>

//#define CONFIG_DEVKITC
#define CONFIG_LILYGO_T7

//const char* const HOSTNAME = "esp-vorona";
const char* const HOSTNAME = "esp-hollanda";

#ifdef CONFIG_DEVKITC
const gpio_num_t BUTTON_GPIO = GPIO_NUM_16;
const gpio_num_t LED_GPIO = GPIO_NUM_17;
const gpio_num_t RELAY_GPIO = GPIO_NUM_21;
#elif defined(CONFIG_LILYGO_T7)
const gpio_num_t BUTTON_GPIO = GPIO_NUM_25;
const gpio_num_t LED_GPIO = GPIO_NUM_27;
const gpio_num_t RELAY_GPIO = GPIO_NUM_21;
#else
#error "No device configured"
#endif

const char* const MQTT_BROKER = "mqtts://elias.alexan.org:8883";  // The MQTT broker address

const bool USE_SNTP = true;  // Whether to use the SNTP service
const char* const SNTP_TIME_SERVER = "pool.ntp.org";  // Which SNTP server to sync with

// The maximum time to wait for operations that should complete quickly
const int MAX_WAIT_SECONDS = 10;
const int MAX_WAIT = (MAX_WAIT_SECONDS * configTICK_RATE_HZ);  // ticks

