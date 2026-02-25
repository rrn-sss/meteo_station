#include "MeteoWidgets.h"
#include "tasks_common.h"
#include <cmath>
#include <ctime>
#include <esp_log.h>
#include <time.h>

MeteoWidgets *MeteoWidgets::currentInstance = nullptr;

// Файловый указатель на целевой спрайт, используемый колбэками PNG
static lgfx::LGFX_Sprite *s_pngTarget = nullptr;

// LovyanGFX filesystem overloads will be used (loadFont(LittleFS, "/file.vlw"))

const char *MeteoWidgets::WIND_PNG_NAME = "/icons/arrow2_48.png";
const char *MeteoWidgets::HUMIDITY_PNG_NAME = "/icons/humidity2_48.png";
const char *MeteoWidgets::THERMOMETER_PNG_NAME = "/icons/thermometer_48.png";
const char *MeteoWidgets::HOME_PNG_NAME = "/icons/home_128.png";
const char *MeteoWidgets::WIFI_GREEN_PNG_NAME = "/icons/wifi-green_32.png";
const char *MeteoWidgets::WIFI_RED_PNG_NAME = "/icons/wifi-red_32.png";
const char *MeteoWidgets::UPDOWN_GREEN_GREEN_PNG_NAME = "/icons/up_green-down_green_32.png";
const char *MeteoWidgets::UPDOWN_GREEN_RED_PNG_NAME = "/icons/up_green-down_red_32.png";
const char *MeteoWidgets::UPDOWN_RED_GREEN_PNG_NAME = "/icons/up_red-down_green_32.png";
const char *MeteoWidgets::UPDOWN_RED_RED_PNG_NAME = "/icons/up_red-down_red_32.png";

const char *MeteoWidgets::BATTERY_0_PNG_NAME = "/icons/battery_0_32.png";
const char *MeteoWidgets::BATTERY_25_PNG_NAME = "/icons/battery_25_32.png";
const char *MeteoWidgets::BATTERY_50_PNG_NAME = "/icons/battery_50_32.png";
const char *MeteoWidgets::BATTERY_75_PNG_NAME = "/icons/battery_75_32.png";
const char *MeteoWidgets::BATTERY_100_PNG_NAME = "/icons/battery_100_32.png";

const std::unordered_map<uint8_t, MeteoWidgets::WeatherInfo> MeteoWidgets::weatherInfoRu = {
    {0, {"Ясное небо", "clear-sky.png"}},
    {1, {"В основном ясно", "clear-sky.png"}},
    {2, {"Местами облачно", "partly-cloudy.png"}},
    {3, {"Пасмурно", "overcast.png"}},
    {45, {"Туман", "fog.png"}},
    {48, {"Туман с инеем", "fog.png"}},
    {51, {"Морось слабая", "drizzle.png"}},
    {53, {"Морось умеренная", "drizzle.png"}},
    {55, {"Морось сильная", "drizzle.png"}},
    {56, {"Замерзающая морось слабая", "overcast-sleet.png"}},
    {57, {"Замерзающая морось сильная", "overcast-sleet.png"}},
    {61, {"Дождь слабый", "rain.png"}},
    {63, {"Дождь умеренный", "rain.png"}},
    {65, {"Дождь сильный", "extreme-rain.png"}},
    {66, {"Замерзающий дождь слабый", "overcast-sleet.png"}},
    {67, {"Замерзающий дождь сильный", "overcast-sleet.png"}},
    {71, {"Снегопад слабый", "snow.png"}},
    {73, {"Снегопад умеренный", "snow.png"}},
    {75, {"Снегопад сильный", "extreme-snow.png"}},
    {77, {"Снежные зерна", "snow.png"}},
    {80, {"Ливневый дождь слабый", "rain.png"}},
    {81, {"Ливневый дождь умеренный", "rain.png"}},
    {82, {"Ливневый дождь сильный", "extreme-rain.png"}},
    {85, {"Ливневый снег слабый", "snow.png"}},
    {86, {"Ливневый снег сильный", "extreme-snow.png"}},
    {95, {"Гроза", "thunderstorms-overcast-rain.png"}},
    {96, {"Гроза с градом слабым", "thunderstorms-overcast-rain.png"}},
    {99, {"Гроза с градом сильным", "thunderstorms-overcast-rain.png"}}};

MeteoWidgets::MeteoWidgets(LGFX &tft)
    : tft(tft)
{
  currentInstance = this;
  // Инициализируем кеш уровня батареи как неустановленный
  lastBatteryLevel = 255;
}

MeteoWidgets::~MeteoWidgets()
{
  // Nothing to clean up here
}

MeteoWidgets *MeteoWidgets::createInstance(LGFX &tft)
{
  if (currentInstance)
    return currentInstance;

  // allocate single instance on heap; keep pointer in static currentInstance
  MeteoWidgets *inst = new MeteoWidgets(tft);
  return inst;
}

MeteoWidgets *MeteoWidgets::getInstance()
{
  return currentInstance;
}

void MeteoWidgets::destroyInstance()
{
  if (currentInstance)
  {
    delete currentInstance;
    currentInstance = nullptr;
  }
}

void MeteoWidgets::init()
{
  tft.setRotation(1);
  tft.fillScreen(WIDGET_BG_COLOR);
}

MeteoWidgets::WeatherInfo MeteoWidgets::getWeatherInfo(int code)
{
  auto it = weatherInfoRu.find(code);
  if (it != weatherInfoRu.end())
    return it->second;
  return {"Неизвестно", "code-red.png"};
}

uint16_t MeteoWidgets::getTempColor(float temp)
{
  if (temp > 35)
    return TFT_RED;
  else if (temp >= 0)
    return TFT_YELLOW;
  else
    return TFT_BLUE;
}

int MeteoWidgets::pngDraw128Callback(PNGDRAW *pDraw)
{
  uint16_t lineBuffer[128];
  if (!s_pngTarget)
    return 0;
  currentInstance->png.getLineAsRGB565(pDraw, lineBuffer, PNG_RGB565_BIG_ENDIAN, 0xffffffff);
  s_pngTarget->pushImage(0, 0 + pDraw->y, pDraw->iWidth, 1, lineBuffer);
  return 1;
}

