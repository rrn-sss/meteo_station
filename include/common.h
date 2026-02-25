#ifndef _COMMON_H_
#define _COMMON_H_
#include "User_Setup.h"
#include <LittleFS.h>
#include <LovyanGFX.hpp>
#include <WString.h>
#include <freertos/FreeRTOS.h>
#include <freertos/Queue.h>
#include <freertos/event_groups.h>

// I2C pins for BME280
#define I2C_SDA_PIN 8
#define I2C_SCL_PIN 9

// nRF24L01+ pins
#define NRF_MOSI_PIN 15
#define NRF_MISO_PIN 16
#define NRF_SCK_PIN 6
#define NRF_CSN_PIN 5
#define NRF_CE_PIN 4

#define DATA_QUEUE_SIZE 10
#define DATA_QUEUE_ITEM_SIZE sizeof(QueDataItem_t)

// Provide a simple LGFX wrapper type (common pattern used in LovyanGFX examples)
class LGFX : public lgfx::LGFX_Device
{
  // Пример конфигурации для дисплея ILI9488 (смотрите User_Setup.h)
  lgfx::Panel_ILI9488 _panel_instance;
  lgfx::Bus_SPI _bus_instance;

public:
  LGFX()
  {
    {
      auto cfg = _bus_instance.config();
#ifdef USE_HSPI_PORT
      cfg.spi_host = SPI3_HOST; // HSPI on ESP32-S3
#else
      cfg.spi_host = SPI2_HOST; // default FSPI/VSPI
#endif
      cfg.spi_mode = 0;
#ifdef SPI_FREQUENCY
      cfg.freq_write = SPI_FREQUENCY;
#else
      cfg.freq_write = 40000000;
#endif
#ifdef SPI_READ_FREQUENCY
      cfg.freq_read = SPI_READ_FREQUENCY;
#else
      cfg.freq_read = 16000000;
#endif
      cfg.spi_3wire = false;
      cfg.use_lock = true;
      cfg.dma_channel = 1;
#ifdef TFT_SCLK
      cfg.pin_sclk = TFT_SCLK;
#else
      cfg.pin_sclk = NRF_SCK_PIN;
#endif
#ifdef TFT_MOSI
      cfg.pin_mosi = TFT_MOSI;
#else
      cfg.pin_mosi = NRF_MOSI_PIN;
#endif
#ifdef TFT_MISO
      cfg.pin_miso = TFT_MISO;
#else
      cfg.pin_miso = NRF_MISO_PIN;
#endif
#ifdef TFT_DC
      cfg.pin_dc = TFT_DC;
#else
      cfg.pin_dc = -1;
#endif
      _bus_instance.config(cfg);
      _panel_instance.setBus(&_bus_instance);
    }
    {
      auto cfg = _panel_instance.config();
#ifdef TFT_CS
      cfg.pin_cs = TFT_CS;
#else
      cfg.pin_cs = NRF_CSN_PIN;
#endif
#ifdef TFT_RST
      cfg.pin_rst = TFT_RST;
#else
      cfg.pin_rst = -1;
#endif
      cfg.pin_busy = -1;
// Use landscape 480x320 physical resolution by default
#if defined(TFT_WIDTH) && defined(TFT_HEIGHT)
      // Use User_Setup.h defined physical panel width/height directly
      cfg.memory_width = TFT_WIDTH;
      cfg.memory_height = TFT_HEIGHT;
#else
      cfg.memory_width = 480;
      cfg.memory_height = 320;
#endif
      cfg.panel_width = cfg.memory_width;
      cfg.panel_height = cfg.memory_height;
      cfg.offset_x = 0;
      cfg.offset_y = 0;
      cfg.offset_rotation = 0;
      cfg.dummy_read_pixel = 8;
      cfg.dummy_read_bits = 1;
      cfg.readable = true;
      cfg.invert = false;
      cfg.rgb_order = false;
      cfg.dlen_16bit = false;
      cfg.bus_shared = true;
      _panel_instance.config(cfg);
    }
    setPanel(&_panel_instance);
  }
};

// Общие настройки задач
// #define STACK_SIZE 8192                     // размер стека для каждой задачи
#define PROTASK_TFT_STACK_SIZE 8192            // размер стека задачи TFT
#define PROTASK_NETWORKING_STACK_SIZE 8192     // размер стека задачи NETWORKING (увеличен для HTTPS)
#define PROTASK_HOME_SENSOR_STACK_SIZE 3072    // размер стека задачи HOME_SENSOR
#define PROTASK_NRF_RECEIVER_STACK_SIZE 4096   // размер стека задачи NRF_RECEIVER
#define PROTASK_MQTT_PUBLISHER_STACK_SIZE 4096 // размер стека задачи MQTT_PUBLISHER
#define PROTASK_OTA_STACK_SIZE 10240           // размер стека задачи OTA (увеличен для HTTPS)

