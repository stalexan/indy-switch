#include <esp_log.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "freertos/portmacro.h"
#include "indy_switch.h"

extern "C" void app_main() {
  // Create and initialize the switch
  IndySwitch* indy_switch = new IndySwitch();
  indy_switch->Setup();

  // NO-OP and don't return
  while (1) {
    TickType_t ticks = configTICK_RATE_HZ;  // 1 second
    vTaskDelay(ticks);
  }
}

