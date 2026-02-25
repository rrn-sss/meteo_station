#include "webportal.h"
#include "common.h"
#include "tasks_common.h"
#include <ArduinoJSON.h>
#include <HTTPClient.h>
#include <LittleFS.h>
#include <WiFiUdp.h>
#include <atomic>

// флаг для сохранения данных
std::atomic<bool> shouldSaveConfig{false};
static const char *TAG = "WM";

int timeout = 120; // время выполнения в секундах

// callback уведомляющий о необходимости сохранить конфигурацию
void saveConfigCallback()
{
  ESP_LOGI(TAG, "Should save config\n");
  shouldSaveConfig = true;
}

// Constructor: initialize WiFiManagerParameter members and register with WiFiManager
WebConfig::WebConfig(WiFiManager &wm_ref)
    : wm(wm_ref),
      custom_mqtt_server("server", "MQTT server"),
      custom_mqtt_port("port", "MQTT port"),
      custom_mqtt_user("user", "MQTT login"),
      custom_mqtt_pass("pass", "MQTT password"),
      custom_mqtt_prefix("prefix", "MQTT topics prefix"),
      custom_bot_token("bot_token", "Telegram bot token"),
      custom_bot_chat_id("bot_chat_id", "Telegram bot chat identificator"),
      custom_lat("latitude", "Latitude for Open-Meteo (example \"47.2329\")"),
      custom_long("longitude", "Longitude for Open-Meteo (example \"39.7075\")"),
      custom_gmt_offset("gmt_offset_sec", "GMT offset seconds (example \"10800\")")
{
  wm.addParameter(&custom_mqtt_server);
  wm.addParameter(&custom_mqtt_port);
  wm.addParameter(&custom_mqtt_user);
  wm.addParameter(&custom_mqtt_pass);
  wm.addParameter(&custom_mqtt_prefix);
  wm.addParameter(&custom_bot_token);
  wm.addParameter(&custom_bot_chat_id);
  wm.addParameter(&custom_lat);
  wm.addParameter(&custom_long);
  wm.addParameter(&custom_gmt_offset);
}

