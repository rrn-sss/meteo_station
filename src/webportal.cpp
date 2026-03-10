/**
 * @file webportal.cpp
 * @brief WiFiManager captive portal configuration — custom modern UI styling.
 *
 * MIT License — Meteo Station Project
 */

#include "webportal.h"
#include "common.h"
#include "nvscfg.h"
#include <WiFiUdp.h>
#include <atomic>

// флаг для сохранения данных
std::atomic<bool> shouldSaveConfig{false};
static const char *TAG = "WM";

constexpr int PORTAL_TIMEOUT_SEC = 120;

// ---------------------------------------------------------------------------
// Custom CSS injected into the <head> of every WiFiManager page
// ---------------------------------------------------------------------------
static const char PORTAL_HEAD_CSS[] PROGMEM = R"rawliteral(
<style>
  :root {
    --bg:        #0f1117;
    --surface:   #1a1d27;
    --surface2:  #22263a;
    --accent:    #4f8ef7;
    --accent2:   #6dd5fa;
    --text:      #e2e8f0;
    --text-muted:#8892a0;
    --border:    #2d3348;
    --radius:    12px;
    --shadow:    0 8px 32px rgba(0,0,0,.45);
  }

  *, *::before, *::after { box-sizing: border-box; margin: 0; padding: 0; }

  html, body {
    background: var(--bg);
    color: var(--text);
    font-family: 'Segoe UI', system-ui, -apple-system, sans-serif;
    font-size: 15px;
    min-height: 100vh;
  }

  /* ── Top header bar ─────────────────────────────────── */
  .wm-header {
    background: linear-gradient(135deg, #1a2a4a 0%, #0f1117 100%);
    border-bottom: 1px solid var(--border);
    padding: 18px 24px;
    display: flex;
    align-items: center;
    gap: 14px;
  }
  .wm-header-icon {
    width: 40px; height: 40px;
    background: linear-gradient(135deg, var(--accent), var(--accent2));
    border-radius: 10px;
    display: flex; align-items: center; justify-content: center;
    font-size: 20px;
  }
  .wm-header-title { font-size: 18px; font-weight: 700; letter-spacing: .3px; }
  .wm-header-sub   { font-size: 12px; color: var(--text-muted); margin-top: 1px; }

  /* ── Main container ─────────────────────────────────── */
  .container {
    max-width: 520px;
    margin: 32px auto;
    padding: 0 16px 40px;
  }

  /* ── Card ───────────────────────────────────────────── */
  div.msg, form {
    background: var(--surface);
    border: 1px solid var(--border);
    border-radius: var(--radius);
    padding: 28px 28px 24px;
    box-shadow: var(--shadow);
    margin-bottom: 20px;
  }

  /* ── Section headings ───────────────────────────────── */
  h1, h2, h3 {
    color: var(--text);
    font-weight: 600;
    font-size: 16px;
    margin-bottom: 18px;
    padding-bottom: 10px;
    border-bottom: 1px solid var(--border);
    display: flex; align-items: center; gap: 8px;
  }
  h1::before { content: '📡'; }
  h3::before { content: '⚙️'; }

  /* ── Labels & inputs ─────────────────────────────────── */
  label {
    display: block;
    color: var(--text-muted);
    font-size: 12px;
    font-weight: 600;
    text-transform: uppercase;
    letter-spacing: .6px;
    margin-bottom: 6px;
    margin-top: 14px;
  }
  label:first-of-type { margin-top: 0; }

  input[type=text],
  input[type=password],
  input[type=number],
  select {
    width: 100%;
    padding: 10px 14px;
    background: var(--surface2);
    border: 1px solid var(--border);
    border-radius: 8px;
    color: var(--text);
    font-size: 14px;
    outline: none;
    transition: border-color .2s, box-shadow .2s;
    appearance: none;
  }
  input:focus, select:focus {
    border-color: var(--accent);
    box-shadow: 0 0 0 3px rgba(79,142,247,.18);
  }
  input::placeholder { color: var(--text-muted); }

  /* ── Buttons ─────────────────────────────────────────── */
  input[type=submit], button, .btn {
    display: block;
    width: 100%;
    margin-top: 20px;
    padding: 12px 20px;
    background: linear-gradient(135deg, var(--accent) 0%, var(--accent2) 100%);
    border: none;
    border-radius: 8px;
    color: #fff;
    font-size: 15px;
    font-weight: 700;
    letter-spacing: .3px;
    cursor: pointer;
    transition: opacity .15s, transform .1s;
    box-shadow: 0 4px 14px rgba(79,142,247,.35);
  }
  input[type=submit]:hover, button:hover { opacity: .88; transform: translateY(-1px); }
  input[type=submit]:active, button:active { opacity: 1; transform: none; }

  /* ── Secondary / back links ──────────────────────────── */
  a {
    color: var(--accent2);
    text-decoration: none;
    font-weight: 600;
    transition: color .15s;
  }
  a:hover { color: #fff; }

  /* ── WiFi list items ─────────────────────────────────── */
  .wifi-item {
    background: var(--surface2);
    border: 1px solid var(--border);
    border-radius: 8px;
    padding: 12px 16px;
    margin-bottom: 8px;
    cursor: pointer;
    display: flex; align-items: center; justify-content: space-between;
    transition: border-color .2s, background .2s;
  }
  .wifi-item:hover {
    border-color: var(--accent);
    background: #1e2436;
  }
  .rssi-badge {
    font-size: 11px;
    padding: 3px 8px;
    border-radius: 20px;
    background: rgba(79,142,247,.15);
    color: var(--accent2);
    font-weight: 700;
  }

  /* ── Info / success messages ─────────────────────────── */
  div.msg {
    color: var(--text-muted);
    font-size: 14px;
    line-height: 1.6;
  }
  div.msg b { color: var(--text); }

  /* ── Footer ──────────────────────────────────────────── */
  .wm-footer {
    text-align: center;
    font-size: 11px;
    color: var(--text-muted);
    margin-top: 12px;
  }

  /* ── Scrollbar ───────────────────────────────────────── */
  ::-webkit-scrollbar { width: 6px; }
  ::-webkit-scrollbar-track { background: var(--bg); }
  ::-webkit-scrollbar-thumb { background: var(--border); border-radius: 3px; }
</style>
)rawliteral";

// ---------------------------------------------------------------------------
// Custom HTML injected at the very top of <body> — branding header
// ---------------------------------------------------------------------------
static const char PORTAL_BODY_HEADER[] PROGMEM = R"rawliteral(
<div class="wm-header">
  <div class="wm-header-icon">🌡️</div>
  <div>
    <div class="wm-header-title">Meteo Station</div>
    <div class="wm-header-sub">Configuration Portal</div>
  </div>
</div>
)rawliteral";

