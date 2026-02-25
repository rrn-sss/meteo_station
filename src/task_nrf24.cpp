#include "task_nrf24.h"
#include "common.h"
#include "stack_monitor.h"
#include "tasks_common.h"
#include <Arduino.h>
#include <RF24.h>
#include <SPI.h>
#include <esp_log.h>

RF24 radio(NRF_CE_PIN, NRF_CSN_PIN); // радиоприемник nRF24L01+

// Временные метрики приёмов: разница, максимум, сумма для среднего и счётчик
static uint32_t nrf_last_recv_ms = 0;
static uint32_t nrf_max_delta_ms = 0;
static uint64_t nrf_sum_deltas_ms = 0;
static uint32_t nrf_recv_count = 0;

void task_nrf24_exec(void *pvParameters)
{
  (void)pvParameters;
  // Task handle not needed for polling mode

  // Initialize SPI for nRF24 and radio
  SPI.begin(NRF_SCK_PIN, NRF_MISO_PIN, NRF_MOSI_PIN, NRF_CSN_PIN);

  if (!radio.begin())
  {
    ESP_LOGE("NRF24", "nRF24L01+ init failed in task, will retry every 5s");
    for (;;)
    {
      vTaskDelay(pdMS_TO_TICKS(5000));
      if (radio.begin())
        break;
    }
  }

  // Diagnostic info: check chip connection and print radio details
  if (!radio.isChipConnected())
    ESP_LOGW("NRF24", "Radio chip not responding (isChipConnected() == false)");
  else
    ESP_LOGI("NRF24", "Radio chip is connected");

  // Configure radio for simple reception
  // Set channel, data rate, power level and address (pipe)
  radio.setChannel(103);                  // channel 103
  radio.setDataRate(RF24_250KBPS);        // 250kbps
  radio.setPALevel(RF24_PA_HIGH);         // high power
  radio.setAutoAck(true);                 // enable auto acknowledgment
  uint64_t pipeAddress = 0xA337D135B1ULL; // expected pipe address
  radio.openReadingPipe(0, pipeAddress);
  radio.startListening();

  radio.printDetails();

  // Polling mode: no hardware IRQs or ISR installed

  StackMonitor_t stackMon;

  ESP_LOGI("NRF24", "nRF24 receiver task started, waiting for data...");

  stack_monitor_init(&stackMon, "NRF24");
  for (;;)
  {
    stack_monitor_sample(&stackMon, PROTASK_NRF_RECEIVER_STACK_SIZE);

    if (radio.available())
    {
      // Read all available payloads and update last-received time
      while (radio.available())
      {
        OutSensorData_t data{};
        size_t len = sizeof(data);
        radio.read(&data, len);
        // Время приёма и статистика интервалов
        uint32_t now_ms = millis();
        uint32_t delta_ms = 0;
        if (nrf_last_recv_ms != 0)
        {
          if (now_ms >= nrf_last_recv_ms)
            delta_ms = now_ms - nrf_last_recv_ms;
          else
            delta_ms = (UINT32_MAX - nrf_last_recv_ms) + now_ms + 1; // millis() wrap-around safety

          nrf_sum_deltas_ms += delta_ms;
          nrf_recv_count++;
          if (delta_ms > nrf_max_delta_ms)
            nrf_max_delta_ms = delta_ms;
        }
        nrf_last_recv_ms = now_ms;

        float avg_ms = nrf_recv_count ? (float)nrf_sum_deltas_ms / nrf_recv_count : 0.0f;

        ESP_LOGI("NRF24", "Received OutSensorData: T=%.2f, H=%.2f, P=%u, BAT=%u",
                 data.temperature, data.humidity, data.pressure, data.bat_charge);
        ESP_LOGI("NRF24", "Inter-receive: %ums, max=%ums, avg=%.2fms",
                 delta_ms, nrf_max_delta_ms, avg_ms);

        // Send received data to TFT task queue for drawing
        OutSensorData_t *payload = new OutSensorData_t();
        if (!payload)
        {
          ESP_LOGE("NRF24", "Failed to allocate OutSensorData_t for TFT queue");
        }
        else
        {
          *payload = data;
          QueDataItem_t qitem;
          qitem.type = QUE_DATATYPE_OUT_SENSOR_DATA;
          qitem.data = payload;
          if (pdPASS != xQueueSend(xQueue[PROTASK_TFT], &qitem, pdMS_TO_TICKS(100)))
          {
            ESP_LOGE("NRF24", "Failed to send OutSensorData to TFT queue");
            delete payload;
          }
          else
          {
            // Also send a copy to MQTT publisher queue
            OutSensorData_t *payload_mqtt = new OutSensorData_t();
            if (payload_mqtt)
            {
              *payload_mqtt = *payload;
              QueDataItem_t qitem_m;
              qitem_m.type = QUE_DATATYPE_OUT_SENSOR_DATA;
              qitem_m.data = payload_mqtt;
              if (pdPASS != xQueueSend(xQueue[PROTASK_NETWORKING], &qitem_m, pdMS_TO_TICKS(100)))
              {
                delete payload_mqtt;
              }
            }
          }
        }
      }
    }

    if (!radio.isChipConnected())
      ESP_LOGW("NRF24", "Radio chip not responding (isChipConnected() == false)");
    // Wait 5 seconds before next poll
    vTaskDelay(pdMS_TO_TICKS(5000));
  }
}