void *MeteoWidgets::pngOpenCallback(const char *filename, int32_t *size)
{
  MeteoWidgets *self = currentInstance;
  if (!self)
  {
    ESP_LOGE("PNG", "pngOpenCallback: currentInstance is NULL");
    *size = 0;
    return nullptr;
  }

  // Open into a temporary then move into a heap-allocated fs::File to
  // avoid any issues with copying temporaries or invalid internal state.
  fs::File tmp = LittleFS.open(filename, "r");
  if (!tmp)
  {
    ESP_LOGE("PNG", "pngOpenCallback: Failed to open file: %s", filename);
    *size = 0;
    return nullptr;
  }

  fs::File *f = new fs::File();
  *f = std::move(tmp);
  if (!f || !(*f))
  {
    ESP_LOGE("PNG", "pngOpenCallback: Failed to allocate/move file: %s", filename);
    if (f)
      delete f;
    *size = 0;
    return nullptr;
  }

  ESP_LOGD("PNG", "pngOpenCallback: opened %s -> handle %p, size=%u", filename, (void *)f, (unsigned)f->size());
  *size = f->size();
  return static_cast<void *>(f);
}

void MeteoWidgets::pngCloseCallback(void *handle)
{
  fs::File *f = static_cast<fs::File *>(handle);
  if (f)
  {
    if (*f)
      f->close();
    delete f;
  }
}

int32_t MeteoWidgets::pngReadCallback(PNGFILE *page, uint8_t *buffer, int32_t length)
{
  if (!page || !page->fHandle)
  {
    ESP_LOGW("PNG", "pngReadCallback: Invalid PNGFILE or handle");
    return 0;
  }
  fs::File *f = static_cast<fs::File *>(page->fHandle);
  if (!f || !(*f))
    return 0;
  return f->read(buffer, length);
}

int32_t MeteoWidgets::pngSeekCallback(PNGFILE *page, int32_t position)
{
  if (!page || !page->fHandle)
    return 0;
  fs::File *f = static_cast<fs::File *>(page->fHandle);
  if (!f || !(*f))
    return 0;
  return f->seek(position);
}

bool MeteoWidgets::draw_png_2_sprite(const String &png_file_name, lgfx::LGFX_Sprite &target)
{
  // Проверка валидности currentInstance (защита от race condition)
  if (currentInstance == nullptr)
  {
    ESP_LOGE("PNG", "currentInstance is NULL, cannot decode PNG");
    return false;
  }

  // Захватываем мьютекс LittleFS для защиты от конкурентного доступа
  if (xSemaphoreTake(xLittleFSMutex, pdMS_TO_TICKS(1000)) != pdTRUE)
  {
    ESP_LOGE("PNG", "Failed to acquire LittleFS mutex");
    return false;
  }

  // Проверка существования файла перед открытием
  if (!LittleFS.exists(png_file_name))
  {
    ESP_LOGE("PNG", "File not found: %s", png_file_name.c_str());
    xSemaphoreGive(xLittleFSMutex);
    return false;
  }

  s_pngTarget = &target;
  int16_t rc = png.open(png_file_name.c_str(), pngOpenCallback, pngCloseCallback, pngReadCallback, pngSeekCallback, pngDraw128Callback);
  if (rc != PNG_SUCCESS)
  {
    ESP_LOGE("PNG", "Failed to open PNG: %s, error code: %d", png_file_name.c_str(), rc);
    s_pngTarget = nullptr;
    xSemaphoreGive(xLittleFSMutex);
    return false;
  }

  // Decode is synchronous in PNGdec: it will call the draw callback for each
  // decoded line. Keep the target sprite valid until decode finishes.
  int16_t dec_rc = png.decode(NULL, 0);

  // Close the PNG and clear the target pointer.
  png.close();
  s_pngTarget = nullptr;

  // Освобождаем мьютекс LittleFS
  xSemaphoreGive(xLittleFSMutex);

  if (dec_rc != PNG_SUCCESS)
  {
    ESP_LOGE("PNG", "Failed to decode PNG: %s, error code: %d", png_file_name.c_str(), dec_rc);
    return false;
  }

  // short yield to allow scheduler to run other tasks
  vTaskDelay(pdMS_TO_TICKS(1));

  return true;
}

bool MeteoWidgets::draw_dig_clock_widget(uint16_t pos_x, uint16_t pos_y, uint8_t hh, uint8_t mm)
{
  lgfx::LGFX_Sprite widget_bg_digs_sprite(&tft);
  lgfx::LGFX_Sprite clock_digs_sprite(&tft);

  char buf[9];
  sprintf(buf, "%02d:%02d ", hh, mm);

  if (!widget_bg_digs_sprite.createSprite(CLOCK_DIGS_W, CLOCK_DIGS_H))
  {
    ESP_LOGE("WIDGET", "createSprite for widget DIG_CLOCK failed!");
    ESP_LOGI("WIDGET", "Heap largest free block: %u", heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT));
    return false;
  }
  ESP_LOGI("WIDGET", "Draw widget DIG_CLOCK");
  widget_bg_digs_sprite.fillSprite(WIDGET_BG_COLOR);

  if (!clock_digs_sprite.createSprite(CLOCK_DIGS_W, CLOCK_DIGS_H))
  {
    ESP_LOGE("WIDGET", "createSprite(clock_digs_sprite) for widget DIG_CLOCK failed!");
    ESP_LOGI("WIDGET", "Heap largest free block: %u", heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT));
    widget_bg_digs_sprite.deleteSprite();
    return false;
  }
  clock_digs_sprite.setTextColor(DATETIME_COLOR, TFT_TRANSPARENT);
  clock_digs_sprite.loadFont(LittleFS, "/dseg748.vlw");
  clock_digs_sprite.fillSprite(TFT_TRANSPARENT);
  clock_digs_sprite.setTextDatum(MC_DATUM);
  clock_digs_sprite.drawString(buf, CLOCK_DIGS_W / 2, 0 /*CLOCK_DIGS_H / 2*/);
  clock_digs_sprite.unloadFont();
  clock_digs_sprite.pushSprite(&widget_bg_digs_sprite, 0, 0, TFT_TRANSPARENT);
  clock_digs_sprite.deleteSprite();

  widget_bg_digs_sprite.pushSprite(pos_x, pos_y);

  // удаляем спрайты из памяти желательно в обратном порядке их созданию
  widget_bg_digs_sprite.deleteSprite();

  // short yield to allow scheduler to run other tasks
  vTaskDelay(pdMS_TO_TICKS(1));

  return true;
}

