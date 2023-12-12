#ifndef COMPONENTS_INDY_COMMON_INDY_UTIL_H_
#define COMPONENTS_INDY_COMMON_INDY_UTIL_H_

#include <freertos/FreeRTOS.h>

#include <string>

extern const time_t NULL_TIME;

std::string FormatString(const char* format, ...);
std::string FormatString2(const char* context, const char* format, ...);

#endif  // COMPONENTS_INDY_COMMON_INDY_UTIL_H_
