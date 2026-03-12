#include "openmeteo.h"
#include "certs.h"
#include "http_helpers.h"
#include "netprocessor.h"
#include <ArduinoJson.h>
#include <WiFi.h>
#include <esp_log.h>
#include <type_traits>

#define TAG "WEATHER"

// Алиасы для обратной совместимости
const char *ca_amazon_root = CERT_AMAZON_ROOT_CA1;
const char *isrg_ca = CERT_ISRG_ROOT_X1;

OpenMeteo::OpenMeteo(const String &latitude, const String &longitude,
                     ProjectTasks_t send_to, QueueHandle_t (&queue_array_ref)[_PROTASK_NUM_])
    : lat(latitude), lon(longitude), task_to_send(send_to), xQueues(queue_array_ref)
{
}

OpenMeteo::~OpenMeteo()
{
  // nothing special to cleanup
}

bool OpenMeteo::process_meteo_data()
{
  bool ret = false;
  bool http_error = false;
  size_t sent_cnt = 0; // счетчик успешно отправленных записей
  JsonDocument json;
  auto deserializeError = deserializeJson(json, "{}"); // инициализация пустым JSON

  if (1) // для экономии стека
  {
#if 0
    String serverPath = "http://api.open-meteo.com/v1/forecast?latitude=" + lat + "&longitude=" + lon + "&current=weather_code,temperature_2m,precipitation,wind_speed_10m,wind_direction_10m,relative_humidity_2m,surface_pressure&daily=temperature_2m_max,temperature_2m_min,precipitation_sum,precipitation_probability_max,wind_speed_10m_max,wind_direction_10m_dominant,weather_code&forecast_days=3&wind_speed_unit=ms&timezone=Europe/Moscow&models=icon_seamless";
    tring weather_str = send_HTTP_GET_request(serverPath.c_str());
#else
    String serverPath = "https://api.open-meteo.com/v1/forecast?latitude=" + lat + "&longitude=" + lon + "&current=weather_code,temperature_2m,precipitation,wind_speed_10m,wind_direction_10m,relative_humidity_2m,surface_pressure&daily=temperature_2m_max,temperature_2m_min,precipitation_sum,precipitation_probability_max,wind_speed_10m_max,wind_direction_10m_dominant,weather_code&forecast_days=3&wind_speed_unit=ms&timezone=Europe/Moscow&models=icon_seamless";
    String weather_str = "{}";
    weather_str = send_HTTPS_GET_request(serverPath.c_str(), isrg_ca);
#endif
    http_error = weather_str.equals("{}");
    deserializeError = deserializeJson(json, weather_str);
  }

  if (!deserializeError && !http_error)
  {
    // Use a function-local static buffer to avoid heap fragmentation
    static OpenMeteoData tmp_buf[_METEO_DATA_NUM_];
    OpenMeteoData *tmp = tmp_buf;
    // Fill temporary array while JSON still available
    for (auto i = 0; i < _METEO_DATA_NUM_; ++i)
    {
      if (i == METEO_DATA_CURRENT)
      {
        tmp[i].index = METEO_DATA_CURRENT;
        tmp[i].weather_code = json["current"]["weather_code"].as<int>();
        tmp[i].temperature = json["current"]["temperature_2m"].as<float>();
        tmp[i].pressure = json["current"]["surface_pressure"].as<int>();
        tmp[i].relative_humidity = json["current"]["relative_humidity_2m"].as<int>();
        tmp[i].wind_speed = json["current"]["wind_speed_10m"].as<float>();
        tmp[i].wind_direction = json["current"]["wind_direction_10m"].as<int>();

        ESP_LOGI(TAG, "Tmp CURRENT: index=%d", tmp[i].index);
        ESP_LOGI(TAG, "Tmp CURRENT: weather_code=%d", tmp[i].weather_code);
        ESP_LOGI(TAG, "Tmp CURRENT: temperature=%.2f", tmp[i].temperature);
        ESP_LOGI(TAG, "Tmp CURRENT: pressure=%d", tmp[i].pressure);
        ESP_LOGI(TAG, "Tmp CURRENT: relative_humidity=%d", tmp[i].relative_humidity);
        ESP_LOGI(TAG, "Tmp CURRENT: temperature_max=%.2f", tmp[i].temperature_max);
        ESP_LOGI(TAG, "Tmp CURRENT: temperature_min=%.2f", tmp[i].temperature_min);
        ESP_LOGI(TAG, "Tmp CURRENT: precipitation=%.2f", tmp[i].precipitation);
        ESP_LOGI(TAG, "Tmp CURRENT: wind_speed=%.2f", tmp[i].wind_speed);
        ESP_LOGI(TAG, "Tmp CURRENT: wind_direction=%d", tmp[i].wind_direction);
        ESP_LOGI(TAG, "Tmp CURRENT: date=%s", (tmp[i].date[0] ? tmp[i].date : "N/A"));
      }
      else
      {
        tmp[i].index = static_cast<OpenMeteoDataIndex_t>(i);
        tmp[i].weather_code = json["daily"]["weather_code"][i - 1].as<int>();
        tmp[i].temperature = json["daily"]["temperature_2m_max"][i - 1].as<float>();
        tmp[i].temperature_max = json["daily"]["temperature_2m_max"][i - 1].as<float>();
        tmp[i].temperature_min = json["daily"]["temperature_2m_min"][i - 1].as<float>();
        tmp[i].precipitation = json["daily"]["precipitation_sum"][i - 1].as<float>();
        tmp[i].wind_speed = json["daily"]["wind_speed_10m_max"][i - 1].as<float>();
        tmp[i].wind_direction = json["daily"]["wind_direction_10m_dominant"][i - 1].as<int>();
        const char *srcDate = json["daily"]["time"][i - 1].as<const char *>();
        if (srcDate && strlen(srcDate) >= 10)
        {
          char dateChars[11] = {0};
          int year = atoi(srcDate);
          int month = atoi(srcDate + 5);
          int day = atoi(srcDate + 8);
          snprintf(dateChars, sizeof(dateChars), "%02d-%02d-%04d", day, month, year);
          strncpy(tmp[i].date, dateChars, sizeof(tmp[i].date) - 1);
          tmp[i].date[sizeof(tmp[i].date) - 1] = '\0';
        }
        else
        {
          tmp[i].date[0] = '\0';
        }

        ESP_LOGI(TAG, "Tmp DAILY[%d]: index=%d", static_cast<int>(tmp[i].index), static_cast<int>(tmp[i].index));
        ESP_LOGI(TAG, "Tmp DAILY[%d]: date=%s", static_cast<int>(tmp[i].index), (tmp[i].date[0] ? tmp[i].date : "N/A"));
        ESP_LOGI(TAG, "Tmp DAILY[%d]: weather_code=%d", static_cast<int>(tmp[i].index), tmp[i].weather_code);
        ESP_LOGI(TAG, "Tmp DAILY[%d]: temperature=%.2f", static_cast<int>(tmp[i].index), tmp[i].temperature);
        ESP_LOGI(TAG, "Tmp DAILY[%d]: temperature_max=%.2f", static_cast<int>(tmp[i].index), tmp[i].temperature_max);
        ESP_LOGI(TAG, "Tmp DAILY[%d]: temperature_min=%.2f", static_cast<int>(tmp[i].index), tmp[i].temperature_min);
        ESP_LOGI(TAG, "Tmp DAILY[%d]: precipitation=%.2f", static_cast<int>(tmp[i].index), tmp[i].precipitation);
        ESP_LOGI(TAG, "Tmp DAILY[%d]: wind_speed=%.2f", static_cast<int>(tmp[i].index), tmp[i].wind_speed);
        ESP_LOGI(TAG, "Tmp DAILY[%d]: wind_direction=%d", static_cast<int>(tmp[i].index), tmp[i].wind_direction);
        ESP_LOGI(TAG, "Tmp DAILY[%d]: pressure=%d", static_cast<int>(tmp[i].index), tmp[i].pressure);
        ESP_LOGI(TAG, "Tmp DAILY[%d]: relative_humidity=%d", static_cast<int>(tmp[i].index), tmp[i].relative_humidity);
      }
    }

    // Free JSON resources before sending to queue to reduce memory usage
    json.clear();

    // Now send items to the queue — create individual heap copies for the receiver
    for (auto i = 0; i < _METEO_DATA_NUM_; ++i)
    {
      OpenMeteoData *payload = new (std::nothrow) OpenMeteoData;
      if (!payload)
      {
        ESP_LOGE(TAG, "Failed to allocate OpenMeteoData for queue item %d", i);
        continue;
      }

      *payload = tmp[i];

      QueDataItem_t qitem;
      qitem.type = QUE_DATATYPE_METEO;
      qitem.data = payload;

      // ESP_LOGI(TAG, "   Heap largest free block before send: %d", heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));

      if (pdPASS == xQueueSend(this->xQueues[task_to_send], &qitem, pdMS_TO_TICKS(100)))
      {
        ++sent_cnt;
        ESP_LOGI(TAG, "Meteo data has been sent to queue [task %d]...", task_to_send);
      }
      else
      {
        ESP_LOGE(TAG, "Failed to add meteo data to queue [task %d]!", task_to_send);
        delete payload;
      }
    }
  }
  else
  {
    if (http_error)
      ESP_LOGE(TAG, "HTTP error when getting OpenMeteo data");
    else
      ESP_LOGE(TAG, "Failed to parse OpenMeteo JSON response: %s", deserializeError.c_str());
  }

  return (_METEO_DATA_NUM_ == sent_cnt) ? true : false;
}

