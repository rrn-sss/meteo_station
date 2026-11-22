#include "http_helpers.h"
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <esp_log.h>

static const char *TAG = "HTTP_HELPERS";

namespace
{
// Успешным считается только ответ 2xx: тела 4xx/5xx не должны выглядеть
// как валидные данные (HTML-страница ошибки и т.п.)
bool is_success_status(int code)
{
  return code >= 200 && code <= 299;
}
} // namespace

String send_HTTP_GET_request(const char *serverName)
{
  if (WiFi.status() != WL_CONNECTED)
  {
    ESP_LOGW(TAG, "HTTP GET: WiFi not connected");
    return String("{}");
  }

  WiFiClient client;
  HTTPClient http;

  if (!http.begin(client, serverName))
  {
    ESP_LOGE(TAG, "HTTP GET: failed to start request to %s", serverName);
    return String("{}");
  }

  const int httpResponseCode = http.GET();

  String payload = "{}";

  if (is_success_status(httpResponseCode))
  {
    ESP_LOGI(TAG, "HTTP Response code: %d", httpResponseCode);
    payload = http.getString();
  }
  else if (httpResponseCode > 0)
    ESP_LOGE(TAG, "HTTP error code: %d (expected 2xx), body ignored", httpResponseCode);
  else
    ESP_LOGW(TAG, "HTTP request failed: %d", httpResponseCode);

  http.end();
  return payload;
}

String send_HTTPS_GET_request(const char *serverName, const char *ca_cert, const String &user_agent)
{
  if (WiFi.status() != WL_CONNECTED)
  {
    ESP_LOGW(TAG, "HTTPS GET: WiFi not connected");
    return String("{}");
  }

  WiFiClientSecure client;
  HTTPClient http;

  if (ca_cert && strlen(ca_cert) > 0)
    client.setCACert(ca_cert);
  else
    client.setInsecure();

  if (!http.begin(client, serverName))
  {
    ESP_LOGE(TAG, "HTTPS GET: failed to start request to %s", serverName);
    return String("{}");
  }

  if (!user_agent.isEmpty())
    http.addHeader("User-Agent", user_agent);

  const int httpResponseCode = http.GET();

  String payload = "{}";

  if (is_success_status(httpResponseCode))
  {
    ESP_LOGI(TAG, "HTTPS request: %s", serverName);
    ESP_LOGI(TAG, "HTTPS Response code: %d", httpResponseCode);
    payload = http.getString();
  }
  else if (httpResponseCode > 0)
    ESP_LOGE(TAG, "HTTPS error code: %d (expected 2xx), body ignored", httpResponseCode);
  else
    ESP_LOGW(TAG, "HTTPS request failed: %d", httpResponseCode);

  http.end();
  return payload;
}
