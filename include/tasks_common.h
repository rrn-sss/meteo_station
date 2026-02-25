#ifndef _TASKS_COMMON_H_
#define _TASKS_COMMON_H_

#include "common.h"
#include <Adafruit_BME280.h>
#include <LovyanGFX.hpp>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>

extern LGFX tft;
extern TaskHandle_t xHandles[_PROTASK_NUM_];
extern QueueHandle_t xQueue[_PROTASK_NUM_];
extern StaticQueue_t xQueueBuffer[_PROTASK_NUM_];
extern uint8_t xQueueStorage[_PROTASK_NUM_][DATA_QUEUE_SIZE * DATA_QUEUE_ITEM_SIZE];
extern StaticEventGroup_t xEventGroupBuffer;
extern EventGroupHandle_t xEventGroup;
extern Adafruit_BME280 bme;

// Мьютекс для синхронизации доступа к LittleFS между задачами
extern SemaphoreHandle_t xLittleFSMutex;

#endif // _TASKS_COMMON_H_