GeoMagneticKpMax parseMaxKpForecast(const String &txt)
{
  GeoMagneticKpMax result = {0.0, 0.0, 0.0};

  // Найти начало таблицы
  int pos = txt.indexOf("NOAA Kp index forecast");
  if (pos == -1)
    return result;

  // Перейти к первой строке с данными (пропускаем 2 строки заголовка)
  pos = txt.indexOf('\n', pos);
  pos = txt.indexOf('\n', pos + 1);

  for (int i = 0; i < 8; i++)
  {
    int lineEnd = txt.indexOf('\n', pos);
    if (lineEnd == -1)
      break;

    String line = txt.substring(pos, lineEnd);
    pos = lineEnd + 1;

    // Пропустить пустые строки
    line.trim();
    if (line.length() == 0)
    {
      i--;
      continue;
    }

    // Удаляем интервал времени (первые 8 символов "00-03UT")
    if (line.length() < 10)
      continue;
    line = line.substring(8);
    line.trim();

    // Парсинг трёх чисел
    int s1 = line.indexOf(' ');
    if (s1 == -1)
      continue;

    float v1 = line.substring(0, s1).toFloat();
    line = line.substring(s1);
    line.trim();

    int s2 = line.indexOf(' ');
    if (s2 == -1)
      continue;

    float v2 = line.substring(0, s2).toFloat();
    float v3 = line.substring(s2).toFloat();

    // Обновление максимумов
    if (v1 > result.kpmax_today)
      result.kpmax_today = v1;
    if (v2 > result.kpmax_tomorrow)
      result.kpmax_tomorrow = v2;
    if (v3 > result.kpmax_tomorrow2)
      result.kpmax_tomorrow2 = v3;
  }

  return result;
}