bool MeteoWidgets::draw_current_date_widget(uint16_t pos_x, uint16_t pos_y, const String &date)
{
  lgfx::LGFX_Sprite date_bg(&tft);
  lgfx::LGFX_Sprite date_sprite(&tft);

  if (!date_bg.createSprite(CLOCK_DIGS_W, CLOCK_DIGS_H))
  {
    ESP_LOGE("WIDGET", "createSprite for widget DATE failed!");
    ESP_LOGI("WIDGET", "Heap largest free block: %u", heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT));
    return false;
  }
  ESP_LOGI("WIDGET", "Draw widget DATE");
  date_bg.fillSprite(WIDGET_BG_COLOR);

  if (!date_sprite.createSprite(CLOCK_DIGS_W, CLOCK_DIGS_H))
  {
    ESP_LOGE("WIDGET", "createSprite(date_sprite) for widget DATE failed!");
    ESP_LOGI("WIDGET", "Heap largest free block: %u", heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT));
    date_bg.deleteSprite();
    return false;
  }
  // Draw date at the top of the sprite using font metrics to stack day below
  date_sprite.fillSprite(TFT_TRANSPARENT);
  date_sprite.loadFont(LittleFS, "/dseg720.vlw");
  date_sprite.setTextDatum(ML_DATUM);
  date_sprite.setTextColor(DATETIME_COLOR, TFT_TRANSPARENT);
  // determine font metrics for the date string so we can place the day exactly below
  // obtain rendered font height (takes current text style / scaling into account)
  int16_t date_h = date_sprite.fontHeight();
  // draw date at y=0 (top of sprite)
  date_sprite.drawString(date, 0, 0);
  date_sprite.unloadFont();
  date_sprite.pushSprite(&date_bg, 0, 0, TFT_TRANSPARENT);

  // Draw day stacked directly below the date using the date font height
  date_sprite.fillSprite(TFT_TRANSPARENT);
  date_sprite.loadFont(LittleFS, "/arial_cyr28.vlw");
  date_sprite.setTextDatum(ML_DATUM);
  date_sprite.setTextColor(DATETIME_COLOR, TFT_TRANSPARENT);
  String day = MeteoWidgets::getDayOfWeek(date);
  // use the measured date font height to position the day immediately below
  int16_t day_y = date_h;
  date_sprite.drawString(day, 0, day_y - 4);
  date_sprite.unloadFont();
  date_sprite.pushSprite(&date_bg, 0, 0, TFT_TRANSPARENT);
  date_sprite.deleteSprite();

  date_bg.pushSprite(pos_x, pos_y);

  // удаляем спрайты из памяти желательно в обратном порядке их созданию
  date_bg.deleteSprite();

  // short yield to allow scheduler to run other tasks
  vTaskDelay(pdMS_TO_TICKS(1));

  return true;
}

bool MeteoWidgets::draw_wind_widget(float wind_speed, uint16_t wind_dir, lgfx::LGFX_Sprite &nested_sprite, uint16_t nested_pos_x, uint16_t nested_pos_y)
{
  lgfx::LGFX_Sprite widget_bg_for_wind(&tft);
  lgfx::LGFX_Sprite sprite_48(&tft);
  lgfx::LGFX_Sprite windtxt_for_sprite(&tft);
  lgfx::LGFX_Sprite rotated_48_sprite(&tft);

  if (!widget_bg_for_wind.createSprite(WIDGET_FOR_WIND_W, WIDGET_FOR_WIND_H))
  {
    ESP_LOGE("WIDGET", "createSprite for widget WIND failed!");
    ESP_LOGI("WIDGET", "Heap largest free block: %u", heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT));
    return false;
  }
  widget_bg_for_wind.fillSprite(WIDGET_BG_COLOR);

  sprite_48.createSprite(WIND_ICON_WH, WIND_ICON_WH);
  rotated_48_sprite.createSprite(WIND_ICON_WH, WIND_ICON_WH);
  if (draw_png_2_sprite(WIND_PNG_NAME, sprite_48))
  {
    sprite_48.setPivot(WIND_ICON_WH / 2, WIND_ICON_WH / 2);
    rotated_48_sprite.fillSprite(TFT_BLACK);
    sprite_48.pushRotated(&rotated_48_sprite, (wind_dir + 180) % 360, TFT_BLACK);
    rotated_48_sprite.pushSprite(&widget_bg_for_wind, (WIDGET_FOR_WIND_W - WIND_ICON_WH) / 2, -4, TFT_BLACK);
  }
  rotated_48_sprite.deleteSprite();
  sprite_48.deleteSprite();

  widget_bg_for_wind.loadFont(LittleFS, "/arial_cyr18.vlw");

  String wind_dir_str = "";
  if ((wind_dir >= 338 && wind_dir < 360) || ((wind_dir >= 0 && wind_dir < 23)))
    wind_dir_str = "C";
  else if (wind_dir >= 23 && wind_dir < 68)
    wind_dir_str = "CВ";
  else if (wind_dir >= 68 && wind_dir < 113)
    wind_dir_str = "В";
  else if (wind_dir >= 113 && wind_dir < 158)
    wind_dir_str = "ЮВ";
  else if (wind_dir >= 158 && wind_dir < 203)
    wind_dir_str = "Ю";
  else if (wind_dir >= 203 && wind_dir < 248)
    wind_dir_str = "ЮЗ";
  else if (wind_dir >= 248 && wind_dir < 293)
    wind_dir_str = "З";
  else if (wind_dir >= 293 && wind_dir < 338)
    wind_dir_str = "СЗ";

  widget_bg_for_wind.drawString(wind_dir_str, WIDGET_FOR_WIND_W / 2 - (wind_dir_str.length() > 2 ? 10 : 6), WIND_ICON_WH * 0.9f - 6);
  widget_bg_for_wind.unloadFont();

  windtxt_for_sprite.createSprite(WINDTXT_FOR_WIND_W, WINDTXT_FOR_WIND_H);
  windtxt_for_sprite.fillSprite(TFT_TRANSPARENT);
  windtxt_for_sprite.loadFont(LittleFS, "/arial_cyr18.vlw");
  windtxt_for_sprite.setTextDatum(MC_DATUM);
  windtxt_for_sprite.drawString(String(static_cast<uint8_t>(std::round(wind_speed))) + " м/с", WINDTXT_FOR_WIND_W / 2, WINDTXT_FOR_WIND_H / 2);
  windtxt_for_sprite.unloadFont();
  windtxt_for_sprite.pushSprite(&widget_bg_for_wind, 0, WIDGET_FOR_WIND_H - WINDTXT_FOR_WIND_H, TFT_TRANSPARENT);
  windtxt_for_sprite.deleteSprite();

  widget_bg_for_wind.pushSprite(&nested_sprite, nested_pos_x, nested_pos_y, TFT_BLACK);

  widget_bg_for_wind.deleteSprite();

  return true;
}

