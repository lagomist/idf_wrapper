#include "ble_wrapper.h"
#include "wrapper_config.h"

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"
#include "esp_gatt_common_api.h"
#include "esp_gattc_api.h"
#include "esp_log.h"
#include <string.h>
#include <atomic>


namespace BleWrapper {

namespace Client{

constexpr static const char *TAG = "gattc_wrapper";


static std::atomic_uint16_t _mtu_size = 27;
static std::string _remote_device_name;
static ConnCallback _connect_cb = nullptr;
static ConnCallback _disconnect_cb = nullptr;
static RecvCallback _recv_cb = nullptr;
static std::atomic_bool	_is_connected = false;
static bool _get_service = false;


static const esp_bt_uuid_t _notify_descr_uuid = {
    .len = ESP_UUID_LEN_16,
    .uuid = {.uuid16 = ESP_GATT_UUID_CHAR_CLIENT_CONFIG,},
};

static esp_ble_scan_params_t _ble_scan_params = {
    .scan_type              = BLE_SCAN_TYPE_ACTIVE,
    .own_addr_type          = BLE_ADDR_TYPE_PUBLIC,
    .scan_filter_policy     = BLE_SCAN_FILTER_ALLOW_ALL,
    .scan_interval          = 0x50,
    .scan_window            = 0x30,
    .scan_duplicate         = BLE_SCAN_DUPLICATE_DISABLE
};


/**
 * @brief gatt characteristic instance
 * 
 */
struct gattc_char_inst {
    uint16_t handle;
    esp_bt_uuid_t uuid;
    struct gattc_char_inst *next;
};

struct gattc_profile_inst{
    uint16_t gattc_if;
    uint16_t app_id;
    uint16_t conn_id;
    uint16_t service_start_handle;
    uint16_t service_end_handle;
    esp_bt_uuid_t service_uuid;
    esp_bd_addr_t remote_bda;
    gattc_char_inst *characteristic;
};

/**
 * @brief gatt client profile service instance
 * 
 */
static gattc_profile_inst _gl_profile_tab;



/**
 * @brief 添加一个特征实例
 * 
 * @param inst_list 特征实例链表头
 */
static gattc_char_inst *add_gattc_char_instance(gattc_char_inst **inst_list) 
{
    gattc_char_inst *new_char = (gattc_char_inst *)malloc(sizeof(gattc_char_inst));  
    if (new_char == NULL) return NULL;
    if (*inst_list == NULL) {
        *inst_list = new_char;
    } else {
        gattc_char_inst *current;
        current = *inst_list;
        while (current->next != NULL) {
            current = current->next;
        }
        current->next = new_char;
    }

    memset(new_char, 0, sizeof(gattc_char_inst));
    new_char->next = NULL;
    return new_char;
}





static void esp_gap_cb(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
    switch (event) {
    case ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT:
        esp_ble_gap_start_scanning(0);
        break;
    case ESP_GAP_BLE_SCAN_START_COMPLETE_EVT:
        // scan start complete event to indicate scan start successfully or failed
        if (param->scan_start_cmpl.status != ESP_BT_STATUS_SUCCESS) {
            ESP_LOGE(TAG, "scan start failed, error status = %x", param->scan_start_cmpl.status);
            break;
        }
        ESP_LOGI(TAG, "scan start.");

        break;
    case ESP_GAP_BLE_SCAN_RESULT_EVT: {
        esp_ble_gap_cb_param_t *scan_result = param;
        switch (scan_result->scan_rst.search_evt) {
        case ESP_GAP_SEARCH_INQ_RES_EVT:
            if (_is_connected == false) {
                uint8_t adv_name_len = 0;
                uint8_t *adv_name = esp_ble_resolve_adv_data(scan_result->scan_rst.ble_adv,
                                                    ESP_BLE_AD_TYPE_NAME_CMPL, &adv_name_len);
                if (adv_name != NULL) {
                    if (_remote_device_name.length() == adv_name_len && strncmp((char *)adv_name, _remote_device_name.data(), adv_name_len) == 0) {
                        ESP_LOGI(TAG, "searched device: %s", _remote_device_name.c_str());
                        esp_ble_gap_stop_scanning();
                        esp_ble_gattc_open(_gl_profile_tab.gattc_if, scan_result->scan_rst.bda, scan_result->scan_rst.ble_addr_type, true);
                    }
                }
            }
            break;
        case ESP_GAP_SEARCH_INQ_CMPL_EVT:
            ESP_LOGI(TAG,"timeout scan stop");
            break;
        default:
            break;
        }
        break;
    }

    case ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT:
        if (param->scan_stop_cmpl.status != ESP_BT_STATUS_SUCCESS){
            ESP_LOGE(TAG, "scan stop failed, error status = %x", param->scan_stop_cmpl.status);
            break;
        }
        ESP_LOGI(TAG, "scan stop.");
        break;

    case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
        if (param->adv_stop_cmpl.status != ESP_BT_STATUS_SUCCESS){
            ESP_LOGE(TAG, "adv stop failed, error status = %x", param->adv_stop_cmpl.status);
            break;
        }
        break;
    case ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT:
        // ESP_LOGI(TAG, "update connection params status = %d, min_int = %d, max_int = %d,conn_int = %d,latency = %d, timeout = %d",
        //         param->update_conn_params.status,
        //         param->update_conn_params.min_int,
        //         param->update_conn_params.max_int,
        //         param->update_conn_params.conn_int,
        //         param->update_conn_params.latency,
        //         param->update_conn_params.timeout);
        break;
    default:
        break;
    }
}

static void esp_gattc_cb(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param) {
    esp_ble_gattc_cb_param_t *p_data = (esp_ble_gattc_cb_param_t *)param;

    switch (event) {
    case ESP_GATTC_REG_EVT: {
        /* If event is register event, store the gattc_if for each profile */
        if (param->reg.status == ESP_GATT_OK) {
            _gl_profile_tab.gattc_if = gattc_if;
            _gl_profile_tab.app_id = param->reg.app_id;
        }
        
        esp_err_t scan_ret = esp_ble_gap_set_scan_params(&_ble_scan_params);
        if (scan_ret){
            ESP_LOGE(TAG, "set scan params error, error code = %x", scan_ret);
        }
    }
        break;
    case ESP_GATTC_CONNECT_EVT:{
        ESP_LOGI(TAG, "Remote device connected.");
        _is_connected = true;
        _gl_profile_tab.conn_id = p_data->connect.conn_id;
        memcpy(_gl_profile_tab.remote_bda, p_data->connect.remote_bda, sizeof(esp_bd_addr_t));
        esp_err_t mtu_ret = esp_ble_gattc_send_mtu_req (gattc_if, p_data->connect.conn_id);
        if (mtu_ret) {
            ESP_LOGE(TAG, "config MTU error, error code = %x", mtu_ret);
        }
        break;
    }
    case ESP_GATTC_OPEN_EVT:
        if (param->open.status != ESP_GATT_OK){
            ESP_LOGE(TAG, "open failed, status %d", p_data->open.status);
            break;
        }
        break;
    case ESP_GATTC_DIS_SRVC_CMPL_EVT:
        if (param->dis_srvc_cmpl.status != ESP_GATT_OK){
            ESP_LOGE(TAG, "discover service failed, status %d", param->dis_srvc_cmpl.status);
            break;
        }
        esp_ble_gattc_search_service(gattc_if, param->cfg_mtu.conn_id, &_gl_profile_tab.service_uuid);
        break;
    case ESP_GATTC_CFG_MTU_EVT:
        if (param->cfg_mtu.status != ESP_GATT_OK){
            ESP_LOGE(TAG,"config mtu failed, error status = %x", param->cfg_mtu.status);
        }
        ESP_LOGI(TAG, "MTU configure: %d.", param->cfg_mtu.mtu);
        break;
    case ESP_GATTC_SEARCH_RES_EVT: {
        if (p_data->search_res.srvc_id.uuid.len == ESP_UUID_LEN_16 && p_data->search_res.srvc_id.uuid.uuid.uuid16 == _gl_profile_tab.service_uuid.uuid.uuid16) {
            ESP_LOGI(TAG, "found service.");
            _get_service = true;
            _gl_profile_tab.service_start_handle = p_data->search_res.start_handle;
            _gl_profile_tab.service_end_handle = p_data->search_res.end_handle;
        }
        break;
    }
    case ESP_GATTC_SEARCH_CMPL_EVT:
        if (p_data->search_cmpl.status != ESP_GATT_OK){
            ESP_LOGE(TAG, "search service failed, error status = %x", p_data->search_cmpl.status);
            break;
        }
        if (_get_service){
            uint16_t count = 0;
            esp_ble_gattc_get_attr_count( gattc_if,
                                        p_data->search_cmpl.conn_id,
                                        ESP_GATT_DB_CHARACTERISTIC,
                                        _gl_profile_tab.service_start_handle,
                                        _gl_profile_tab.service_end_handle,
                                        0,
                                        &count);
            if (count > 0) {
                esp_gattc_char_elem_t *char_elem_result = (esp_gattc_char_elem_t *)malloc(sizeof(esp_gattc_char_elem_t) * count);
                if (!char_elem_result){
                    ESP_LOGE(TAG, "gattc no mem");
                }else{
                    /* 遍历特征列表 搜索特征 */
                    for (gattc_char_inst *char_inst = _gl_profile_tab.characteristic; char_inst; char_inst = char_inst->next) {
                        esp_ble_gattc_get_char_by_uuid( gattc_if,
                                                        p_data->search_cmpl.conn_id,
                                                        _gl_profile_tab.service_start_handle,
                                                        _gl_profile_tab.service_end_handle,
                                                        char_inst->uuid,
                                                        char_elem_result,
                                                        &count);
                        if (count > 0) {
                            char_inst->handle = char_elem_result[0].char_handle;
                            if (char_elem_result[0].properties & ESP_GATT_CHAR_PROP_BIT_NOTIFY){
                                // 使能通知
                                esp_ble_gattc_register_for_notify(gattc_if, _gl_profile_tab.remote_bda, char_elem_result[0].char_handle);
                            }
                        }
                    }
                    /* free char_elem_result */
                    free(char_elem_result);
                    ESP_LOGI(TAG, "found characteristic.");
                    /* 特征获取成功 进入连接成功回调 */
                    if (_connect_cb)
                        _connect_cb();
                }
            } else {
                ESP_LOGE(TAG, "no char found");
            }
        }
        break;
    case ESP_GATTC_REG_FOR_NOTIFY_EVT: {
        if (p_data->reg_for_notify.status != ESP_GATT_OK){
            ESP_LOGE(TAG, "REG FOR NOTIFY failed: error status = %d", p_data->reg_for_notify.status);
        }else{
            uint16_t count = 1;
            uint16_t notify_en = 1;
            esp_gattc_descr_elem_t *descr_elem_result = (esp_gattc_descr_elem_t *)malloc(sizeof(esp_gattc_descr_elem_t));
            if (!descr_elem_result) {
                ESP_LOGE(TAG, "malloc error, gattc no mem");
            } else {
                esp_ble_gattc_get_descr_by_char_handle( gattc_if,
                                                        _gl_profile_tab.conn_id,
                                                        p_data->reg_for_notify.handle,
                                                        _notify_descr_uuid,
                                                        descr_elem_result,
                                                        &count);
                if (count > 0 && descr_elem_result->uuid.len == ESP_UUID_LEN_16 && descr_elem_result->uuid.uuid.uuid16 == ESP_GATT_UUID_CHAR_CLIENT_CONFIG) {
                    esp_ble_gattc_write_char_descr(gattc_if,
                                                    _gl_profile_tab.conn_id,
                                                    descr_elem_result->handle,
                                                    sizeof(notify_en),
                                                    (uint8_t *)&notify_en,
                                                    ESP_GATT_WRITE_TYPE_RSP,
                                                    ESP_GATT_AUTH_REQ_NONE);
                }

                /* free descr_elem_result */
                free(descr_elem_result);
            }
        }
        break;
    }
    case ESP_GATTC_NOTIFY_EVT:
        /* receive data into callback handle */
        if (_recv_cb) {
            _recv_cb({p_data->notify.value, p_data->notify.value_len});
        }

        break;
    case ESP_GATTC_WRITE_DESCR_EVT:
        if (p_data->write.status != ESP_GATT_OK){
            ESP_LOGE(TAG, "write descr failed, error status = %x", p_data->write.status);
            break;
        }
        ESP_LOGI(TAG, "characteristic notify enable.");
        break;
    case ESP_GATTC_SRVC_CHG_EVT: {
        esp_bd_addr_t bda;
        memcpy(bda, p_data->srvc_chg.remote_bda, sizeof(esp_bd_addr_t));
        ESP_LOGI(TAG, "ESP_GATTC_SRVC_CHG_EVT, bd_addr:");
        esp_log_buffer_hex(TAG, bda, sizeof(esp_bd_addr_t));
        break;
    }
    case ESP_GATTC_WRITE_CHAR_EVT:
        if (p_data->write.status != ESP_GATT_OK){
            ESP_LOGE(TAG, "write char failed, error status = %x", p_data->write.status);
            break;
        }
        break;
    case ESP_GATTC_DISCONNECT_EVT:
        ESP_LOGW(TAG, "Disconnected, reason = %d", p_data->disconnect.reason);
        _is_connected = false;
        if (_disconnect_cb)
            _disconnect_cb();
        _get_service = false;
        esp_ble_gap_start_scanning(0);
        break;
    default:
        break;
    }

}


void config(std::string_view server_name, uint16_t service_uuid) {
    _remote_device_name = server_name;
    _gl_profile_tab.service_uuid.uuid.uuid16 = service_uuid;
    _gl_profile_tab.service_uuid.len = ESP_UUID_LEN_16;
}


void addInterestedChar(uint16_t char_uuid) {
    gattc_char_inst *char_inst = add_gattc_char_instance(&_gl_profile_tab.characteristic);
    if (char_inst != nullptr) {
        char_inst->uuid.uuid.uuid16 = char_uuid;
        char_inst->uuid.len = ESP_UUID_LEN_16;
    }
}

void createDefaultService(std::string_view server_name) {
    config(server_name, BLE_WRAPPER_DEFAULT_SVC_UUID);
    addInterestedChar(BLE_WRAPPER_DEFAULT_CHAR_UUID);
}


uint16_t mtuSize() {
    return _mtu_size;
}

int write(uint16_t uuid, uint8_t *data, int data_len) {
    int ret = -1;
    if (data_len > mtuSize()) {
		ESP_LOGE(TAG, "msg size %u is bigger than mtu size %u", data_len, mtuSize());
		return -1;
	}
    /* 遍历特征列表 搜索特征 */
    for (gattc_char_inst *char_inst = _gl_profile_tab.characteristic; char_inst; char_inst = char_inst->next) {
        if (char_inst->uuid.uuid.uuid16 == uuid) {
            ret = esp_ble_gattc_write_char(_gl_profile_tab.gattc_if, _gl_profile_tab.conn_id, char_inst->handle,
                                        data_len, data, ESP_GATT_WRITE_TYPE_RSP, ESP_GATT_AUTH_REQ_NONE);
            break;
        }
    }
    return ret;
}

int write(IBuf data) {
	return write(BLE_WRAPPER_DEFAULT_CHAR_UUID, (uint8_t *)data.data(), data.size());
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
	esp_ble_gattc_close(_gl_profile_tab.gattc_if, _gl_profile_tab.conn_id);
}


void init() {
    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    esp_err_t ret = esp_bt_controller_init(&bt_cfg);
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

    // register the  callback function to the gap module
    ret = esp_ble_gap_register_callback(esp_gap_cb);
    if (ret) {
        ESP_LOGE(TAG, "%s gap register failed, error code = %x\n", __func__, ret);
        return;
    }

    // register the callback function to the gattc module
    ret = esp_ble_gattc_register_callback(esp_gattc_cb);
    if (ret) {
        ESP_LOGE(TAG, "%s gattc register failed, error code = %x\n", __func__, ret);
        return;
    }
    
    ret = esp_ble_gatt_set_local_mtu(BLE_WRAPPER_DEFAULT_MTU_SIZE);
    if (ret) {
        ESP_LOGE(TAG, "set local MTU failed, error code = %x", ret);
    }
    /* register gattc app */
    ret = esp_ble_gattc_app_register(0);
    if (ret){
        ESP_LOGE(TAG, "ble gattc app register failed");
    }
}

void deinit() {
	esp_bluedroid_disable();
	esp_bluedroid_deinit();
	esp_bt_controller_disable();
	esp_bt_controller_deinit();
}

} /* namespace BleWrapper::Client */

} /* namespace BleWrapper */
