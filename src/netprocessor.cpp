#include "netprocessor.h"
#include "http_helpers.h"
#include <WiFi.h>
#include <esp_log.h>

// Global NetProcessor instance is owned/constructed by main(); no singletons here.

NetProcessor::NetProcessor(QueueHandle_t (&queue_array_ref)[_PROTASK_NUM_], const String &latitude, const String &longitude)
    : xQueues(queue_array_ref), wm(), webConfig(wm), openMeteo(latitude, longitude, PROTASK_TFT, queue_array_ref), mqttSender(queue_array_ref)
{
  ESP_LOGI("NETWORKING", "NetProcessor constructed");
}

NetProcessor::~NetProcessor()
{
  ESP_LOGI("NETWORKING", "NetProcessor destructed");
}

/**
 * @brief Отправляет данные наружного датчика в NarodMon
 * @param data Ссылка на структуру `OutSensorData_t` с последними значениями
 * @return true при успешной отправке/ответе, иначе false
 *
 * Метод формирует URL в соответствии с требованиями NarodMon и вызывает
 * сетевой helper для выполнения GET запроса.
 */
bool NetProcessor::sendNarodMon(const OutSensorData_t &data)
{
  PrjCfgData cfg;
  bool configOk = webConfig.get_config(cfg);
  if (!configOk)
  {
    ESP_LOGW("NETWORKING", "Cannot send to NarodMon: failed to get config");
    return false;
  }
  // Build NarodMon GET URL and use OpenMeteo helper to perform HTTP GET
  String mac = WiFi.macAddress();
  mac.toUpperCase();

  String serverPath = "http://narodmon.ru/get?ID=" + mac;
  serverPath += "&T1=" + String(data.temperature, 2);
  serverPath += "&H1=" + String(data.humidity, 2);

  ESP_LOGI("NETWORKING", "NarodMon GET URL: %s", serverPath.c_str());

  String resp = send_HTTP_GET_request(serverPath.c_str());
  // String resp = "{}"; // AFDPO
  // ESP_LOGI("NETWORKING", "Sending NarodMon HTTP GET request... %s", serverPath.c_str()); // AFDPO

  if (resp.equals("{}") || resp.length() == 0)
  {
    ESP_LOGW("NETWORKING", "NarodMon send returned empty or error response");
    return false;
  }

  ESP_LOGI("NETWORKING", "NarodMon response: %s", resp.c_str());
  return true;
}

// HTTPS helper implemented in src/http_helpers.cpp (send_HTTPS_GET_request)

bool NetProcessor::configureNTPFromConfig()
{
  PrjCfgData cfg;
  bool configOk = webConfig.get_config(cfg);
  if (!configOk)
  {
    ESP_LOGW("NETWORKING", "Cannot configure NTP: failed to get config");
    return false;
  }

  // Дождаться подключения к WiFi (максимум 15 секунд)
  const unsigned long waitTimeoutMs = 15000;
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - start) < waitTimeoutMs)
  {
    vTaskDelay(200 / portTICK_PERIOD_MS);
  }
  if (WiFi.status() != WL_CONNECTED)
  {
    ESP_LOGW("NETWORKING", "WiFi not connected after %lu ms, aborting NTP config", waitTimeoutMs);
    return false;
  }

  const char *ntp_server = "pool.ntp.org";        // NTP-сервер
  const char *ntp_server_alt = "time.google.com"; // альтернативный NTP-сервер
  const char *ntp_server_final = "ntp1.ru";       // финальный NTP-сервер
  long gmt_offset = 3 * 60 * 60;                  // default смещение GMT в секундах (3 часа)
  if (configOk && strlen(cfg.gmt_offset_sec) > 0)
  {
    gmt_offset = strtol(cfg.gmt_offset_sec, NULL, 10);
  }

  configTime(gmt_offset, 0, ntp_server, ntp_server_alt, ntp_server_final);

  return true;
}

void NetProcessor::NearestCityProcessing()
{
  PrjCfgData cfg;
  if (!webConfig.get_config(cfg))
  {
    ESP_LOGW("NETWORKING", "Failed to get config for nearest city processing");
    return;
  }

  double lat = atof(cfg.latitude);
  double lon = atof(cfg.longitude);
  String city = openMeteo.getNearestCityName(lat, lon);
  // Send to TFT queue (String* allocated on heap)
  String *payload = new String(city);
  if (payload)
  {
    QueDataItem_t qitem;
    qitem.type = QUE_DATATYPE_CITYNAME;
    qitem.data = payload;
    if (pdPASS != xQueueSend(xQueues[PROTASK_TFT], &qitem, pdMS_TO_TICKS(200)))
    {
      ESP_LOGW("NETWORKING", "Failed to send city name to TFT queue");
      delete payload;
    }
    else
    {
      ESP_LOGI("NETWORKING", "Sent city name to TFT: %s", city.c_str());
    }
  }
}
