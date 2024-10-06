#include "ble_wrapper.h"
#include "wrapper_config.h"

#include "esp_gap_ble_api.h"
#include "esp_bt_main.h"
#include "esp_bt.h"
#include "esp_gatt_common_api.h"
#include "esp_gatts_api.h"
#include "esp_log.h"
#include <string>
#include <atomic>
#include <memory.h>



namespace BleWrapper {

namespace Server {

/**
 * @brief gatt characteristic instance
 * 
 */
struct gatts_char_inst {
    uint16_t handle;
    esp_bt_uuid_t uuid;
    esp_gatt_perm_t perm;
    esp_gatt_char_prop_t property;
    uint16_t descr_handle;
    esp_bt_uuid_t descr_uuid;
    struct gatts_char_inst *next;
};

/* gatt server service profile instance */
struct gatts_profile_inst {
    RecvCallback recv_cb;
    uint16_t gatts_if;
    uint16_t app_id;
    uint16_t conn_id;
    uint16_t service_handle;
    esp_gatt_srvc_id_t service_id;
    gatts_char_inst* characteristic;
    struct gatts_profile_inst *next;
};


constexpr static const char *TAG = "gatts_wrapper";

/* 服务实例配置文件链表头 */
static gatts_profile_inst*  _profile_inst_head = nullptr;
static int32_t              _profile_inst_app = -1;
static std::atomic_uint16_t _mtu_size = 27;
static uint8_t              _default_char_value[1] = {0x00};
static uint8_t              _default_char_ccc[2] = {0x00,0x00};

static ConnCallback         _connect_cb = nullptr;
static ConnCallback         _disconnect_cb = nullptr;
static std::atomic_bool		_is_connected = false;

static esp_attr_value_t _default_attr_value = {
    .attr_max_len = DEFAULT_GATTS_CHAR_VAL_LEN_MAX,
    .attr_len     = sizeof(_default_char_value),
    .attr_value   = _default_char_value,
};
/* 客户端配置描述符值定义 */
static esp_attr_value_t _descr_value = {
    .attr_max_len = sizeof (uint16_t),
    .attr_len = sizeof(_default_char_ccc),
    .attr_value = _default_char_ccc,
};

static uint8_t              _adv_service_uuid128[16] = {
    /* LSB <--------------------------------------------------------------------------------> MSB */
    // first uuid, 16bit, [12],[13] is the value
    0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00,
};

// adv data The length of adv data must be less than 31 bytes
static esp_ble_adv_data_t _adv_data = {
    .set_scan_rsp = false,
    .include_name = true,
    .include_txpower = false,
    .min_interval = 0x0006, //slave connection min interval, Time = min_interval * 1.25 msec
    .max_interval = 0x0010, //slave connection max interval, Time = max_interval * 1.25 msec
    .appearance = 0x00,
    .manufacturer_len = 0, //TEST_MANUFACTURER_DATA_LEN,
    .p_manufacturer_data =  nullptr, //&test_manufacturer[0],
    .service_data_len = 0,
    .p_service_data = nullptr,
    .service_uuid_len = sizeof(_adv_service_uuid128),
    .p_service_uuid = _adv_service_uuid128,
    .flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
};
// scan response data
static esp_ble_adv_data_t _scan_rsp_data = {
    .set_scan_rsp = true,
    .include_name = true,
    .include_txpower = true,
    .appearance = 0x00,
    .manufacturer_len = 0,
    .p_manufacturer_data =  nullptr,
    .service_data_len = 0,
    .p_service_data = nullptr,
    .service_uuid_len = sizeof(_adv_service_uuid128),
    .p_service_uuid = _adv_service_uuid128,
    .flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
};

static esp_ble_adv_params_t _adv_params = {
    .adv_int_min        = 0x20,
    .adv_int_max        = 0x40,
    .adv_type           = ADV_TYPE_IND,
    .own_addr_type      = BLE_ADDR_TYPE_PUBLIC,
    .channel_map        = ADV_CHNL_ALL,
    .adv_filter_policy  = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

/**
 * @brief add a list node of gatt service profile instance
 * 
 * @param app_id register APP id
 * @return gatts_profile_inst* new instance pointer
 */
static gatts_profile_inst* add_gatts_profile_instance(uint16_t app_id) {  
    gatts_profile_inst *new_inst = (gatts_profile_inst *)malloc(sizeof(gatts_profile_inst));  
    if (new_inst == nullptr) return nullptr;
    if (_profile_inst_head == nullptr) {
        _profile_inst_head = new_inst;
    } else {
        gatts_profile_inst *current;
        current = _profile_inst_head;
        while (current->next != nullptr) {
            current = current->next;
        }
        current->next = new_inst;
    }

    // 设置节点信息
    memset(new_inst, 0, sizeof(gatts_profile_inst));
    new_inst->app_id = app_id;
    new_inst->gatts_if = ESP_GATT_IF_NONE;
    new_inst->next = nullptr;
    return new_inst;
}


/**
 * @brief delete a list node of gatt service profile instance
 * 
 * @param app_id 
 */
static void delete_gatts_profile_instance(uint16_t app_id) {
    if (_profile_inst_head == nullptr) return;
    gatts_profile_inst *current, *prev;
    current = _profile_inst_head;
    // 判断是否为第一个节点
    if (current != nullptr && current->app_id == app_id) {
        _profile_inst_head = current->next;
        free(current);
        return;
    }
    while (current->next != nullptr) {
        prev = current;
        current = current->next;
        if (current != nullptr && current->app_id == app_id) {
            prev->next = current->next;

            /* release characteristic list */
            while (current->characteristic != nullptr) {
                current->characteristic = current->characteristic->next;
                free(current->characteristic);
            }
            /* release service profile list */
            free(current);
            break;
        }
    }
}


/**
 * @brief 添加一个特征实例
 * 
 * @param inst_list 特征实例链表头
 */
static gatts_char_inst *add_gatts_char_instance(gatts_char_inst **inst_list) 
{
    gatts_char_inst *new_char = (gatts_char_inst *)malloc(sizeof(gatts_char_inst));  
    if (new_char == nullptr) return nullptr;
    if (*inst_list == nullptr) {
        *inst_list = new_char;
    } else {
        gatts_char_inst *current;
        current = *inst_list;
        while (current->next != nullptr) {
            current = current->next;
        }
        current->next = new_char;
    }

    memset(new_char, 0, sizeof(gatts_char_inst));
    new_char->next = nullptr;
    return new_char;
}


static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    switch (event) {
    case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
        esp_ble_gap_start_advertising(&_adv_params);
        break;
    case ESP_GAP_BLE_SCAN_RSP_DATA_SET_COMPLETE_EVT:
        break;
    case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
        // advertising start complete event to indicate advertising start successfully or failed
        if (param->adv_start_cmpl.status != ESP_BT_STATUS_SUCCESS) {
            ESP_LOGE(TAG, "Advertising start failed");
        } else {
            ESP_LOGI(TAG, "BLE Advertising start.");
        }
        break;
    case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
        if (param->adv_stop_cmpl.status != ESP_BT_STATUS_SUCCESS) {
            ESP_LOGE(TAG, "Advertising stop failed\n");
        } else {
            ESP_LOGI(TAG, "Stop adv successfully\n");
        }
        break;
    case ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT:
         ESP_LOGI(TAG, "update connection params status = %d, min_int = %d, max_int = %d,conn_int = %d,latency = %d, timeout = %d",
                  param->update_conn_params.status,
                  param->update_conn_params.min_int,
                  param->update_conn_params.max_int,
                  param->update_conn_params.conn_int,
                  param->update_conn_params.latency,
                  param->update_conn_params.timeout);
        break;
    default:
        break;
    }
}

/**
 * @brief 服务配置事件处理
 * 
 * @param event 事件
 * @param profile 服务配置指针
 */
static void profile_event_handle(int event, gatts_profile_inst *profile) {
    switch (event) {
    case ESP_GATTS_CREATE_EVT:
        /* 遍历特征列表 添加特征 */
        for (gatts_char_inst *char_inst = profile->characteristic; char_inst; char_inst = char_inst->next) {
            if (char_inst->handle == 0) {
                esp_ble_gatts_add_char(profile->service_handle, &char_inst->uuid,
                                        char_inst->perm, char_inst->property, &_default_attr_value, nullptr);
                break;
            }
        }
        break;
    case ESP_GATTS_ADD_CHAR_EVT:
        /* 遍历特征列表 添加特征或描述符 */
        for (gatts_char_inst *char_inst = profile->characteristic; char_inst; char_inst = char_inst->next) {
            if (char_inst->handle == 0) {
                esp_ble_gatts_add_char(profile->service_handle, &char_inst->uuid,
                                        char_inst->perm, char_inst->property, &_default_attr_value, nullptr);
                break;
            }
            if (char_inst->descr_handle == 0 && char_inst->descr_uuid.len != 0) {
                esp_ble_gatts_add_char_descr(profile->service_handle, &char_inst->descr_uuid,
                                        ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE, &_descr_value, nullptr);
                break;
            }
        }
        break;
    case ESP_GATTS_ADD_CHAR_DESCR_EVT:
        /* 遍历特征列表 添加下一个特征 */
        for (gatts_char_inst *char_inst = profile->characteristic; char_inst; char_inst = char_inst->next) {
            if (char_inst->handle == 0) {
                esp_ble_gatts_add_char(profile->service_handle, &char_inst->uuid,
                                        char_inst->perm, char_inst->property, &_default_attr_value, nullptr);
                break;
            }
        }
        break;
    default:
        break;
    }
}


/**
 * @brief GATT service profile 处理
 * 
 * @param event GATT Server callback function events
 * @param gatts_if Gatt interface type, different application on GATT client use different gatt_if
 * @param param Gatt server callback parameters union
 */
static void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
{
    gatts_profile_inst *profile_inst = nullptr;
    for (profile_inst = _profile_inst_head; profile_inst; profile_inst = profile_inst->next) {
        if (profile_inst->gatts_if == gatts_if) {
            break;
        }
    }
    if (profile_inst == nullptr && event != ESP_GATTS_REG_EVT) return;

    switch (event) {
    case ESP_GATTS_REG_EVT: {
        if (param->reg.status == ESP_GATT_OK) {
            for (profile_inst = _profile_inst_head; profile_inst; profile_inst = profile_inst->next) {
                if (profile_inst->app_id == param->reg.app_id) {
                    profile_inst->gatts_if = gatts_if;
                    break;
                }
            }
        } else {
            ESP_LOGE(TAG, "Reg app failed, app_id %04x, status %d\n",
                    param->reg.app_id,
                    param->reg.status);
            return;
        }
        if (profile_inst->app_id == _profile_inst_app) {
            // config adv data
            esp_err_t ret = esp_ble_gap_config_adv_data(&_adv_data);
            if (ret){
                ESP_LOGE(TAG, "config adv data failed, error code = %x", ret);
            }
            // config scan response data
            ret = esp_ble_gap_config_adv_data(&_scan_rsp_data);
            if (ret){
                ESP_LOGE(TAG, "config scan response data failed, error code = %x", ret);
            }
        }

        /* 特征计数 创建服务 */
        uint16_t num_handle = 1;    // Service Declaration
        for (gatts_char_inst *char_inst = profile_inst->characteristic; char_inst; char_inst = char_inst->next) {
            if (char_inst->descr_uuid.len == 0) {
                num_handle += 2;    // Characteristic Declaration + Characteristic Value
            } else {
                num_handle += 3;    // + Client Characteristic Configuration Descriptor
            }
        }
        esp_ble_gatts_create_service(gatts_if, &profile_inst->service_id, num_handle);
        break;
    }
    case ESP_GATTS_READ_EVT: {
        uint16_t length = 0;
        const uint8_t *prf_char;
        esp_gatt_rsp_t rsp = {0};
        esp_ble_gatts_get_attr_value(param->read.handle,  &length, &prf_char);
        rsp.attr_value.handle = param->read.handle;
        rsp.attr_value.len = length;
        memcpy(rsp.attr_value.value, prf_char, length);
        esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, ESP_GATT_OK, &rsp);
        break;
    }
    case ESP_GATTS_WRITE_EVT: {
        if (param->write.is_prep) {
            /* disable prep write */
            esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, ESP_GATT_ERROR, nullptr);
        } else {
            /* send response when param->write.need_rsp is true*/
            if (param->write.need_rsp){
                esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, ESP_GATT_OK, nullptr);
            }
            /* 遍历特征列表 判断写入特征描述 */
            for (gatts_char_inst *char_inst = profile_inst->characteristic; char_inst; char_inst = char_inst->next) {
                if (char_inst->descr_handle == param->write.handle) {
                    esp_ble_gatts_set_attr_value(param->write.handle, param->write.len, param->write.value);
                    break;
                } else if (char_inst->handle == param->write.handle) {
                    // 接收数据送入回调函数
                    if (profile_inst->recv_cb != nullptr) {
                        RecvEvtParam info = {0};
                        info.gatts_if = gatts_if;
                        info.conn_id = param->write.conn_id;
                        info.data = param->write.value;
                        info.len = param->write.len;
                        info.handle = param->write.handle;

                        profile_inst->recv_cb(info);
                    }
                    break;
                }
            }
        }
        break;
    }
    case ESP_GATTS_EXEC_WRITE_EVT:
        break;
    case ESP_GATTS_MTU_EVT:
        if (profile_inst->app_id == _profile_inst_app) {
            _mtu_size = param->mtu.mtu;
            ESP_LOGI(TAG, "MTU change to:%d", param->mtu.mtu);
        }
        break;
    case ESP_GATTS_UNREG_EVT:
        break;
    case ESP_GATTS_CREATE_EVT: {
        profile_inst->service_handle = param->create.service_handle;

        esp_ble_gatts_start_service(profile_inst->service_handle);
        profile_event_handle(ESP_GATTS_CREATE_EVT, profile_inst);
        break;
    }
    case ESP_GATTS_ADD_INCL_SRVC_EVT:
        break;
    case ESP_GATTS_ADD_CHAR_EVT: {
        /* 查找特征UUID 保存特征句柄 */
        gatts_char_inst *char_inst = nullptr;
        for (char_inst = profile_inst->characteristic; char_inst; char_inst = char_inst->next) {
            if (char_inst->uuid.uuid.uuid16 == param->add_char.char_uuid.uuid.uuid16) {
                char_inst->handle = param->add_char.attr_handle;
                break;
            }
        }
        profile_event_handle(ESP_GATTS_ADD_CHAR_EVT, profile_inst);
        break;
    }
    case ESP_GATTS_ADD_CHAR_DESCR_EVT: {
        for (gatts_char_inst *char_inst = profile_inst->characteristic; char_inst; char_inst = char_inst->next) {
            if (char_inst->descr_handle == 0 && char_inst->descr_uuid.len != 0) {
                char_inst->descr_handle = param->add_char_descr.attr_handle;
                break;
            }
        }
        profile_event_handle(ESP_GATTS_ADD_CHAR_DESCR_EVT, profile_inst);
        break;
    }
    case ESP_GATTS_DELETE_EVT:
        break;
    case ESP_GATTS_START_EVT:
        ESP_LOGI(TAG, "GATTS service started, service_handle: %d", param->start.service_handle);
        break;
    case ESP_GATTS_STOP_EVT:
        break;
    case ESP_GATTS_CONNECT_EVT: {
        if (profile_inst->app_id == _profile_inst_app) {
            esp_ble_conn_update_params_t conn_params = {0};
            memcpy(conn_params.bda, param->connect.remote_bda, sizeof(esp_bd_addr_t));
            /* For the IOS system, please reference the apple official documents about the ble connection parameters restrictions. */
            conn_params.latency = 0;
            conn_params.max_int = 0x20;    // max_int = 0x20*1.25ms = 40ms
            conn_params.min_int = 0x10;    // min_int = 0x10*1.25ms = 20ms
            conn_params.timeout = 400;    // timeout = 400*10ms = 4000ms
            // start sent the update connection parameters to the peer device.
            esp_ble_gap_update_conn_params(&conn_params);
            _is_connected = true;
            if (_connect_cb)
				_connect_cb();
        }
        profile_inst->conn_id = param->connect.conn_id;
        break;
    }
    case ESP_GATTS_DISCONNECT_EVT:
        if (profile_inst->app_id == _profile_inst_app) {
            _is_connected = false;
            ESP_LOGW(TAG, "disconnected, reason 0x%x", param->disconnect.reason);
            if (_disconnect_cb)
				_disconnect_cb();
            esp_ble_gap_start_advertising(&_adv_params);
        }
        break;
    case ESP_GATTS_CONF_EVT:
    case ESP_GATTS_OPEN_EVT:
    case ESP_GATTS_CANCEL_OPEN_EVT:
    case ESP_GATTS_CLOSE_EVT:
    case ESP_GATTS_LISTEN_EVT:
    case ESP_GATTS_CONGEST_EVT:
    default:
        break;
    }
}


