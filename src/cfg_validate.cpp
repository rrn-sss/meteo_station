#include "cfg_validate.h"
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <esp_log.h>

static const char *TAG = "CFGVALIDATE";

namespace
{

/// @brief Пределы допустимых значений
constexpr long kMqttPortMin = 1;
constexpr long kMqttPortMax = 65535;
constexpr double kLatitudeMin = -90.0;
constexpr double kLatitudeMax = 90.0;
constexpr double kLongitudeMin = -180.0;
constexpr double kLongitudeMax = 180.0;
constexpr long kGmtOffsetMin = -12 * 60 * 60; // UTC-12
constexpr long kGmtOffsetMax = 14 * 60 * 60;  // UTC+14

bool isSpaceChar(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }

const char *skipSpaces(const char *s)
{
  while (*s != '\0' && isSpaceChar(*s))
    ++s;
  return s;
}

bool isBlankString(const char *s)
{
  for (; *s != '\0'; ++s)
  {
    if (!isSpaceChar(*s))
      return false;
  }
  return true;
}

bool hasAnyOf(const char *s, const char *chars) { return strpbrk(s, chars) != nullptr; }

// --- правила для отдельных полей (используются и в check(), и в applyDefaults()) ---

bool serverOk(const PrjCfgData &cfg)
{
  return !isBlankString(cfg.mqtt_server) && !hasAnyOf(cfg.mqtt_server, " \t\r\n/:");
}

bool portOk(const PrjCfgData &cfg)
{
  long port = 0;
  return CfgValidate::parseLong(cfg.mqtt_port, kMqttPortMin, kMqttPortMax, port);
}

bool prefixOk(const PrjCfgData &cfg)
{
  return !isBlankString(cfg.mqtt_prefix) && !hasAnyOf(cfg.mqtt_prefix, " \t\r\n/");
}

bool latitudeOk(const PrjCfgData &cfg)
{
  double value = 0.0;
  return CfgValidate::parseDouble(cfg.latitude, kLatitudeMin, kLatitudeMax, value);
}

bool longitudeOk(const PrjCfgData &cfg)
{
  double value = 0.0;
  return CfgValidate::parseDouble(cfg.longitude, kLongitudeMin, kLongitudeMax, value);
}

bool gmtOffsetOk(const PrjCfgData &cfg)
{
  long value = 0;
  return CfgValidate::parseLong(cfg.gmt_offset_sec, kGmtOffsetMin, kGmtOffsetMax, value);
}

/// @brief Скопировать строковое поле с гарантированной null-терминацией
template <size_t N>
void copyField(char (&dst)[N], const char (&src)[N])
{
  snprintf(dst, N, "%s", src);
}

} // namespace

// ---------------------------------------------------------------------------
bool CfgValidate::parseLong(const char *value, long min_value, long max_value, long &out)
{
  if (value == nullptr)
    return false;

  const char *p = skipSpaces(value);
  if (*p == '\0')
    return false;

  char *end = nullptr;
  errno = 0;
  const long parsed = strtol(p, &end, 10);
  if (end == p || errno == ERANGE)
    return false;

  // После числа допускаются только пробелы
  if (*skipSpaces(end) != '\0')
    return false;

  if (parsed < min_value || parsed > max_value)
    return false;

  out = parsed;
  return true;
}

// ---------------------------------------------------------------------------
bool CfgValidate::parseDouble(const char *value, double min_value, double max_value, double &out)
{
  if (value == nullptr)
    return false;

  const char *p = skipSpaces(value);
  if (*p == '\0')
    return false;

  // Копия с заменой ',' на '.' (strtod в C-locale понимает только точку)
  char buf[40];
  size_t len = 0;
  for (; p[len] != '\0'; ++len)
  {
    if (len >= sizeof(buf) - 1)
      return false; // слишком длинное значение — заведомо не координата
    buf[len] = (p[len] == ',') ? '.' : p[len];
  }
  buf[len] = '\0';

  char *end = nullptr;
  errno = 0;
  const double parsed = strtod(buf, &end);
  if (end == buf || errno == ERANGE)
    return false;

  if (*skipSpaces(end) != '\0')
    return false;

  if (!std::isfinite(parsed))
    return false;

  if (parsed < min_value || parsed > max_value)
    return false;

  out = parsed;
  return true;
}

// ---------------------------------------------------------------------------
CfgValidate::Error CfgValidate::check(const PrjCfgData &cfg)
{
  if (!serverOk(cfg))
    return Error("mqtt_server", "должен быть непустым именем хоста без пробелов, '/' и ':'");
  if (!portOk(cfg))
    return Error("mqtt_port", "должен быть целым числом в диапазоне 1..65535");
  if (!prefixOk(cfg))
    return Error("mqtt_prefix", "должен быть непустым префиксом топиков без пробелов и '/'");
  if (!latitudeOk(cfg))
    return Error("latitude", "должна быть числом в диапазоне -90..90");
  if (!longitudeOk(cfg))
    return Error("longitude", "должна быть числом в диапазоне -180..180");
  if (!gmtOffsetOk(cfg))
    return Error("gmt_offset_sec", "должен быть целым числом в диапазоне -43200..50400");

  return Error();
}

// ---------------------------------------------------------------------------
int CfgValidate::applyDefaults(PrjCfgData &cfg)
{
  const PrjCfgData defaults;
  int fixed = 0;

  if (!serverOk(cfg))
  {
    ESP_LOGW(TAG, "Invalid mqtt_server '%s' -> default '%s'", cfg.mqtt_server, defaults.mqtt_server);
    copyField(cfg.mqtt_server, defaults.mqtt_server);
    ++fixed;
  }
  if (!portOk(cfg))
  {
    ESP_LOGW(TAG, "Invalid mqtt_port '%s' -> default '%s'", cfg.mqtt_port, defaults.mqtt_port);
    copyField(cfg.mqtt_port, defaults.mqtt_port);
    ++fixed;
  }
  if (!prefixOk(cfg))
  {
    ESP_LOGW(TAG, "Invalid mqtt_prefix '%s' -> default '%s'", cfg.mqtt_prefix, defaults.mqtt_prefix);
    copyField(cfg.mqtt_prefix, defaults.mqtt_prefix);
    ++fixed;
  }
  if (!latitudeOk(cfg))
  {
    ESP_LOGW(TAG, "Invalid latitude '%s' -> default '%s'", cfg.latitude, defaults.latitude);
    copyField(cfg.latitude, defaults.latitude);
    ++fixed;
  }
  if (!longitudeOk(cfg))
  {
    ESP_LOGW(TAG, "Invalid longitude '%s' -> default '%s'", cfg.longitude, defaults.longitude);
    copyField(cfg.longitude, defaults.longitude);
    ++fixed;
  }
  if (!gmtOffsetOk(cfg))
  {
    ESP_LOGW(TAG, "Invalid gmt_offset_sec '%s' -> default '%s'", cfg.gmt_offset_sec, defaults.gmt_offset_sec);
    copyField(cfg.gmt_offset_sec, defaults.gmt_offset_sec);
    ++fixed;
  }

  return fixed;
}
