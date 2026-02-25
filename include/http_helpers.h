#ifndef _HTTP_HELPERS_H_
#define _HTTP_HELPERS_H_

#include <Arduino.h>

/**
 * @brief Выполняет HTTP GET и возвращает тело ответа
 * @param serverName URL для запроса
 * @return тело ответа или "{}" при ошибке
 */
// HTTP helpers are implemented in src/http_helpers.cpp (send_HTTP_GET_request)
String send_HTTP_GET_request(const char *serverName);

/**
 * @brief Выполняет HTTPS GET с опциональным CA сертификатом и User-Agent
 * @param serverName URL для запроса
 * @param ca_cert PEM-строка корневого сертификата или nullptr
 * @param user_agent Заголовок User-Agent (опционально)
 * @return тело ответа или "{}" при ошибке
 */
String send_HTTPS_GET_request(const char *serverName, const char *ca_cert, const String &user_agent = String(""));

#endif // _HTTP_HELPERS_H_