void WebConfig::process_autoconnect_or_config(bool f_on_demand)
{
  PrjCfgData cfg;

  // Если WiFi уже подключен и не требуется принудительный портал - выходим сразу (неблокирующе)
  if (!f_on_demand && WiFi.status() == WL_CONNECTED)
    return;

  // читаем конфигурацию перед запуском портала конфигурирования
  bool configOk = get_config(cfg);
  if (configOk)
  {
    ESP_LOGI(TAG, "WiFi SSID: %s", WiFi.SSID().c_str());

    ESP_LOGI(TAG, "The values in the file are:");
    ESP_LOGI(TAG, "\tmqtt_server        : %s", cfg.mqtt_server);
    ESP_LOGI(TAG, "\tmqtt_port          : %s", cfg.mqtt_port);
    ESP_LOGI(TAG, "\tmqtt_user          : %s", cfg.mqtt_user);
    ESP_LOGI(TAG, "\tmqtt_pass          : %s", cfg.mqtt_pass);
    ESP_LOGI(TAG, "\tmqtt_prefix        : %s", cfg.mqtt_prefix);
    ESP_LOGI(TAG, "\tbot_token          : %s", cfg.bot_token);
    ESP_LOGI(TAG, "\tbot_chat_id        : %s", cfg.bot_chat_id);
    ESP_LOGI(TAG, "\tlatitude           : %s", cfg.latitude);
    ESP_LOGI(TAG, "\tlongitude          : %s", cfg.longitude);
    ESP_LOGI(TAG, "\tgmt_offset_sec     : %s", cfg.gmt_offset_sec);
  }

  // Обновить значения параметров WiFiManager из конфигурации
  custom_mqtt_server.setValue(cfg.mqtt_server, sizeof(cfg.mqtt_server));
  custom_mqtt_port.setValue(cfg.mqtt_port, sizeof(cfg.mqtt_port));
  custom_mqtt_user.setValue(cfg.mqtt_user, sizeof(cfg.mqtt_user));
  custom_mqtt_pass.setValue(cfg.mqtt_pass, sizeof(cfg.mqtt_pass));
  custom_mqtt_prefix.setValue(cfg.mqtt_prefix, sizeof(cfg.mqtt_prefix));
  custom_bot_token.setValue(cfg.bot_token, sizeof(cfg.bot_token));
  custom_bot_chat_id.setValue(cfg.bot_chat_id, sizeof(cfg.bot_chat_id));
  custom_lat.setValue(cfg.latitude, sizeof(cfg.latitude));
  custom_long.setValue(cfg.longitude, sizeof(cfg.longitude));
  custom_gmt_offset.setValue(cfg.gmt_offset_sec, sizeof(cfg.gmt_offset_sec));

  bool needPortal = f_on_demand || !configOk;

  // Попытка подключения к WiFi (используем credentials из NVS ESP32)
  if (!needPortal && WiFi.status() != WL_CONNECTED)
  {
    ESP_LOGI(TAG, "Attempting WiFi connection using saved credentials...");
    WiFi.begin(); // ESP32 использует сохранённые в NVS SSID/пароль
    // Ожидаем подключения не более 10 секунд
    unsigned long startAttempt = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - startAttempt) < 10000)
    {
      vTaskDelay(100 / portTICK_PERIOD_MS);
    }
    if (WiFi.status() != WL_CONNECTED)
    {
      ESP_LOGW(TAG, "WiFi connection failed, will start config portal");
      needPortal = true;
    }
    else
    {
      ESP_LOGI(TAG, "WiFi connected, IP: %s", WiFi.localIP().toString().c_str());
      ESP_LOGI(TAG, "WiFi mac: %s", WiFi.macAddress().c_str());
    }
  }

  // Запуск блокирующего портала только при необходимости
  if (needPortal)
  {
    ESP_LOGI(TAG, "Starting config portal...");
    wm.setSaveConfigCallback(saveConfigCallback);
    wm.setConfigPortalTimeout(timeout);
    wm.setDebugOutput(false);
    wm.startConfigPortal("AutoConnectAP");
  }

  // Прочитать обновлённые параметры из WiFiManager (после портала)
  strcpy(cfg.mqtt_server, custom_mqtt_server.getValue());
  strcpy(cfg.mqtt_port, custom_mqtt_port.getValue());
  strcpy(cfg.mqtt_user, custom_mqtt_user.getValue());
  strcpy(cfg.mqtt_pass, custom_mqtt_pass.getValue());
  strcpy(cfg.mqtt_prefix, custom_mqtt_prefix.getValue());
  strcpy(cfg.bot_token, custom_bot_token.getValue());
  strcpy(cfg.bot_chat_id, custom_bot_chat_id.getValue());
  strcpy(cfg.latitude, custom_lat.getValue());
  strcpy(cfg.longitude, custom_long.getValue());
  strcpy(cfg.gmt_offset_sec, custom_gmt_offset.getValue());

  // Сохранить пользовательские параметры в FS
  if (shouldSaveConfig)
  {
    set_config(cfg);
    vTaskDelay(5000 / portTICK_PERIOD_MS);
    shouldSaveConfig = false;
  }
}

