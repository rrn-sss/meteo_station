#include "task_ota.h"
#include "certs.h"
#include "common.h"
#include "stack_monitor.h"
#include "version.h"
#include <Preferences.h>
#include <WiFi.h>
#include <esp32FOTA.hpp>
#include <esp_log.h>
#include <cstring>

static const char *TAG = "OTA";

// Пространство имён и ключ для маркера незавершённого OTA-обновления.
// Текущая версия прошивки здесь НЕ хранится — источником истины является
// APP_VERSION (include/version.h), то есть реально работающий образ.
#define NVS_OTA_NAMESPACE "ota"
#define NVS_OTA_PENDING_KEY "fw_pending"
// Ключ с «текущей» версией из прошивок <= 1.1.7: больше не используется
#define NVS_OTA_LEGACY_VERSION_KEY "fw_version"

// URL манифеста прошивки
#define OTA_MANIFEST_URL "https://tag78.ru/ota/meteo_station/manifest.json"

// Интервал проверки обновлений (1 час)
#define OTA_CHECK_INTERVAL_MS (60UL * 60UL * 1000UL)

// Задержка первой проверки после старта (30 секунд после подключения WiFi)
#define OTA_INITIAL_DELAY_MS 30000

/**
 * Читает маркер незавершённого обновления (версию, которую пытались установить)
 */
static String nvs_get_pending_version()
{
  Preferences prefs;
  if (!prefs.begin(NVS_OTA_NAMESPACE, /*readOnly=*/true))
    return String("");

  const String pending = prefs.getString(NVS_OTA_PENDING_KEY, "");
  prefs.end();
  return pending;
}

static void nvs_set_pending_version(const char *version)
{
  Preferences prefs;
  if (!prefs.begin(NVS_OTA_NAMESPACE, /*readOnly=*/false))
  {
    ESP_LOGE(TAG, "Failed to open NVS for writing pending version");
    return;
  }
  prefs.putString(NVS_OTA_PENDING_KEY, version);
  prefs.end();
  ESP_LOGI(TAG, "Saved pending OTA version to NVS: %s", version);
}

static void nvs_clear_pending_version()
{
  Preferences prefs;
  if (!prefs.begin(NVS_OTA_NAMESPACE, /*readOnly=*/false))
  {
    ESP_LOGE(TAG, "Failed to open NVS for clearing pending version");
    return;
  }
  prefs.remove(NVS_OTA_PENDING_KEY);
  prefs.end();
}

/**
 * Удаляет устаревший ключ с версией прошивки, оставшийся от прошивок <= 1.1.7.
 * Значение в этом ключе могло расходиться с реально запущенным образом и
 * сбивало проверку обновлений (в том числе после отката на старую прошивку).
 */
static void nvs_remove_legacy_version()
{
  Preferences prefs;
  if (!prefs.begin(NVS_OTA_NAMESPACE, /*readOnly=*/false))
    return;

  if (prefs.isKey(NVS_OTA_LEGACY_VERSION_KEY))
  {
    prefs.remove(NVS_OTA_LEGACY_VERSION_KEY);
    ESP_LOGI(TAG, "Removed legacy NVS firmware version key");
  }
  prefs.end();
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

  // Маркер в NVS нужен только для диагностики незавершённого обновления:
  // он не участвует в сравнении версий, поэтому не может «подменить» текущую прошивку.
  const String pendingVersion = nvs_get_pending_version();
  if (pendingVersion.length() > 0)
  {
    if (pendingVersion == APP_VERSION)
      ESP_LOGI(TAG, "OTA update to %s completed successfully", pendingVersion.c_str());
    else
      ESP_LOGW(TAG, "OTA to %s did not complete, running %s", pendingVersion.c_str(), APP_VERSION);
    nvs_clear_pending_version();
  }

  // Подчистить ключ версии, оставшийся от старых прошивок
  nvs_remove_legacy_version();

  // Создать объект esp32FOTA:
  //   firmwareType    — имя прошивки (должно совпадать с полем "name" в manifest.json)
  //   currentVersion  — текущая версия прошивки (реально запущенный образ)
  //   validate        — проверять MD5/SHA256 (false = не проверять)
  //   allow_insecure  — разрешить HTTPS без проверки сертификата (false = проверять сертификат)
  esp32FOTA fota("meteo_station", APP_VERSION, false, false);
  // Установить CA-сертификат для проверки HTTPS
  CryptoMemAsset root_ca("ISRG Root X1", CERT_ISRG_ROOT_X1, strlen(CERT_ISRG_ROOT_X1));
  fota.setRootCA(&root_ca);
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
        // semver_render() пишет строку без ограничения длины, поэтому даём запас
        char newVersion[64] = {0};
        fota.getPayloadVersion(newVersion);

        if (newVersion[0] == '\0')
        {
          ESP_LOGW(TAG, "Manifest has no valid version — OTA skipped");
        }
        else if (strcmp(newVersion, APP_VERSION) == 0)
        {
          // Защита от лишней перезаписи тем же образом, даже если библиотека
          // сочла версии разными
          ESP_LOGW(TAG, "Manifest version %s equals running version — OTA skipped", newVersion);
        }
        else
        {
          ESP_LOGI(TAG, "New firmware %s available (running %s), starting OTA...", newVersion, APP_VERSION);

          // Signal other tasks (TFT) that OTA update processing starts
          if (xEventGroup)
            xEventGroupSetBits(xEventGroup, BIT_OTA_UPDATE_BIT);

          // Маркер нужен только для диагностики после перезагрузки и не
          // используется как текущая версия прошивки
          nvs_set_pending_version(newVersion);

          fota.execOTA();

          // После успешного обновления ESP автоматически перезагрузится.
          // Если execOTA вернул управление — произошла ошибка.
          nvs_clear_pending_version();
          // If execOTA returned, update didn't complete — clear OTA bit so UI returns to normal
          if (xEventGroup)
            xEventGroupClearBits(xEventGroup, BIT_OTA_UPDATE_BIT);
          ESP_LOGE(TAG, "OTA update to %s failed, will retry after next interval", newVersion);
        }
      }
      else
      {
        ESP_LOGI(TAG, "No new firmware available. Current version is %s", APP_VERSION);
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