bool OpenMeteo::process_geomagnetic_data()
{
  bool ret = false;
  bool http_error = false;
  GeoMagneticKpMax kp;

  if (1) // для экономии стека
  {
    String serverPath = "https://services.swpc.noaa.gov/text/3-day-geomag-forecast.txt";
    String geomag_str = send_HTTPS_GET_request(serverPath.c_str(), ca_amazon_root);
    http_error = geomag_str.equals("{}");
    ESP_LOGI(TAG, "%s", geomag_str.c_str());
    kp = parseMaxKpForecast(geomag_str);
  }

  if (!http_error)
  {
    GeoMagneticKpMax *payload = new GeoMagneticKpMax(); // !!! TODO удалить после обработки в получателе

    if (payload)
    {
      ESP_LOGI(TAG, "Kp1: %f Kp2: %f Kp3: %f", kp.kpmax_today, kp.kpmax_tomorrow, kp.kpmax_tomorrow2);
      ESP_LOGI(TAG, "Max free heap block after geomag fetch: %d", heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));

      payload->kpmax_today = kp.kpmax_today;
      payload->kpmax_tomorrow = kp.kpmax_tomorrow;
      payload->kpmax_tomorrow2 = kp.kpmax_tomorrow2;

      QueDataItem_t qitem;
      qitem.type = QUE_DATATYPE_GEOMAGNETIC;
      qitem.data = payload;

      if (pdPASS == xQueueSend(this->xQueues[PROTASK_TFT], &qitem, pdMS_TO_TICKS(100)))
      {
        ESP_LOGI(TAG, "Geomagnetic data has been sent to queue [task %d]...", PROTASK_TFT);
        ret = true;
      }
      else
      {
        ESP_LOGE(TAG, "Failed to add geomagnetic data to queue [task %d]!", PROTASK_TFT);
        delete payload;
      }
    }
    else
      ESP_LOGE(TAG, "Failed to allocate GeoMagneticKpMax");
  }
  else
    ESP_LOGE(TAG, "HTTP error when getting geomagnetic data");

  return ret;
}

