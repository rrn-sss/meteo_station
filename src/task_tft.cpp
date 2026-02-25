#include "task_tft.h"
#include "meteowidgets.h"
#include "openmeteo.h"
#include "stack_monitor.h"
#include "tasks_common.h"
#include <esp_log.h>
#include <time.h>

LGFX tft; // Создать экземпляр LovyanGFX (драйвер дисплея)

void task_tft_exec(void *pvParameters)
{
  MeteoWidgets *meteo_widgets = MeteoWidgets::createInstance(tft); // Создать или получить единственный экземпляр MeteoWidgets
  GeoMagneticKpMax geomag{0, 0, 0};                                // прогноз геомагнитной обстановки

  // настройки tft
  if (!tft.init())
  {
    ESP_LOGE("TFT", "LovyanGFX initialization failed, deleting task...");
    vTaskDelete(NULL); // ошибка инициализации, удаляем задачу
  }
  tft.setFileStorage(LittleFS);

  if (!meteo_widgets)
  {
    ESP_LOGE("TFT", "MeteoWidgets parameter is NULL, deleting task...");
    vTaskDelete(NULL); // ошибка параметра, удаляем задачу
  }
  else
    meteo_widgets->init(); // инициализация виджетов TFT

  TickType_t xLastWakeTime = xTaskGetTickCount();
  // Для логирования минимального остатка стека (high-water mark)
  TickType_t xLastStackLog = xTaskGetTickCount();
  StackMonitor_t stackMon;
  stack_monitor_init(&stackMon, "TFT");

  // Буфер для последних полученных данных от OpenMeteo
  OpenMeteoData latestMeteo[_METEO_DATA_NUM_];
  bool haveMeteo[_METEO_DATA_NUM_] = {false, false, false, false};
  TickType_t lastMeteoUpdateTick = 0; // время последнего обновления данных
  bool prevMeteoValid = true;         // предыдущее состояние valid (для детекции перехода в stale)

  // Буферы для хранения данных домашнего и наружнего датчиков
  HomeSensorData_t inSensorData = {0.0f, 0, 0.0f};
  OutSensorData_t outSensorData{}; // используем default initialization (поля уже инициализированы в структуре)
  bool inSensorValid = false;      // флаг валидности данных комнатного (in) датчика
  bool outSensorValid = false;     // флаг валидности данных наружнего датчика
  // Время последнего получения данных от датчиков (для timeout)
  TickType_t lastInSensorTick = 0;
  TickType_t lastOutSensorTick = 0;
  bool f_first = true;
  // Хранение наименования населённого пункта; пока пустая строка
  String cityName = String("");
  // Placeholder external sensor battery level (0..100). Update from NRF handler when available.
  uint8_t externalBatteryLevel = 100;

  struct tm prev_timeinfo; // предыдущее время для детекции смены даты
  getLocalTime(&prev_timeinfo);

  vTaskDelay(10000 / portTICK_PERIOD_MS); // задержка перед началом работы (для "стабилизации" LittleFS и TFT)

  // бесконечный цикл
  while (1)
  {
    // Sample stack high-water mark and handle logging via helper
    stack_monitor_sample(&stackMon, PROTASK_TFT_STACK_SIZE);

    // If OTA update is in progress, display only the update-processing widget
    EventBits_t evt_bits_now = xEventGroupGetBits(xEventGroup);
    if ((evt_bits_now & BIT_OTA_UPDATE_BIT) != 0)
    {
      meteo_widgets->draw_update_processing_widget();
      vTaskDelayUntil(&xLastWakeTime, 1000 / portTICK_PERIOD_MS);
      continue; // skip normal updates while OTA is running
    }

    // Получение текущего времени и обновление виджетов часов/ даты на TFT
    struct tm timeinfo;
    if (getLocalTime(&timeinfo))
    {
      // Обновляем виджет часов только при изменении минут
      uint8_t hh = static_cast<uint8_t>(timeinfo.tm_hour);
      uint8_t mm = static_cast<uint8_t>(timeinfo.tm_min);
      const uint8_t padding = 6;
      bool ret1{false}, ret2{false};

      if (hh != static_cast<uint8_t>(prev_timeinfo.tm_hour) ||
          mm != static_cast<uint8_t>(prev_timeinfo.tm_min) || f_first)
        ret1 = meteo_widgets->draw_dig_clock_widget(padding, padding, hh, mm);

      if (timeinfo.tm_mday != prev_timeinfo.tm_mday ||
          timeinfo.tm_mon != prev_timeinfo.tm_mon ||
          timeinfo.tm_year != prev_timeinfo.tm_year || f_first)
      {
        char datebuf[32];
        snprintf(datebuf, sizeof(datebuf), "%02d-%02d-%04d", timeinfo.tm_mday, timeinfo.tm_mon + 1, timeinfo.tm_year + 1900);
        ret2 = meteo_widgets->draw_current_date_widget(MeteoWidgets::getClockDigsW() + padding, padding, String(datebuf));
      }

      prev_timeinfo = timeinfo; // сохранить текущее время как предыдущее для следующей итерации
      if (ret1 && ret2)
        f_first = false;
    }
    else
    {
      ESP_LOGW("TFT", "getLocalTime() failed — skipping clock update");
    }

    // Неблокирующее чтение группы событий и отрисовка виджета состояния подключения
    {
      EventBits_t bits = xEventGroupGetBits(xEventGroup);
      bool wifiUp = (bits & BIT_WIFI_STATE_UP) != 0;
      bool linkDown = (bits & BIT_OPEN_METEO_UP) != 0;
      bool linkUp = ((bits & BIT_MQTT_STATE_UP) != 0) && ((bits & BIT_NARODMON_UP) != 0);

      if (!wifiUp)
        linkDown = linkUp = false;

      // up = meteoUp (OpenMeteo данные получены), down = тот же статус, wifi = wifiUp
      meteo_widgets->draw_connection_state_widget(linkUp, linkDown, wifiUp);
    }

    // Неблокирующий опрос очереди данных и обновление виджетов при поступлении новых данных
    bool meteoDataReceived = false; // флаг: были ли получены новые метеоданные в этой итерации
    QueDataItem_t qitem;
    while (xQueueReceive(xQueue[PROTASK_TFT], &qitem, 0) == pdPASS) // вычитываем все доступные элементы очереди
    {
      if (qitem.type == QUE_DATATYPE_CITYNAME)
      {
        String *pname = static_cast<String *>(qitem.data);
        if (pname)
        {
          ESP_LOGI("TFT", "Received CITYNAME: %s", pname->c_str());
          cityName = *pname;
          delete pname;
        }
      }
      else if (qitem.type == QUE_DATATYPE_METEO)
      {
        OpenMeteoData *pdata = static_cast<OpenMeteoData *>(qitem.data);
        if (pdata && pdata->index >= 0 && pdata->index < _METEO_DATA_NUM_)
        {
          ESP_LOGI("TFT", "Got data METEO[%u] from queue", pdata->index);
          // Сохранить данные в локальный буфер
          latestMeteo[pdata->index] = *pdata;
          haveMeteo[pdata->index] = true;
          lastMeteoUpdateTick = xTaskGetTickCount();
          meteoDataReceived = true;
          delete pdata;
        }
        else if (pdata)
        {
          delete pdata;
        }
      }
      else if (qitem.type == QUE_DATATYPE_GEOMAGNETIC)
      {
        GeoMagneticKpMax *pdata = static_cast<GeoMagneticKpMax *>(qitem.data);
        if (pdata)
        {
          ESP_LOGI("TFT", "Got data GEOMAGNETIC from queue");
          // запоминаем геомагнитный прогноз в глобальной структуре
          geomag.kpmax_today = pdata->kpmax_today;
          geomag.kpmax_tomorrow = pdata->kpmax_tomorrow;
          geomag.kpmax_tomorrow2 = pdata->kpmax_tomorrow2;

          delete pdata;
        }
      }
      else if (qitem.type == QUE_DATATYPE_IN_SENSOR_DATA)
      {
        HomeSensorData_t *pdata = static_cast<HomeSensorData_t *>(qitem.data);
        if (pdata)
        {
          ESP_LOGI("TFT", "Got data IN_SENSOR from queue");
          // Сохраняем данные домашнего датчика
          inSensorData = *pdata;
          inSensorValid = true;
          lastInSensorTick = xTaskGetTickCount();

          const uint8_t padding = 6;
          int x = 0;
          int y = MeteoWidgets::getClockDigsH() + padding;

          // Перерисовываем два раздельных виджета: наружный (лево) и внутренний (право)
          meteo_widgets->draw_home_out_data_widget(x, y,
                                                   outSensorData.temperature, static_cast<uint8_t>(outSensorData.humidity),
                                                   outSensorValid);
          meteo_widgets->draw_home_in_data_widget(x, y,
                                                  inSensorData.temperature_in, inSensorData.humidity_in,
                                                  inSensorValid);
          meteo_widgets->draw_city_name_widget(200, 60, cityName);

          delete pdata;
        }
      }
      else if (qitem.type == QUE_DATATYPE_OUT_SENSOR_DATA)
      {
        OutSensorData_t *pdata = static_cast<OutSensorData_t *>(qitem.data);
        if (pdata)
        {
          ESP_LOGI("TFT", "Got data OUT_SENSOR from queue");
          // Сохраняем данные наружнего датчика
          outSensorData = *pdata;
          outSensorValid = true;
          lastOutSensorTick = xTaskGetTickCount();

          ESP_LOGI("TFT", "Out sensor data received: temp=%.1f, hum=%.1f, charge=%u%%",
                   outSensorData.temperature, outSensorData.humidity, outSensorData.bat_charge);

          const uint8_t padding = 6;
          int x = 0;
          int y = MeteoWidgets::getClockDigsH() + padding;

          // Перерисовываем два раздельных виджета: наружный (лево) и внутренний (право)
          meteo_widgets->draw_home_out_data_widget(x, y,
                                                   outSensorData.temperature, static_cast<uint8_t>(outSensorData.humidity),
                                                   outSensorValid);
          meteo_widgets->draw_home_in_data_widget(x, y,
                                                  inSensorData.temperature_in, inSensorData.humidity_in,
                                                  inSensorValid);
          // Обновить индикатор заряда батареи внешнего датчика
          meteo_widgets->draw_battery_level_widget(static_cast<uint8_t>(outSensorData.bat_charge));
          meteo_widgets->draw_city_name_widget(200, 60, cityName);

          delete pdata;
        }
      }
      else
      {
        ESP_LOGW("TFT", "Received unsupported queue item type %d", static_cast<int>(qitem.type));
      }
    }

    // Вычислить текущую валидность данных: данные устарели если прошло >= MAX_METEO_VALID_INTERVAL_MS
    bool meteoValid = false;
    if (lastMeteoUpdateTick != 0)
    {
      TickType_t now = xTaskGetTickCount();
      TickType_t diff = (now >= lastMeteoUpdateTick) ? (now - lastMeteoUpdateTick) : portMAX_DELAY;
      meteoValid = (diff < pdMS_TO_TICKS(MAX_METEO_VALID_INTERVAL_MS));
    }

    // Проверить таймауты для комнатного и наружнего датчиков (если не получали > MAX_METEO_VALID_INTERVAL_MS — сбросить valid)
    {
      TickType_t now = xTaskGetTickCount();
      bool sensorsChanged = false;

      if (inSensorValid && lastInSensorTick != 0)
      {
        TickType_t diff = (now >= lastInSensorTick) ? (now - lastInSensorTick) : portMAX_DELAY;
        if (diff >= pdMS_TO_TICKS(MAX_METEO_VALID_INTERVAL_MS))
        {
          inSensorValid = false;
          sensorsChanged = true;
        }
      }

      if (outSensorValid && lastOutSensorTick != 0)
      {
        TickType_t diff = (now >= lastOutSensorTick) ? (now - lastOutSensorTick) : portMAX_DELAY;
        if (diff >= pdMS_TO_TICKS(MAX_METEO_VALID_INTERVAL_MS))
        {
          outSensorValid = false;
          sensorsChanged = true;
        }
      }

      if (sensorsChanged)
      {
        // Перерисовать виджет с текущими флагами валидности
        const uint8_t padding = 6;
        int x = 0;
        int y = MeteoWidgets::getClockDigsH() + padding;
        // Перерисовываем два раздельных виджета: наружный (лево) и внутренний (право)
        meteo_widgets->draw_home_out_data_widget(x, y,
                                                 outSensorData.temperature, static_cast<uint8_t>(outSensorData.humidity),
                                                 outSensorValid);
        meteo_widgets->draw_home_in_data_widget(x, y,
                                                inSensorData.temperature_in, inSensorData.humidity_in,
                                                inSensorValid);
        meteo_widgets->draw_city_name_widget(200, 60, cityName);
      }
    }

    // Определить, нужно ли перерисовать виджеты:
    // 1) Получены новые данные (meteoDataReceived) → отрисовать с valid=true
    // 2) Данные стали невалидными (переход prevMeteoValid → !meteoValid) → отрисовать с valid=false
    bool needRedraw = meteoDataReceived || (prevMeteoValid && !meteoValid);

    if (needRedraw)
    {
      bool validFlag = meteoDataReceived ? true : false; // если получены данные — valid=true, иначе false (stale)

      // Current weather widget
      if (haveMeteo[METEO_DATA_CURRENT])
      {
        OpenMeteoData &d = latestMeteo[METEO_DATA_CURRENT];
        meteo_widgets->draw_meteo_current_widget(MeteoWidgets::getScreenWidth() - MeteoWidgets::getWidgetCurW(), 60,
                                                 d.temperature,
                                                 static_cast<uint8_t>(d.relative_humidity),
                                                 d.wind_speed,
                                                 static_cast<uint16_t>(d.wind_direction),
                                                 static_cast<uint8_t>(d.weather_code), validFlag);
        meteo_widgets->draw_city_name_widget(200, 60, cityName);
      }

      // Forecast widgets (indices 1..3)
      for (int idx = 1; idx < _METEO_DATA_NUM_; ++idx)
      {
        if (haveMeteo[idx])
        {
          OpenMeteoData &d = latestMeteo[idx];
          int col = idx - 1;
          int x = MeteoWidgets::getWidgetForW() * col;
          int y = MeteoWidgets::getScreenHeight() - MeteoWidgets::getWidgetForH();

          // Определить отображаемую дату
          char todayBuf[11];
          snprintf(todayBuf, sizeof(todayBuf), "%02d-%02d-%04d", timeinfo.tm_mday, timeinfo.tm_mon + 1, timeinfo.tm_year + 1900);
          char dateDisplay[16];
          if (d.date[0] && strcmp(d.date, todayBuf) == 0)
            strncpy(dateDisplay, "СЕГОДНЯ", sizeof(dateDisplay) - 1);
          else if (d.date[0])
            strncpy(dateDisplay, d.date, sizeof(dateDisplay) - 1);
          else
            strncpy(dateDisplay, todayBuf, sizeof(dateDisplay) - 1);
          dateDisplay[sizeof(dateDisplay) - 1] = '\0';

          float kp = (idx == METEO_DATA_FORECAST_TODAY)      ? geomag.kpmax_today
                     : (idx == METEO_DATA_FORECAST_TOMORROW) ? geomag.kpmax_tomorrow
                                                             : geomag.kpmax_tomorrow2;

          meteo_widgets->draw_meteo_forecast_widget(x, y,
                                                    d.temperature_min,
                                                    d.temperature_max,
                                                    d.wind_speed,
                                                    static_cast<uint16_t>(d.wind_direction),
                                                    static_cast<uint16_t>(d.precipitation),
                                                    static_cast<uint8_t>(d.weather_code),
                                                    String(dateDisplay), kp, validFlag);
        }
      }
    }

    // Обновить предыдущее состояние valid
    prevMeteoValid = meteoValid;

    vTaskDelayUntil(&xLastWakeTime, 1000 / portTICK_PERIOD_MS); // задержка 1 секунда
  }
}
