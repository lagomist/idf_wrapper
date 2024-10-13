#include "ble_wrapper.h"
#include "wrapper_config.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_gap_ble_api.h"
#include "esp_bt_main.h"
#include "esp_bt.h"
#include "esp_gatt_common_api.h"
#include "esp_gatts_api.h"
#include "esp_log.h"
#include <string.h>
#include <atomic>


namespace BleWrapper {

namespace DefaultServer {

constexpr static const char *TAG = "gatts_table_wrapper";


static std::atomic_uint16_t _mtu_size = 27;
static std::atomic_bool		_is_connected = false;
static std::atomic_uint16_t _conn_id = 0xffff;
static std::atomic<esp_gatt_if_t> _gatts_if = 0xff;

static ConnCallback _connect_cb = nullptr;
static ConnCallback _disconnect_cb = nullptr;
static RecvCallback _recv_cb = nullptr;

/* gatts 属性表特征句柄 */
static uint16_t _gatt_handle_table[HRS_IDX_NB];

/* 广播服务UUID */
static uint8_t _service_uuid[16] = {
    /* LSB <--------------------------------------------------------------------------------> MSB */
    //first uuid, 16bit, [12],[13] is the value
    0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00,
};

// adv data
static esp_ble_adv_data_t _adv_data = {
    .set_scan_rsp = false,
    .include_name = true,
    .include_txpower = false,
    .min_interval = 0x0006, //slave connection min interval, Time = min_interval * 1.25 msec
    .max_interval = 0x0010, //slave connection max interval, Time = max_interval * 1.25 msec
    .appearance = 0x00,
    .manufacturer_len = 0, // The length of adv data must be less than 31 bytes
    .p_manufacturer_data =  NULL,
    .service_data_len = 0,
    .p_service_data = NULL,
    .service_uuid_len    = sizeof(_service_uuid),
    .p_service_uuid      = _service_uuid,
    .flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
};
// scan response data
static esp_ble_adv_data_t _scan_rsp_data = {
    .set_scan_rsp = true,
    .include_name = true,
    .include_txpower = true,
    .appearance = 0x00,
    .manufacturer_len = 0,
    .p_manufacturer_data =  NULL,
    .service_data_len = 0,
    .p_service_data = NULL,
    .service_uuid_len    = sizeof(_service_uuid),
    .p_service_uuid      = _service_uuid,
    .flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
};

/* advertising params */
static esp_ble_adv_params_t _adv_params = {
    .adv_int_min         = 0x20,
    .adv_int_max         = 0x40,
    .adv_type            = ADV_TYPE_IND,
    .own_addr_type       = BLE_ADDR_TYPE_PUBLIC,
    .channel_map         = ADV_CHNL_ALL,
    .adv_filter_policy   = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

/*
 *  Server
 ****************************************************************************************
 */

constexpr static const uint16_t GATT_PRI_SVC_UUID   = ESP_GATT_UUID_PRI_SERVICE;
constexpr static const uint16_t CHAR_DECLARE_UUID   = ESP_GATT_UUID_CHAR_DECLARE;
constexpr static const uint16_t CLIENT_CONFIG_UUID  = ESP_GATT_UUID_CHAR_CLIENT_CONFIG;
constexpr static const uint8_t  CHAR_PROP_ALL       = ESP_GATT_CHAR_PROP_BIT_READ|ESP_GATT_CHAR_PROP_BIT_WRITE_NR|ESP_GATT_CHAR_PROP_BIT_NOTIFY;


// GATT Service
static uint16_t _primary_service_uuid = WrapperConfig::DEFAULT_SVC_UUID;
// Characteristic UUID
static uint16_t  _primary_char_uuid = WrapperConfig::DEFAULT_CHAR_UUID;
/* 预定义特征值 */
static uint8_t  _primary_char_val[1] = {0x00};
static uint8_t  _primary_char_ccc[2] = {0x00,0x00};



// Full HRS Database Description - Used to add attributes into the database
static const esp_gatts_attr_db_t _gatt_db[HRS_IDX_NB] = {
    // Service Declaration
    [IDX_SVC]        =
    {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&GATT_PRI_SVC_UUID, ESP_GATT_PERM_READ,
    sizeof(uint16_t), ESP_UUID_LEN_16, (uint8_t *)&_primary_service_uuid}},

    /* Characteristic Declaration */
	[IDX_PRIMARY_CHAR]             =		// 0x2803	特征声明
    {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&CHAR_DECLARE_UUID, ESP_GATT_PERM_READ,
    sizeof(uint8_t),sizeof(uint8_t), (uint8_t *)&CHAR_PROP_ALL}},
	
    /* Characteristic Value */
	[IDX_PRIMARY_CHAR_VAL]         =		// 自定义UUID 服务特征值
    {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&_primary_char_uuid, ESP_GATT_PERM_READ|ESP_GATT_PERM_WRITE,
    WrapperConfig::CHAR_MAX_LEN, sizeof(_primary_char_val), (uint8_t *)_primary_char_val}},
	
