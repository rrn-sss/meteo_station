#ifndef METEO_WIDGETS_H
#define METEO_WIDGETS_H

#include "common.h"
#include <LittleFS.h>
#include <LovyanGFX.hpp>
#include <PNGdec.h>
#include <unordered_map>

/**
 * @brief Класс для рисования виджетов погоды на TFT-дисплее
 */
class MeteoWidgets
{
public:
  // Публичные аксессоры размеров виджетов/макета (полезно для внешнего позиционирования)
  static uint16_t getScreenWidth()
  {
    return SCREEN_WIDTH;
  }
  static uint16_t getScreenHeight()
  {
    return SCREEN_HEIGHT;
  }
  static uint16_t getClockDigsW()
  {
    return CLOCK_DIGS_W;
  }
  static uint16_t getClockDigsH()
  {
    return CLOCK_DIGS_H;
  }
  static uint16_t getWidgetCurW()
  {
    return WIDGET_CUR_W;
  }
  static uint16_t getWidgetForW()
  {
    return WIDGET_FOR_W;
  }
  static uint16_t getWidgetForH()
  {
    return WIDGET_FOR_H;
  }

  /**
   * @brief Создать или вернуть единственный экземпляр `MeteoWidgets`.
   * @param tft Ссылка на объект TFT_eSPI для работы с дисплеем
   * @return Указатель на единственный экземпляр (создаёт его при необходимости)
   */
  static MeteoWidgets *createInstance(LGFX &tft);

  /**
   * @brief Вернуть указатель на существующий экземпляр `MeteoWidgets`.
   * @return Указатель на экземпляр или nullptr если не создан
   */
  static MeteoWidgets *getInstance();

  /**
   * @brief Удалить единственный экземпляр `MeteoWidgets` (если создан)
   */
  static void destroyInstance();

  /**
   * @brief Инициализация виджетов: создание спрайтов и загрузка шрифтов
   */
  void init();

  /**
   * @brief Рисование цифрового виджета часов
   * @param pos_x Позиция X на экране
   * @param pos_y Позиция Y на экране
   * @param hh Часы
   * @param mm Минуты
   * @return true при успехе, false при ошибке
   */
  bool draw_dig_clock_widget(uint16_t pos_x, uint16_t pos_y, uint8_t hh, uint8_t mm);

  /**
   * @brief Рисование виджета с текущей датой рядом с цифровыми часами
   * @param date Строка с датой
   * @return true при успехе
   */
  bool draw_current_date_widget(uint16_t pos_x, uint16_t pos_y, const String &date);

  /**
   * @brief Рисование виджета текущей погоды
   * @param scr_x_pos Позиция X на экране
   * @param scr_y_pos Позиция Y на экране
   * @param cur_temp Текущая температура
   * @param humidity Влажность (в процентах)
   * @param wind_speed Скорость ветра
   * @param wind_dir Направление ветра
   * @param weather_code Код погоды
   * @return true при успехе, false при ошибке
   */
  bool draw_meteo_current_widget(int scr_x_pos, int scr_y_pos, float cur_temp, uint8_t humidity,
                                 float wind_speed, uint16_t wind_dir, uint8_t weather_code, bool valid);

  // Split current weather widget: icon and info parts (each max 128x128)
  bool draw_meteo_current_icon_widget(int scr_x_pos, int scr_y_pos, uint8_t weather_code, bool valid);
  bool draw_meteo_current_info_widget(int scr_x_pos, int scr_y_pos, float cur_temp, uint8_t humidity,
                                      float wind_speed, uint16_t wind_dir, bool valid);