// ---------------------------------------------------------------------------
// callback уведомляющий о необходимости сохранить конфигурацию
// ---------------------------------------------------------------------------
void saveConfigCallback()
{
  ESP_LOGI(TAG, "Should save config");
  shouldSaveConfig = true;
}

// ---------------------------------------------------------------------------
// Constructor: initialize WiFiManagerParameter members and register with WiFiManager
// ---------------------------------------------------------------------------
WebConfig::WebConfig(WiFiManager &wm_ref)
    : wm(wm_ref),
      custom_mqtt_server("server", "MQTT server"),
      custom_mqtt_port("port", "MQTT port"),
      custom_mqtt_user("user", "MQTT login"),
      custom_mqtt_pass("pass", "MQTT password"),
      custom_mqtt_prefix("prefix", "MQTT topics prefix"),
      custom_bot_token("bot_token", "Telegram bot token"),
      custom_bot_chat_id("bot_chat_id", "Telegram bot chat ID"),
      custom_lat("latitude", "Latitude for Open-Meteo (e.g. \"47.2329\")"),
      custom_long("longitude", "Longitude for Open-Meteo (e.g. \"39.7075\")"),
      custom_gmt_offset("gmt_offset_sec", "GMT offset seconds (e.g. \"10800\")")
{
  wm.addParameter(&custom_mqtt_server);
  wm.addParameter(&custom_mqtt_port);
  wm.addParameter(&custom_mqtt_user);
  wm.addParameter(&custom_mqtt_pass);
  wm.addParameter(&custom_mqtt_prefix);
  wm.addParameter(&custom_bot_token);
  wm.addParameter(&custom_bot_chat_id);
  wm.addParameter(&custom_lat);
  wm.addParameter(&custom_long);
  wm.addParameter(&custom_gmt_offset);
}

// ---------------------------------------------------------------------------
// Apply modern styling to the WiFiManager instance
// ---------------------------------------------------------------------------
void WebConfig::apply_custom_ui()
{
  wm.setCustomHeadElement(PORTAL_HEAD_CSS);
  wm.setCustomMenuHTML(PORTAL_BODY_HEADER);
  wm.setTitle("Meteo Station");
}

