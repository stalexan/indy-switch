#ifndef COMPONENTS_INDY_SWITCH_INDY_SCHEDULER_H_
#define COMPONENTS_INDY_SWITCH_INDY_SCHEDULER_H_

#include <cJSON.h>
#include <freertos/FreeRTOS.h>
#include <freertos/timers.h>

#include <array>
#include <functional>
#include <string>
#include <vector>

#include "indy_json.h"
#include "indy_nvs.h"
#include "indy_task.h"
#include "indy_util.h"

// Holds the time of sunrise and sunset in number of seconds since midnight
struct SunTimeOffsets {
  int sunrise;
  std::string sunrise_str;

  int sunset;
  std::string sunset_str;

  SunTimeOffsets() : sunrise(-1), sunset(-1) {}
  SunTimeOffsets(int sr, const std::string& sr_str, int ss, const  std::string& ss_str) :
    sunrise(sr), sunrise_str(sr_str), sunset(ss), sunset_str(ss_str) {}

  bool IsSet() const { return sunrise != -1 && sunset != -1; }
};

// Holds time of sunrise and sunset as time_t values
struct SunTimes {
  time_t sunrise;
  time_t sunset;

  SunTimes() : sunrise(NULL_TIME), sunset(NULL_TIME) {}
  SunTimes(time_t sr, time_t ss) : sunrise(sr), sunset(ss) {}
};

// Identifies the next action to perform
enum class NextActionEnum {
  NOOP = 0,
  ON = 1,   // Turn switch on
  OFF = 2   // Turn switch off
};

// Manages the schedule for IndySwitch, to turn the switch on at sunset and off at sunrise
class IndyScheduler {
 public:
  IndyScheduler(): task("SchedulerTask") {}
  ~IndyScheduler();

  bool IsActive() const { return nvs != nullptr; }

  // Configuring
  std::string SetSuntimes(const JsonParser& parser, cJSON* settings);
  void Setup(IndyNvs* nvs);

  // Sunrise and sunset times
  SunTimes GetCurrentSunTimes() { return current_sun_times; }
  std::array<SunTimeOffsets, 12> GetSunTimeOffsets() { return sun_time_offsets; }
  cJSON* GetSuntimesJson() { return suntimes_json; }  // Scheduler still owns memory after call

  // Next action: what to do and when
  static const char* NextActionAsStr(NextActionEnum next_action);
  const char* NextActionAsStr();
  NextActionEnum GetNextAction() { return next_action; }
  time_t GetNextActionTime() { return next_action_time; }

  // Next action handlers, to notify listeners that a next action should happen
  using NextActionHandler = std::function<void(bool)>;
  void RegisterNextActionHandler(const NextActionHandler& handler) { handlers.push_back(handler); }

  // Next action timer
  void HandleNextActionTimerExpiry();

  // Random offset range
  uint GetRandomOffsetRange() { return random_offset_range; }
  void SetRandomOffsetRange(uint range) { random_offset_range = range; }

 private:
  // Next action timer
  TimerHandle_t next_action_timer = nullptr;
  static void TimerCallback(TimerHandle_t handle);
  void StartTimer();

  // Scheduler task
  IndyTask task;
  static void TaskFunction(void *arg);

  // Storage
  IndyNvs* nvs = nullptr;

  // Sunrise and sunset times
  SunTimes current_sun_times;
  std::array<SunTimeOffsets, 12> sun_time_offsets;
  cJSON *suntimes_json = nullptr;
  SunTimes* DetermineSunTimes(SunTimes* suntimes);

  // Random offset range
  uint random_offset_range = 0;  // Minutes
  time_t RandomizeTime(time_t time);

  // Next action: what to do and when
  NextActionEnum next_action = NextActionEnum::NOOP;
  time_t next_action_time = NULL_TIME;

  // Next action handlers
  std::vector<NextActionHandler> handlers;
};

#endif  // COMPONENTS_INDY_SWITCH_INDY_SCHEDULER_H_
