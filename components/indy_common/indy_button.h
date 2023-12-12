#ifndef COMPONENTS_INDY_COMMON_INDY_BUTTON_H_
#define COMPONENTS_INDY_COMMON_INDY_BUTTON_H_

#include <functional>
#include <vector>

#include "indy_task.h"

// Manages a button attached to a GPIO pin
class IndyButton {
 public:
  void Setup();

  // Button press handlers
  using ButtonPressHandler = std::function<void()>;
  void RegisterButtonPressHandler(const ButtonPressHandler& handler) { handlers.push_back(handler); }

 private:
  // ISR
  static void ButtonHandlerISR(void *arg);

  // Task
  IndyTask task = IndyTask("ButtonTask");
  static void TaskFunction(void *arg);

  // Button press handlers
  std::vector<ButtonPressHandler> handlers;
};

#endif  //  COMPONENTS_INDY_COMMON_INDY_BUTTON_H_
