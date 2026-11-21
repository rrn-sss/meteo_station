#ifndef _CFG_VALIDATE_H_
#define _CFG_VALIDATE_H_

#include "common.h"

/**
 * @file cfg_validate.h
 * @brief Проверка и нормализация пользовательской конфигурации (PrjCfgData).
 *
 * Значения из веб-портала и NVS нельзя использовать напрямую: atoi/atof/strtol
 * молча возвращают 0 на мусоре, а координаты вообще уходят в URL Open-Meteo
 * строкой. Модуль даёт строгий разбор (без «хвостов» после числа) и проверку
 * диапазонов, чтобы ошибка конфигурации была видна в логе, а не превращалась
 * в неверные данные на экране.
 */
namespace CfgValidate
{
/// @brief Первая найденная ошибка конфигурации
struct Error
{
  const char *field; ///< имя проблемного поля (nullptr — ошибок нет)
  const char *msg;   ///< описание проблемы

  constexpr Error(const char *f = nullptr, const char *m = nullptr) : field(f), msg(m) {}

  explicit operator bool() const { return field != nullptr; }
};

/**
 * @brief Строгий разбор целого числа
 * @param value строка (пробелы по краям допускаются)
 * @param min_value нижняя граница включительно
 * @param max_value верхняя граница включительно
 * @param out результат разбора
 * @return true, если строка целиком является числом в заданном диапазоне
 */
bool parseLong(const char *value, long min_value, long max_value, long &out);

/**
 * @brief Строгий разбор числа с плавающей точкой
 *        Запятая принимается как десятичный разделитель ("47,23" -> 47.23),
 *        "nan"/"inf" и значения с хвостом ("47.2abc") отвергаются.
 * @param value строка (пробелы по краям допускаются)
 * @param min_value нижняя граница включительно
 * @param max_value верхняя граница включительно
 * @param out результат разбора
 * @return true, если строка целиком является числом в заданном диапазоне
 */
bool parseDouble(const char *value, double min_value, double max_value, double &out);

/**
 * @brief Проверка всех полей конфигурации
 * @return первая найденная ошибка либо Error{} (пустой) если конфиг корректен
 */
Error check(const PrjCfgData &cfg);

/**
 * @brief Замена невалидных полей значениями по умолчанию (для старта из NVS)
 * @param cfg конфигурация, изменяется на месте
 * @return количество исправленных полей
 */
int applyDefaults(PrjCfgData &cfg);

} // namespace CfgValidate

#endif // _CFG_VALIDATE_H_