    /* Client Characteristic Configuration Descriptor */
	[IDX_PRIMARY_CHAR_CFG]         =		// 0x2902	 客户端特征配置描述符
    {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&CLIENT_CONFIG_UUID, ESP_GATT_PERM_READ|ESP_GATT_PERM_WRITE,
    sizeof(uint16_t),sizeof(_primary_char_ccc), (uint8_t *)_primary_char_ccc}},
};


static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
    switch (event) {
    case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
        esp_ble_gap_start_advertising(&_adv_params);
        break;
    case ESP_GAP_BLE_SCAN_RSP_DATA_SET_COMPLETE_EVT:
        break;
    case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
        /* advertising start complete event to indicate advertising start successfully or failed */
        if (param->adv_start_cmpl.status != ESP_BT_STATUS_SUCCESS) {
            ESP_LOGE(TAG, "advertising start failed");
        } else {
            ESP_LOGI(TAG, "advertising start.");
        }
        break;
    case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
        if (param->adv_stop_cmpl.status != ESP_BT_STATUS_SUCCESS) {
            ESP_LOGE(TAG, "Advertising stop failed");
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

static void gatts_profile_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param) {
    switch (event) {
    case ESP_GATTS_REG_EVT: {
        if (param->reg.status == ESP_GATT_OK) {
            // 保存服务端口
            _gatts_if = gatts_if;
        } else {
            ESP_LOGE(TAG, "reg app failed, app_id %04x, status %d",
                    param->reg.app_id,
                    param->reg.status);
            return;
        }

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

        ret = esp_ble_gatts_create_attr_tab(_gatt_db, _gatts_if, HRS_IDX_NB, 0);
        if (ret){
            ESP_LOGE(TAG, "create attr table failed, error code = %x", ret);
        }
    }
        break;
    case ESP_GATTS_READ_EVT:
        ESP_LOGD(TAG, "ESP_GATTS_READ_EVT");
        break;
    case ESP_GATTS_WRITE_EVT:
        if (param->write.is_prep) {
            /* disable prep write */
            esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, ESP_GATT_ERROR, nullptr);
        } else {
            // characteristic describe config
            if (_gatt_handle_table[IDX_PRIMARY_CHAR_CFG] == param->write.handle && param->write.len == 2){
                uint16_t descr_value = param->write.value[1]<<8 | param->write.value[0];
                if (descr_value == 0x0001){
                    ESP_LOGI(TAG, "notify enable");
                }else if (descr_value == 0x0002){
                    ESP_LOGI(TAG, "indicate enable");
                }
                else if (descr_value == 0x0000){
                    ESP_LOGI(TAG, "notify/indicate disable ");
                }else{
                    ESP_LOGE(TAG, "unknown descr value");
                }
                break;
            }

            if (param->write.len < WrapperConfig::CHAR_MAX_LEN) {
                // 写入特征值
                esp_ble_gatts_set_attr_value(param->write.handle, param->write.len, param->write.value);
            }
            // 接收数据送入回调函数
            if (_recv_cb)
                _recv_cb({param->write.value, param->write.len});
            
            /* send response when param->write.need_rsp is true*/
            if (param->write.need_rsp){
                esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, ESP_GATT_OK, NULL);
            }
        }
        break;
    case ESP_GATTS_EXEC_WRITE_EVT:
        break;
    case ESP_GATTS_MTU_EVT:
        _mtu_size = param->mtu.mtu;
        ESP_LOGI(TAG, "MTU change to: %d", param->mtu.mtu);
        break;
    case ESP_GATTS_CONF_EVT:
        break;
    case ESP_GATTS_UNREG_EVT:
        break;
    case ESP_GATTS_ADD_INCL_SRVC_EVT:
        
        break;
    case ESP_GATTS_DELETE_EVT:
        break;
    case ESP_GATTS_START_EVT:
        break;
    case ESP_GATTS_STOP_EVT:
        break;
    case ESP_GATTS_CONNECT_EVT: {
        esp_ble_conn_update_params_t conn_params = {0};
        memcpy(conn_params.bda, param->connect.remote_bda, sizeof(esp_bd_addr_t));
        /* For the iOS system, please refer to Apple official documents about the BLE connection parameters restrictions. */
        conn_params.latency = 0;
        conn_params.max_int = 0x20;    // max_int = 0x20*1.25ms = 40ms
        conn_params.min_int = 0x10;    // min_int = 0x10*1.25ms = 20ms
        conn_params.timeout = 400;    // timeout = 400*10ms = 4000ms
        //start sent the update connection parameters to the peer device.
        esp_ble_gap_update_conn_params(&conn_params);
        _is_connected = true;
        _conn_id = param->connect.conn_id;
        _gatts_if = gatts_if;
        if (_connect_cb)
            _connect_cb();
    }
        break;
    case ESP_GATTS_DISCONNECT_EVT:
        ESP_LOGW(TAG, "disconnected, reason = 0x%x", param->disconnect.reason);
        if (_disconnect_cb)
            _disconnect_cb();
        esp_ble_gap_start_advertising(&_adv_params);
        break;
    case ESP_GATTS_OPEN_EVT:
        break;
    case ESP_GATTS_CANCEL_OPEN_EVT:
        break;
    case ESP_GATTS_CLOSE_EVT:
        break;
    case ESP_GATTS_LISTEN_EVT:
        break;
    case ESP_GATTS_CONGEST_EVT:
        break;
    case ESP_GATTS_CREAT_ATTR_TAB_EVT:
        if (param->add_attr_tab.status != ESP_GATT_OK){
            ESP_LOGE(TAG, "create attribute table failed, error code=0x%x", param->add_attr_tab.status);
        } else if (param->add_attr_tab.num_handle != HRS_IDX_NB){
            ESP_LOGE(TAG, "create attribute table abnormally, num_handle (%d) \
                    doesn't equal to HRS_IDX_NB(%d)", param->add_attr_tab.num_handle, HRS_IDX_NB);
        } else {
            memcpy(_gatt_handle_table, param->add_attr_tab.handles, sizeof(_gatt_handle_table));
            esp_ble_gatts_start_service(_gatt_handle_table[IDX_SVC]);
        }
        break;
    
    case ESP_GATTS_SET_ATTR_VAL_EVT:								
    case ESP_GATTS_SEND_SERVICE_CHANGE_EVT:
    default:
        break;
    }
}

