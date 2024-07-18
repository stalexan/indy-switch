#ifndef COMPONENTS_INDY_COMMON_INDY_NVS_H_
#define COMPONENTS_INDY_COMMON_INDY_NVS_H_

#include <nvs_flash.h>

#include <ctime>
#include <string>

// Manages ESP32 NVS (non-volatile storage)
class IndyNvs {
 public:
  void Setup();
  ~IndyNvs();

  bool ReadBool(const char* key, bool* result);
  bool ReadBool(const char* key);
  void WriteBool(const char* key, bool value);

  bool ReadTime(const char* key, time_t* result);
  time_t ReadTime(const char* key);
  void WriteTime(const char* key, time_t value);

  bool ReadInt(const char* key, int32_t* result);
  int32_t ReadInt(const char* key);
  void WriteInt(const char* key, int32_t value);

  bool ReadString(const char* key, std::string* result);
  std::string ReadString(const char* key);
  void WriteString(const char* key, const char* value);

  void Commit();

  void Reset();

 private:
  nvs_handle_t handle = 0;
};

#endif  // COMPONENTS_INDY_COMMON_INDY_NVS_H_
