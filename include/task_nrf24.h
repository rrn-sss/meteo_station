#ifndef _TASK_NRF24_H_
#define _TASK_NRF24_H_

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// Entry point for the nRF24 receiver task
void task_nrf24_exec(void *pvParameters);

#endif // _TASK_NRF24_H_