void adv_update(uint8_t* data, int len) {
    if (len > MANUFACTURER_DATA_LEN_MAX) return;
    _adv_data.p_manufacturer_data = data;
    _adv_data.manufacturer_len = len;
    if(!_is_connected){
		esp_ble_gap_stop_advertising();
        esp_ble_gap_config_adv_data(&_adv_data);
	}
}

void add_service(uint16_t app_id, uint16_t service_uuid) {
    /* 记录第一次调用的APP ID 以处理GAP事件 */
    if (_profile_inst_app == -1) {
        _profile_inst_app = app_id;
        *(uint16_t *)&_adv_service_uuid128[12] = service_uuid;
    }
    /* 创建服务配置文件实例 */
    gatts_profile_inst *new_inst = add_gatts_profile_instance(app_id);
    if (new_inst != nullptr) {
        /*  服务配置 */
        new_inst->service_id.is_primary = true;
        new_inst->service_id.id.inst_id = 0x00;
        new_inst->service_id.id.uuid.len = ESP_UUID_LEN_16;
        new_inst->service_id.id.uuid.uuid.uuid16 = service_uuid;
        
    }
}


void remove_service(uint16_t app_id) {
    gatts_profile_inst *current = nullptr;
    for (current = _profile_inst_head; current; current = current->next) {
        if (current->app_id == app_id) {
            esp_ble_gatts_app_unregister(current->gatts_if);
            delete_gatts_profile_instance(app_id);
            break;
        }
    }
}