  /**
   * @brief Рисование виджета прогноза погоды
   * @param scr_x_pos Позиция X на экране
   * @param scr_y_pos Позиция Y на экране
   * @param min_temp Минимальная температура
   * @param max_temp Максимальная температура
   * @param wind_speed Скорость ветра
   * @param wind_dir Направление ветра
   * @param weather_code Код погоды
   * @param data Строка с датой
   * @param kp_max Максимальный индекс геомагнитной обстановки
   * @return true при успехе, false при ошибке
   */
  bool draw_meteo_forecast_widget(int scr_x_pos, int scr_y_pos, float min_temp, float max_temp,
                                  float wind_speed, uint16_t wind_dir, uint16_t precip_sum,
                                  uint8_t weather_code, const String &data, float kp_max, bool valid);

  /**
   * @brief Рисование виджета внутреннего датчика (в доме)
   * @param scr_x_pos Позиция X на экране
   * @param scr_y_pos Позиция Y на экране
   * @param temp_in Внутренняя температура
   * @param humidity_in Влажность внутри (в процентах)
   * @param in_valid Флаг валидности данных
   * @return true при успехе, false при ошибке
   */
  bool draw_home_in_data_widget(int scr_x_pos, int scr_y_pos, float temp_in, uint8_t humidity_in, bool in_valid);

  /**
   * @brief Рисование виджета внешнего датчика (улица)
   * @param scr_x_pos Позиция X на экране
   * @param scr_y_pos Позиция Y на экране
   * @param temp_out Наружная температура
   * @param humidity_out Влажность снаружи (в процентах)
   * @param out_valid Флаг валидности данных
   * @return true при успехе, false при ошибке
   */
  bool draw_home_out_data_widget(int scr_x_pos, int scr_y_pos, float temp_out, uint8_t humidity_out, bool out_valid);

  /**
   * @brief Рисование состояния соединения (wifi)
   * @param up Вверх (не используется визуально сейчас)
   * @param down Вниз (не используется визуально сейчас)
   * @param wifi Состояние wifi: true = зелёная иконка, false = красная
   * @return true при успехе
   */
  bool draw_connection_state_widget(bool up, bool down, bool wifi);

  /**
   * @brief Рисование виджета с названием населённого пункта
   * @param pos_x Позиция X на экране
   * @param pos_y Позиция Y на экране
   * @param cityName Название населённого пункта (отображается до 24 символов)
   * @return true при успехе, false при ошибке
   */
  bool draw_city_name_widget(uint16_t pos_x, uint16_t pos_y, const String &cityName);

  /**
   * @brief Рисование виджета уровня заряда батареи внешнего датчика
   * @param level Уровень заряда в процентах (0..100)
   * @return true при успехе
   */
  bool draw_battery_level_widget(uint8_t level);

  /**
   * @brief Рисование простого информационного виджета во время обновления
   * Отрисовывает фон как у остальных виджетов и центрированный текст
   * "Update is processing. Please wait."
   * @return true при успехе, false при ошибке
   */
  bool draw_update_processing_widget();

#if 0
  /**
   * @brief Рисование всех виджетов на экране (в тестовых целях с тестовыми данными)
   * @return true при успехе, false при ошибке
   */
  bool test_draw_widgets();
#endif

private:
  // запрещаем копирование и перемещение
  MeteoWidgets(const MeteoWidgets &) = delete;
  MeteoWidgets &operator=(const MeteoWidgets &) = delete;
  MeteoWidgets(MeteoWidgets &&) = delete;
  MeteoWidgets &operator=(MeteoWidgets &&) = delete;

  /**
   * @brief Конструктор (приватный — используйте createInstance)
   */
  MeteoWidgets(LGFX &tft);

  /**
   * @brief Деструктор
   */
  virtual ~MeteoWidgets();
  /** @brief Указатель на текущий экземпляр класса для статических callback-функций */
  static MeteoWidgets *currentInstance;

  /** @brief Ссылка на объект LGFX для работы с дисплеем */
  LGFX &tft;

  /** @brief Объект для декодирования PNG */
  PNG png;

  /** @brief Файл для чтения PNG */
  fs::File pngfile;
  /** @brief Последний закешированный уровень батареи (0..100), 255 = unset */
  uint8_t lastBatteryLevel = 255;
  // Все `TFT_eSprite` теперь создаются локально в реализациях методов

