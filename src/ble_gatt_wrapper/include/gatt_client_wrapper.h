#ifndef GATT_CLIENT_WRAPPER_H
#define GATT_CLIENT_WRAPPER_H

#include "esp_gattc_api.h"

#define GATTC_WRAPPER_DEFAULT_REMOTE_NAME	"ESP_GATTS"
#define GATTC_WRAPPER_DEFAULT_SVC_UUID      0x00FF
#define GATTC_WRAPPER_DEFAULT_CHAR_UUID     0xFF01
#define GATTC_WRAPPER_DEFAULT_CHAR_INDEX    0
#define GATTC_WRAPPER_DEFAULT_MTU_SIZE		500


/* GATT客户端状态类型 */
typedef enum {
    GATTC_WRAPPER_CONNECTED,            // 连接成功
    GATTC_WRAPPER_NOTIFY_ENABLE,        // 通知使能成功
    GATTC_WRAPPER_WRITE_COMP,           // 数据发送成功
} gattc_wrapper_status_t;


/* 接收回调参数类型 */
typedef struct {
    esp_gatt_if_t gattc_if;
    uint8_t *data;
    uint16_t len;
    uint16_t handle;
    uint8_t index;
} gattc_wrapper_recv_t;

// GATTC 接收数据的回调函数原型
typedef void (*gattc_wrapper_recv_cb_t)(gattc_wrapper_recv_t);


/* 特征配置结构体 */
typedef struct {
	uint8_t index;						// 特征标识索引
    esp_bt_uuid_t uuid;					// 特征UUID
} gattc_wrapper_char_config_t;


/* 服务配置类型定义 */
typedef struct {
    char *server_name;              // 目标设备名
    esp_bt_uuid_t service_uuid;     // 目标服务UUID
} gattc_wrapper_svc_config_t;

/* --------------- declare function ---------------- */


/**
 * @brief 写入特征值，发送数据
 * 
 * @param index 特征标识
 * @param data 发送数据
 * @param data_len 数据长度
 * @return int 0：成功，其他：失败
 */
int gattc_wrapper_write_char(uint8_t index, uint8_t *data, uint16_t data_len);

/**
 * @brief BLE GATT客户端初始化
 * 
 */
void gatt_client_wrapper_init();

/**
 * @brief 客户端服务配置
 * 
 * @param config 配置远程设备名、服务UUID
 */
void gattc_wrapper_service_config(gattc_wrapper_svc_config_t *config);

/**
 * @brief 添加感兴趣的特征，开启通知
 * 
 * @param config 特征配置
 */
void gattc_wrapper_add_interested_char(gattc_wrapper_char_config_t *config);

/**
 * @brief 根据默认配置创建服务
 * 
 */
void gattc_wrapper_create_default_service();

/**
 * @brief 注册接收回调，并注册GATTC APP启动服务。该函数应最后调用
 * 
 * @param callback 特征通知接收回调
 */
void gattc_wrapper_register_callback(gattc_wrapper_recv_cb_t callback);

/**
 * @brief 等待事件状态
 * 
 * @param status 状态位
 * @param wait_time 等待时间（Tick）
 * @return true 已置位
 * @return false 未置位
 */
bool gattc_wrapper_wait_status(gattc_wrapper_status_t status, uint32_t wait_time);

#endif 
