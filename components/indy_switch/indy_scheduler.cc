#include "indy_scheduler.h"

#include <stdlib.h>

#include <esp_log.h>
#include <esp_random.h>
#include <esp_system.h>
#include <FreeRTOSConfig.h>

#include <algorithm>
#include <memory>
#include <sstream>
#include <string>

#include "indy_config.h"
#include "indy_json.h"
#include "indy_time.h"
#include "indy_util.h"

namespace {
  const int SECONDS_PER_MINUTE = 60;
  const int SECONDS_PER_HOUR = 60 * 60;
  const int SECONDS_PER_DAY = 24 * 60 * 60;

  const char *TAG = "indy_scheduler";
}

// NVS keys
#define NVS_KEY_NEXT_ACTION      "nxtact"
#define NVS_KEY_NEXT_ACTION_TIME "nxtact_time"

// Returns `next_action` as a string
const char* IndyScheduler::NextActionAsStr(NextActionEnum next_action) {
  switch (next_action) {
    case NextActionEnum::NOOP:
      return "NOOP";
    case NextActionEnum::ON:
      return "ON";
    case NextActionEnum::OFF:
      return "OFF";
    default:
      return "INVALID";
  }
}

// Returns current `next_action` as a string
const char* IndyScheduler::NextActionAsStr() {
  return NextActionAsStr(next_action);
}

// Returns the total number of seconds in the specified `hours`, `minutes`, and `seconds`
int ComputeSeconds(int hours, int minutes, int seconds) {
  return (hours * SECONDS_PER_HOUR) + (minutes * SECONDS_PER_MINUTE) + seconds;
}

// Computes the number of `hours` and `minutes` in the specified `seconds`, dropping any remainder
void ComputeHoursAndMinutes(int seconds, int* hours, int* minutes) {
  *hours = seconds / SECONDS_PER_HOUR;
  int remaining_seconds = seconds % SECONDS_PER_HOUR;
  *minutes = remaining_seconds / SECONDS_PER_MINUTE;
}

// Parses `time_str` to compute seconds since midnight, stored to `result`.
// The `time_str` is time of day expressed as "HH:MM AM/PM"; e.g. "6:23 AM".
// Returns an error message if there was an error.
std::string ParseSunTime(const std::string& time_str, int* result) {
  // Parse the string
  std::stringstream ss(time_str.c_str());
  int hours, minutes;
  std::string period;
  char colon;
  if (!(ss >> hours >> colon >> minutes >> period) ||
    colon != ':' ||
    (period != "AM" && period != "PM") ||
    hours < 1 || hours > 12 ||
    minutes < 0 || minutes > 59) {
    return FormatString("Unable to parse sun time '%s'", time_str.c_str());
  }

  // Compute number of seconds since midnight
  if (period == "PM")
    hours += 12;
  *result = ComputeSeconds(hours, minutes, 0);

  return "";
}

// Parses the JSON `suntimes` that was parsed using `parser`, and stores results to sun_time_offsets
std::string IndyScheduler::SetSuntimes(const JsonParser& parser, cJSON* suntimes) {
  // Lookup month keys
  std::vector<std::string> month_keys = parser.LookupKeys(suntimes);

  // Parse each month
  std::array<SunTimeOffsets, 12> new_offsets;
  for (const std::string& month_key : month_keys) {
    // Which month is this?
    const char* str = month_key.c_str();
    char* str_end = nullptr;
    int64_t month = strtol(str, &str_end, 10);
    if (str_end == str || month < 1 || month > 12) {
      return FormatString("Month key '%s' is not an integer between 1 and 12", str);
    }

    // Get times for this month
    JsonResult<std::vector<std::string>> month_times = parser.GetStringArray(
      suntimes, "suntimes", month_key.c_str());
    if (month_times.is_error)
      return month_times.message;
    if (month_times.value.size() != 2)
      return FormatString2("suntimes", "expecting 2 times for month '%s' but found '%d'",
        month_key.c_str(), month_times.value.size());

    // Parse the times for this month
    int sunrise_offset, sunset_offset;
    std::string sunrise_str = month_times.value[0];
    std::string error = ParseSunTime(sunrise_str, &sunrise_offset);
    if (error.size() > 0)
      return error;
    std::string sunset_str = month_times.value[1];
    error = ParseSunTime(sunset_str, &sunset_offset);
    if (error.size() > 0)
      return error;
    if (sunrise_offset >= sunset_offset)
      return FormatString("Sunrise %s needs to be before sunset %s for month %s",
        sunrise_str.c_str(), sunset_str.c_str(), month_key.c_str());

    // Save times for this month
    int month_index = month - 1;
    new_offsets[month_index] = SunTimeOffsets(sunrise_offset, sunrise_str, sunset_offset, sunset_str);
  }

  // Check that all months are set
  for (size_t ii = 0; ii < new_offsets.size(); ii++) {
      if (!new_offsets[ii].IsSet())
        return FormatString("Suntimes for month %d are not set", ii + 1);
  }

  // Save new times
  sun_time_offsets = new_offsets;

  // Save copy of JSON version of suntimes
  if (suntimes_json != nullptr)
    cJSON_Delete(suntimes_json);
  suntimes_json = JsonParser::CloneJSON(suntimes);

  return "";
}

