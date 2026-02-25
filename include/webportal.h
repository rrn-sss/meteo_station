#ifndef _WEBPORTAL_H_
#define _WEBPORTAL_H_
#include "common.h"
#include <FS.h> // должен быть включён первым, иначе может привести к сбоям
#include <LittleFS.h>
#include <WiFiManager.h> //https://github.com/tzapu/WiFiManager

class WebConfig
{
public:
  WebConfig(WiFiManager &wm_ref);

public:
  void process_autoconnect_or_config(bool f_on_demand);
  static bool get_config(PrjCfgData &cfg);
  bool set_config(const PrjCfgData &cfg);

  String getWiFiSSID();

  String getWiFiPass();

private:
  WiFiManager &wm;
  int timeout = 120; // время выполнения в секундах
  WiFiManagerParameter custom_mqtt_server;
  WiFiManagerParameter custom_mqtt_port;
  WiFiManagerParameter custom_mqtt_user;
  WiFiManagerParameter custom_mqtt_pass;
  WiFiManagerParameter custom_mqtt_prefix;
  WiFiManagerParameter custom_bot_token;
  WiFiManagerParameter custom_bot_chat_id;
  WiFiManagerParameter custom_lat;
  WiFiManagerParameter custom_long;
  WiFiManagerParameter custom_gmt_offset;
};

#endif // _WEBPORTAL_H_