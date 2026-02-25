#include "task_ota.h"
#include "common.h"
#include "stack_monitor.h"
#include <Preferences.h>
#include <WiFi.h>
#include <esp32FOTA.hpp>
#include <esp_log.h>

static const char *TAG = "OTA";

// Пространство имён и ключ для хранения версии в NVS
#define NVS_OTA_NAMESPACE "ota"
#define NVS_OTA_VERSION_KEY "fw_version"

// URL манифеста прошивки
#define OTA_MANIFEST_URL "https://tag78.ru/ota/meteo_station/manifest.json"

// Интервал проверки обновлений (1 час)
#define OTA_CHECK_INTERVAL_MS (60UL * 60UL * 1000UL)

// Задержка первой проверки после старта (30 секунд после подключения WiFi)
#define OTA_INITIAL_DELAY_MS 30000

// Версия прошивки — переопределить через -DAPP_VERSION=... в platformio.ini
#ifndef APP_VERSION
#define APP_VERSION "0.0.0"
#endif

/**
 * Читает версию прошивки из NVS
 */
static String nvs_get_version()
{
  Preferences prefs;
  prefs.begin(NVS_OTA_NAMESPACE, false);

  String storedVersion = prefs.getString(NVS_OTA_VERSION_KEY, "");

  if (storedVersion.length() == 0)
  {
    ESP_LOGI(TAG, "No firmware version in NVS. Saving default: 0.0.0");
    prefs.putString(NVS_OTA_VERSION_KEY, "0.0.0");
    storedVersion = "0.0.0";
  }

  prefs.end();
  ESP_LOGI(TAG, "Current firmware version in NVS: %s", storedVersion.c_str());
  return storedVersion;
}

static void set_nvs_version(const char *version)
{
  Preferences prefs;
  prefs.begin(NVS_OTA_NAMESPACE, false);
  prefs.putString(NVS_OTA_VERSION_KEY, version);
  prefs.end();
  ESP_LOGI(TAG, "Saved expected new version to NVS: %s", version);
}

void task_ota_exec(void *pvParameters)
{
  ESP_LOGI(TAG, "OTA task started, waiting for WiFi...");

  // Ожидаем подключения к WiFi
  while (WiFi.status() != WL_CONNECTED)
  {
    vTaskDelay(pdMS_TO_TICKS(5000));
  }

  ESP_LOGI(TAG, "WiFi connected. Firmware version: %s", APP_VERSION);
  ESP_LOGI(TAG, "Manifest URL: %s", OTA_MANIFEST_URL);

  // Проверяем версию в NVS: определяем, было ли предыдущее OTA-обновление успешным
  String nvsVersion = nvs_get_version();

  // Создать объект esp32FOTA:
  //   firmwareType    — имя прошивки (должно совпадать с полем "name" в manifest.json)
  //   currentVersion  — текущая версия прошивки
  //   validate        — проверять MD5/SHA256 (false = не проверять)
  //   allow_insecure  — разрешить HTTPS без проверки сертификата (true = разрешить)
  esp32FOTA fota("meteo_station", nvsVersion.c_str(), false, true);
  // Указать URL манифеста через API библиотеки
  fota.setManifestURL(OTA_MANIFEST_URL);

  StackMonitor_t stackMon;
  stack_monitor_init(&stackMon, TAG);

  // Начальная задержка, чтобы система успела инициализироваться
  vTaskDelay(pdMS_TO_TICKS(OTA_INITIAL_DELAY_MS));

  while (1)
  {
    if (WiFi.status() == WL_CONNECTED)
    {
      ESP_LOGI(TAG, "Checking for firmware update...");

      bool updateAvailable = fota.execHTTPcheck();

      if (updateAvailable)
      {
        ESP_LOGI(TAG, "New firmware available! Starting OTA update...");
        // Signal other tasks (TFT) that OTA update processing starts
        if (xEventGroup)
          xEventGroupSetBits(xEventGroup, BIT_OTA_UPDATE_BIT);
        // Сохраняем версию новой прошивки в NVS до перезагрузки
        {
          char newVersion[32] = {0};
          fota.getPayloadVersion(newVersion);
          if (newVersion[0] != '\0')
            set_nvs_version(newVersion);
          else
            ESP_LOGW(TAG, "Unable to get new firmware version from manifest, not updating NVS");
        }
        fota.execOTA();
        // If execOTA returned, update didn't complete — clear OTA bit so UI returns to normal
        if (xEventGroup)
          xEventGroupClearBits(xEventGroup, BIT_OTA_UPDATE_BIT);
        // После успешного обновления ESP автоматически перезагрузится.
        // Если execOTA вернул управление — произошла ошибка.
        ESP_LOGE(TAG, "OTA update failed, will retry after next interval");
        // Откатываем версию в NVS, так как обновление не применилось
        set_nvs_version("0.0.0");
      }
      else
      {
        ESP_LOGI(TAG, "No new firmware available. Current version is %s", nvsVersion.c_str());
      }
    }
    else
    {
      ESP_LOGW(TAG, "WiFi not connected, skipping OTA check");
    }

    stack_monitor_sample(&stackMon, PROTASK_OTA_STACK_SIZE);

    vTaskDelay(pdMS_TO_TICKS(OTA_CHECK_INTERVAL_MS));
  }
}