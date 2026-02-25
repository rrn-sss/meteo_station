#ifndef _STACK_MONITOR_H_
#define _STACK_MONITOR_H_

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

typedef struct
{
  const char *tag;        // лог-тег задачи
  TickType_t last_log;    // время последнего лога
  UBaseType_t global_max; // максимальное значение за всё время (words)
  UBaseType_t minute_max; // максимальное значение за последнюю минуту (words)
} StackMonitor_t;

static inline void stack_monitor_init(StackMonitor_t *mon, const char *tag)
{
  mon->tag = tag;
  // Set last_log one minute in the past so first sample logs immediately
  mon->last_log = xTaskGetTickCount() - pdMS_TO_TICKS(60000);
  mon->global_max = 0;
  mon->minute_max = 0;
  // Ensure ESP logging for our STACK tag is enabled at INFO level
  esp_log_level_set("STACK", ESP_LOG_INFO);
}

// Снимать с текущей задачи high-water mark, обновлять maxima и логировать раз в минуту
void stack_monitor_sample(StackMonitor_t *mon, uint16_t maxsize);

#endif // _STACK_MONITOR_H_