void add_char(uint16_t app_id, uint16_t char_uuid, CharProperty property) {
    gatts_profile_inst *current = nullptr;
    for (current = _profile_inst_head; current; current = current->next) {
        if (current->app_id == app_id) {
            /* 添加特征配置 */
            gatts_char_inst *char_inst = add_gatts_char_instance(&current->characteristic);
            if (char_inst != nullptr) {
                char_inst->perm = ESP_GATT_PERM_READ;
                char_inst->property = property;
                char_inst->uuid.len = ESP_UUID_LEN_16;
                char_inst->uuid.uuid.uuid16 = char_uuid;
                if (property & ESP_GATT_CHAR_PROP_BIT_WRITE) {
                    char_inst->perm = ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE;
                }
                if (property & ESP_GATT_CHAR_PROP_BIT_NOTIFY) {
                    /* 描述符配置 */
                    char_inst->descr_uuid.len = ESP_UUID_LEN_16;
                    char_inst->descr_uuid.uuid.uuid16 = ESP_GATT_UUID_CHAR_CLIENT_CONFIG;
                }

            }
            break;
        }
    }
}

void register_recv_cb(uint16_t app_id, RecvCallback callback) {
    gatts_profile_inst *current = nullptr;
    for (current = _profile_inst_head; current; current = current->next) {
        if (current->app_id == app_id) {
            /* 接收回调配置 */
            current->recv_cb = callback;
            break;
        }
    }
}


