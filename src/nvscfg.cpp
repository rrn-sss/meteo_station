/**
 * @file nvscfg.cpp
 * @brief NVS-based persistent configuration storage implementation.
 *        Uses ESP32 Arduino Preferences API — survives OTA firmware updates.
 *
 * MIT License — Meteo Station Project
 */

#include "nvscfg.h"
#include "tasks_common.h"
#include <ArduinoJson.h>
#include <FS.h>
#include <LittleFS.h>
#include <Preferences.h>
#include <esp_log.h>

static const char *TAG = "NvsCfg";

// NVS key names — must be <= 15 characters each
static constexpr const char *kKeyMqttServer = "mqtt_srv";
static constexpr const char *kKeyMqttPort = "mqtt_port";
static constexpr const char *kKeyMqttUser = "mqtt_user";
static constexpr const char *kKeyMqttPass = "mqtt_pass";
static constexpr const char *kKeyMqttPrefix = "mqtt_prefix";
static constexpr const char *kKeyBotToken = "bot_token";
static constexpr const char *kKeyBotChatId = "bot_chat_id";
static constexpr const char *kKeyLatitude = "latitude";
static constexpr const char *kKeyLongitude = "longitude";
static constexpr const char *kKeyGmtOffset = "gmt_offset";

// ---------------------------------------------------------------------------
bool NvsCfg::load(PrjCfgData &cfg)
{
  Preferences prefs;
  if (!prefs.begin(k_namespace, /*readOnly=*/true))
  {
    ESP_LOGW(TAG, "NVS namespace '%s' not found — first boot or empty", k_namespace);
    return false;
  }

  // A namespace that opened successfully but has no keys means first boot
  if (!prefs.isKey(kKeyMqttServer))
  {
    ESP_LOGW(TAG, "NVS key '%s' missing — assuming first boot, using defaults", kKeyMqttServer);
    prefs.end();
    return false;
  }

  // Helper lambda: read a String key into a fixed char buffer
  auto readStr = [&](const char *key, char *dst, size_t dst_size)
  {
    const String val = prefs.getString(key, "");
    if (val.length() > 0)
    {
      strncpy(dst, val.c_str(), dst_size - 1);
      dst[dst_size - 1] = '\0'; // Ensure null-termination
    }
  };

  readStr(kKeyMqttServer, cfg.mqtt_server, sizeof(cfg.mqtt_server));
  readStr(kKeyMqttPort, cfg.mqtt_port, sizeof(cfg.mqtt_port));
  readStr(kKeyMqttUser, cfg.mqtt_user, sizeof(cfg.mqtt_user));
  readStr(kKeyMqttPass, cfg.mqtt_pass, sizeof(cfg.mqtt_pass));
  readStr(kKeyMqttPrefix, cfg.mqtt_prefix, sizeof(cfg.mqtt_prefix));
  //readStr(kKeyBotToken, cfg.bot_token, sizeof(cfg.bot_token));
  //readStr(kKeyBotChatId, cfg.bot_chat_id, sizeof(cfg.bot_chat_id));
  readStr(kKeyLatitude, cfg.latitude, sizeof(cfg.latitude));
  readStr(kKeyLongitude, cfg.longitude, sizeof(cfg.longitude));
  readStr(kKeyGmtOffset, cfg.gmt_offset_sec, sizeof(cfg.gmt_offset_sec));

  prefs.end();
  ESP_LOGI(TAG, "Config loaded from NVS OK");
  return true;
}

// ---------------------------------------------------------------------------
bool NvsCfg::save(const PrjCfgData &cfg)
{
  Preferences prefs;
  if (!prefs.begin(k_namespace, /*readOnly=*/false))
  {
    ESP_LOGE(TAG, "Failed to open NVS namespace '%s' for writing", k_namespace);
    return false;
  }

  prefs.putString(kKeyMqttServer, cfg.mqtt_server);
  prefs.putString(kKeyMqttPort, cfg.mqtt_port);
  prefs.putString(kKeyMqttUser, cfg.mqtt_user);
  prefs.putString(kKeyMqttPass, cfg.mqtt_pass);
  prefs.putString(kKeyMqttPrefix, cfg.mqtt_prefix);
  //prefs.putString(kKeyBotToken, cfg.bot_token);
  //prefs.putString(kKeyBotChatId, cfg.bot_chat_id);
  prefs.putString(kKeyLatitude, cfg.latitude);
  prefs.putString(kKeyLongitude, cfg.longitude);
  prefs.putString(kKeyGmtOffset, cfg.gmt_offset_sec);

  prefs.end();
  ESP_LOGI(TAG, "Config saved to NVS OK");
  return true;
}