IndyScheduler::~IndyScheduler() {
  // Clean up timer
  if (next_action_timer != nullptr) {
    if (xTimerDelete(next_action_timer, 0) == pdFAIL) {
      ESP_LOGE(TAG, "Unable to delete timer");
    }
    next_action_timer = nullptr;
  }

  // Clean up JSON
  if (suntimes_json != nullptr) {
    cJSON_Delete(suntimes_json);
    suntimes_json  = nullptr;
  }
}

// Returns sun time as a time_t given:
// * `now_tm`: the current time,
// * `current_offset`: the current time as number of seconds since midnight, and
// * `sun_time_offset`: the number of seconds from midnight for the desired sun time.
time_t DetermineSunTime(const struct tm& now_tm, int current_offset, int sun_time_offset) {
  // When is the sun time for today?
  struct tm suntime_tm = now_tm;
  int hours, minutes;
  ComputeHoursAndMinutes(sun_time_offset, &hours, &minutes);
  suntime_tm.tm_hour = hours;
  suntime_tm.tm_min = minutes;
  suntime_tm.tm_sec = 0;
  time_t suntime = mktime(&suntime_tm);

  // Add one day if the next sun time is tomorrow
  if (current_offset > sun_time_offset) {
    suntime += SECONDS_PER_DAY;
  }

  return suntime;
}

// Populates `suntimes` with next sunrise and sunset. Returns nullptr if there
// was an error, or otherwise `suntimes`.
SunTimes* IndyScheduler::DetermineSunTimes(SunTimes* suntimes) {
  // What time is it?
  time_t now = time(nullptr);
  struct tm now_tm;
  if (localtime_r(&now, &now_tm) == nullptr) {
    ESP_LOGE(TAG, "Unable to convert time_t %lld to local time", now);
    return nullptr;
  }

  // Lookup SunTimeOffsets for the current month
  const SunTimeOffsets& offsets = sun_time_offsets[now_tm.tm_mon];

  // How many seconds have passed since midnight?
  int current_offset = ComputeSeconds(now_tm.tm_hour, now_tm.tm_min, now_tm.tm_sec);

  // Compute next sunrise and sunset.
  suntimes->sunrise = DetermineSunTime(now_tm, current_offset, offsets.sunrise);
  ESP_LOGI(TAG, "Next sunrise: %s", IndyTime::FormatTime(suntimes->sunrise).c_str());
  suntimes->sunset = DetermineSunTime(now_tm, current_offset, offsets.sunset);
  ESP_LOGI(TAG, "Next sunset: %s", IndyTime::FormatTime(suntimes->sunset).c_str());

  return suntimes;
}

// Sets up IndyScheduer. System time and timezone must have been set first.
void IndyScheduler::Setup(IndyNvs* nvs) {
  // Restore state
  this->nvs = nvs;
  next_action = (NextActionEnum) nvs->ReadInt(NVS_KEY_NEXT_ACTION);
  ESP_LOGI(TAG, "Restored next action is %s", NextActionAsStr());
  next_action_time = nvs->ReadTime(NVS_KEY_NEXT_ACTION_TIME);
  ESP_LOGI(TAG, "Restored next action time is %s", IndyTime::FormatTime(next_action_time).c_str());

  // Lookup sun times
  if (DetermineSunTimes(&current_sun_times) == nullptr) {
      ESP_LOGE(TAG, "Unable to determine sun times");
      return;
  }

  // Initialize next action
  time_t now = time(nullptr);
  if (next_action == NextActionEnum::NOOP || now > next_action_time) {
    // There is no next action or the next next action has expired, either
    // because this is a new switch or the switch was off when the next action
    // would have happened. Schedule the next action for now, based on whether
    // it's day or night.
    next_action_time = now;
    ESP_LOGI(TAG, "Next action time has expired. Scheduling next action for now.");

    // Is it day or night?
    bool is_night = current_sun_times.sunrise < current_sun_times.sunset;
    ESP_LOGI(TAG, "The current time is %s and it's %s",
      IndyTime::FormatTime(now).c_str(), is_night ? "nighttime" : "daytime");

    // Determine next action
    next_action = is_night ? NextActionEnum::ON : NextActionEnum::OFF;
    ESP_LOGI(TAG, "Next action is %s", NextActionAsStr());
  }

  // Create task
  task.CreateTask(TaskFunction, this);

  // Start timer
  StartTimer();
}

