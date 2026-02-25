#include <Adafruit_BME280.h>
#include <LittleFS.h>
#include <LovyanGFX.hpp>
#include <RF24.h>
#include <SPI.h>
#include <WiFi.h>
#include <Wire.h>
#include <esp_log.h>
#include <stdio.h>
#include <time.h>

#include "common.h"
#include "meteowidgets.h"
#include "netprocessor.h"
#include "openmeteo.h"
#include "webportal.h"

#include "task_home_sensor.h"
#include "task_networking.h"
#include "task_nrf24.h"
#include "task_ota.h"
#include "task_tft.h"

// Task buffers/stacks are allocated statically via xTaskCreateStatic
// Дескрипторы задач
TaskHandle_t xHandles[_PROTASK_NUM_];
// Статические буферы задач и стеки (создаются статически для xTaskCreateStatic)
StaticTask_t xTaskBuffer[_PROTASK_NUM_];
// StackType_t xTaskStack[_PROTASK_NUM_][STACK_SIZE];
StackType_t xTaskStack_PROTASK_TFT[PROTASK_TFT_STACK_SIZE];
StackType_t xTaskStack_PROTASK_NETWORKING[PROTASK_NETWORKING_STACK_SIZE];
StackType_t xTaskStack_PROTASK_HOME_SENSOR[PROTASK_HOME_SENSOR_STACK_SIZE];
StackType_t xTaskStack_PROTASK_NRF_RECEIVER[PROTASK_NRF_RECEIVER_STACK_SIZE];
StackType_t xTaskStack_PROTASK_OTA[PROTASK_OTA_STACK_SIZE];
// Дескрипторы очередей данных
QueueHandle_t xQueue[_PROTASK_NUM_];
// буфер очереди данных
StaticQueue_t xQueueBuffer[_PROTASK_NUM_];
// хранилище очереди
uint8_t xQueueStorage[_PROTASK_NUM_][DATA_QUEUE_SIZE * DATA_QUEUE_ITEM_SIZE];

// Статический буфер и дескриптор группы событий состояния системы
StaticEventGroup_t xEventGroupBuffer;
EventGroupHandle_t xEventGroup = NULL;

// Мьютекс для синхронизации доступа к LittleFS между задачами
SemaphoreHandle_t xLittleFSMutex = NULL;

