#ifndef _NETPROCESSOR_H_
#define _NETPROCESSOR_H_

#include "mqttsender.h"
#include "openmeteo.h"
#include "webportal.h"

/**
 * @brief Класс обработки сетевых операций и вспомогательных HTTP/HTTPS запросов
 *
 * NetProcessor инкапсулирует функциональность, связанную с сетью: менеджер WiFi,
 * веб-портал конфигурации, клиент OpenMeteo и отправка данных в удалённые сервисы.
 * Он также предоставляет утилиты для выполнения HTTP/HTTPS GET запросов и вспомогательные
 * методы для взаимодействия с внешними API (например, NarodMon).
 */
class NetProcessor
{
public:
  /**
   * @brief Конструктор NetProcessor
   * @param queue_array_ref Ссылка на массив очередей проекта
   * @param latitude Широта по умолчанию (строка)
   * @param longitude Долгота по умолчанию (строка)
   */
  NetProcessor(QueueHandle_t (&queue_array_ref)[_PROTASK_NUM_],
               const String &latitude = "55.7558", const String &longitude = "37.6173");
  ~NetProcessor();
  /** @brief Ссылка на массив очередей проекта */
  QueueHandle_t (&xQueues)[_PROTASK_NUM_];
  WiFiManager wm;        // WiFiManager instance (no dynamic allocation)
  WebConfig webConfig;   // WebConfig instance (constructed with wm)
  OpenMeteo openMeteo;   // OpenMeteo instance owned by NetProcessor (no heap)
  MqttSender mqttSender; // MqttSender instance owned by NetProcessor (no heap)
  /**
   * @brief Выполнить обратное геокодирование и отправить название ближайшего города в очередь TFT
   *
   * Метод читает координаты из конфигурации, запрашивает имя населённого пункта у сервиса
   * обратного геокодирования и отправляет результат (новый выделенный `String*`) в очередь
   * задачи `PROTASK_TFT`.
   */
  void NearestCityProcessing();
  bool configureNTPFromConfig();
  /**
   * @brief Отправить данные наружнего датчика на сервис NarodMon
   * @param data Ссылка на структуру `OutSensorData_t` с последними значениями сенсора
   * @return true в случае успешной отправки/получения ответа, false при ошибке
   */
  bool sendNarodMon(const OutSensorData_t &data);

  /**
   * @brief Выполнить простой HTTP GET и вернуть тело ответа как `String`
   * @param serverName URL с протоколом и путем
   * @return Тело ответа в виде `String` или "{}" при ошибке
   */
  // network helpers moved to http_helpers.h/cpp (free functions)

private:
  // запрет копирования/перемещения
  NetProcessor(const NetProcessor &) = delete;
  NetProcessor &operator=(const NetProcessor &) = delete;
  NetProcessor(NetProcessor &&) = delete;
  NetProcessor &operator=(NetProcessor &&) = delete;
};

#endif // _NETPROCESSOR_H_