bool MeteoWidgets::draw_humidity_widget(uint8_t humidity, lgfx::LGFX_Sprite &nested_sprite, uint16_t nested_pos_x, uint16_t nested_pos_y)
{
  lgfx::LGFX_Sprite humidity_bg(&tft);
  lgfx::LGFX_Sprite sprite_48(&tft);

  if (!humidity_bg.createSprite(HUMIDITY_SPRITE_W, HUMIDITY_SPRITE_H))
  {
    ESP_LOGE("WIDGET", "createSprite for widget HUMIDITY failed!");
    ESP_LOGI("WIDGET", "Heap largest free block: %u", heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT));
    return false;
  }
  humidity_bg.fillSprite(WIDGET_BG_COLOR);

  sprite_48.createSprite(HUMIDITY_ICON_WH, HUMIDITY_ICON_WH);

  if (draw_png_2_sprite(HUMIDITY_PNG_NAME, sprite_48))
    sprite_48.pushSprite(&humidity_bg, 0, 0, TFT_TRANSPARENT);

  humidity_bg.loadFont(LittleFS, "/arial_cyr32.vlw");
  humidity_bg.setTextDatum(TC_DATUM);
  humidity_bg.drawString(String(humidity), HUMIDITY_SPRITE_W / 2, HUMIDITY_SPRITE_H / 2 - 6);
  humidity_bg.unloadFont();

  humidity_bg.pushSprite(&nested_sprite, nested_pos_x, nested_pos_y, TFT_BLACK);

  // удаляем спрайты из памяти желательно в обратном порядке их создания (для избежания фрагментации кучи)
  sprite_48.deleteSprite();
  humidity_bg.deleteSprite();

  return true;
}

bool MeteoWidgets::draw_geomagnetic_widget(uint8_t kr, lgfx::LGFX_Sprite &nested_sprite, uint16_t nested_pos_x, uint16_t nested_pos_y)
{
  lgfx::LGFX_Sprite bg_sprite(&tft);
  lgfx::LGFX_Sprite sprite_24(&tft);

  if (!bg_sprite.createSprite(GEOMAGNETIC_SPRITE_WH, GEOMAGNETIC_SPRITE_WH))
  {
    ESP_LOGE("WIDGET", "createSprite for widget GEOMAGNETIC failed!");
    ESP_LOGI("WIDGET", "Heap largest free block: %u", heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT));
    return false;
  }
  bg_sprite.fillSprite(WIDGET_BG_COLOR);

  sprite_24.createSprite(GEOMAGNETIC_SPRITE_WH, GEOMAGNETIC_SPRITE_WH);

  String strname = "/icons/" + String(GEOMAGNETIC_SPRITE_WH) + "/" + "kr" + String(kr) + "_" + String(GEOMAGNETIC_SPRITE_WH) + ".png";
  if (draw_png_2_sprite(strname, sprite_24))
    sprite_24.pushSprite(&bg_sprite, 0, 0, TFT_TRANSPARENT);

  bg_sprite.pushSprite(&nested_sprite, nested_pos_x, nested_pos_y, TFT_BLACK);

  // удаляем спрайты из памяти желательно в обратном порядке их создания (для избежания фрагментации кучи)
  sprite_24.deleteSprite();
  bg_sprite.deleteSprite();

  return true;
}

bool MeteoWidgets::draw_meteo_current_widget(int scr_x_pos, int scr_y_pos, float cur_temp, uint8_t humidity, float wind_speed, uint16_t wind_dir, uint8_t weather_code, bool valid)
{
  // Render icon and info parts so each sprite <= 128x128
  // Icon: 128x128 at (scr_x_pos, scr_y_pos)
  // Info: 128x128 pushed at (scr_x_pos + ICON_WH - 4, scr_y_pos) to preserve original overlap
  bool ok_icon = draw_meteo_current_icon_widget(scr_x_pos, scr_y_pos, weather_code, valid);
  bool ok_info = draw_meteo_current_info_widget(scr_x_pos + ICON_WH, scr_y_pos, cur_temp, humidity, wind_speed, wind_dir, valid);
  return ok_icon && ok_info;
}

bool MeteoWidgets::draw_meteo_current_icon_widget(int scr_x_pos, int scr_y_pos, uint8_t weather_code, bool valid)
{
  if (!valid)
    return false;

  lgfx::LGFX_Sprite widget_bg_for_sprite(&tft);
  lgfx::LGFX_Sprite sprite_128(&tft);

  if (!widget_bg_for_sprite.createSprite(ICON_WH, ICON_WH))
  {
    ESP_LOGE("WIDGET", "createSprite (bg) failed!");
    ESP_LOGI("WIDGET", "Heap largest free block: %u", heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT));
    return false;
  }
  widget_bg_for_sprite.fillSprite(WIDGET_BG_COLOR);

  if (!sprite_128.createSprite(ICON_WH, ICON_WH))
  {
    ESP_LOGE("WIDGET", "createSprite (icon) failed!");
    ESP_LOGI("WIDGET", "Heap largest free block: %u", heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT));
    widget_bg_for_sprite.deleteSprite();
    return false;
  }
  ESP_LOGI("WIDGET", "Draw widget METEO_CURRENT_ICON");

  // sprite_128.fillSprite(WIDGET_BG_COLOR);
  WeatherInfo wi = getWeatherInfo(weather_code);
  String strname = "/icons/" + String(ICON_WH) + "/" + wi.iconName;
  if (draw_png_2_sprite(strname, sprite_128))
    sprite_128.pushSprite(&widget_bg_for_sprite, 0, 0, TFT_BLACK);
  sprite_128.deleteSprite();

  widget_bg_for_sprite.pushSprite(scr_x_pos, scr_y_pos);
  widget_bg_for_sprite.deleteSprite();

  return true;
}

bool MeteoWidgets::draw_meteo_current_info_widget(int scr_x_pos, int scr_y_pos, float cur_temp, uint8_t humidity, float wind_speed, uint16_t wind_dir, bool valid)
{
  lgfx::LGFX_Sprite info_sprite(&tft);
  if (!info_sprite.createSprite(ICON_WH, ICON_WH))
  {
    ESP_LOGE("WIDGET", "createSprite failed!");
    ESP_LOGI("WIDGET", "Heap largest free block: %u", heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT));
    return false;
  }
  ESP_LOGI("WIDGET", "Draw widget METEO_CURRENT_INFO");
  info_sprite.fillSprite(WIDGET_BG_COLOR);

  if (valid)
  {
    draw_humidity_widget(humidity, info_sprite, 0, TEMP_CUR_SPRITE_H - 10);
    draw_wind_widget(wind_speed, wind_dir, info_sprite, (ICON_WH - WIDGET_FOR_WIND_W), TEMP_CUR_SPRITE_H - 5);

    lgfx::LGFX_Sprite temp_cur_sprite(&tft);
    temp_cur_sprite.createSprite(TEMP_CUR_SPRITE_W, TEMP_CUR_SPRITE_H);
    temp_cur_sprite.setTextColor(getTempColor(cur_temp), TFT_TRANSPARENT);
    temp_cur_sprite.fillSprite(TFT_TRANSPARENT);
    temp_cur_sprite.loadFont(LittleFS, "/arial_cyr56.vlw");
    temp_cur_sprite.setTextDatum(MC_DATUM);
    temp_cur_sprite.drawString(String(static_cast<int8_t>(round(cur_temp))) + "°", TEMP_CUR_SPRITE_W / 2, TEMP_CUR_SPRITE_H / 2);
    temp_cur_sprite.unloadFont();
    temp_cur_sprite.pushSprite(&info_sprite, 4, 0, TFT_TRANSPARENT);
    temp_cur_sprite.deleteSprite();
  }

  // push info sprite so it aligns with icon and reproduces original appearance
  info_sprite.pushSprite(scr_x_pos, scr_y_pos);
  info_sprite.deleteSprite();
  return true;
}

