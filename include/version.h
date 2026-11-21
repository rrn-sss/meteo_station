#ifndef _VERSION_H_
#define _VERSION_H_

/**
 * @file version.h
 * @brief Версия прошивки, которая сравнивается с manifest.json при OTA.
 *
 * Значение — версия, реально работающая в устройстве. Оно не хранится в NVS:
 * NVS-записи о прошивке недостоверны после прошивки по USB, ручного отката
 * и сбоя питания. Для релизных сборок значение переопределяется в CI:
 *   PLATFORMIO_BUILD_FLAGS='-DAPP_VERSION="1.2.3"'
 * Локальная сборка использует значение ниже, его нужно поднимать вручную.
 */

#ifndef APP_VERSION
#define APP_VERSION "1.1.7"
#endif

#endif // _VERSION_H_
