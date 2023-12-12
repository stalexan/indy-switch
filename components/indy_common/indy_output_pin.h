#ifndef COMPONENTS_INDY_COMMON_INDY_OUTPUT_PIN_H_
#define COMPONENTS_INDY_COMMON_INDY_OUTPUT_PIN_H_

#include <driver/gpio.h>

// Manages an ESP32 GPIO used for digital output
class IndyDigitalOutputPin {
 private:
  gpio_num_t pin;

 public:
  explicit IndyDigitalOutputPin(gpio_num_t pin) { this->pin = pin; }

  void Setup();

  void TurnOn();
  void TurnOff();
};

#endif  // COMPONENTS_INDY_COMMON_INDY_OUTPUT_PIN_H_
