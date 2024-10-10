#pragma once

/* wifi wrapper config */
#define WIFI_WRAPPER_CONFIG_DNS                 "8.8.8.8"
#define WIFI_WRAPPER_CONFIG_CHANNEL             7
#define WIFI_WRAPPER_SOFTAP_MAXCON              8

/* ble wrapper config */
#define BLE_WRAPPER_DEFAULT_SVC_UUID			0x00FF
#define BLE_WRAPPER_DEFAULT_CHAR_UUID			0xFF01
#define BLE_WRAPPER_DEFAULT_MTU_SIZE			512
/**
*  The max length of characteristic value. When the GATT client performs a write or prepare write operation,
*  the data length must be less than DEFAULT_GATTS_CHAR_VAL_LEN_MAX.
*/
#define DEFAULT_GATTS_CHAR_VAL_LEN_MAX          256
#define MANUFACTURER_DATA_LEN_MAX               30