bool MeteoWidgets::draw_meteo_forecast_widget(int scr_x_pos, int scr_y_pos, float min_temp, float max_temp,
                                              float wind_speed, uint16_t wind_dir, uint16_t precip_sum,
                                              uint8_t weather_code, const String &data, float kp_max, bool valid)
{
  lgfx::LGFX_Sprite widget_bg_for_sprite(&tft);
  lgfx::LGFX_Sprite sprite_128(&tft);
  lgfx::LGFX_Sprite temp_for_sprite(&tft);
  lgfx::LGFX_Sprite day_for_sprite(&tft);
  lgfx::LGFX_Sprite precip_for_sprite(&tft);

  if (!widget_bg_for_sprite.createSprite(WIDGET_FOR_W, WIDGET_FOR_H))
  {
    ESP_LOGE("WIDGET", "createSprite failed!");
    ESP_LOGI("WIDGET", "Heap largest free block: %u", heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT));
    return false;
  }
  ESP_LOGI("WIDGET", "Draw widget METEO_FORECAST");
  widget_bg_for_sprite.fillSprite(WIDGET_BG_COLOR);

  if (valid)
  {
    draw_wind_widget(wind_speed, wind_dir, widget_bg_for_sprite, WIDGET_FOR_W - WIDGET_FOR_WIND_W, TEMP_FOR_SPRITE_H - 5);

    sprite_128.createSprite(ICON_WH, ICON_WH);
    WeatherInfo wi = getWeatherInfo(weather_code);
    String strname = wi.iconName;
    strname = "/icons/" + String(ICON_WH) + "/" + strname;
    if (draw_png_2_sprite(strname, sprite_128))
      sprite_128.pushSprite(&widget_bg_for_sprite, 0, 0, TFT_BLACK);
    sprite_128.deleteSprite();

    temp_for_sprite.createSprite(TEMP_FOR_SPRITE_W, TEMP_FOR_SPRITE_H);
    temp_for_sprite.fillSprite(TFT_TRANSPARENT);
    temp_for_sprite.loadFont(LittleFS, "/arial_cyr32.vlw");
    temp_for_sprite.setTextDatum(BC_DATUM);
    temp_for_sprite.setTextColor(getTempColor(max_temp), TFT_TRANSPARENT);
    temp_for_sprite.drawString(String(static_cast<int8_t>(round(max_temp))) + "°",
                               TEMP_FOR_SPRITE_W / 2, TEMP_FOR_SPRITE_H / 2);
    temp_for_sprite.setTextDatum(TC_DATUM);
    temp_for_sprite.setTextColor(getTempColor(min_temp), TFT_TRANSPARENT);
    temp_for_sprite.drawString(String(static_cast<int8_t>(round(min_temp))) + "°",
                               TEMP_FOR_SPRITE_W / 2, TEMP_FOR_SPRITE_H / 2 - 8);
    temp_for_sprite.unloadFont();
    temp_for_sprite.pushSprite(&widget_bg_for_sprite, WIDGET_FOR_W - TEMP_FOR_SPRITE_W, 4, TFT_TRANSPARENT);
    temp_for_sprite.deleteSprite();

    day_for_sprite.createSprite(DAY_FOR_SPRITE_W, DAY_FOR_SPRITE_H);
    day_for_sprite.setTextColor(TFT_WHITE, TFT_TRANSPARENT);
    day_for_sprite.fillSprite(TFT_TRANSPARENT);
    day_for_sprite.loadFont(LittleFS, "/arial_cyr18.vlw");
    day_for_sprite.setTextDatum(MC_DATUM);
    day_for_sprite.drawString(data, DAY_FOR_SPRITE_W / 2, DAY_FOR_SPRITE_H / 2);
    day_for_sprite.unloadFont();
    day_for_sprite.pushSprite(&widget_bg_for_sprite, 0, 0, TFT_TRANSPARENT);
    day_for_sprite.deleteSprite();

    precip_for_sprite.createSprite(PRECIP_FOR_SPRITE_W, PRECIP_FOR_SPRITE_H);
    precip_for_sprite.setTextColor(TFT_WHITE, TFT_TRANSPARENT);
    precip_for_sprite.fillSprite(TFT_TRANSPARENT);
    if (precip_sum > 0)
    {
      precip_for_sprite.loadFont(LittleFS, "/arial_cyr18.vlw");
      precip_for_sprite.setTextDatum(MR_DATUM);
      precip_for_sprite.drawString(String(precip_sum) + " мм.", PRECIP_FOR_SPRITE_W / 2, PRECIP_FOR_SPRITE_H / 2);
      precip_for_sprite.unloadFont();
    }
    precip_for_sprite.pushSprite(&widget_bg_for_sprite, 0, WIDGET_FOR_H - PRECIP_FOR_SPRITE_H, TFT_TRANSPARENT);
    precip_for_sprite.deleteSprite();

    draw_geomagnetic_widget(static_cast<int>(kp_max), widget_bg_for_sprite, 5, DAY_FOR_SPRITE_H + 5);
  }
  widget_bg_for_sprite.drawRoundRect(0, 0, WIDGET_FOR_W, WIDGET_FOR_H, 8, TFT_WHITE);
  widget_bg_for_sprite.pushSprite(scr_x_pos, scr_y_pos);

  widget_bg_for_sprite.deleteSprite();

  // short yield to allow scheduler to run other tasks
  vTaskDelay(pdMS_TO_TICKS(1));

  return true;
}