  /** @brief Ширина экрана */
  static const uint16_t SCREEN_WIDTH = 480;
  /** @brief Высота экрана */
  static const uint16_t SCREEN_HEIGHT = 320;

  /** @brief Имя файла PNG для ветра */
  static const char *WIND_PNG_NAME;
  /** @brief Имя файла PNG для влажности */
  static const char *HUMIDITY_PNG_NAME;
  /** @brief Имя файла PNG для термометра */
  static const char *THERMOMETER_PNG_NAME;
  /** @brief Имя файла PNG для дома */
  static const char *HOME_PNG_NAME;
  /** @brief Имя файла PNG для wifi (зелёная) */
  static const char *WIFI_GREEN_PNG_NAME;
  /** @brief Имя файла PNG для wifi (красная) */
  static const char *WIFI_RED_PNG_NAME;
  /** @brief Имя файла PNG для статуса up/down (зелен-зелен) */
  static const char *UPDOWN_GREEN_GREEN_PNG_NAME;
  /** @brief Имена файлов PNG для индикатора батареи */
  static const char *BATTERY_0_PNG_NAME;
  static const char *BATTERY_25_PNG_NAME;
  static const char *BATTERY_50_PNG_NAME;
  static const char *BATTERY_75_PNG_NAME;
  static const char *BATTERY_100_PNG_NAME;
  /** @brief Имя файла PNG для статуса up/down (зелен-красн) */
  static const char *UPDOWN_GREEN_RED_PNG_NAME;
  /** @brief Размер иконки батареи (ширина/высота) */
  static const uint16_t BATTERY_ICON_WH = 32;
  /** @brief Имя файла PNG для статуса up/down (красн-зелен) */
  static const char *UPDOWN_RED_GREEN_PNG_NAME;
  /** @brief Имя файла PNG для статуса up/down (красн-красн) */
  static const char *UPDOWN_RED_RED_PNG_NAME;

  /** @brief Размер иконки погоды (ширина и высота) */
  static const uint16_t ICON_WH = 128;
  /** @brief Размер иконки дома (ширина и высота) */

