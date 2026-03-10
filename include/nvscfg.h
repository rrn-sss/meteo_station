/**
 * @file nvscfg.h
 * @brief NVS-based persistent configuration storage for Meteo Station.
 *        Replaces LittleFS config.json — survives OTA firmware updates.
 *
 * MIT License — Meteo Station Project
 */

#ifndef _NVSCFG_H_
#define _NVSCFG_H_

#include "common.h"

/**
 * @brief Static utility class for saving/loading PrjCfgData to/from NVS.
 *
 * Uses the ESP32 Arduino Preferences API (namespace "meteo_cfg").
 * The NVS partition is never erased during OTA updates, so settings
 * written here persist across firmware upgrades.
 *
 * Thread-safety: Preferences API uses its own internal NVS mutex,
 * so no external synchronisation is required.
 */
class NvsCfg
{
public:
  NvsCfg() = delete;

  /**
   * @brief Load configuration from NVS into cfg.
   * @param cfg  Reference to PrjCfgData filled with values from NVS.
   *             Fields not found in NVS keep their default-initialised values.
   * @return true  if at least the mqtt_server key was found (namespace exists).
   * @return false on first boot or if namespace is missing/corrupt.
   */
  static bool load(PrjCfgData &cfg);

  /**
   * @brief Save all fields of cfg to NVS.
   * @param cfg  Configuration to persist.
   * @return true  on success.
   * @return false if NVS could not be opened for writing.
   */
  static bool save(const PrjCfgData &cfg);

  /**
   * @brief Migrate config from LittleFS config.json to NVS.
   *
   * Called on first boot or when NVS is empty but LittleFS config exists.
   * Reads /config.json from LittleFS and saves all values to NVS.
   * After successful migration, optionally removes the old config.json.
   *
   * @param delete_old_file  If true, removes config.json after successful migration.
   * @return true  if migration succeeded or no migration needed.
   * @return false if migration failed (e.g., LittleFS not mounted, file not found, parse error).
   */
  static bool migrate(bool delete_old_file = false);

private:
  // NVS namespace — max 15 characters
  static constexpr const char *k_namespace = "meteo_cfg";
};

#endif // _NVSCFG_H_
