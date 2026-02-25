#include "task_networking.h"
#include "common.h"
#include "netprocessor.h"
#include "openmeteo.h"
#include "stack_monitor.h"
#include "tasks_common.h"
#include "webportal.h"
#include <WiFi.h>
#include <esp_log.h>
#include <time.h>

static NetProcessor net(xQueue); // объект NetProcessor, владеющий WebConfig, OpenMeteo и MqttSender

static const char *TAG = "NETWORKING";

// Interval for sending data to NarodMon (milliseconds)
#define NARODMON_INTERVAL_MS (5 * 60 * 1000)
// Last received OUT sensor data and timestamp (millis)
static OutSensorData_t g_lastOutData{};
static uint32_t g_lastOutDataMillis = 0;
static TickType_t g_lastNarodmonTick = 0;

void task_networking_exec(void *pvParameters)
{
  WebConfig *webConfig = &net.webConfig;
  OpenMeteo *openMeteo = &net.openMeteo;

  if (webConfig == nullptr || openMeteo == nullptr)
  {
    ESP_LOGE("NETWORKING", "WebConfig or OpenMeteo is NULL, deleting task...");
    vTaskDelay(3000 / portTICK_PERIOD_MS);
    vTaskDelete(NULL);
  }

  // обработка веб-портала конфигурации WiFi и других параметров
  while (WiFi.status() != WL_CONNECTED)
  {
    webConfig->process_autoconnect_or_config(false); // false = обычный режим, true = принудительный портал
  }

  PrjCfgData cfg;
  webConfig->get_config(cfg);

  // реализовать вычитывание конфигурации и установку координат в OpenMeteo
  if (1) // для освобождения стека
  {

    openMeteo->set_config(cfg.latitude, cfg.longitude);

    // Настроить NTP из сохранённой конфигурации (реализовано в NetProcessor)
    if (!net.configureNTPFromConfig())
    {
      ESP_LOGW("NETWORKING", "NTP configuration/sync failed");
    }

    // При старте послать запрос на обратное геокодирование для получения наименования ближайшего города
    // (WiFi уже подключён к этому моменту) и передать результат в очередь задачи TFT
    net.NearestCityProcessing();
  }

  TickType_t xLastWakeTime = xTaskGetTickCount();
  // Для логирования минимального остатка стека (high-water mark)
  TickType_t xLastStackLog = xTaskGetTickCount();
  StackMonitor_t stackMon;
  stack_monitor_init(&stackMon, "NETWORKING");
  // Установить время последнего запроса метео в прошлом, чтобы первый вызов произошёл немедленно
  TickType_t xLastMeteoTime = xTaskGetTickCount() - pdMS_TO_TICKS(METEO_POLL_INTERVAL_MS);

  // Предыдущее локальное время для детекции смены даты (если время меняется — отправить запрос)
  struct tm prev_timeinfo;
  if (!getLocalTime(&prev_timeinfo))
  {
    // Если получить локальное время не удалось — обнулить структуру, сравнения будут работать позже
    memset(&prev_timeinfo, 0, sizeof(prev_timeinfo));
  }

  while (1)
  {
    // Sample stack high-water mark and handle logging via helper
    stack_monitor_sample(&stackMon, PROTASK_NETWORKING_STACK_SIZE);

    // Проверка подключения к WiFi и установка битов состояния
    if (WiFi.status() != WL_CONNECTED)
    {
      xEventGroupClearBits(xEventGroup, BIT_WIFI_STATE_UP);
      ESP_LOGW("NET", "WiFi is not connected. Attempting reconnect ...");

      // Попробовать восстановить соединение (non-blocking)
      WiFi.reconnect();

      vTaskDelay(5000 / portTICK_PERIOD_MS);
      continue; // повторить попытку на следующей итерации
    }
    else
      xEventGroupSetBits(xEventGroup, BIT_WIFI_STATE_UP | BIT_NARODMON_UP);

    // MQTT: ensure connection and process any queued sensor data forwarded to networking
    if (WiFi.status() == WL_CONNECTED)
    {
      net.mqttSender.loop(cfg);

      // Try to receive messages destined for networking (home sensor / nRF)
      QueDataItem_t qitem;
      if (xQueueReceive(xQueue[PROTASK_NETWORKING], &qitem, pdMS_TO_TICKS(50)) == pdPASS)
      {
        // If this is OUT sensor data - cache the last value + timestamp
        if (qitem.type == QUE_DATATYPE_OUT_SENSOR_DATA)
        {
          OutSensorData_t *p = static_cast<OutSensorData_t *>(qitem.data);
          if (p)
          {
            g_lastOutData = *p;
            g_lastOutDataMillis = millis();
            ESP_LOGI("NETWORKING", "Cached OUT for NarodMon: T=%.2f H=%.2f P=%u BAT=%u",
                     g_lastOutData.temperature, g_lastOutData.humidity, g_lastOutData.pressure, g_lastOutData.bat_charge);
          }
        }

        // Forward to MQTT processing as before (it will free qitem.data)
        net.mqttSender.processing(cfg, qitem);
      }
    }

    // Вызов обработки метео-данных каждые METEO_POLL_INTERVAL_MS или при смене даты
    bool dateChanged = false;
    struct tm timeinfo;
    if (getLocalTime(&timeinfo))
    {
      if (timeinfo.tm_mday != prev_timeinfo.tm_mday ||
          timeinfo.tm_mon != prev_timeinfo.tm_mon ||
          timeinfo.tm_year != prev_timeinfo.tm_year)
      {
        dateChanged = true;
        prev_timeinfo = timeinfo; // обновить предыдущее время
        ESP_LOGI("NETWORKING", "Date change detected — forcing meteo request");
      }
    }

    if ((xTaskGetTickCount() - xLastMeteoTime) >= pdMS_TO_TICKS(METEO_POLL_INTERVAL_MS) || dateChanged)
    {
      if (WiFi.status() == WL_CONNECTED)
      {
        bool GeoMagOk = openMeteo->process_geomagnetic_data(); // должен быть вызван до обработки метео-данных
        bool meteoOk = openMeteo->process_meteo_data();
        if (meteoOk)
          xEventGroupSetBits(xEventGroup, BIT_OPEN_METEO_UP);
        else
          xEventGroupClearBits(xEventGroup, BIT_OPEN_METEO_UP);
      }
      xLastMeteoTime = xTaskGetTickCount();
    }

    // Periodic NarodMon sender (every NARODMON_INTERVAL_MS)
    if (g_lastNarodmonTick == 0)
      g_lastNarodmonTick = xTaskGetTickCount();

    if ((xTaskGetTickCount() - g_lastNarodmonTick) >= pdMS_TO_TICKS(NARODMON_INTERVAL_MS))
    {
      // advance the tick regardless of whether we will send (reset interval)
      g_lastNarodmonTick = xTaskGetTickCount();

      // If we have received data within the last interval, send it
      if (g_lastOutDataMillis != 0)
      {
        uint32_t now_ms = millis();
        uint32_t delta_ms = 0;
        if (now_ms >= g_lastOutDataMillis)
          delta_ms = now_ms - g_lastOutDataMillis;
        else
          delta_ms = (UINT32_MAX - g_lastOutDataMillis) + now_ms + 1; // wrap safety

        if (delta_ms <= NARODMON_INTERVAL_MS)
        {
          // Build NarodMon GET request
          // Use NetProcessor helper to send NarodMon GET
          if (!net.sendNarodMon(g_lastOutData))
            ESP_LOGW("NETWORKING", "NetProcessor::sendNarodMon failed");
          else
            xEventGroupSetBits(xEventGroup, BIT_NARODMON_UP);
        }
        else
        {
          ESP_LOGI("NETWORKING", "No recent OUT data (%.0fs ago), skipping NarodMon send",
                   (double)delta_ms / 1000.0);
          xEventGroupClearBits(xEventGroup, BIT_NARODMON_UP);
        }
      }
      else
      {
        ESP_LOGI("NETWORKING", "No OUT data cached, skipping NarodMon send");
        // set EventGroup bit to indicate NarodMon not sent?
        xEventGroupClearBits(xEventGroup, BIT_NARODMON_UP);
      }
    }

    vTaskDelayUntil(&xLastWakeTime, 10 / portTICK_PERIOD_MS); // раз в 10 мс
  }
}
