#include "task_home_sensor.h"
#include "stack_monitor.h"
#include "tasks_common.h"
#include <Adafruit_BME280.h>
#include <esp_log.h>

Adafruit_BME280 bme; // датчик BME280 (температура, давление, влажность, I2C)

void task_home_sensor_exec(void *pvParameters)
{
  (void)pvParameters;
  const char *TAG = "HOME_SENSOR";

  // Попытаться инициализировать BME280 (обычно адрес 0x76 или 0x77)
  bool found = false;
  if (bme.begin(0x76))
    found = true;
  else if (bme.begin(0x77))
    found = true;

  if (!found)
    ESP_LOGW(TAG, "BME280 not found at 0x76/0x77; will retry periodically");

  TickType_t xLastWakeTime = xTaskGetTickCount();
  StackMonitor_t stackMon;
  stack_monitor_init(&stackMon, TAG);

  vTaskDelay(20000 / portTICK_PERIOD_MS); // небольшая задержка перед началом работы

  for (;;)
  {
    // Sample stack high-water mark and handle logging via helper
    stack_monitor_sample(&stackMon, PROTASK_HOME_SENSOR_STACK_SIZE);

    if (!found)
    {
      // попытка повторной инициализации
      if (bme.begin(0x76) || bme.begin(0x77))
      {
        found = true;
        ESP_LOGI(TAG, "BME280 initialized");
      }
      else
      {
        vTaskDelay(pdMS_TO_TICKS(5000));
        continue;
      }
    }

    // Буферы для усреднения последних HOME_SENSOR_AVG_SAMPLES измерений
    static float temp_buf[HOME_SENSOR_AVG_SAMPLES] = {0.0f};
    static float press_buf[HOME_SENSOR_AVG_SAMPLES] = {0.0f};
    static uint8_t hum_buf[HOME_SENSOR_AVG_SAMPLES] = {0};
    static int buf_pos = 0;
    static int buf_count = 0;
    static float sum_temp = 0.0f;
    static float sum_press = 0.0f;
    static int sum_hum = 0;

    // Считывание с BME280 (текущее измерение)
    float cur_t = bme.readTemperature();                              // C
    float cur_p = bme.readPressure() / 100.0F;                        // hPa
    uint8_t cur_h = static_cast<uint8_t>(roundf(bme.readHumidity())); // %

    // Обновление сумм и буфера (циклический буфер длины 6)
    if (buf_count < HOME_SENSOR_AVG_SAMPLES)
    {
      temp_buf[buf_pos] = cur_t;
      press_buf[buf_pos] = cur_p;
      hum_buf[buf_pos] = cur_h;
      sum_temp += cur_t;
      sum_press += cur_p;
      sum_hum += cur_h;
      buf_count++;
    }
    else
    {
      // вычесть старое, записать новое
      sum_temp -= temp_buf[buf_pos];
      sum_press -= press_buf[buf_pos];
      sum_hum -= hum_buf[buf_pos];
      temp_buf[buf_pos] = cur_t;
      press_buf[buf_pos] = cur_p;
      hum_buf[buf_pos] = cur_h;
      sum_temp += cur_t;
      sum_press += cur_p;
      sum_hum += cur_h;
    }
    buf_pos = (buf_pos + 1) % HOME_SENSOR_AVG_SAMPLES;

    // Вычислить усреднённые значения (за buf_count измерений, до 6)
    float avg_t = sum_temp / (buf_count ? buf_count : 1);
    float avg_p = sum_press / (buf_count ? buf_count : 1);
    uint8_t avg_h = static_cast<uint8_t>(roundf(static_cast<float>(sum_hum) / (buf_count ? buf_count : 1)));

    ESP_LOGI(TAG, "BME280 cur T=%.2f C P=%.2f hPa H=%d%% avg(T,P,H) = %.2f C / %.2f hPa / %d%%",
             cur_t, cur_p, cur_h, avg_t, avg_p, avg_h);

    // Формируем payload с усреднёнными значениями и отправляем в очередь
    HomeSensorData_t *payload = new HomeSensorData_t(); // TODO удалить после приема !!!
    if (!payload)
    {
      ESP_LOGE(TAG, "Failed to allocate HomeSensorData_t");
      vTaskDelay(pdMS_TO_TICKS(5000));
      continue;
    }
    payload->temperature_in = avg_t;
    payload->pressure_in = avg_p;
    payload->humidity_in = avg_h;

    QueDataItem_t qitem;
    qitem.type = QUE_DATATYPE_IN_SENSOR_DATA;
    qitem.data = payload;

    if (pdPASS != xQueueSend(xQueue[PROTASK_TFT], &qitem, pdMS_TO_TICKS(100)))
    {
      ESP_LOGE(TAG, "Failed to send averaged home sensor data to TFT queue");
      delete payload;
    }
    else
    {
      // Also send a copy to MQTT publisher queue if present
      HomeSensorData_t *payload_mqtt = new HomeSensorData_t();
      if (payload_mqtt)
      {
        *payload_mqtt = *payload;
        QueDataItem_t qitem_m;
        qitem_m.type = QUE_DATATYPE_IN_SENSOR_DATA;
        qitem_m.data = payload_mqtt;
        if (pdPASS != xQueueSend(xQueue[PROTASK_NETWORKING], &qitem_m, pdMS_TO_TICKS(100)))
        {
          delete payload_mqtt;
        }
      }
    }

    // Интервал опроса 10 секунд
    vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(60000));
  }

  vTaskDelete(NULL);
}
