#ifndef COMPONENTS_INDY_COMMON_INDY_TIME_H_
#define COMPONENTS_INDY_COMMON_INDY_TIME_H_

#include <ctime>
#include <functional>
#include <string>
#include <vector>

class IndyTime {
 public:
  // Time synced handlers
  using TimeSyncedHandler = std::function<void()>;
  void RegisterTimeInitializedHandler(const TimeSyncedHandler& handler) { handlers.push_back(handler); }

  void Setup(const TimeSyncedHandler& handler);

  void HandleSntpTimeSync();

  static std::string FormatTime(time_t time);
  static std::string FormatCurrentTime();

 private:
  // Time synced handlers
  std::vector<TimeSyncedHandler> handlers;
};

#endif  // COMPONENTS_INDY_COMMON_INDY_TIME_H_
