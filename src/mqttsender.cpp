#include "mqttsender.h"
#include "cfg_validate.h"
#include "openmeteo.h"
#include "tasks_common.h"
#include <esp_log.h>
#include <esp_system.h>

static const char *TAG = "MQTTSENDER";

// Интервал повторного подключения к MQTT при неудаче (мс)
static const uint32_t MQTT_RECONNECT_INTERVAL_MS = 30000;

namespace
{
// Формирует топик "<user>/<prefix>/<suffix>". Возвращает false, если строка
// не поместилась в буфер: публикация в обрезанный топик хуже, чем её пропуск.
bool format_mqtt_topic(char *buf, size_t buf_size, const PrjCfgData &cfg, const char *suffix)
{
  const int written = snprintf(buf, buf_size, "%s/%s/%s", cfg.mqtt_user, cfg.mqtt_prefix, suffix);
  return written > 0 && static_cast<size_t>(written) < buf_size;
}
} // namespace

// Static callback for incoming MQTT messages
static void mqtt_callback(char *topic, byte *payload, unsigned int length)
{
  ESP_LOGI(TAG, "MQTT msg on %s, len=%u", topic, length);
}

MqttSender::MqttSender(QueueHandle_t (&queue_array_ref)[_PROTASK_NUM_])
    : xQueues(queue_array_ref), wifiClient(), mqttClient(wifiClient), lastConnectAttemptMs(0)
{
  ESP_LOGI(TAG, "MqttSender constructed");
}

MqttSender::~MqttSender()
{
  ESP_LOGI(TAG, "MqttSender destructed");
}

void MqttSender::begin()
{
  // No-op for now; client will be configured on connect
}

bool MqttSender::connect(const PrjCfgData &cfg)
{
  if (strlen(cfg.mqtt_server) == 0)
    return false;

  long port = 0;
  if (!CfgValidate::parseLong(cfg.mqtt_port, 1, 65535, port))
  {
    ESP_LOGE(TAG, "Invalid MQTT port '%s' (expected 1..65535), connect aborted", cfg.mqtt_port);
    return false;
  }

  mqttClient.setServer(cfg.mqtt_server, static_cast<uint16_t>(port));
  mqttClient.setCallback(mqtt_callback);

  String clientId = String("meteo_pub_") + String((uint32_t)esp_random(), HEX);
  if (mqttClient.connect(clientId.c_str(), cfg.mqtt_user, cfg.mqtt_pass))
  {
    ESP_LOGI(TAG, "Connected to MQTT broker %s", cfg.mqtt_server);
    return true;
  }
  else
  {
    ESP_LOGW(TAG, "MQTT connect failed: rc=%d (srv: %s, user: %s, pass: %s)",
             mqttClient.state(), cfg.mqtt_server, cfg.mqtt_user, cfg.mqtt_pass);
    return false;
  }
}

void MqttSender::loop(const PrjCfgData &cfg)
{
  // If not connected, try to connect (non-blocking with interval)
  if (!mqttClient.connected())
  {
    uint32_t nowMs = millis();
    // Handle millis() overflow
    uint32_t elapsed = (nowMs >= lastConnectAttemptMs) ? (nowMs - lastConnectAttemptMs) : (UINT32_MAX - lastConnectAttemptMs + nowMs + 1);

    if (lastConnectAttemptMs == 0 || elapsed >= MQTT_RECONNECT_INTERVAL_MS)
    {
      lastConnectAttemptMs = nowMs;
      if (connect(cfg))
        xEventGroupSetBits(xEventGroup, BIT_MQTT_STATE_UP);
      else
        xEventGroupClearBits(xEventGroup, BIT_MQTT_STATE_UP);
    }
  }
  else
  {
    mqttClient.loop();
  }
}

bool MqttSender::connected()
{
  return mqttClient.connected();
}

bool MqttSender::publish(const char *topic, const String &payload)
{
  if (!mqttClient.connected())
    return false;
  return mqttClient.publish(topic, payload.c_str());
}

void MqttSender::processing(const PrjCfgData &cfg, QueDataItem_t &qitem)
{
  // Payload ownership is transferred to this method: it must be freed even if
  // MQTT is disconnected, otherwise queued sensor data leaks.
  const bool mqttConnected = mqttClient.connected();

  switch (qitem.type)
  {
  case QUE_DATATYPE_IN_SENSOR_DATA:
  {
    HomeSensorData_t *p = static_cast<HomeSensorData_t *>(qitem.data);
    if (p)
    {
      if (mqttConnected)
      {
        // Буфер рассчитан на максимальные значения полей конфигурации:
        // "<user>/<prefix>/in" вместе с завершающим нулём.
        char topic[sizeof(cfg.mqtt_user) + sizeof(cfg.mqtt_prefix) + sizeof("/in")];
        char payload[128];
        snprintf(payload, sizeof(payload), "{\"t\":%.1f,\"p\":%.0f,\"h\":%u}",
                 p->temperature_in, p->pressure_in, p->humidity_in);
        if (!format_mqtt_topic(topic, sizeof(topic), cfg, "in"))
          ESP_LOGE(TAG, "MQTT IN topic too long (user='%s', prefix='%s'), publish skipped",
                   cfg.mqtt_user, cfg.mqtt_prefix);
        else if (!publish(topic, payload))
          ESP_LOGW(TAG, "Failed publish to %s", topic);
        else
          ESP_LOGI(TAG, "Published IN -> %s", topic);
      }
      delete p;
    }
    break;
  }

  case QUE_DATATYPE_OUT_SENSOR_DATA:
  {
    OutSensorData_t *p = static_cast<OutSensorData_t *>(qitem.data);
    if (p)
    {
      if (mqttConnected)
      {
        // Буфер рассчитан на максимальные значения полей конфигурации:
        // "<user>/<prefix>/out" вместе с завершающим нулём.
        char topic[sizeof(cfg.mqtt_user) + sizeof(cfg.mqtt_prefix) + sizeof("/out")];
        char payload[128];
        snprintf(payload, sizeof(payload), "{\"t\":%.1f,\"p\":%u,\"h\":%.0f,\"bat\":%u}",
                 p->temperature, p->pressure, p->humidity, p->bat_charge);
        if (!format_mqtt_topic(topic, sizeof(topic), cfg, "out"))
          ESP_LOGE(TAG, "MQTT OUT topic too long (user='%s', prefix='%s'), publish skipped",
                   cfg.mqtt_user, cfg.mqtt_prefix);
        else if (!publish(topic, payload))
          ESP_LOGW(TAG, "Failed publish to %s", topic);
        else
          ESP_LOGI(TAG, "Published OUT -> %s", topic);
      }
      delete p;
    }
    break;
  }

  // Other queue payload types are not published here, but their memory is
  // still owned by the queue consumer and must be released.
  case QUE_DATATYPE_CITYNAME:
    delete static_cast<String *>(qitem.data);
    break;

  case QUE_DATATYPE_METEO:
    delete static_cast<OpenMeteoData *>(qitem.data);
    break;

  case QUE_DATATYPE_GEOMAGNETIC:
    delete static_cast<GeoMagneticKpMax *>(qitem.data);
    break;

  case QUE_DATATYPE_CFG:
    delete static_cast<PrjCfgData *>(qitem.data);
    break;

  default:
    ESP_LOGW(TAG, "Unknown QueDataType_t %d in MQTT processing", static_cast<int>(qitem.type));
    // Unknown payload type: no safe typed delete is possible here.
    break;
  }

  qitem.data = nullptr;
}
