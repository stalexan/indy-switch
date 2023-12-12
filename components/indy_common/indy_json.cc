#include "indy_json.h"

#include <cJSON.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <cstring>
#include <string>
#include <cstdarg>
#include <vector>

#include "indy_util.h"

namespace {
  const char *TAG = "indy_json";
}

// Returns `data` truncated to max length `MAX_LEN_TO_LOG`
static std::string ShortenDataToLog(const std::string& data) {
  const int MAX_LEN_TO_LOG = 256;
  return data.substr(0, MAX_LEN_TO_LOG);
}

// Parses `json` and saves results to `root`. Returns an error message if the
// was an error.
std::string JsonParser::Parse() {
  // Parse JSON
  root = cJSON_Parse(json.c_str());
  if (root == nullptr) {
    std::string error_message = FormatString("%s: data is:\n%s",
      error_message_prefix.c_str(),
      ShortenDataToLog(json).c_str());
    ESP_LOGE(tag.c_str(), "%s", error_message.c_str());
    return error_message;
  }

  return "";
}

JsonParser::~JsonParser() {
  // Clean up.
  if (root != nullptr) {
    cJSON_Delete(root);
    root = nullptr;
  }
}

// Logs parsing error `message`
void JsonParser::LogError(const std::string &message) const {
  ESP_LOGE(tag.c_str(), "%s: %s: data is:\n%s",
    error_message_prefix.c_str(),
    message.c_str(),
    ShortenDataToLog(json).c_str());
}

// Returns the JSON value found in `object` at key `attr`. If `object` is `nullptr`, `root` is used instead.
JsonResult<cJSON*> JsonParser::GetItem(const cJSON* object, const char *context, const char *attr) const {
  // Get item
  cJSON *result = cJSON_GetObjectItem(object != nullptr ? object : root, attr);
  if (result == nullptr) {
    std::string message = FormatString2(context, "attribute '%s' was not found", attr);
    LogError(message);
    return JsonResult<cJSON*>(message.c_str());
  }

  return JsonResult<cJSON*>(result);
}

// Returns the JSON object found in `object` at key `attr`. If `object` is `nullptr`, `root` is used instead.
JsonResult<cJSON*> JsonParser::GetObject(const cJSON* object, const char *context, const char *attr) const {
  // Get item and check that it's an object
  JsonResult<cJSON*> result = GetItem(object, context, attr);
  if (result.is_error)
    return result;
  if (!cJSON_IsObject(result.value)) {
    std::string message = FormatString2(context, "the value for attribute '%s' is not an object", attr);
    LogError(message);
    return JsonResult<cJSON*>(message.c_str());
  }

  return JsonResult<cJSON*>(result);
}

// Returns the JSON string found in `object` at key `attr`. If `object` is `nullptr`, `root` is used instead.
JsonResult<std::string> JsonParser::GetString(const cJSON* object, const char *context, const char *attr) const {
  // Get item and check that it's a string
  JsonResult<cJSON *> result = GetItem(object, context, attr);
  if (result.is_error) {
    return JsonResult<std::string>(result.message);
  } else if (!cJSON_IsString(result.value)) {
    std::string message = FormatString2(context, "the value for attribute '%s' is not a string", attr);
    LogError(message);
    return JsonResult<std::string>(message);
  }
  return JsonResult(std::string(result.value->valuestring));
}

// Returns the string array found in `object` at key `attr`. If `object` is `nullptr`, `root` is used instead.
JsonResult<std::vector<std::string>> JsonParser::GetStringArray(
  const cJSON* object, const char *context, const char *attr) const {
  // Get array
  JsonResult<cJSON*> array = GetItem(object, context, attr);
  if (array.is_error)
    return JsonResult<std::vector<std::string>>(array.message.c_str());
  if (!cJSON_IsArray(array.value)) {
    std::string message = FormatString2(context, "the value for attribute '%s' is not an array", attr);
    LogError(message);
    return JsonResult<std::vector<std::string>>(message.c_str());
  }

  // Get strings in array
  std::vector<std::string> strings;
  int array_size = cJSON_GetArraySize(array.value);
  for (int ii = 0; ii < array_size; ii++) {
    cJSON* item = cJSON_GetArrayItem(array.value, ii);
    if (!cJSON_IsString(item)) {
      std::string message = FormatString2(context, "array entry %d in '%s' is not a string", ii, attr);
      LogError(message);
      return JsonResult<std::vector<std::string>>(message.c_str());
    }
    strings.push_back(item->valuestring);
  }

  return JsonResult(strings);
}

// Returns the JSON bool found in `object` at key `attr`. If `object` is `nullptr`, `root` is used instead.
JsonResult<bool> JsonParser::GetBool(const cJSON* object, const char *context, const char *attr) const {
  // Get item and check that it's a bool
  JsonResult<cJSON*> result = GetItem(object, context, attr);
  if (result.is_error) {
    return JsonResult<bool>(result.message.c_str());
  } else if (!cJSON_IsBool(result.value)) {
    std::string message = FormatString2(context, "the value for attribute '%s' is not a bool", attr);
    LogError(message);
    return JsonResult<bool>(message.c_str());
  }
  return JsonResult<bool>(cJSON_IsTrue(result.value));
}

// Returns the JSON int found in `object` at key `attr`. If `object` is `nullptr`, `root` is used instead.
JsonResult<int> JsonParser::GetInt(const cJSON* object, const char *context, const char *attr) const {
  // Get item and check that it's an int
  JsonResult<cJSON*> result = GetItem(object, context, attr);
  if (result.is_error) {
    return JsonResult<int>(result.message.c_str());
  } else if (!cJSON_IsNumber(result.value)) {
    std::string message = FormatString2(context, "the value for attribute '%s' is not an integer", attr);
    LogError(message);
    return JsonResult<int>(message.c_str());
  }
  return JsonResult<int>(result.value->valueint);
}

// Returns the keys found in `object`
std::vector<std::string> JsonParser::LookupKeys(const cJSON* object) const {
  std::vector<std::string> keys;
  for (const cJSON* key = object->child; key != nullptr; key = key->next) {
    if (key->string == nullptr) {
      LogError("Key is not string in attempt to lookup keys");
      break;
    }
    keys.push_back(std::string(key->string));
  }
  return keys;
}


// Returns a clone of `json`. Caller owns returned memory.
cJSON* JsonParser::CloneJSON(cJSON* json) {
    if (json == nullptr)
      return nullptr;

    // Convert json to a string
    char* json_str = cJSON_PrintUnformatted(json);
    if (json_str == nullptr) {
      ESP_LOGE(TAG, "Unable to create string version of JSON to clone");
      return nullptr;
    }

    // Parse string to create a clone
    cJSON* clone = cJSON_Parse(json_str);

    // Clean up
    free(json_str);

    return clone;
}

