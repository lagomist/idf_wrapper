#ifndef GATTS_SVC_TABLE_WRAPPER_H
#define GATTS_SVC_TABLE_WRAPPER_H

#ifndef GATT_SERVER_WRAPPER_H
#include "esp_gatts_api.h"


#define DEFAULT_GATTS_SERVICE_UUID		0x00FF
#define DEFAULT_GATTS_CHAR_UUID			0xFF01
#define DEFAULT_GATTS_MTU_SIZE			500
/**
*  The max length of characteristic value. When the GATT client performs a write or prepare write operation,
*  the data length must be less than DEFAULT_GATTS_CHAR_VAL_LEN_MAX.
*/
#define DEFAULT_GATTS_CHAR_VAL_LEN_MAX	500


/* Attributes State Machine */
enum
{
    IDX_SVC,

	IDX_PRIMARY_CHAR,
	IDX_PRIMARY_CHAR_VAL,
	IDX_PRIMARY_CHAR_CFG,

    HRS_IDX_NB,
};


typedef struct {
	esp_gatt_if_t gatts_if;
	uint16_t conn_id;
	uint8_t index;
	uint8_t *data;
	int len;
} gatts_recv_data_t;

// 定义 GATTS 接收数据的回调函数原型
typedef void (*gatts_receive_callback_t)(gatts_recv_data_t);


/* GATT服务器配置 */
typedef struct {
	char *dev_name;			// GATT服务器名与广播名
	uint16_t service_uuid;	// 服务UUID
	uint16_t char_uuid;		// 服务特征UUID
} gatts_wrapper_svc_config_t;


/*---------------------------------------------*/

/**
 * @brief GATT服务器配置
 * 
 * @param config 
 */
void gatt_server_wrapper_config(gatts_wrapper_svc_config_t config);

/**
 * @brief 初始化蓝牙，开启GATT服务
 * 
 */
void ble_gatt_server_wrapper_init();

// 找到属性表特征下标
uint8_t find_char_and_desr_index(uint16_t handle);

// 注册 GATTS 接收数据的回调函数
void gatt_server_register_recv_callback(gatts_receive_callback_t callback);

#endif /* #ifndef GATT_SERVER_WRAPPER_H */

#endif /* #ifndef GATTS_SVC_TABLE_WRAPPER_H */