bool MeteoWidgets::draw_home_in_data_widget(int scr_x_pos, int scr_y_pos, float temp_in, uint8_t humidity_in, bool in_valid)
{
  lgfx::LGFX_Sprite widget_bg_cur_sprite(&tft);
  // Right part width = HOME_ICON_WH (128) so it contains icon and indoor values
  if (!widget_bg_cur_sprite.createSprite(HOME_ICON_WH, WIDGET_HOME_H))
  {
    ESP_LOGE("WIDGET", "createSprite failed!");
    ESP_LOGI("WIDGET", "Heap largest free block: %u", heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT));
    return false;
  }
  ESP_LOGI("WIDGET", "Draw widget HOME_IN_DATA");
  widget_bg_cur_sprite.fillSprite(WIDGET_BG_COLOR);

  // Home icon (top-right)
  lgfx::LGFX_Sprite sprite_128(&tft);
  if (sprite_128.createSprite(HOME_ICON_WH, HOME_ICON_WH))
  {
    if (draw_png_2_sprite(HOME_PNG_NAME, sprite_128))
      sprite_128.pushSprite(&widget_bg_cur_sprite, 0, 0, TFT_BLACK);
    else
      ESP_LOGE("WIDGET", "Failed to load home icon: %s", HOME_PNG_NAME);
    sprite_128.deleteSprite();
  }

  // Indoor temperature
  lgfx::LGFX_Sprite temp_sprite(&tft);
  temp_sprite.createSprite(TEMP_HOME_SPRITE_W, TEMP_HOME_SPRITE_H);
  temp_sprite.fillSprite(TFT_TRANSPARENT);
  temp_sprite.loadFont(LittleFS, "/arial_cyr32.vlw");
  temp_sprite.setTextDatum(MC_DATUM);
  if (in_valid)
  {
    temp_sprite.setTextColor(getTempColor(temp_in), TFT_TRANSPARENT);
    temp_sprite.drawString(String(temp_in, 1) + "°", TEMP_HOME_SPRITE_W / 2, TEMP_HOME_SPRITE_H / 2);
  }
  else
  {
    temp_sprite.setTextColor(TFT_WHITE, TFT_TRANSPARENT);
    temp_sprite.drawString("--", TEMP_HOME_SPRITE_W / 2, TEMP_HOME_SPRITE_H / 2);
  }
  temp_sprite.unloadFont();
  temp_sprite.pushSprite(&widget_bg_cur_sprite, (HOME_ICON_WH - TEMP_HOME_SPRITE_W) / 2, WIDGET_HOME_H - HOME_ICON_WH * 0.65f, TFT_TRANSPARENT);
  temp_sprite.deleteSprite();

  // Indoor humidity
  lgfx::LGFX_Sprite humidity_sprite(&tft);
  humidity_sprite.createSprite(TEMP_HOME_SPRITE_W, TEMP_HOME_SPRITE_H);
  humidity_sprite.fillSprite(TFT_TRANSPARENT);
  humidity_sprite.loadFont(LittleFS, "/arial_cyr32.vlw");
  humidity_sprite.setTextDatum(MC_DATUM);
  if (in_valid)
    humidity_sprite.setTextColor(TFT_WHITE, TFT_TRANSPARENT), humidity_sprite.drawString(String(humidity_in) + "%", TEMP_HOME_SPRITE_W / 2, TEMP_HOME_SPRITE_H / 2);
  else
    humidity_sprite.setTextColor(TFT_WHITE, TFT_TRANSPARENT), humidity_sprite.drawString("--", TEMP_HOME_SPRITE_W / 2, TEMP_HOME_SPRITE_H / 2);
  humidity_sprite.unloadFont();
  humidity_sprite.pushSprite(&widget_bg_cur_sprite, (HOME_ICON_WH - TEMP_HOME_SPRITE_W) / 2, WIDGET_HOME_H - HOME_ICON_WH * 0.35f, TFT_TRANSPARENT);
  humidity_sprite.deleteSprite();

  widget_bg_cur_sprite.pushSprite(scr_x_pos + (WIDGET_HOME_W - HOME_ICON_WH), scr_y_pos);
  widget_bg_cur_sprite.deleteSprite();
  return true;
}

bool MeteoWidgets::draw_home_out_data_widget(int scr_x_pos, int scr_y_pos, float temp_out, uint8_t humidity_out, bool out_valid)
{
  lgfx::LGFX_Sprite widget_bg_cur_sprite(&tft);
  // Left part width = WIDGET_HOME_W - HOME_ICON_WH (92)
  if (!widget_bg_cur_sprite.createSprite(WIDGET_HOME_W - HOME_ICON_WH, WIDGET_HOME_H))
  {
    ESP_LOGE("WIDGET", "createSprite failed!");
    ESP_LOGI("WIDGET", "Heap largest free block: %u", heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT));
    return false;
  }
  ESP_LOGI("WIDGET", "Draw widget HOME_OUT_DATA");
  widget_bg_cur_sprite.fillSprite(WIDGET_BG_COLOR);

  // Outdoor temperature
  lgfx::LGFX_Sprite temp_sprite(&tft);
  temp_sprite.createSprite(TEMP_HOME_SPRITE_W, TEMP_HOME_SPRITE_H);
  temp_sprite.fillSprite(TFT_TRANSPARENT);
  temp_sprite.loadFont(LittleFS, "/arial_cyr32.vlw");
  temp_sprite.setTextDatum(MC_DATUM);
  if (out_valid)
  {
    temp_sprite.setTextColor(getTempColor(temp_out), TFT_TRANSPARENT);
    temp_sprite.drawString(String(round(temp_out), 0) + "°", TEMP_HOME_SPRITE_W / 2, TEMP_HOME_SPRITE_H / 2);
  }
  else
  {
    temp_sprite.setTextColor(TFT_WHITE, TFT_TRANSPARENT);
    temp_sprite.drawString("--", TEMP_HOME_SPRITE_W / 2, TEMP_HOME_SPRITE_H / 2);
  }
  temp_sprite.unloadFont();
  temp_sprite.pushSprite(&widget_bg_cur_sprite, 10, WIDGET_HOME_H - HOME_ICON_WH * 0.65f, TFT_TRANSPARENT);
  temp_sprite.deleteSprite();

  // Outdoor humidity
  lgfx::LGFX_Sprite humidity_sprite(&tft);
  humidity_sprite.createSprite(TEMP_HOME_SPRITE_W, TEMP_HOME_SPRITE_H);
  humidity_sprite.fillSprite(TFT_TRANSPARENT);
  humidity_sprite.loadFont(LittleFS, "/arial_cyr32.vlw");
  humidity_sprite.setTextDatum(MC_DATUM);
  humidity_sprite.setTextColor(TFT_WHITE, TFT_TRANSPARENT);
  if (out_valid)
    humidity_sprite.drawString(String(humidity_out) + "%", TEMP_HOME_SPRITE_W / 2, TEMP_HOME_SPRITE_H / 2);
  else
    humidity_sprite.drawString("--", TEMP_HOME_SPRITE_W / 2, TEMP_HOME_SPRITE_H / 2);
  humidity_sprite.unloadFont();
  humidity_sprite.pushSprite(&widget_bg_cur_sprite, 10, WIDGET_HOME_H - HOME_ICON_WH * 0.35f, TFT_TRANSPARENT);
  humidity_sprite.deleteSprite();

  // Label "УЛИЦА:" left-top
  lgfx::LGFX_Sprite txt_sprite(&tft);
  txt_sprite.createSprite(TEMP_HOME_SPRITE_W, TEMP_HOME_SPRITE_H);
  txt_sprite.fillSprite(TFT_TRANSPARENT);
  txt_sprite.loadFont(LittleFS, "/arial_cyr18.vlw");
  txt_sprite.setTextDatum(TC_DATUM);
  txt_sprite.drawString(/*"УЛИЦА:"*/ "", TEMP_HOME_SPRITE_W / 2, TEMP_HOME_SPRITE_H / 2);
  txt_sprite.unloadFont();
  txt_sprite.pushSprite(&widget_bg_cur_sprite, 0, 0, TFT_TRANSPARENT);
  txt_sprite.deleteSprite();

  widget_bg_cur_sprite.pushSprite(scr_x_pos, scr_y_pos);
  widget_bg_cur_sprite.deleteSprite();
  return true;
}

