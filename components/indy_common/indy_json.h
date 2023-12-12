#ifndef COMPONENTS_INDY_COMMON_INDY_JSON_H_
#define COMPONENTS_INDY_COMMON_INDY_JSON_H_

#include <cJSON.h>

#include <string>
#include <vector>

// Represents the result of a JSON lookup done with JsonParser
template <typename T>
struct JsonResult {
  T value;                // The value found
  bool is_error;          // Whether there was an error attempting to find the value
  std::string message;    // Error message if there was an error

  JsonResult(): is_error(true) {}
  explicit JsonResult(const char* message): is_error(true), message(message) {}
  explicit JsonResult(const T& val) : value(val), is_error(false) {}
};

// Manages parsing JSON
class JsonParser {
 public:
  JsonParser(const char* json, const std::string &tag, const std::string &error_message_prefix) :
    json(json), tag(tag), error_message_prefix(error_message_prefix) {}
  ~JsonParser();

  void SetTag(const std::string &tag) { this->tag = tag; }

  cJSON* GetRoot() { return root; }

  std::string Parse();
  JsonResult<cJSON*> GetObject(const cJSON* object, const char *context, const char *attr) const;
  JsonResult<std::string> GetString(const cJSON* object, const char *context, const char *attr) const;
  JsonResult<std::vector<std::string>> GetStringArray(const cJSON* object, const char *context, const char *attr) const;
  JsonResult<bool> GetBool(const cJSON* object, const char *context, const char *attr) const;
  JsonResult<int> GetInt(const cJSON* object, const char *context, const char *attr) const;
  std::vector<std::string> LookupKeys(const cJSON* object) const;

  static cJSON* CloneJSON(cJSON *json);

 private:
  std::string json;
  cJSON *root = nullptr;

  JsonResult<cJSON*> GetItem(const cJSON* object, const char *context, const char *attr) const;

  // Logging
  std::string tag;
  std::string error_message_prefix;
  void LogError(const std::string &message) const;
};

#endif  // COMPONENTS_INDY_COMMON_INDY_JSON_H_