bool WebConfig::get_config(PrjCfgData &cfg)
{
  bool ret = true;

  // Захватываем мьютекс LittleFS для защиты от конкурентного доступа
  if (xSemaphoreTake(xLittleFSMutex, pdMS_TO_TICKS(2000)) != pdTRUE)
  {
    ESP_LOGE(TAG, "Failed to acquire LittleFS mutex for get_config");
    return false;
  }

  ESP_LOGI(TAG, "mounted file system\n");
  if (LittleFS.exists("/config.json"))
  {
    // file exists, reading and loading
    ESP_LOGI(TAG, "reading config file\n");
    File configFile = LittleFS.open("/config.json", "r");
    if (configFile)
    {
      ESP_LOGI(TAG, "opened config file\n");
      size_t size = configFile.size();
      // Выделить буфер для хранения содержимого файла
      std::unique_ptr<char[]> buf(new char[size]);

      configFile.readBytes(buf.get(), size);

      JsonDocument json;
      auto deserializeError = deserializeJson(json, buf.get());
      if (!deserializeError)
      {
        ESP_LOGI(TAG, "\nparsed json:\n");

        strcpy(cfg.mqtt_server, json["mqtt_server"]);
        strcpy(cfg.mqtt_port, json["mqtt_port"]);
        strcpy(cfg.mqtt_user, json["mqtt_user"]);
        strcpy(cfg.mqtt_pass, json["mqtt_pass"]);
        strcpy(cfg.mqtt_prefix, json["mqtt_prefix"]);
        strcpy(cfg.bot_token, json["bot_token"]);
        strcpy(cfg.bot_chat_id, json["bot_chat_id"]);
        strcpy(cfg.latitude, json["latitude"]);
        strcpy(cfg.longitude, json["longitude"]);
        strcpy(cfg.gmt_offset_sec, json["gmt_offset_sec"]);

        ESP_LOGI(TAG, "\t[\"mqtt_server\"]        : %s", cfg.mqtt_server);
        ESP_LOGI(TAG, "\t[\"mqtt_port\"]          : %s", cfg.mqtt_port);
        ESP_LOGI(TAG, "\t[\"mqtt_user\"]          : %s", cfg.mqtt_user);
        ESP_LOGI(TAG, "\t[\"mqtt_pass\"]          : %s", cfg.mqtt_pass);
        ESP_LOGI(TAG, "\t[\"mqtt_prefix\"]        : %s", cfg.mqtt_prefix);
        ESP_LOGI(TAG, "\t[\"bot_token\"]          : %s", cfg.bot_token);
        ESP_LOGI(TAG, "\t[\"bot_chat_id\"]        : %s", cfg.bot_chat_id);
        ESP_LOGI(TAG, "\t[\"latitude\"]           : %s", cfg.latitude);
        ESP_LOGI(TAG, "\t[\"longitude\"]          : %s", cfg.longitude);
        ESP_LOGI(TAG, "\t[\"gmt_offset_sec\"]     : %s", cfg.gmt_offset_sec);
      }
      else
      {
        ESP_LOGE(TAG, "failed to load json config\n");
        ret = false;
      }
      configFile.close();
    }
  }
  // конец чтения

  // Освобождаем мьютекс LittleFS
  xSemaphoreGive(xLittleFSMutex);

  return ret;
}

bool WebConfig::set_config(const PrjCfgData &cfg)
{
  ESP_LOGI(TAG, "saving config\n");

  // Захватываем мьютекс LittleFS для защиты от конкурентного доступа
  if (xSemaphoreTake(xLittleFSMutex, pdMS_TO_TICKS(2000)) != pdTRUE)
  {
    ESP_LOGE(TAG, "Failed to acquire LittleFS mutex for set_config");
    return false;
  }

  JsonDocument json;
  json["mqtt_server"] = cfg.mqtt_server;
  json["mqtt_port"] = cfg.mqtt_port;
  json["mqtt_user"] = cfg.mqtt_user;
  json["mqtt_pass"] = cfg.mqtt_pass;
  json["mqtt_prefix"] = cfg.mqtt_prefix;
  json["bot_token"] = cfg.bot_token;
  json["bot_chat_id"] = cfg.bot_chat_id;
  json["latitude"] = cfg.latitude;
  json["longitude"] = cfg.longitude;
  json["gmt_offset_sec"] = cfg.gmt_offset_sec;

  File configFile = LittleFS.open("/config.json", "w");
  if (!configFile)
    ESP_LOGI(TAG, "failed to open config file for writing\n");

  serializeJson(json, configFile);
  configFile.close();

  // Освобождаем мьютекс LittleFS
  xSemaphoreGive(xLittleFSMutex);

  // конец сохранения
  return true;
}

String WebConfig::getWiFiSSID()
{
  return wm.getWiFiSSID();
}

String WebConfig::getWiFiPass()
{
  return wm.getWiFiPass();
}