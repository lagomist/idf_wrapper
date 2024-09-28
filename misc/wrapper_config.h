#pragma once

/* wifi wrapper config */
#define WIFI_WRAPPER_CONFIG_DNS                 "8.8.8.8"
#define WIFI_WRAPPER_CONFIG_CHANNEL             7
#define WIFI_WRAPPER_SOFTAP_MAXCON              8

/* ble wrapper config */
#define GATTS_WRAPPER_DEFAULT_SVC_UUID			0x00FF
#define GATTS_WRAPPER_DEFAULT_CHAR_UUID			0xFF01
#define GATTS_WRAPPER_DEFAULT_MTU_SIZE			500
/**
*  The max length of characteristic value. When the GATT client performs a write or prepare write operation,
*  the data length must be less than GATTS_WRAPPER_VAL_LEN_MAX.
*/
#define GATTS_WRAPPER_VAL_LEN_MAX				32