// ---------------------------------------------------------------------------
void WebConfig::process_autoconnect_or_config(bool f_on_demand)
{
  PrjCfgData cfg;

  // Если WiFi уже подключен и не требуется принудительный портал — выходим сразу
  if (!f_on_demand && WiFi.status() == WL_CONNECTED)
    return;

  // Миграция конфигурации из LittleFS в NVS (при первом запуске после обновления)
  NvsCfg::migrate(/*delete_old_file=*/true);

  // читаем конфигурацию перед запуском портала конфигурирования
  const bool configOk = get_config(cfg);
  if (configOk)
  {
    ESP_LOGI(TAG, "WiFi SSID: %s", WiFi.SSID().c_str());
    ESP_LOGI(TAG, "Config loaded:");
    ESP_LOGI(TAG, "  mqtt_server    : %s", cfg.mqtt_server);
    ESP_LOGI(TAG, "  mqtt_port      : %s", cfg.mqtt_port);
    ESP_LOGI(TAG, "  mqtt_user      : %s", cfg.mqtt_user);
    ESP_LOGI(TAG, "  mqtt_pass      : %s", cfg.mqtt_pass);
    ESP_LOGI(TAG, "  mqtt_prefix    : %s", cfg.mqtt_prefix);
    ESP_LOGI(TAG, "  bot_token      : %s", cfg.bot_token);
    ESP_LOGI(TAG, "  bot_chat_id    : %s", cfg.bot_chat_id);
    ESP_LOGI(TAG, "  latitude       : %s", cfg.latitude);
    ESP_LOGI(TAG, "  longitude      : %s", cfg.longitude);
    ESP_LOGI(TAG, "  gmt_offset_sec : %s", cfg.gmt_offset_sec);
  }

  // Обновить значения параметров WiFiManager из конфигурации
  custom_mqtt_server.setValue(cfg.mqtt_server, sizeof(cfg.mqtt_server));
  custom_mqtt_port.setValue(cfg.mqtt_port, sizeof(cfg.mqtt_port));
  custom_mqtt_user.setValue(cfg.mqtt_user, sizeof(cfg.mqtt_user));
  custom_mqtt_pass.setValue(cfg.mqtt_pass, sizeof(cfg.mqtt_pass));
  custom_mqtt_prefix.setValue(cfg.mqtt_prefix, sizeof(cfg.mqtt_prefix));
  custom_bot_token.setValue(cfg.bot_token, sizeof(cfg.bot_token));
  custom_bot_chat_id.setValue(cfg.bot_chat_id, sizeof(cfg.bot_chat_id));
  custom_lat.setValue(cfg.latitude, sizeof(cfg.latitude));
  custom_long.setValue(cfg.longitude, sizeof(cfg.longitude));
  custom_gmt_offset.setValue(cfg.gmt_offset_sec, sizeof(cfg.gmt_offset_sec));

  bool needPortal = f_on_demand || !configOk;

  // Попытка подключения к WiFi (используем credentials из NVS ESP32)
  if (!needPortal && WiFi.status() != WL_CONNECTED)
  {
    ESP_LOGI(TAG, "Attempting WiFi connection using saved credentials...");
    WiFi.begin();
    const unsigned long startAttempt = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - startAttempt) < 10000)
    {
      vTaskDelay(pdMS_TO_TICKS(100));
    }
    if (WiFi.status() != WL_CONNECTED)
    {
      ESP_LOGW(TAG, "WiFi connection failed — starting config portal");
      needPortal = true;
    }
    else
    {
      ESP_LOGI(TAG, "WiFi connected  IP : %s", WiFi.localIP().toString().c_str());
      ESP_LOGI(TAG, "WiFi MAC        : %s", WiFi.macAddress().c_str());
    }
  }

  // Запуск блокирующего портала только при необходимости
  if (needPortal)
  {
    ESP_LOGI(TAG, "Starting config portal...");
    apply_custom_ui();
    wm.setSaveConfigCallback(saveConfigCallback);
    wm.setConfigPortalTimeout(PORTAL_TIMEOUT_SEC);
    wm.setDebugOutput(false);
    wm.startConfigPortal("MeteoStation-AP");
  }

  // Прочитать обновлённые параметры из WiFiManager (после портала)
  strncpy(cfg.mqtt_server, custom_mqtt_server.getValue(), sizeof(cfg.mqtt_server) - 1);
  strncpy(cfg.mqtt_port, custom_mqtt_port.getValue(), sizeof(cfg.mqtt_port) - 1);
  strncpy(cfg.mqtt_user, custom_mqtt_user.getValue(), sizeof(cfg.mqtt_user) - 1);
  strncpy(cfg.mqtt_pass, custom_mqtt_pass.getValue(), sizeof(cfg.mqtt_pass) - 1);
  strncpy(cfg.mqtt_prefix, custom_mqtt_prefix.getValue(), sizeof(cfg.mqtt_prefix) - 1);
  strncpy(cfg.bot_token, custom_bot_token.getValue(), sizeof(cfg.bot_token) - 1);
  strncpy(cfg.bot_chat_id, custom_bot_chat_id.getValue(), sizeof(cfg.bot_chat_id) - 1);
  strncpy(cfg.latitude, custom_lat.getValue(), sizeof(cfg.latitude) - 1);
  strncpy(cfg.longitude, custom_long.getValue(), sizeof(cfg.longitude) - 1);
  strncpy(cfg.gmt_offset_sec, custom_gmt_offset.getValue(), sizeof(cfg.gmt_offset_sec) - 1);

  // Сохранить пользовательские параметры в FS
  if (shouldSaveConfig)
  {
    set_config(cfg);
    vTaskDelay(pdMS_TO_TICKS(5000));
    shouldSaveConfig = false;
  }
}

// ---------------------------------------------------------------------------
bool WebConfig::get_config(PrjCfgData &cfg)
{
  return NvsCfg::load(cfg);
}

// ---------------------------------------------------------------------------
bool WebConfig::set_config(const PrjCfgData &cfg)
{
  return NvsCfg::save(cfg);
}

// ---------------------------------------------------------------------------
String WebConfig::getWiFiSSID()
{
  return wm.getWiFiSSID();
}

String WebConfig::getWiFiPass()
{
  return wm.getWiFiPass();
}
