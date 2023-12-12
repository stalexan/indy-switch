#include "indy_output_pin.h"

#include <driver/gpio.h>

// Sets up the GPIO for digital output
void IndyDigitalOutputPin::Setup() {
  // Configure pin for output.
  gpio_config_t io_conf;
  io_conf.intr_type = GPIO_INTR_DISABLE;         // Disable interrupt
  io_conf.mode = GPIO_MODE_OUTPUT;               // Set as output mode
  io_conf.pin_bit_mask = 1ULL << pin;            // Bitmask of the pin
  io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;  // Disable pull-down
  io_conf.pull_up_en = GPIO_PULLUP_DISABLE;      // Disable pull-up
  ESP_ERROR_CHECK(gpio_config(&io_conf));
}

// Sets the pin high
void IndyDigitalOutputPin::TurnOn() {
  ESP_ERROR_CHECK(gpio_set_level(pin, 1));
}

// Sets the pin low
void IndyDigitalOutputPin::TurnOff() {
  ESP_ERROR_CHECK(gpio_set_level(pin, 0));
}