// ---------------------------------------------------------------------------
bool NvsCfg::migrate(bool delete_old_file)
{
  // Check if NVS already has data (migration already done)
  Preferences prefs;
  if (prefs.begin(k_namespace, /*readOnly=*/true))
  {
    if (prefs.isKey(kKeyMqttServer))
    {
      ESP_LOGI(TAG, "NVS already has config — skipping migration");
      prefs.end();
      return true;
    }
    prefs.end();
  }

  // Acquire LittleFS mutex to protect from concurrent access with TFT task
  if (xLittleFSMutex && xSemaphoreTake(xLittleFSMutex, pdMS_TO_TICKS(1000)) != pdTRUE)
  {
    ESP_LOGE(TAG, "Failed to acquire LittleFS mutex for migration");
    return false;
  }

  // Try to read from LittleFS config.json
  if (!LittleFS.exists("/config.json"))
  {
    if (xLittleFSMutex)
      xSemaphoreGive(xLittleFSMutex);
    ESP_LOGI(TAG, "No LittleFS config.json found — using defaults");
    return true; // Not an error, just no migration needed
  }

  ESP_LOGI(TAG, "Migrating config from LittleFS to NVS...");

  File configFile = LittleFS.open("/config.json", "r");
  if (!configFile)
  {
    if (xLittleFSMutex)
      xSemaphoreGive(xLittleFSMutex);
    ESP_LOGE(TAG, "Failed to open config.json for migration");
    return false;
  }

  const size_t sz = configFile.size();
  std::unique_ptr<char[]> buf(new char[sz + 1]());
  configFile.readBytes(buf.get(), sz);
  configFile.close();

  JsonDocument json;
  const auto err = deserializeJson(json, buf.get());
  if (err)
  {
    if (xLittleFSMutex)
      xSemaphoreGive(xLittleFSMutex);
    ESP_LOGE(TAG, "Failed to parse JSON config during migration: %s", err.c_str());
    return false;
  }

  // Build config from JSON and save to NVS
  PrjCfgData cfg{};
  // snprintf гарантирует null-терминацию даже при усечении значения
  snprintf(cfg.mqtt_server, sizeof(cfg.mqtt_server), "%s", json["mqtt_server"] | "");
  snprintf(cfg.mqtt_port, sizeof(cfg.mqtt_port), "%s", json["mqtt_port"] | "");
  snprintf(cfg.mqtt_user, sizeof(cfg.mqtt_user), "%s", json["mqtt_user"] | "");
  snprintf(cfg.mqtt_pass, sizeof(cfg.mqtt_pass), "%s", json["mqtt_pass"] | "");
  snprintf(cfg.mqtt_prefix, sizeof(cfg.mqtt_prefix), "%s", json["mqtt_prefix"] | "");
  //snprintf(cfg.bot_token, sizeof(cfg.bot_token), "%s", json["bot_token"] | "");
  //snprintf(cfg.bot_chat_id, sizeof(cfg.bot_chat_id), "%s", json["bot_chat_id"] | "");
  snprintf(cfg.latitude, sizeof(cfg.latitude), "%s", json["latitude"] | "");
  snprintf(cfg.longitude, sizeof(cfg.longitude), "%s", json["longitude"] | "");
  snprintf(cfg.gmt_offset_sec, sizeof(cfg.gmt_offset_sec), "%s", json["gmt_offset_sec"] | "");

  if (!save(cfg))
  {
    if (xLittleFSMutex)
      xSemaphoreGive(xLittleFSMutex);
    ESP_LOGE(TAG, "Failed to save migrated config to NVS");
    return false;
  }

  ESP_LOGI(TAG, "Config migrated successfully");

  // Optionally delete old config.json
  if (delete_old_file)
  {
    if (LittleFS.remove("/config.json"))
    {
      ESP_LOGI(TAG, "Old config.json removed after migration");
    }
    else
    {
      ESP_LOGW(TAG, "Failed to remove old config.json");
    }
  }

  // Release LittleFS mutex
  if (xLittleFSMutex)
    xSemaphoreGive(xLittleFSMutex);

  return true;
}
