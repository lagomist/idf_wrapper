#pragma once

#include "bufdef.h"

namespace ble42 {

///Attributes State Machine
enum Attr: uint8_t {
    SPP_IDX_SVC,

    SPP_IDX_SPP_CHAR1,
    SPP_IDX_SPP_VAL1,
	SPP_IDX_SPP_NTY1,

	SPP_IDX_SPP_CHAR2,
    SPP_IDX_SPP_VAL2,
	SPP_IDX_SPP_NTY2,
	//统计个数，不要动！
    SPP_IDX_NB,
};

uint16_t mtu_size();

//不判断是否连接，直接发送。交由上层判断
int send(Attr attr, IBuf data);

bool connected();

//设置连接和接收回调
using RecvCallback = void (*)(IBuf);
using ConnCallback = void (*)();
void register_recv_cb(Attr attr, RecvCallback cb);
void register_connect_cb(ConnCallback cb);
void register_disconnect_cb(ConnCallback cb);

void deauth();

//host_name12字节，作为广播帧中间数据
//sn前6字节作为广播帧尾
void init(std::string_view host_name, std::string_view sn);
void deinit();

using pfBLERecv = void(*)(unsigned char*, int);

// void beacon_start_recv(pfBLERecv cb);//开启接收
void beacon_update(unsigned char* value, int len);
// void beacon_stop_recv();//关闭接收


}