String MeteoWidgets::getDayOfWeek(const String &date)
{
  // Статический массив дней недели на русском (0 = Воскресенье, ..., 6 = Суббота)
  static const char *const daysOfWeek[] = {
      "Воскресенье", "Понедельник", "Вторник", "Среда", "Четверг", "Пятница", "Суббота"};

  // Разбор входной строки даты (dd-mm-yyyy)
  std::tm timeStruct = {};
  if (strptime(date.c_str(), "%d-%m-%Y", &timeStruct) == nullptr)
  {
    return "Неверная дата";
  }

  // Преобразовать в time_t
  std::time_t time = std::mktime(&timeStruct);
  if (time == -1)
  {
    return "Неверная дата";
  }

  // Использовать потокобезопасную версию localtime_r
  std::tm localTimeStruct = {};
  if (localtime_r(&time, &localTimeStruct) == nullptr)
  {
    return "Ошибка времени";
  }

  // Проверка на валидность индекса
  int dayOfWeek = localTimeStruct.tm_wday;
  if (dayOfWeek < 0 || dayOfWeek > 6)
  {
    return "Неверный день";
  }

  return daysOfWeek[dayOfWeek];
}

bool MeteoWidgets::draw_connection_state_widget(bool up, bool down, bool wifi)
{
  // choose up/down combined icon based on booleans
  const char *updown_png = UPDOWN_RED_RED_PNG_NAME;
  if (up && down)
    updown_png = UPDOWN_GREEN_GREEN_PNG_NAME;
  else if (up && !down)
    updown_png = UPDOWN_GREEN_RED_PNG_NAME;
  else if (!up && down)
    updown_png = UPDOWN_RED_GREEN_PNG_NAME;

  // create sprites for up/down and wifi icons
  lgfx::LGFX_Sprite updown_sprite(&tft);
  lgfx::LGFX_Sprite wifi_sprite(&tft);

  updown_sprite.createSprite(UPDOWN_ICON_WH, UPDOWN_ICON_WH);
  updown_sprite.fillSprite(TFT_TRANSPARENT);
  bool draw_updown_ok = draw_png_2_sprite(String(updown_png), updown_sprite);

  wifi_sprite.createSprite(WIFI_ICON_WH, WIFI_ICON_WH);
  wifi_sprite.fillSprite(TFT_TRANSPARENT);
  bool draw_wifi_ok = draw_png_2_sprite(wifi ? WIFI_GREEN_PNG_NAME : WIFI_RED_PNG_NAME, wifi_sprite);

  // positions: right-top corner, updown to the left of wifi
  const uint16_t padding = 4;
  uint16_t wifi_x = SCREEN_WIDTH - WIFI_ICON_WH - padding;
  uint16_t wifi_y = padding;
  // place updown icon immediately to the left of wifi icon
  uint16_t updown_x = wifi_x - UPDOWN_ICON_WH;
  uint16_t updown_y = padding;

  if (draw_updown_ok)
    updown_sprite.pushSprite(updown_x, updown_y, TFT_BLACK);
  if (draw_wifi_ok)
    wifi_sprite.pushSprite(wifi_x, wifi_y, TFT_BLACK);

  // Draw battery level widget to the left of updown icon if available
  // Placeholder level: leave caller responsible for providing real level.
  // If an external caller wants to update battery, call draw_battery_level_widget(level) separately.
  // Here we simply draw nothing; the actual placement is handled by draw_battery_level_widget when called.

  updown_sprite.deleteSprite();
  wifi_sprite.deleteSprite();
  return true;
}

bool MeteoWidgets::draw_city_name_widget(uint16_t pos_x, uint16_t pos_y, const String &cityName)
{
  // Widget size: use existing constants for consistency
  const uint16_t w = 160;
  const uint16_t h = 25;

  // Use a background sprite and an inner sprite for text, then push with transparency
  lgfx::LGFX_Sprite widget_bg_cur_sprite(&tft);
  if (!widget_bg_cur_sprite.createSprite(w, h))
  {
    ESP_LOGE("WIDGET", "createSprite for city bg failed!");
    return false;
  }
  widget_bg_cur_sprite.fillSprite(WIDGET_BG_COLOR);

  // Inner sprite to render text
  lgfx::LGFX_Sprite sprite(&tft);
  if (!sprite.createSprite(w, h))
  {
    ESP_LOGE("WIDGET", "createSprite for city name failed!");
    widget_bg_cur_sprite.deleteSprite();
    return false;
  }
  sprite.fillSprite(TFT_TRANSPARENT);

  // Prepare text: truncate to 24 characters
  String text = cityName;
  if (text.length() > 15 * 2)
    text = text.substring(0, 15 * 2);

  // Draw centered text using font arial_cyr18 and DATETIME_COLOR
  sprite.loadFont(LittleFS, "/arial_cyr18.vlw");
  sprite.setTextColor(DATETIME_COLOR, TFT_TRANSPARENT);
  sprite.setTextDatum(MC_DATUM); // middle center
  sprite.drawString(text, w / 2, h / 2);
  sprite.unloadFont();

  // Push inner sprite onto background (transparent pixels ignored), then push bg to screen preserving underlying pixels
  sprite.pushSprite(&widget_bg_cur_sprite, 0, 0, TFT_TRANSPARENT);
  sprite.deleteSprite();

  widget_bg_cur_sprite.pushSprite(pos_x, pos_y, TFT_TRANSPARENT);
  widget_bg_cur_sprite.deleteSprite();

  return true;
}