  static const uint16_t HOME_ICON_WH = ICON_WH;
  /** @brief Размер иконки термометра */
  static const uint16_t THERMOMETER_ICON_WH = 48;
  /** @brief Размер иконки ветра */
  static const uint16_t WIND_ICON_WH = 48;
  /** @brief Ширина спрайта влажности */
  static const uint16_t HUMIDITY_SPRITE_W = 48;
  /** @brief Высота спрайта влажности */
  static const uint16_t HUMIDITY_SPRITE_H = 100;
  /** @brief Размер иконки влажности */
  static const uint16_t HUMIDITY_ICON_WH = 48;
  /** @brief Размер иконки wifi */
  static const uint16_t WIFI_ICON_WH = 32;
  /** @brief Размер иконки up/down (ширина/высота) */
  static const uint16_t UPDOWN_ICON_WH = 32;
  /** @brief Ширина виджета текущей погоды */
  static const uint16_t WIDGET_CUR_W = (ICON_WH + 122);
  /** @brief Высота виджета текущей погоды */
  static const uint16_t WIDGET_CUR_H = (ICON_WH + 0);
  /** @brief Ширина виджета прогноза */
  static const uint16_t WIDGET_FOR_W = (ICON_WH + 32);
  /** @brief Высота виджета прогноза */
  static const uint16_t WIDGET_FOR_H = (ICON_WH + 0);
#if DRAW_THERMOMETER_ICON_IN_CURRENT_WIDGET
  /** @brief Ширина спрайта температуры текущей */
  static const uint16_t TEMP_CUR_SPRITE_W = 92;
  /** @brief Высота спрайта температуры текущей */
  static const uint16_t TEMP_CUR_SPRITE_H = 60;
#else
  /** @brief Ширина спрайта температуры текущей */
  static const uint16_t TEMP_CUR_SPRITE_W = (WIDGET_CUR_W - ICON_WH);
  /** @brief Высота спрайта температуры текущей */
  static const uint16_t TEMP_CUR_SPRITE_H = 60;
#endif
  /** @brief Ширина спрайта температуры прогноза */
  static const uint16_t TEMP_FOR_SPRITE_W = 64;
  /** @brief Высота спрайта температуры прогноза */
  static const uint16_t TEMP_FOR_SPRITE_H = 60;
  /** @brief Ширина спрайта цифр часов */
  static const uint16_t CLOCK_DIGS_W = 170;
  /** @brief Высота спрайта цифр часов */
  static const uint16_t CLOCK_DIGS_H = 50;
  /** @brief Ширина спрайта дня прогноза */
  static const uint16_t DAY_FOR_SPRITE_W = (WIDGET_FOR_W - TEMP_FOR_SPRITE_W);
  /** @brief Высота спрайта дня прогноза */
  static const uint16_t DAY_FOR_SPRITE_H = 30;
  /** @brief Ширина спрайта осадков прогноза */
  static const uint16_t PRECIP_FOR_SPRITE_W = 130;
  /** @brief Высота спрайта осадков прогноза */
  static const uint16_t PRECIP_FOR_SPRITE_H = 20;
  /** @brief Ширина виджета ветра прогноза */
  static const uint16_t WIDGET_FOR_WIND_W = (TEMP_FOR_SPRITE_W);
  /** @brief Высота виджета ветра прогноза */
  static const uint16_t WIDGET_FOR_WIND_H = (WIDGET_FOR_H - TEMP_FOR_SPRITE_H + 5);
  /** @brief Ширина текста ветра */
  static const uint16_t WINDTXT_FOR_WIND_W = WIDGET_FOR_WIND_W;
  /** @brief Высота текста ветра */
  static const uint16_t WINDTXT_FOR_WIND_H = 20;
  /** @brief Ширина виджета дома */
  static const uint16_t WIDGET_HOME_W = 220;
  /** @brief Высота виджета дома */
  static const uint16_t WIDGET_HOME_H = WIDGET_CUR_H;
  /** @brief Ширина спрайта температуры дома */
  static const uint16_t TEMP_HOME_SPRITE_W = 88;
  /** @brief Высота спрайта температуры дома */
  static const uint16_t TEMP_HOME_SPRITE_H = 50;
  /** @brief Ширина спрайта геомагнитной интенсивности */
  static const uint16_t GEOMAGNETIC_SPRITE_WH = 24; // 16;

  /** @brief Цвет фона виджетов */
  static const uint16_t WIDGET_BG_COLOR = TFT_DARKGREY;
  /** @brief Цвет часов/даты */
  static const uint16_t DATETIME_COLOR = TFT_YELLOW;

  /**@brief Структура для информации о погоде */
  struct WeatherInfo
  {
    /** @brief Описание погоды */
    String description;
    /** @brief Имя файла иконки */
    String iconName;
  };

  /** @brief Карта кодов погоды к информации о погоде на русском */
  static const std::unordered_map<uint8_t, WeatherInfo> weatherInfoRu;

  /**
   * @brief Получение информации о погоде по коду
   * @param code Код погоды
   * @return Структура WeatherInfo с описанием и иконкой
   */
  WeatherInfo getWeatherInfo(int code);

  /**
   * @brief Получение цвета для температуры
   * @param temp Значение температуры
   * @return Цвет в формате uint16_t
   */
  uint16_t getTempColor(float temp);

