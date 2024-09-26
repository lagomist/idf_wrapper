#ifndef GATT_SERVER_WRAPPER_H
#define GATT_SERVER_WRAPPER_H

/* ensure not include gatts_svc_table_wrapper.h */
#ifndef GATTS_SVC_TABLE_WRAPPER_H

#include "esp_gatts_api.h"

#define GATTS_WRAPPER_DEFAULT_NAME				"ESP_GATTS"
#define GATTS_WRAPPER_DEFAULT_APP_ID      		0

#define GATTS_WRAPPER_DEFAULT_SVC_UUID			0x00FF
#define GATTS_WRAPPER_DEFAULT_CHAR_UUID			0xFF01
#define GATTS_WRAPPER_DEFAULT_CHAR_INDEX    	0
#define GATTS_WRAPPER_DEFAULT_MTU_SIZE			500
/**
*  The max length of characteristic value. When the GATT client performs a write or prepare write operation,
*  the data length must be less than GATTS_WRAPPER_VAL_LEN_MAX.
*/
#define GATTS_WRAPPER_VAL_LEN_MAX				32


typedef struct {
	esp_gatt_if_t gatts_if;		// 服务接口
	uint16_t conn_id;			// 连接ID
	uint16_t handle;			// 属性句柄
	uint8_t *data;				// 接收数据
	int len;					// 接收数据长度
	uint8_t index;				// 特征标识
} gatts_wrapper_recv_t;

// 定义 GATTS 接收数据的回调函数原型
typedef void (*gatts_wrapper_recv_cb_t)(gatts_wrapper_recv_t);

/* 特征配置结构体 */
typedef struct {
	uint8_t index;						// 特征标识索引
    esp_bt_uuid_t uuid;					// 特征UUID
    esp_gatt_perm_t perm;				// 特征值属性权限
    esp_gatt_char_prop_t property;		// 特征属性
} gatts_wrapper_char_config_t;


/* GAP 配置结构体 */
typedef struct {
	char *dev_name;			// 广播设备名
	uint16_t adv_uuid;		// 广播服务UUID
	uint8_t *adv_data;		// 广播制造商数据
	uint8_t adv_len;		// 广播制造商数据长度
} gatts_wrapper_gap_config_t;



/* ------------------function declared-------------------- */

/* 发送通知或指示 */ 
// esp_ble_gatts_send_indicate();


/**
 * @brief BLE GATT服务器GAP配置
 * 
 * @param config GAP配置
 */
void gatts_wrapper_gap_config(gatts_wrapper_gap_config_t *config);

/**
 * @brief BLE GATT服务器初始化
 * 
 */
void gatt_server_wrapper_init();

/**
 * @brief GATT 创建一个默认服务
 * 
 */
void gatts_wrapper_create_default_service();

/**
 * @brief GATT 添加一个服务实例
 * 
 * @param app_id 实例ID
 * @param service_uuid 服务UUID配置
 */
void gatts_wrapper_add_service(uint16_t app_id, esp_bt_uuid_t *service_uuid);

/**
 * @brief GATT 删除一个服务实例
 * 
 * @param app_id 要删除的实例ID
 */
void gatt_server_wrapper_delete_service(uint16_t app_id);

/**
 * @brief GATT 服务添加一个特征
 * 
 * @param app_id 服务实例ID
 * @param config 特征配置
 */
void gatts_wrapper_service_add_char(uint16_t app_id, gatts_wrapper_char_config_t *config);

/**
 * @brief GATT 服务注册接收回调，并启动服务实例
 * 
 * @param app_id 服务实例ID
 * @param callback 特征数据接收回调
 */
void gatts_wrapper_service_register_callback(uint16_t app_id, gatts_wrapper_recv_cb_t callback);

/**
 * @brief GATT 默认服务接收回调，启动默认服务实例
 * 
 * @param callback 接收回调
 */
void gatts_wrapper_default_service_register_callback(gatts_wrapper_recv_cb_t callback);


#endif /* ifndef GATTS_SVC_TABLE_WRAPPER_H */

#endif /* #ifndef GATT_SERVER_WRAPPER_H */
