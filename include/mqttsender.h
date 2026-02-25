#ifndef _MQTTSENDER_H_
#define _MQTTSENDER_H_

#include "common.h"
#include <PubSubClient.h>
#include <WiFi.h>

class MqttSender
{
public:
  MqttSender(QueueHandle_t (&queue_array_ref)[_PROTASK_NUM_]);
  ~MqttSender();

  void begin();
  bool connect(const PrjCfgData &cfg);
  void loop(const PrjCfgData &cfg);
  bool connected();
  bool publish(const char *topic, const String &payload);
  void processing(const PrjCfgData &cfg, QueDataItem_t &qitem);

private:
  // Ссылка на массив очередей проекта
  QueueHandle_t (&xQueues)[_PROTASK_NUM_];

  // Network client and MQTT client
  WiFiClient wifiClient;
  PubSubClient mqttClient;

  // запрет копирования/перемещения
  MqttSender(const MqttSender &) = delete;
  MqttSender &operator=(const MqttSender &) = delete;
  MqttSender(MqttSender &&) = delete;
  MqttSender &operator=(MqttSender &&) = delete;
};

#endif // _MQTTSENDER_H_
