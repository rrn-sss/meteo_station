#include "mqttsender.h"
#include "tasks_common.h"
#include <esp_log.h>
#include <esp_system.h>

static const char *TAG = "MQTTSENDER";

// Static callback for incoming MQTT messages
static void mqtt_callback(char *topic, byte *payload, unsigned int length)
{
  ESP_LOGI(TAG, "MQTT msg on %s, len=%u", topic, length);
}

MqttSender::MqttSender(QueueHandle_t (&queue_array_ref)[_PROTASK_NUM_])
    : xQueues(queue_array_ref), wifiClient(), mqttClient(wifiClient)
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

  mqttClient.setServer(cfg.mqtt_server, atoi(cfg.mqtt_port));
  mqttClient.setCallback(mqtt_callback);

  String clientId = String("meteo_pub_") + String((uint32_t)esp_random(), HEX);
  if (mqttClient.connect(clientId.c_str(), cfg.mqtt_user, cfg.mqtt_pass))
  {
    ESP_LOGI(TAG, "Connected to MQTT broker %s", cfg.mqtt_server);
    return true;
  }
  else
  {
    ESP_LOGW(TAG, "MQTT connect failed: rc=%d", mqttClient.state());
    return false;
  }
}

void MqttSender::loop(const PrjCfgData &cfg)
{
  // If not connected, try to connect and update the global event bit
  if (!mqttClient.connected())
  {
    if (connect(cfg))
      xEventGroupSetBits(xEventGroup, BIT_MQTT_STATE_UP);
    else
      xEventGroupClearBits(xEventGroup, BIT_MQTT_STATE_UP);
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
  if (mqttClient.connected())
  {
    if (qitem.type == QUE_DATATYPE_IN_SENSOR_DATA)
    {
      HomeSensorData_t *p = static_cast<HomeSensorData_t *>(qitem.data);
      if (p)
      {
        char topic[64];
        char payload[128];
        snprintf(topic, sizeof(topic), "%s/in", cfg.mqtt_prefix);
        snprintf(payload, sizeof(payload), "{\"t\":%.1f,\"p\":%.0f,\"h\":%u}",
                 p->temperature_in, p->pressure_in, p->humidity_in);
        if (!publish(topic, payload))
          ESP_LOGW(TAG, "Failed publish to %s", topic);
        else
          ESP_LOGI(TAG, "Published IN -> %s", topic);

        delete p; // free memory after publishing
      }
    }
    else if (qitem.type == QUE_DATATYPE_OUT_SENSOR_DATA)
    {
      OutSensorData_t *p = static_cast<OutSensorData_t *>(qitem.data);
      if (p)
      {
        char topic[64];
        char payload[128];
        snprintf(topic, sizeof(topic), "%s/out", cfg.mqtt_prefix);
        snprintf(payload, sizeof(payload), "{\"t\":%.1f,\"p\":%u,\"h\":%.0f,\"bat\":%u}",
                 p->temperature, p->pressure, p->humidity, p->bat_charge);
        if (!publish(topic, payload))
          ESP_LOGW(TAG, "Failed publish to %s", topic);
        else
          ESP_LOGI(TAG, "Published OUT -> %s", topic);

        delete p; // free memory after publishing
      }
    }
    else
    {
      ESP_LOGW(TAG, "Unknown QueDataType_t %d in MQTT processing", static_cast<int>(qitem.type));
      // Unknown data type; just free the data pointer
      if (qitem.data)
        delete static_cast<uint8_t *>(qitem.data);
    }
  }
}