bool MeteoWidgets::draw_battery_level_widget(uint8_t level)
{
  // Avoid unnecessary redraw when level hasn't changed
  if (lastBatteryLevel == level)
    return true;

  const uint16_t padding = 4;
  uint16_t wifi_x = SCREEN_WIDTH - WIFI_ICON_WH - padding;
  // updown is immediately left of wifi
  uint16_t updown_x = wifi_x - UPDOWN_ICON_WH + padding;
  // battery sits to the left of updown
  uint16_t battery_x = (updown_x >= BATTERY_ICON_WH) ? (updown_x - BATTERY_ICON_WH) : 0;
  uint16_t y = padding; // slightly lower than wifi/updown изза высоты иконки батареи

  const char *icon = BATTERY_100_PNG_NAME;
  if (level <= 10)
    icon = BATTERY_0_PNG_NAME;
  else if (level <= 35)
    icon = BATTERY_25_PNG_NAME;
  else if (level <= 65)
    icon = BATTERY_50_PNG_NAME;
  else if (level <= 90)
    icon = BATTERY_75_PNG_NAME;
  else
    icon = BATTERY_100_PNG_NAME;

  lgfx::LGFX_Sprite battery_sprite(&tft);
  battery_sprite.createSprite(BATTERY_ICON_WH, BATTERY_ICON_WH);
  battery_sprite.fillSprite(TFT_TRANSPARENT);
  bool ok = draw_png_2_sprite(String(icon), battery_sprite);
  if (ok)
    battery_sprite.pushSprite(battery_x, y, TFT_BLACK);
  battery_sprite.deleteSprite();

  if (ok)
    lastBatteryLevel = level;

  return ok;
}

bool MeteoWidgets::draw_update_processing_widget()
{
  // Make the widget occupy the whole screen and center the message
  const uint16_t w = SCREEN_WIDTH;
  const uint16_t h = 80;

  lgfx::LGFX_Sprite widget_bg(&tft);
  lgfx::LGFX_Sprite txt_sprite(&tft);

  if (!widget_bg.createSprite(w, h))
  {
    ESP_LOGE("WIDGET", "createSprite for update-processing bg failed!");
    return false;
  }
  // Fill full-screen background with widget background color
  widget_bg.fillSprite(WIDGET_BG_COLOR);

  // Draw a rounded red framed rectangle inset from screen edges (draw twice for thicker border)
  const uint16_t margin = 20;
  const uint16_t rect_w = (w > margin * 2) ? (w - margin * 2) : w;
  const uint16_t rect_h = (h > margin * 2) ? (h - margin * 2) : h;
  const uint16_t radius_outer = 12;
  const uint16_t radius_inner = 10;
  // Outer rounded border
  widget_bg.drawRoundRect(margin, margin, rect_w, rect_h, radius_outer, TFT_RED);
  // Inner rounded border to make the frame visually thicker
  if (rect_w > 6 && rect_h > 6)
  {
    widget_bg.drawRoundRect(margin + 2, margin + 2, rect_w - 4, rect_h - 4, radius_inner, TFT_RED);
  }

  // Create an inner sprite for the centered text (use transparent background)
  if (!txt_sprite.createSprite(rect_w, rect_h))
  {
    ESP_LOGE("WIDGET", "createSprite for update-processing text failed!");
    widget_bg.deleteSprite();
    return false;
  }
  txt_sprite.fillSprite(TFT_TRANSPARENT);
  txt_sprite.loadFont(LittleFS, "/arial_cyr18.vlw");
  txt_sprite.setTextDatum(MC_DATUM);
  txt_sprite.setTextColor(TFT_WHITE, TFT_TRANSPARENT);
  txt_sprite.drawString("Update is processing. Please wait.", rect_w / 2, rect_h / 2);
  txt_sprite.unloadFont();

  // Composite: push the text into the framed rectangle area, then push full-screen
  txt_sprite.pushSprite(&widget_bg, margin, margin, TFT_TRANSPARENT);
  txt_sprite.deleteSprite();

  // Always push to (0,0) so widget covers entire screen
  widget_bg.pushSprite(0, (SCREEN_HEIGHT - h) / 2, TFT_TRANSPARENT);
  widget_bg.deleteSprite();

  vTaskDelay(pdMS_TO_TICKS(1));
  return true;
}

#if 0
bool MeteoWidgets::test_draw_widgets()
{
  static uint16_t wind_dir = 0;
  uint8_t padding = 6;

  // Отобразить виджет состояния соединения в правом верхнем углу
  draw_connection_state_widget(true, true, true);

  // Отобразить виджет "домашних" (датчик в доме и на улице) данных в верхнем левом углу
  draw_home_data_widget(0, CLOCK_DIGS_H + padding, 22.51f, 55, -5.3f, 80);

  static uint32_t last_clock_ms = 0;
  uint32_t now_ms = millis();
  std::time_t t = std::time(nullptr);
  std::tm *lt = std::localtime(&t);

  Serial.printf(" (delta ms = %u) \n", now_ms - last_clock_ms);
  if (now_ms - last_clock_ms >= 1000)
  {
    if (lt)
    {
      Serial.printf("Current time: %02d:%02d:%02d\n", lt->tm_hour, lt->tm_min, lt->tm_sec);
      // Показать цифровые часы в левом верхнем углу
      draw_dig_clock_widget(padding, padding, static_cast<uint8_t>(lt->tm_hour), static_cast<uint8_t>(lt->tm_min));
    }
    last_clock_ms = now_ms;
  }

  uint8_t weather_code = 1;

  // Показать текущую дату рядом с цифровыми часами
  if (lt)
  {
    char datestr[11];
    sprintf(datestr, "%04d-%02d-%02d", lt->tm_year + 1900, lt->tm_mon + 1, lt->tm_mday);
    draw_current_date_widget(CLOCK_DIGS_W + padding, 0 + padding, String(datestr));
  }

  // Показать виджет с текущей погодой в правом верхнем углу
  draw_meteo_current_widget(SCREEN_WIDTH - WIDGET_CUR_W, 60,
                            -38.3f,
                            100,
                            2.1f,
                            349,
                            1, true);

  // Показать виджеты с прогнозом погоды внизу экрана
  for (int i = 0; i < 3; i++)
  {
    draw_meteo_forecast_widget(WIDGET_FOR_W * i, SCREEN_HEIGHT - WIDGET_FOR_H,
                   -1.1f + i * 2,
                   -1.1f + i * 2 - 2.3f,
                   4.2 * i,
                   wind_dir,
                   i, // precip_sum
                   weather_code + i,
                   i == 1 ? "2026-01-04" : (i == 2 ? "2026-01-05" : "СЕГОДНЯ"), 1, true);
    wind_dir += 30;
  }

  return true;
}
#endif
