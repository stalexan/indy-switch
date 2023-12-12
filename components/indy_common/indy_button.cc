#include "indy_button.h"

#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/portmacro.h>
#include <freertos/task.h>
#include <FreeRTOSConfig.h>

#include "indy_config.h"
#include "indy_util.h"

// Notify the button task that there's been a change on the GPIO pin for this button
void IndyButton::ButtonHandlerISR(void *arg) {
  ESP_ERROR_CHECK(gpio_intr_disable(BUTTON_GPIO));  // Ignore additional changes while handling this change
  IndyButton* button = reinterpret_cast<IndyButton*>(arg);
  button->task.TaskNotifyGiveFromISR();
}

// Returns the state of `pin` when the pin is stable.
int debounce(gpio_num_t pin) {
  TickType_t debounce_delay = pdMS_TO_TICKS(50);  // Debounce time in milliseconds

  // Initialize debounce
  int pin_state = gpio_get_level(pin);
  int last_pin_state = pin_state;
  TickType_t last_debounce_time = xTaskGetTickCount();

  while (1) {
    // What's the pin state now?
    pin_state = gpio_get_level(pin);

    // Did the state change?
    if (pin_state != last_pin_state) {
      // The state changed. Start waiting again
      last_debounce_time = xTaskGetTickCount();
      last_pin_state = pin_state;
    } else {
      // The state didn't change. Is it stable?
      if ((xTaskGetTickCount() - last_debounce_time) > debounce_delay) {
        return pin_state;
      }
    }

    // Wait for next check.
    vTaskDelay(pdMS_TO_TICKS(5));  // Wait time in milliseconds
  }
}

// Notifies button handlers if the button is pressed
void IndyButton::TaskFunction(void *arg) {
  bool is_pressed = debounce(BUTTON_GPIO) == 0;  // Read pin state. Low means pressed.
  if (is_pressed) {
    // Call button press handlers
    IndyButton* button = reinterpret_cast<IndyButton*>(arg);
    for (const ButtonPressHandler& handler : button->handlers) {
      handler();
    }
  }
  ESP_ERROR_CHECK(gpio_intr_enable(BUTTON_GPIO));  // Resume watching for pin changes
}

// Sets up the IndyButton
void IndyButton::Setup() {
  // Configure BUTTON_GPIO pin for input
  gpio_config_t io_conf;
  io_conf.intr_type = GPIO_INTR_ANYEDGE;       // Watch for button presses
  io_conf.mode = GPIO_MODE_INPUT;              // Input mode
  io_conf.pin_bit_mask = 1ULL << BUTTON_GPIO;  // GPIO pin mask
  io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
  io_conf.pull_up_en = GPIO_PULLUP_ENABLE;     // Enable internal pull-up resistor
  ESP_ERROR_CHECK(gpio_config(&io_conf));

  // Create the button task
  task.CreateTask(TaskFunction, this);

  // Install the interrupt handler that watches for button presses
  ESP_ERROR_CHECK(gpio_install_isr_service(0));
  ESP_ERROR_CHECK(gpio_isr_handler_add(BUTTON_GPIO, ButtonHandlerISR, this));
}
