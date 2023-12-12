#include "indy_task_manager.h"

#include <esp_log.h>

#include "freertos/portmacro.h"
#include "indy_config.h"
#include "indy_task.h"

namespace {
  const char *TAG = "indy_task_manager";
}

// Sets up this IndyTaskManager
IndyTaskManager::IndyTaskManager() {
  // Create mutex
  mutex = xSemaphoreCreateMutex();
  if (mutex == nullptr) {
    ESP_LOGE(TAG, "Create task manager mutex failed");
    abort();
  }

  // Create running task count
  const UBaseType_t MAX_TASKS = 10;
  running_tasks_count = xSemaphoreCreateCounting(MAX_TASKS, 0);
  if (running_tasks_count == nullptr) {
    ESP_LOGE(TAG, "Create running tasks count semaphore failed");
    abort();
  }
}

// Tells tasks to end and then waits for tasks to end
void IndyTaskManager::Exit() {
  // Tell tasks to end
  ESP_LOGI(TAG, "Ending tasks");
  atomic_store(&exiting, true);
  Lock();
  for (auto& task : tasks)
    task->TaskNotifyGive();
  Unlock();

  // Wait for tasks to end
  TickType_t wait_ticks = pdMS_TO_TICKS(100);
  for (TickType_t tick_count = 0;
      tick_count < MAX_WAIT && uxSemaphoreGetCount(running_tasks_count) > 0;
      tick_count += wait_ticks) {
    vTaskDelay(wait_ticks);
  }
  if (uxSemaphoreGetCount(running_tasks_count) == 0)
    ESP_LOGI(TAG, "Tasks ended");
  else
    ESP_LOGE(TAG, "Timed out waiting for tasks to end");
}

// Registers `task` with the task manager
void IndyTaskManager::RegisterTask(IndyTask* task) {
  ESP_LOGI(TAG, "Registering %s", task->GetName().c_str());

  // Add task to tasks
  if (!Lock()) {
    ESP_LOGE(TAG, "Failed to acquire lock to register %s", task->GetName().c_str());
    abort();
  }
  tasks.push_back(task);
  if (!Unlock()) {
    ESP_LOGE(TAG, "Failed to release lock after registering %s", task->GetName().c_str());
  }
}

bool IndyTaskManager::Lock() {
  // Acquire mutex
  return xSemaphoreTake(mutex, MAX_WAIT) == pdTRUE;
}

bool IndyTaskManager::Unlock() {
  // Release mutex
  return xSemaphoreGive(mutex) == pdTRUE;
}

void IndyTaskManager::IncrementRunningTaskCount() {
  if (xSemaphoreGive(running_tasks_count) != pdTRUE)
    ESP_LOGE(TAG, "Failed to increment running task count");
}

void IndyTaskManager::DecrementRunningTaskCount() {
  if (xSemaphoreTake(running_tasks_count, MAX_WAIT) != pdTRUE)
    ESP_LOGE(TAG, "Failed to decrement running task count");
}