#define METEO_POLL_INTERVAL_MS (60000 * 10)                      // интервал опроса метео-данных (10 минут)
#define MAX_METEO_VALID_INTERVAL_MS (METEO_POLL_INTERVAL_MS * 3) // максимальный интервал валидности метео-данных (30 минут)

/// @brief перечень задач проекта
enum ProjectTasks_t
{
  PROTASK_TFT = 0,        // задача обновления данных на TFT
  PROTASK_NETWORKING,     // задача работы с сетью (WiFi, MQTT и т.п.)
  PROTASK_HOME_SENSOR,    // задача работы с "домашним" датчиком домашней метеостанции
  PROTASK_NRF_RECEIVER,   // задача приёма данных с наружнего датчика с помощью nRF24L01+
  PROTASK_MQTT_PUBLISHER, // задача публикации данных в MQTT
  PROTASK_OTA,            // задача обновления прошивки по OTA
  _PROTASK_NUM_
};

/// @brief типы данных в очереди
enum QueDataType_t
{
  QUE_DATATYPE_CFG = 0,         // конфигурационные данные (PrjCfgData)
  QUE_DATATYPE_METEO,           // метеосводка (массив структуры MeteoData_t)
  QUE_DATATYPE_GEOMAGNETIC,     // геомагнитная обстановка (массив структуры GeoMagneticKpMax)
  QUE_DATATYPE_IN_SENSOR_DATA,  // данные с комнатного (in) датчика метеостанции (структура HomeSensorData_t)
  QUE_DATATYPE_OUT_SENSOR_DATA, // данные с наружнего датчика метеостанции (структура OutSensorData_t)
  QUE_DATATYPE_CITYNAME,        // наименование населённого пункта (String*)
  _QUE_DATATYPE_NUM_,
};

/// @brief элемент данных в очереди
struct QueDataItem_t
{
  QueDataType_t type; // тип данных в очереди
  void *data;         // указатель на данные
};

// Количество измерений для усреднения данных домашнего сенсора
static const int HOME_SENSOR_AVG_SAMPLES = 5;

/// @brief структура данных конфигурации проекта (конфигурируется через веб-портал)
struct PrjCfgData
{
  char mqtt_server[32] = {"tag78.ru"};                       // сервер mqtt брокера
  char mqtt_port[5] = {"1883"};                              // сервер mqtt брокера
  char mqtt_user[32] = {"xxxxxx"};                           // логин на mqtt сервере srv2.clusterfly.ru
  char mqtt_pass[32] = {"yyyyyy"};                           // пароль на mqtt сервере srv2.clusterfly.ru
  char mqtt_prefix[32] = {"meteo_station"};                  // префикс топиков (например "waterleaker_sensor")
  char bot_token[64] = {"XXXXXXXXXX:YYYYYYYYYYYYYYYYYYYYY"}; // токен телеграмм бота
  char bot_chat_id[16] = {"-ZZZZZZZZZZZZZ"};                 // идентификатор чата телеграмм бота
  char latitude[8] = {"47.2362"};                            // широта для open-meteo
  char longitude[8] = {"38.8969"};                           // долгота для open-meteo
  char gmt_offset_sec[6] = {"10800"};                        // смещение часового пояса в секундах (Москва +3 часа = 10800 секунд)
};

/// @brief структура данных внутреннего датчика метеостанции
struct HomeSensorData_t
{
  float temperature_in; // температура внутри помещения
  uint8_t humidity_in;  // влажность внутри помещения
  float pressure_in;    // давление внутри помещения
};

/// @brief структура данных выходных данных с наружнего датчика (по ESP-NOW)
typedef struct __attribute__((packed))
{
  float temperature{0.0f};
  float humidity{0.0f};
  uint16_t pressure{0};
  uint16_t bat_charge{0}; // заряд батареи в процентах // TODO: реализовать
} OutSensorData_t;

/// @brief биты состояния системы
#define BIT_WIFI_STATE_UP BIT1 // WiFi подключен
#define BIT_MQTT_STATE_UP BIT3 // MQTT подключен
#define BIT_OPEN_METEO_UP BIT5 // Данные от Open-Meteo получены
#define BIT_NARODMON_UP BIT7   // Данные для NarodMon отправлены успешно
// Бит уведомления о процессе OTA-обновления (если установлен — показывать экран "обновление")
#define BIT_OTA_UPDATE_BIT BIT9

#endif // _COMMON_H_