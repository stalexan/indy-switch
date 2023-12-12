#ifndef COMPONENTS_INDY_COMMON_INDY_TASK_MANAGER_H_
#define COMPONENTS_INDY_COMMON_INDY_TASK_MANAGER_H_

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <stdatomic.h>

#include <vector>

class IndyTask;

// Manages tasks so that they're shut down cleanly
class IndyTaskManager {
 public:
  static IndyTaskManager& GetInstance() {
    static IndyTaskManager instance;
    return instance;
  }

  void RegisterTask(IndyTask* task);
  void IncrementRunningTaskCount();
  void DecrementRunningTaskCount();

  void Exit();
  bool Exiting() { return atomic_load(&exiting); }

 private:
    IndyTaskManager();

    // Tasks
    std::vector<IndyTask*> tasks;
    SemaphoreHandle_t running_tasks_count;

    // Mutex for locking task manager
    SemaphoreHandle_t mutex;
    bool Lock();
    bool Unlock();

    // Exiting flag
    atomic_bool exiting = ATOMIC_VAR_INIT(false);

    // Prevent copy and assignment since IndyTaskManager is a singleton.
    IndyTaskManager(const IndyTaskManager&) = delete;
    IndyTaskManager& operator=(const IndyTaskManager&) = delete;
};

#endif  //  COMPONENTS_INDY_COMMON_INDY_TASK_MANAGER_H_