void start_service(uint16_t app_id) {
    /* register gatts app and start */
    if (esp_ble_gatts_app_register(app_id)) {
        ESP_LOGE(TAG, "gatts app register failed!");
    }
}

void start_default_service(uint16_t app_id) {
    add_service(app_id, BLE_WRAPPER_DEFAULT_SVC_UUID);
    add_char(app_id, BLE_WRAPPER_DEFAULT_CHAR_UUID, CharProperty::READ_WRITE_NOTIFY);
    start_service(app_id);
}

uint16_t mtu_size() {
	return _mtu_size;
}

bool is_connected() {
	return _is_connected;
}

void register_connect_cb(ConnCallback cb) {
	_connect_cb = cb;
}

void register_disconnect_cb(ConnCallback cb) {
	_disconnect_cb = cb;
}

void deauth() {
	if (!is_connected())
		return;
    if (_profile_inst_head)
        esp_ble_gatts_close(_profile_inst_head->gatts_if, _profile_inst_head->conn_id);
}

int send(uint16_t uuid, uint8_t* data, int len) {
    int err;
    gatts_profile_inst *profile_inst = nullptr;
    gatts_char_inst *char_inst = nullptr;
    if (len > mtu_size()) {
		ESP_LOGE(TAG, "msg size %u is bigger than mtu size %u", len, mtu_size());
		return -1;
	}
    for (profile_inst = _profile_inst_head; profile_inst; profile_inst = profile_inst->next) {
        for (char_inst = profile_inst->characteristic; char_inst; char_inst = char_inst->next) {
            if (char_inst->uuid.uuid.uuid16 == uuid) {
                err = esp_ble_gatts_send_indicate(profile_inst->gatts_if, profile_inst->conn_id,
                                                    char_inst->handle, len, data, false);
                return err;
            }
        }
    }
	return -2;
}

