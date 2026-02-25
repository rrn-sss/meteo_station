#include "stack_monitor.h"
#include "common.h"
#include <Arduino.h>

void stack_monitor_sample(StackMonitor_t *mon, uint16_t maxsize)
{
#if 0  // включать только для отладки из-за накладных расходов
  if (!mon)
    return;
  // uxTaskGetStackHighWaterMark() returns the minimum amount of free stack (in words)
  UBaseType_t free_words = uxTaskGetStackHighWaterMark(NULL);
  // Convert to used words based on configured stack size (STACK_SIZE is in words)
  UBaseType_t used_words = (maxsize > free_words) ? (maxsize - free_words) : 0;

  // Track maxima of used words (all-time and per-minute)
  if (mon->minute_max == 0 || used_words > mon->minute_max)
    mon->minute_max = used_words;
  if (mon->global_max == 0 || used_words > mon->global_max)
    mon->global_max = used_words;

  if ((xTaskGetTickCount() - mon->last_log) >= pdMS_TO_TICKS(60000))
  {
    unsigned int global_bytes = (unsigned int)(mon->global_max * sizeof(StackType_t));
    unsigned int minute_bytes = (unsigned int)(mon->minute_max * sizeof(StackType_t));
    unsigned int global_pct = (maxsize > 0) ? (unsigned int)((mon->global_max * 100u) / maxsize) : 0u;
    unsigned int minute_pct = (maxsize > 0) ? (unsigned int)((mon->minute_max * 100u) / maxsize) : 0u;

    ESP_LOGI("STACK", "[%s] max used (all-time): %u bytes (%u%%); last-minute max used: %u bytes (%u%%)",
             mon->tag,
             global_bytes, global_pct,
             minute_bytes, minute_pct);
    mon->minute_max = 0;
    mon->last_log = xTaskGetTickCount();    
  }
#endif // Disabled to reduce overhead
}
