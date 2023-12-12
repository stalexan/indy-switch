#include "indy_util.h"

#include <cJSON.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <cstring>
#include <string>
#include <cstdarg>
#include <vector>

const time_t NULL_TIME = -1;

// Returns a formatted string using the printf-style `format` string
std::string FormatString(const char* format, ...) {
  // Determine buffer length needed
  va_list args;
  va_start(args, format);
  int length = vsnprintf(nullptr, 0, format, args) + 1;  // +1 for null terminator
  va_end(args);
  if (length <= 1)
    return "";

  // Format string
  std::vector<char> buffer(length);
  va_start(args, format);
  int result = vsnprintf(buffer.data(), length, format, args);
  va_end(args);
  if (result < 0 || result >= length)
    return "";

  return std::string(buffer.data());
}

// Returns a formatted string using the printf-style `format` string, with an
// optional trailing `context` message
std::string FormatString2(const char* context, const char* format, ...) {
  // Format the string
  va_list args;
  va_start(args, format);
  std::string message = FormatString(format, args);
  va_end(args);

  // Add context
  if (context != nullptr)
    message += FormatString(" in '%s'", context);

  return message;
}