void init(std::string_view host_name) {
    esp_err_t ret;

    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ret = esp_bt_controller_init(&bt_cfg);
    if (ret) {
        ESP_LOGE(TAG, "%s initialize controller failed: %s\n", __func__, esp_err_to_name(ret));
        return;
    }

    ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    if (ret) {
        ESP_LOGE(TAG, "%s enable controller failed: %s\n", __func__, esp_err_to_name(ret));
        return;
    }
    ret = esp_bluedroid_init();
    if (ret) {
        ESP_LOGE(TAG, "%s init bluetooth failed: %s\n", __func__, esp_err_to_name(ret));
        return;
    }
    ret = esp_bluedroid_enable();
    if (ret) {
        ESP_LOGE(TAG, "%s enable bluetooth failed: %s\n", __func__, esp_err_to_name(ret));
        return;
    }

    ret = esp_ble_gatts_register_callback(gatts_event_handler);
    if (ret){
        ESP_LOGE(TAG, "gatts register error, error code = %x", ret);
        return;
    }
    ret = esp_ble_gap_register_callback(gap_event_handler);
    if (ret){
        ESP_LOGE(TAG, "gap register error, error code = %x", ret);
        return;
    }

    ret = esp_ble_gatt_set_local_mtu(BLE_WRAPPER_DEFAULT_MTU_SIZE);
    if (ret){
        ESP_LOGE(TAG, "set local  MTU failed, error code = %x", ret);
    }

    ret = esp_ble_gap_set_device_name(host_name.data());
    if (ret){
        ESP_LOGE(TAG, "set device name failed, error code = %x", ret);
    }
}


void deinit() {
    _profile_inst_app = -1;
	esp_bluedroid_disable();
	esp_bluedroid_deinit();
	esp_bt_controller_disable();
	esp_bt_controller_deinit();
}

} /* namespace server */

} /* namespace BleWrapper */