// Starts the next action timer, which will expire when it's time for the next action
void IndyScheduler::StartTimer() {
  // In how many seconds should the timer expire? Use 1 as minimum since
  // xTimerCreate ticks parameter must not be 0.
  time_t now = time(nullptr);
  time_t seconds_until_target = std::max(next_action_time - now, (time_t) 1);

  // Create or change timer
  TickType_t ticks = seconds_until_target * configTICK_RATE_HZ;
  if (next_action_timer == nullptr) {
    // Create timer
    ESP_LOGI(TAG, "Creating timer");
    next_action_timer = xTimerCreate("Next Action Timer", ticks, pdFALSE, this, TimerCallback);
    if (next_action_timer == nullptr) {
      ESP_LOGE(TAG, "Unable to create timer");
      return;
    }
  } else {
    // Change timer
    ESP_LOGI(TAG, "Changing timer period");
    if (xTimerChangePeriod(next_action_timer, ticks, 0) == pdFAIL) {
      ESP_LOGE(TAG, "Unable to change timer period");
      return;
    }
  }

  // Start timer
  ESP_LOGI(TAG, "Starting timer to expire in %lld second(s), at %s",
    seconds_until_target, IndyTime::FormatTime(next_action_time).c_str());
  if (xTimerStart(next_action_timer, 0) == pdFAIL)
    ESP_LOGE(TAG, "Unable to start timer");
  ESP_LOGI(TAG, "Timer started");
}

// Notifies the scheduler task that it's time for the next action. A task is
// used so the timer service task doesn't block.
void IndyScheduler::TimerCallback(TimerHandle_t handle) {
  ESP_LOGI(TAG, "Next action timer expired");

  // Signal task that the timer has expired
  IndyScheduler* scheduler = reinterpret_cast<IndyScheduler*>(pvTimerGetTimerID(handle));
  scheduler->task.TaskNotifyGive();
}

// Notifies scheduler that it's time for the next action
void IndyScheduler::TaskFunction(void *arg) {
  // Do next action
  IndyScheduler* scheduler = reinterpret_cast<IndyScheduler*>(arg);
  scheduler->HandleNextActionTimerExpiry();
}

// Randomizes `time` by +/- random_offset_range minutes
time_t IndyScheduler::RandomizeTime(time_t time) {
  // Generate random offset: +/- random_offset minutes
  int offset = static_cast<int>((esp_random() % (2 * random_offset_range + 1)) - static_cast<int>(random_offset_range));
  ESP_LOGI(TAG, "Random offset is %d minutes", offset);

  return time + (offset * SECONDS_PER_MINUTE);
}

// Notifies listeners that it's time for the next action, and then schedules the
// subsequent next action
void IndyScheduler::HandleNextActionTimerExpiry() {
  ESP_LOGI(TAG, "Handling timer expiry for next action %s", NextActionAsStr());
  ESP_LOGI(TAG, "The current time is %s", IndyTime::FormatTime(time(nullptr)).c_str());

  // Check next action
  if (next_action == NextActionEnum::NOOP) {
    ESP_LOGE(TAG, "Next action is NOOP");
    return;
  }

  // Notify next action handlers
  bool on = next_action == NextActionEnum::ON;
  for (const NextActionHandler& handler : handlers)
    handler(on);

  // Lookup sun times
  if (DetermineSunTimes(&current_sun_times) == nullptr) {
      ESP_LOGE(TAG, "Unable to determine sun times");
      return;
  }

  // Determine next action
  if (on) {
    next_action = NextActionEnum::OFF;
    next_action_time = RandomizeTime(current_sun_times.sunrise);
  } else {
    next_action = NextActionEnum::ON;
    next_action_time = RandomizeTime(current_sun_times.sunset);
  }
  ESP_LOGI(TAG, "Scheduling next action %s for %s",
    NextActionAsStr(), IndyTime::FormatTime(next_action_time).c_str());

  // Save updated state to storage
  nvs->WriteInt(NVS_KEY_NEXT_ACTION, (int32_t) next_action);
  nvs->WriteTime(NVS_KEY_NEXT_ACTION_TIME, next_action_time);
  nvs->Commit();

  // Start timer
  StartTimer();
}