void setup(void)
{
  Serial.begin(115200);
  esp_log_level_set("*", ESP_LOG_INFO);
  ESP_LOGI("MAIN", "Initialization...");

  ESP_LOGI("MAIN", "Mount LittleFS...");
  if (!LittleFS.begin())
  {
    ESP_LOGE("MAIN", "LittleFS Mount Failed, restart ESP...");
    delay(3000);
    ESP.restart();
  }

  // Инициализация I2C шины
  if (!Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN))
  {
    ESP_LOGE("MAIN", "I2C initialization failed, restart ESP...");
    delay(3000);
    ESP.restart();
  }

  // Создание мьютекса для синхронизации доступа к LittleFS
  xLittleFSMutex = xSemaphoreCreateMutex();
  if (!xLittleFSMutex)
  {
    ESP_LOGE("MAIN", "LittleFS mutex is not created, restart ESP...");
    delay(3000);
    ESP.restart();
  }

  // Создание группы событий для отслеживания состояния системы
  ESP_LOGI("MAIN", "Create event group...");
  xEventGroup = xEventGroupCreateStatic(&xEventGroupBuffer);
  if (!xEventGroup)
  {
    ESP_LOGE("MAIN", "Event group is not created, restart ESP...");
    delay(5000);
    ESP.restart();
  }

  // создание очередей (по одной для каждой задачи) — ДО создания задач!
  ESP_LOGI("MAIN", "Create queues ...");
  for (auto i = 0; i < _PROTASK_NUM_; i++)
  {
    xQueue[i] = xQueueCreateStatic(DATA_QUEUE_SIZE, DATA_QUEUE_ITEM_SIZE, &xQueueStorage[i][0], &xQueueBuffer[i]);
    if (!xQueue[i])
    {
      ESP_LOGE("MAIN", "Queue %d is not created, restart ESP...", i);
      delay(5000);
      ESP.restart();
    }
  }

  ESP_LOGI("MAIN", "Create tasks ...");
  // создание задачи обновления часов на TFT (статическая инициализация)
  xHandles[PROTASK_TFT] = xTaskCreateStatic(
      task_tft_exec,            // функция задачи
      "TFT",                    // имя задачи
      PROTASK_TFT_STACK_SIZE,   // размер стека задачи
      nullptr,                  // параметр задачи
      tskIDLE_PRIORITY + 1,     // приоритет задачи
      xTaskStack_PROTASK_TFT,   // стек (статический буфер)
      &xTaskBuffer[PROTASK_TFT] // структура задачи
  );
  if (xHandles[PROTASK_TFT] == NULL)
  {
    ESP_LOGE("MAIN", "TFT Task is not created, restart ESP...");
    delay(5000);
    ESP.restart();
  }

  // создание задачи работы с сетью (WiFi, MQTT и т.п.) — динамически
  // создание задачи работы с сетью (WiFi, MQTT и т.п.) — статически
  xHandles[PROTASK_NETWORKING] = xTaskCreateStatic(
      task_networking_exec,
      "NETWORKING",
      PROTASK_NETWORKING_STACK_SIZE,
      0,
      tskIDLE_PRIORITY + 1,
      xTaskStack_PROTASK_NETWORKING,
      &xTaskBuffer[PROTASK_NETWORKING]);
  if (xHandles[PROTASK_NETWORKING] == NULL)
  {
    ESP_LOGE("MAIN", "NETWORKING Task is not created, restart ESP...");
    delay(5000);
    ESP.restart();
  }

  // создание задачи работы с домашним датчиком — динамически
  // создание задачи работы с домашним датчиком — статически
  xHandles[PROTASK_HOME_SENSOR] = xTaskCreateStatic(
      task_home_sensor_exec,
      "HOME_SENSOR",
      PROTASK_HOME_SENSOR_STACK_SIZE,
      0,
      tskIDLE_PRIORITY + 1,
      xTaskStack_PROTASK_HOME_SENSOR,
      &xTaskBuffer[PROTASK_HOME_SENSOR]);
  if (xHandles[PROTASK_HOME_SENSOR] == NULL)
  {
    ESP_LOGE("MAIN", "HOME_SENSOR Task is not created, restart ESP...");
    delay(5000);
    ESP.restart();
  }

  // создание задачи приёма данных с nRF24 (IRQ-driven)
  // создание задачи приёма данных с nRF24 (IRQ-driven) — статически
  xHandles[PROTASK_NRF_RECEIVER] = xTaskCreateStatic(
      task_nrf24_exec,
      "NRF24",
      PROTASK_NRF_RECEIVER_STACK_SIZE,
      0,
      tskIDLE_PRIORITY + 2, // приоритет выше, чем у всех остальных задач
      xTaskStack_PROTASK_NRF_RECEIVER,
      &xTaskBuffer[PROTASK_NRF_RECEIVER]);
  if (xHandles[PROTASK_NRF_RECEIVER] == NULL)
  {
    ESP_LOGE("MAIN", "NRF24 Task is not created, restart ESP...");
    delay(5000);
    ESP.restart();
  }

  // External sensor reception moved to nRF24 task (ESP-NOW removed)

  // создание задачи OTA-обновления прошивки
  xHandles[PROTASK_OTA] = xTaskCreateStatic(
      task_ota_exec,
      "OTA",
      PROTASK_OTA_STACK_SIZE,
      nullptr,
      tskIDLE_PRIORITY + 1,
      xTaskStack_PROTASK_OTA,
      &xTaskBuffer[PROTASK_OTA]);
  if (xHandles[PROTASK_OTA] == NULL)
  {
    ESP_LOGE("MAIN", "OTA Task is not created, restart ESP...");
    delay(5000);
    ESP.restart();
  }

  ESP_LOGI("MAIN", "Initialization complete...");
}

void loop()
{
  ;
}
