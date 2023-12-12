#ifndef COMPONENTS_INDY_COMMON_INDY_TASK_H_
#define COMPONENTS_INDY_COMMON_INDY_TASK_H_

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <string>

class IndyTaskManager;

// Represents a task for registration with the IndyTaskManager
class IndyTask {
 public:
  explicit IndyTask(const char* const name);

  std::string GetName() { return name; }

  void CreateTask(TaskFunction_t function, void* const parameters);

  void TaskNotifyGive();
  void TaskNotifyGiveFromISR();

  bool IsRunning() { return handle != nullptr; }

 private:
  IndyTaskManager* task_manager;

  std::string name;

  TaskHandle_t handle = nullptr;

  TaskFunction_t task_function = nullptr;
  void* task_parameters = nullptr;

  static void TaskLoop(void *arg);
};

#endif  //  COMPONENTS_INDY_COMMON_INDY_TASK_H_