  /**
   * @brief Callback для рисования PNG 128*128
   * @param pDraw Указатель на структуру PNGDRAW
   * @return Код результата
   */
  static int pngDraw128Callback(PNGDRAW *pDraw);
  /**
   * @brief Callback для рисования PNG 48*48
   * @param pDraw Указатель на структуру PNGDRAW
   * @return Код результата
   */
  static int pngDraw48Callback(PNGDRAW *pDraw);
  /**
   * @brief Callback для рисования PNG 16*16
   * @param pDraw Указатель на структуру PNGDRAW
   * @return Код результата
   */
  static int pngDraw16Callback(PNGDRAW *pDraw);
  /**
   * @brief Callback для рисования PNG 24*24
   * @param pDraw Указатель на структуру PNGDRAW
   * @return Код результата
   */
  static int pngDraw24Callback(PNGDRAW *pDraw);
  /**
   * @brief Callback для рисования PNG влажности
   * @param pDraw Указатель на структуру PNGDRAW
   * @return Код результата
   */
  static int pngDrawHumidityCallback(PNGDRAW *pDraw);
  /**
   * @brief Callback для открытия файла PNG
   * @param filename Имя файла
   * @param size Указатель на размер файла
   * @return Указатель на handle файла
   */
  static void *pngOpenCallback(const char *filename, int32_t *size);
  /**
   * @brief Callback для закрытия файла PNG
   * @param handle Указатель на handle файла
   */
  static void pngCloseCallback(void *handle);
  /**
   * @brief Callback для чтения из файла PNG
   * @param page Указатель на структуру PNGFILE
   * @param buffer Буфер для данных
   * @param length Длина чтения
   * @return Количество прочитанных байт
   */
  static int32_t pngReadCallback(PNGFILE *page, uint8_t *buffer, int32_t length);
  /**
   * @brief Callback для позиционирования в файле PNG
   * @param page Указатель на структуру PNGFILE
   * @param position Позиция
   * @return Новая позиция
   */
  static int32_t pngSeekCallback(PNGFILE *page, int32_t position);
  /**
   * @brief Рисование PNG до 128*128 в спрайт
   * @param png_file_name Имя файла PNG
   * @param target Спрайт в который будет рисоваться
   * @return true при успехе, false при ошибке
   */
  bool draw_png_2_sprite(const String &png_file_name, lgfx::LGFX_Sprite &target);

  /**
   * @brief Рисование виджета влажности
   * @param humidity Значение влажности в процентах
   * @param nested_sprite Спрайт для вложенного рисования
   * @param nested_pos_x Позиция X внутри спрайта
   * @param nested_pos_y Позиция Y внутри спрайта
   * @return true при успехе, false при ошибке
   */
  bool draw_humidity_widget(uint8_t humidity, lgfx::LGFX_Sprite &nested_sprite, uint16_t nested_pos_x, uint16_t nested_pos_y);
  /**
   * @brief Рисование виджета геомагнитной обстановки
   * @param kr геомагнитный индекс
   * @param nested_sprite Спрайт для вложенного рисования
   * @param nested_pos_x Позиция X внутри спрайта
   * @param nested_pos_y Позиция Y внутри спрайта
   * @return true при успехе, false при ошибке
   */
  bool draw_geomagnetic_widget(uint8_t kr, lgfx::LGFX_Sprite &nested_sprite, uint16_t nested_pos_x, uint16_t nested_pos_y);
  /**
   * @brief Рисование виджета ветра
   * @param wind_speed Скорость ветра
   * @param wind_dir Направление ветра в градусах
   * @param nested_sprite Спрайт для вложенного рисования
   * @param nested_pos_x Позиция X внутри спрайта
   * @param nested_pos_y Позиция Y внутри спрайта
   * @return true при успехе, false при ошибке
   */
  bool draw_wind_widget(float wind_speed, uint16_t wind_dir, lgfx::LGFX_Sprite &nested_sprite, uint16_t nested_pos_x, uint16_t nested_pos_y);
  /**
   * @brief Получение дня недели на русском языке по дате
   * @param date Дата в формате yyyy-mm-dd
   * @return Название дня недели на русском языке
   */
  static String getDayOfWeek(const String &date);
};

#endif // METEO_WIDGETS_H