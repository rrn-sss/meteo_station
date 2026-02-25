#ifndef _OPENMETEO_H_
#define _OPENMETEO_H_

#include "common.h"

// индексы массива данных погоды
enum OpenMeteoDataIndex_t
{
  METEO_DATA_CURRENT = 0,                // текущее состояние погоды
  METEO_DATA_FORECAST_TODAY = 1,         // прогноз на сегодня
  METEO_DATA_FORECAST_TOMORROW = 2,      // прогноз на завтра
  METEO_DATA_FORECAST_AFTERTOMORROW = 3, // прогноз на послезавтра
  _METEO_DATA_NUM_ = 4                   // количество записей
};

// структура данных погоды (от сервиса Open-Meteo)
struct OpenMeteoData
{
  OpenMeteoDataIndex_t index;  // индекс записи (текущее состояние или прогноз на день)
  float temperature{0.0f};     // температура в  градусах Цельсия
  int pressure{0};             // давление в hPa
  int relative_humidity{0};    // влажность в %
  float temperature_max{0.0f}; // максимальная температура в  градусах Цельсия
  float temperature_min{0.0f}; // минимальная температура в  градусах Цельсия
  float precipitation{0.0f};   // осадки в мм
  float wind_speed{0.0f};      // скорость ветра в м/с
  int wind_direction{0};       // направление ветра в градусах
  int weather_code{0};         // WMO код погоды
  char date[11] = {0};         // дата в формате DD-MM-YYYY (10 chars + NUL)
};

// структура данных прогноза геомагнитной обстановки на 3 дня
struct GeoMagneticKpMax
{
  float kpmax_today;     // максимальное значение индекса на сегодня
  float kpmax_tomorrow;  // максимальное значение индекса на завтра
  float kpmax_tomorrow2; // максимальное значение индекса на послезавтра
};

// класс обработчик метео-информации
// process_meteo_data() - выполн¤ет HTTP GET запрос, получает ответ с метео-сводкой и отправл¤ет ее в заданную очередь (TODO fix)
/**
 * @brief Класс обработчик метео-информации от сервиса Open-Meteo
 *
 * Выполняет HTTP GET запросы к Open-Meteo, парсит ответ и отправляет
 * подготовленные данные в очередь одной из задач проекта.
 */
class OpenMeteo
{
public:
  /**
   * @brief Конструктор OpenMeteo скрыт (используйте createInstance)
   */

  // Public constructor/destructor
  OpenMeteo(const String &latitude, const String &longitude, ProjectTasks_t send_to, QueueHandle_t (&queue_array_ref)[_PROTASK_NUM_]);
  ~OpenMeteo();

  /**
   * @brief Выполнить получение и обработку метео-данных
   *
   * Делает HTTP GET запрос, парсит JSON и формирует массив `OpenMeteoData`.
   * Затем помещает указатель на массив в очередь задачи `task_to_send`.
   *
   * @return true при успешной отправке данных в очередь, иначе false.
   */
  bool process_meteo_data();

  /**
   * @brief Выполнить получение и обработку геомагнитных данных
   *
   * Делает HTTP GET запрос, парсит TXT и формирует массив `GeoMagneticData`.
   * Затем помещает указатель на массив в очередь задачи `task_to_send`.
   *
   * @return true при успешной отправке данных в очередь, иначе false.
   */
  bool process_geomagnetic_data();

  /**
   * @brief Обновить конфигурацию координат для запросов
   *
   * Заменяет внутренние строки `lat` и `lon` на новые значения.
   *
   * @param latitude Новая широта.
   * @param longitude Новая долгота.
   */
  void set_config(const String &latitude, const String &longitude);

  /**
   * @brief Получить наименование ближайшего населённого пункта (город/посёлок) для заданных координат
   *
   * Выполняет HTTP GET запрос к сервису обратного геокодирования (Nominatim OpenStreetMap)
   * с параметром `accept-language=ru` и парсит ответ JSON, возвращая наименование на русском языке
   * если оно присутствует в ответе.
   *
   * @param latitude широта в градусах (например 55.7558)
   * @param longitude долгота в градусах (например 37.6173)
   * @return `String` с названием населённого пункта на русском или пустая строка при ошибке
   */
  String getNearestCityName(double latitude, double longitude);

private:
  // запрет копирования/перемещения
  OpenMeteo(const OpenMeteo &) = delete;
  OpenMeteo &operator=(const OpenMeteo &) = delete;
  OpenMeteo(OpenMeteo &&) = delete;
  OpenMeteo &operator=(OpenMeteo &&) = delete;

  // Конструктор публичный
  // OpenMeteo(const String &latitude, const String &longitude, ProjectTasks_t send_to, QueueHandle_t (&queue_array_ref)[_PROTASK_NUM_]);
  /** @brief Широта (latitude) для запросов Open-Meteo */
  String lat;

  /** @brief Долгота (longitude) для запросов Open-Meteo */
  String lon;

  /** @brief К какому таску (индексу очереди) отправлять данные */
  ProjectTasks_t task_to_send;

  /** @brief Ссылка на массив очередей, в которые будут отправляться элементы с метео-данными */
  QueueHandle_t (&xQueues)[_PROTASK_NUM_];
};

#endif // _OPENMETEO_H_