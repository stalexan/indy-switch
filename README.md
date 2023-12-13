# indy-switch

ESP32 IoT device for turning a 120v switch off and on at sunrise and sunset,
written in C and C++ using the [ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/).

# Configuring Credentials

To configure user names and passwords for wifi and MQTT connections, create
a file called `indy_config_secrets.cc` in the directory
`components/indy_common/`, and add usernames and passwords using this template:

```
#include "indy_config.h"

const char* const WIFI_SSID = "foobar";
const char* const WIFI_PASSWORD = "changeme";

const char* const MQTT_USER = "foobar";
const char* const MQTT_PASSWORD = "changeme";
```

TODO: README
