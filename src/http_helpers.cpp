#include "http_helpers.h"
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <esp_log.h>

static const char *TAG = "HTTP_HELPERS";

String send_HTTP_GET_request(const char *serverName)
{
  if (WiFi.status() != WL_CONNECTED)
  {
    ESP_LOGW(TAG, "HTTP GET: WiFi not connected");
    return String("{}");
  }

  WiFiClient client;
  HTTPClient http;

  http.begin(client, serverName);

  int httpResponseCode = http.GET();

  String payload = "{}";

  if (httpResponseCode > 0)
  {
    ESP_LOGI(TAG, "HTTP Response code: %u", httpResponseCode);
    payload = http.getString();
  }
  else
    ESP_LOGI(TAG, "Error code: %u", httpResponseCode);

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

  http.begin(client, serverName);

  if (!user_agent.isEmpty())
    http.addHeader("User-Agent", user_agent);

  int httpResponseCode = http.GET();

  String payload = "{}";

  if (httpResponseCode > 0)
  {
    ESP_LOGI(TAG, "HTTPS request: %s", serverName);
    ESP_LOGI(TAG, "HTTPS Response code: %u", httpResponseCode);
    payload = http.getString();
  }
  else
    ESP_LOGI(TAG, "Error code: %u", httpResponseCode);

  http.end();
  return payload;
}
