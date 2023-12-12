#include "indy_task.h"

#include <esp_log.h>

#include "indy_task_manager.h"

namespace {
  const char *TAG = "indy_task";
}

// Creates an instance of IndyTask with the given task `name`
IndyTask::IndyTask(const char* const name) {
  this->name = name;

  // Register this task
  task_manager = &IndyTaskManager::GetInstance();
  task_manager->RegisterTask(this);
}

// Creates the FreeRTOS task for this IndyTask. Work will be done by `function`
// which will be passed `parameters`.
void IndyTask::CreateTask(TaskFunction_t function, void* const parameters) {
  ESP_LOGI(TAG, "Creating %s", name.c_str());

  task_function = function;
  task_parameters = parameters;

  // Start the task
  const uint32_t TASK_STACK_DEPTH = 8192;
  UBaseType_t PRIORITY = 1;
  BaseType_t err = xTaskCreate(TaskLoop, name.c_str(), TASK_STACK_DEPTH, this, PRIORITY, &handle);
  assert(err == pdPASS);
}

// Sends the task a notification that work is ready to do
void IndyTask::TaskNotifyGive() {
  xTaskNotifyGive(handle);
}

// Sends the task a notification from an ISR that work is ready to do
void IndyTask::TaskNotifyGiveFromISR() {
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  vTaskNotifyGiveFromISR(handle, &xHigherPriorityTaskWoken);
  portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

// Calls the task's function each time there's a notification that work is
// ready, and ends when the task manager says it's time to end
void IndyTask::TaskLoop(void *arg) {
  IndyTaskManager::GetInstance().IncrementRunningTaskCount();
  IndyTask* task = reinterpret_cast<IndyTask*>(arg);
  while (1) {
    // Wait for work
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    // Do work unless exiting
    if (IndyTaskManager::GetInstance().Exiting())
      break;
    task->task_function(task->task_parameters);
    if (IndyTaskManager::GetInstance().Exiting())
      break;
  }

  // End task
  IndyTaskManager::GetInstance().DecrementRunningTaskCount();
  vTaskDelete(nullptr);
}