void OpenMeteo::set_config(const String &latitude, const String &longitude)
{
  lat = latitude;
  lon = longitude;
  ESP_LOGI(TAG, "OpenMeteo config updated: lat=%s lon=%s", lat.c_str(), lon.c_str());
}

// HTTP helpers moved to NetProcessor; OpenMeteo will use gNetProcessor to perform requests.

String OpenMeteo::getNearestCityName(double latitude, double longitude)
{
  // Используем Nominatim reverse geocoding с accept-language=ru
  String serverPath = "https://nominatim.openstreetmap.org/reverse?format=json&addressdetails=1&accept-language=ru&lat=" + String(latitude, 6) + "&lon=" + String(longitude, 6) + "&zoom=10";

  // Nominatim requires a valid User-Agent identifying the application and a contact.
  const String nominatimUserAgent = "meteo_station/1.0 (tag78@tutamail.com)";
  String resp = "{}";
  resp = send_HTTPS_GET_request(serverPath.c_str(), isrg_ca, nominatimUserAgent);
  if (resp.equals("{}") || resp.length() == 0)
    return String("");

  JsonDocument doc;
  auto err = deserializeJson(doc, resp);
  if (err)
  {
    ESP_LOGW("WEATHER", "Failed to parse reverse geocode JSON: %s", err.c_str());
    return String("");
  }

  // address can contain city, town, village, municipality, county, state
  JsonObject address = doc["address"].as<JsonObject>();
  const char *name = nullptr;
  if (!address["city"].isNull())
    name = address["city"].as<const char *>();
  else if (!address["town"].isNull())
    name = address["town"].as<const char *>();
  else if (!address["village"].isNull())
    name = address["village"].as<const char *>();
  else if (!address["municipality"].isNull())
    name = address["municipality"].as<const char *>();
  else if (!address["county"].isNull())
    name = address["county"].as<const char *>();
  else if (!address["state"].isNull())
    name = address["state"].as<const char *>();

  if (name && strlen(name) > 0)
    return String(name);

  // fallback to display_name (take first comma-separated part)
  const char *display = doc["display_name"];
  if (display && strlen(display) > 0)
  {
    String dn(display);
    int p = dn.indexOf(',');
    if (p != -1)
      return dn.substring(0, p);
    return dn;
  }

  return String("");
}

// HTTPS helper moved to NetProcessor: use gNetProcessor->send_HTTPS_GET_request(...)
