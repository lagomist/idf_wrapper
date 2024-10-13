#pragma once

#include <stdint.h>

namespace WrapperConfig {

/* wifi wrapper config */
constexpr const char* WIFI_DNS      = "8.8.8.8";
constexpr uint8_t WIFI_CHANNEL      = 7;
constexpr uint8_t WIFI_MAXCON       = 8;

/* ble wrapper config */
constexpr uint16_t DEFAULT_SVC_UUID     = 0x00FF;
constexpr uint16_t DEFAULT_CHAR_UUID    = 0xFF01;
constexpr uint16_t DEFAULT_MTU_SIZE     = 512;
constexpr uint16_t CHAR_MAX_LEN         = 256;
constexpr uint8_t ADV_MAX_LEN           = 30;

/* socket wrapper config */
constexpr uint8_t SOCK_MAXCON_NUM       = 8;

}
