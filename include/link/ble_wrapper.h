#pragma once

#include "bufdef.h"
#include <string_view>

namespace Wrapper {

namespace BLE {

namespace Server {

enum CharProperty : uint8_t {
	READ = (1 << 1),
	WRITE = (1 << 3),
	NOTIFY = (1 << 4),
	READ_WRITE = (1 << 1) | (1 << 3),
	WRITE_NOTIFY = (1 << 3) | (1 << 4),
	READ_WRITE_NOTIFY = (1 << 1) | (1 << 3) | (1 << 4),
};

struct RecvEvtParam {
	uint8_t gatts_if;			// 服务接口
	uint16_t conn_id;			// 连接ID
	uint16_t handle;			// 属性句柄
	uint8_t *data;				// 接收数据
	int len;					// 接收数据长度
};

// 定义 GATTS 接收数据的回调函数原型
using RecvCallback = void (*)(RecvEvtParam);
using ConnCallback = void (*)();


/* ------------------function declared-------------------- */

uint16_t mtu_size();

/**
 * @brief 放送通知或指示
 * 
 * @param uuid 特征UUID
 * @param data 数据指针
 * @param len 数据长度
 * @return int 发生结果
 */
int send(uint16_t uuid, uint8_t* data, int len);

/**
 * @brief gap广播更新数据
 * 
 * @param data 数据指针
 * @param len 数据长度，不超过30字节
 */
void adv_update(uint8_t* data, int len);

/**
 * @brief 断开蓝牙连接
 * 
 */
void deauth();

/**
 * @brief 初始化BLE驱动
 * 
 * @param host_name 蓝牙广播名
 */
void init(std::string_view host_name);

/**
 * @brief 反初始化BLE
 * 
 */
void deinit();


/**
 * @brief 启动服务，必须在添加服务和特征之后调用，后续添加的特征无效
 * 
 * @param app_id 服务ID
 */
void start_service(uint16_t app_id);

/**
 * @brief GATT 创建一个默认服务
 * 
 */
void start_default_service(uint16_t app_id);

/**
 * @brief GATT 添加一个服务实例
 * 
 * @param app_id 实例ID
 * @param service_uuid 服务UUID配置
 */
void add_service(uint16_t app_id, uint16_t service_uuid);

/**
 * @brief GATT 删除一个服务实例
 * 
 * @param app_id 要删除的实例ID
 */
void remove_service(uint16_t app_id);

/**
 * @brief GATT 服务添加一个特征
 * 
 * @param app_id 服务实例ID
 * @param config 特征配置
 */
void add_char(uint16_t app_id, uint16_t char_uuid, CharProperty property);

/**
 * @brief GATT 服务注册接收回调，并启动服务实例
 * 
 * @param app_id 服务实例ID
 * @param callback 特征数据接收回调
 */
void register_recv_cb(uint16_t app_id, RecvCallback callback);

void register_connect_cb(ConnCallback cb);
void register_disconnect_cb(ConnCallback cb);

}

namespace DefaultServer {

/* Attributes State Machine */
enum Attr: uint8_t {
    IDX_SVC,
	IDX_PRIMARY_CHAR,
	IDX_PRIMARY_CHAR_VAL,
	IDX_PRIMARY_CHAR_CFG,
    HRS_IDX_NB,
};


uint16_t mtuSize();

int send(uint8_t* data, int len);
int send(IBuf data);

bool isConnected();

using RecvCallback = void (*)(IBuf);
using ConnCallback = void (*)();

void registerRecvCallback(RecvCallback cb);
void registerConnectCallback(ConnCallback cb);
void registerDisconnectCallback(ConnCallback cb);

void deauth();

void init(std::string_view host_name);
void deinit();

void advUpdeta(uint8_t* data, int len);


} /* namespace BLE::DefaultServer */


namespace Client {

using RecvCallback = void (*)(IBuf);
using ConnCallback = void (*)();

void init();
void deinit();
void deauth();
void config(std::string_view server_name, uint16_t service_uuid);
void addInterestedChar(uint16_t char_uuid);
void createDefaultService(std::string_view server_name);

uint16_t mtuSize();
int write(uint16_t uuid, uint8_t *data, int data_len);
int write(IBuf data);

void registerRecvCallback(RecvCallback cb);
void registerConnectCallback(ConnCallback cb);
void registerDisconnectCallback(ConnCallback cb);

}


} /* namespace BLE */

} /* namespace Wrapper */