uint16_t mtuSize() {
	return _mtu_size;
}

bool isConnected() {
	return _is_connected;
}

void registerRecvCallback(RecvCallback cb) {
	_recv_cb = cb;
}

void registerConnectCallback(ConnCallback cb) {
	_connect_cb = cb;
}

void registerDisconnectCallback(ConnCallback cb) {
	_disconnect_cb = cb;
}

void deauth() {
	if (!_is_connected)
		return;
	esp_ble_gatts_close(_gatts_if, _conn_id);
}

void advUpdeta(uint8_t* data, int len) {
    if (len > WrapperConfig::ADV_MAX_LEN) return;
    _adv_data.p_manufacturer_data = data;
    _adv_data.manufacturer_len = len;
    if(!_is_connected){
		esp_ble_gap_stop_advertising();
        esp_ble_gap_config_adv_data(&_adv_data);
	}
}


int send(uint8_t* data, int len) {
	if (len > mtuSize()) {
		ESP_LOGE(TAG, "msg size %u is bigger than mtu size %u", len, mtuSize());
		return -1;
	}
	int err = esp_ble_gatts_send_indicate(_gatts_if, _conn_id, _gatt_handle_table[IDX_PRIMARY_CHAR], len, data, false);
	return err;
}

int send(IBuf data) {
	return send((uint8_t *)data.data(), data.size());
}


void init(std::string_view host_name) {
    esp_err_t ret;
    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ret = esp_bt_controller_init(&bt_cfg);
    if (ret) {
        ESP_LOGE(TAG, "%s enable controller failed: %s", __func__, esp_err_to_name(ret));
        return;
    }

    ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    if (ret) {
        ESP_LOGE(TAG, "%s enable controller failed: %s", __func__, esp_err_to_name(ret));
        return;
    }

    ret = esp_bluedroid_init();
    if (ret) {
        ESP_LOGE(TAG, "%s init bluetooth failed: %s", __func__, esp_err_to_name(ret));
        return;
    }

    ret = esp_bluedroid_enable();
    if (ret) {
        ESP_LOGE(TAG, "%s enable bluetooth failed: %s", __func__, esp_err_to_name(ret));
        return;
    }

    ret = esp_ble_gatts_register_callback(gatts_profile_event_handler);
    if (ret){
        ESP_LOGE(TAG, "gatts register error, error code = %x", ret);
        return;
    }

    ret = esp_ble_gap_register_callback(gap_event_handler);
    if (ret){
        ESP_LOGE(TAG, "gap register error, error code = %x", ret);
        return;
    }

    ret = esp_ble_gatts_app_register(0x55);
    if (ret){
        ESP_LOGE(TAG, "gatts app register error, error code = %x", ret);
        return;
    }

    ret = esp_ble_gatt_set_local_mtu(WrapperConfig::DEFAULT_MTU_SIZE);
    if (ret){
        ESP_LOGE(TAG, "set local  MTU failed, error code = %x", ret);
    }
    ret = esp_ble_gap_set_device_name(host_name.data());
    if (ret){
        ESP_LOGE(TAG, "set device name failed, error code = %x", ret);
    }
}

void deinit() {
	esp_bluedroid_disable();
	esp_bluedroid_deinit();
	esp_bt_controller_disable();
	esp_bt_controller_deinit();
}

} /* namespace DefaultServer */

} /* namespace BleWrapper */
