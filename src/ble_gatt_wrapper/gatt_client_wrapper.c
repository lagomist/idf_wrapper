#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"
#include "esp_gatt_common_api.h"
#include "esp_log.h"

#include "gatt_client_wrapper.h"


#define PROFILE_APP_ID              0
#define INVALID_HANDLE              0
#define SCAN_ALL_THE_TIME           0

#define GATTC_CONNECTED_BIT         BIT0
#define GATTC_NOTIFY_ENABLE_BIT     BIT1
#define GATTC_WRITE_COMP_BIT        BIT2

static const char *TAG = "gattc_wrapper";


/* GATT event group handle */
static EventGroupHandle_t gattc_event_group = NULL;

static gattc_wrapper_recv_cb_t gattc_recv_callback = NULL;
static char remote_device_name[32] = GATTC_WRAPPER_DEFAULT_REMOTE_NAME;
static bool get_service = false;
static bool find_device = false;


static const esp_bt_uuid_t notify_descr_uuid = {
    .len = ESP_UUID_LEN_16,
    .uuid = {.uuid16 = ESP_GATT_UUID_CHAR_CLIENT_CONFIG,},
};

static esp_ble_scan_params_t ble_scan_params = {
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
typedef struct gattc_wrapper_char_inst {
    uint8_t index;
    uint16_t handle;
    esp_bt_uuid_t uuid;
    struct gattc_wrapper_char_inst *next;
} gattc_wrapper_char_inst_t;

typedef struct gattc_profile_inst {
    uint16_t gattc_if;
    uint16_t app_id;
    uint16_t conn_id;
    uint16_t service_start_handle;
    uint16_t service_end_handle;
    esp_bt_uuid_t service_uuid;
    esp_bd_addr_t remote_bda;
    gattc_wrapper_char_inst_t *characteristic;
} gattc_profile_inst_t;

/**
 * @brief gatt client profile service instance
 * 
 */
static gattc_profile_inst_t gl_profile_tab = {
    .gattc_if = ESP_GATT_IF_NONE,       /* Not get the gatt_if, so initial is ESP_GATT_IF_NONE */
    .service_uuid.uuid.uuid16 = GATTC_WRAPPER_DEFAULT_SVC_UUID,
    .service_uuid.len = ESP_UUID_LEN_16,
};



/**
 * @brief 添加一个特征实例
 * 
 * @param inst_list 特征实例链表头
 */
static gattc_wrapper_char_inst_t *add_gattc_char_instance(gattc_wrapper_char_inst_t **inst_list) 
{
    gattc_wrapper_char_inst_t *new_char = (gattc_wrapper_char_inst_t *)malloc(sizeof(gattc_wrapper_char_inst_t));  
    if (new_char == NULL) return NULL;
    if (*inst_list == NULL) {
        *inst_list = new_char;
    } else {
        gattc_wrapper_char_inst_t *current;
        current = *inst_list;
        while (current->next != NULL) {
            current = current->next;
        }
        current->next = new_char;
    }

    memset(new_char, 0, sizeof(gattc_wrapper_char_inst_t));
    new_char->next = NULL;
    return new_char;
}


int gattc_wrapper_write_char(uint8_t index, uint8_t *data, uint16_t data_len)
{
    int ret = -1;
    gattc_wrapper_char_inst_t *char_inst = NULL;
    /* 遍历特征列表 搜索特征 */
    for (char_inst = gl_profile_tab.characteristic; char_inst; char_inst = char_inst->next) {
        if (char_inst->index == index) break;
    }
    if (char_inst != NULL && char_inst->handle != 0) {
        ret = esp_ble_gattc_write_char(gl_profile_tab.gattc_if, gl_profile_tab.conn_id, char_inst->handle,
                                        data_len, data, ESP_GATT_WRITE_TYPE_RSP, ESP_GATT_AUTH_REQ_NONE);
    }

    return ret;
}


static void esp_gap_cb(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    switch (event) {
    case ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT:
        esp_ble_gap_start_scanning(SCAN_ALL_THE_TIME);
        break;
    case ESP_GAP_BLE_SCAN_START_COMPLETE_EVT:
        //scan start complete event to indicate scan start successfully or failed
        if (param->scan_start_cmpl.status != ESP_BT_STATUS_SUCCESS) {
            ESP_LOGE(TAG, "scan start failed, error status = %x", param->scan_start_cmpl.status);
            break;
        }
        ESP_LOGI(TAG, "scan start success");

        break;
    case ESP_GAP_BLE_SCAN_RESULT_EVT: {
        esp_ble_gap_cb_param_t *scan_result = param;
        switch (scan_result->scan_rst.search_evt) {
        case ESP_GAP_SEARCH_INQ_RES_EVT:
            if (find_device == false) {
                uint8_t adv_name_len = 0;
                uint8_t *adv_name = esp_ble_resolve_adv_data(scan_result->scan_rst.ble_adv,
                                                    ESP_BLE_AD_TYPE_NAME_CMPL, &adv_name_len);
                if (adv_name != NULL) {
                    if (strlen(remote_device_name) == adv_name_len && strncmp((char *)adv_name, remote_device_name, adv_name_len) == 0) {
                        ESP_LOGI(TAG, "searched device: %s", remote_device_name);
                        find_device = true;
                        esp_ble_gap_stop_scanning();
                        esp_ble_gattc_open(gl_profile_tab.gattc_if, scan_result->scan_rst.bda, scan_result->scan_rst.ble_addr_type, true);
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
        ESP_LOGI(TAG, "stop scan successfully");
        break;

    case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
        if (param->adv_stop_cmpl.status != ESP_BT_STATUS_SUCCESS){
            ESP_LOGE(TAG, "adv stop failed, error status = %x", param->adv_stop_cmpl.status);
            break;
        }
        ESP_LOGI(TAG, "stop adv successfully");
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

static void esp_gattc_cb(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param)
{
    esp_ble_gattc_cb_param_t *p_data = (esp_ble_gattc_cb_param_t *)param;

    switch (event) {
    case ESP_GATTC_REG_EVT:
        /* If event is register event, store the gattc_if for each profile */
        if (param->reg.status == ESP_GATT_OK) {
            gl_profile_tab.gattc_if = gattc_if;
            gl_profile_tab.app_id = param->reg.app_id;
        } else {
            ESP_LOGI(TAG, "reg app failed, app_id %04x, status %d",
                    param->reg.app_id,
                    param->reg.status);
            return;
        }
        
        esp_err_t scan_ret = esp_ble_gap_set_scan_params(&ble_scan_params);
        if (scan_ret){
            ESP_LOGE(TAG, "set scan params error, error code = %x", scan_ret);
        }
        break;
    case ESP_GATTC_CONNECT_EVT:{
        ESP_LOGI(TAG, "Remote device connected.");
        xEventGroupSetBits(gattc_event_group, GATTC_CONNECTED_BIT);
        gl_profile_tab.conn_id = p_data->connect.conn_id;
        memcpy(gl_profile_tab.remote_bda, p_data->connect.remote_bda, sizeof(esp_bd_addr_t));
        esp_err_t mtu_ret = esp_ble_gattc_send_mtu_req (gattc_if, p_data->connect.conn_id);
        if (mtu_ret){
            ESP_LOGE(TAG, "config MTU error, error code = %x", mtu_ret);
        }
        break;
    }
    case ESP_GATTC_OPEN_EVT:
        if (param->open.status != ESP_GATT_OK){
            ESP_LOGE(TAG, "open failed, status %d", p_data->open.status);
            find_device = false;
            break;
        }
        ESP_LOGI(TAG, "open success");
        break;
    case ESP_GATTC_DIS_SRVC_CMPL_EVT:
        if (param->dis_srvc_cmpl.status != ESP_GATT_OK){
            ESP_LOGE(TAG, "discover service failed, status %d", param->dis_srvc_cmpl.status);
            break;
        }
        ESP_LOGI(TAG, "discover service complete.");
        esp_ble_gattc_search_service(gattc_if, param->cfg_mtu.conn_id, &gl_profile_tab.service_uuid);
        break;
    case ESP_GATTC_CFG_MTU_EVT:
        if (param->cfg_mtu.status != ESP_GATT_OK){
            ESP_LOGE(TAG,"config mtu failed, error status = %x", param->cfg_mtu.status);
        }
        ESP_LOGI(TAG, "MTU configure: %d.", param->cfg_mtu.mtu);
        break;
    case ESP_GATTC_SEARCH_RES_EVT: {
        if (p_data->search_res.srvc_id.uuid.len == ESP_UUID_LEN_16 && p_data->search_res.srvc_id.uuid.uuid.uuid16 == gl_profile_tab.service_uuid.uuid.uuid16) {
            ESP_LOGI(TAG, "service found");
            get_service = true;
            gl_profile_tab.service_start_handle = p_data->search_res.start_handle;
            gl_profile_tab.service_end_handle = p_data->search_res.end_handle;
        }
        break;
    }
    case ESP_GATTC_SEARCH_CMPL_EVT:
        if (p_data->search_cmpl.status != ESP_GATT_OK){
            ESP_LOGE(TAG, "search service failed, error status = %x", p_data->search_cmpl.status);
            break;
        }
        if(p_data->search_cmpl.searched_service_source == ESP_GATT_SERVICE_FROM_REMOTE_DEVICE) {
            ESP_LOGI(TAG, "Get service information from remote device");
        } else if (p_data->search_cmpl.searched_service_source == ESP_GATT_SERVICE_FROM_NVS_FLASH) {
            ESP_LOGI(TAG, "Get service information from flash");
        } else {
            ESP_LOGI(TAG, "unknown service source");
        }
        if (get_service){
            uint16_t count = 0;
            esp_gatt_status_t status = esp_ble_gattc_get_attr_count( gattc_if,
                                                                     p_data->search_cmpl.conn_id,
                                                                     ESP_GATT_DB_CHARACTERISTIC,
                                                                     gl_profile_tab.service_start_handle,
                                                                     gl_profile_tab.service_end_handle,
                                                                     INVALID_HANDLE,
                                                                     &count);
            if (status != ESP_GATT_OK) {
                ESP_LOGE(TAG, "esp_ble_gattc_get_attr_count error");
            }

            if (count > 0) {
                esp_gattc_char_elem_t *char_elem_result = (esp_gattc_char_elem_t *)malloc(sizeof(esp_gattc_char_elem_t) * count);
                if (!char_elem_result){
                    ESP_LOGE(TAG, "gattc no mem");
                }else{
                    /* 遍历特征列表 搜索特征 */
                    for (gattc_wrapper_char_inst_t *char_inst = gl_profile_tab.characteristic; char_inst; char_inst = char_inst->next) {
                        status = esp_ble_gattc_get_char_by_uuid(gattc_if,
                                                                p_data->search_cmpl.conn_id,
                                                                gl_profile_tab.service_start_handle,
                                                                gl_profile_tab.service_end_handle,
                                                                char_inst->uuid,
                                                                char_elem_result,
                                                                &count);
                        if (status != ESP_GATT_OK){
                            ESP_LOGE(TAG, "esp_ble_gattc_get_char_by_uuid error");
                        }

                        /*  Every service have only one char in our 'ESP_GATTS_DEMO' demo, so we used first 'char_elem_result' */
                        if (count > 0 && (char_elem_result[0].properties & ESP_GATT_CHAR_PROP_BIT_NOTIFY)){
                            char_inst->handle = char_elem_result[0].char_handle;
                            esp_ble_gattc_register_for_notify(gattc_if, gl_profile_tab.remote_bda, char_elem_result[0].char_handle);
                        }
                    }
                    
                }
                /* free char_elem_result */
                free(char_elem_result);
            }else{
                ESP_LOGE(TAG, "no char found");
            }
        }
         break;
    case ESP_GATTC_REG_FOR_NOTIFY_EVT: {
        if (p_data->reg_for_notify.status != ESP_GATT_OK){
            ESP_LOGE(TAG, "REG FOR NOTIFY failed: error status = %d", p_data->reg_for_notify.status);
        }else{
            uint16_t count = 1;
            esp_gatt_status_t ret_status;
            uint16_t notify_en = 1;
            esp_gattc_descr_elem_t *descr_elem_result = malloc(sizeof(esp_gattc_descr_elem_t));
            if (!descr_elem_result){
                ESP_LOGE(TAG, "malloc error, gattc no mem");
            }else{
                ret_status = esp_ble_gattc_get_descr_by_char_handle( gattc_if,
                                                                        gl_profile_tab.conn_id,
                                                                        p_data->reg_for_notify.handle,
                                                                        notify_descr_uuid,
                                                                        descr_elem_result,
                                                                        &count);
                if (ret_status != ESP_GATT_OK){
                    ESP_LOGE(TAG, "esp_ble_gattc_get_descr_by_char_handle error");
                }
                /* Every char has only one descriptor in our 'ESP_GATTS_DEMO' demo, so we used first 'descr_elem_result' */
                if (count > 0 && descr_elem_result->uuid.len == ESP_UUID_LEN_16 && descr_elem_result->uuid.uuid.uuid16 == ESP_GATT_UUID_CHAR_CLIENT_CONFIG){
                    ret_status = esp_ble_gattc_write_char_descr(gattc_if,
                                                                gl_profile_tab.conn_id,
                                                                descr_elem_result->handle,
                                                                sizeof(notify_en),
                                                                (uint8_t *)&notify_en,
                                                                ESP_GATT_WRITE_TYPE_RSP,
                                                                ESP_GATT_AUTH_REQ_NONE);
                }

                if (ret_status != ESP_GATT_OK){
                    ESP_LOGE(TAG, "esp_ble_gattc_write_char_descr error");
                }

                /* free descr_elem_result */
                free(descr_elem_result);
            }
        }
        break;
    }
    case ESP_GATTC_NOTIFY_EVT:
        /* receive data into callback handle */
        if (gattc_recv_callback != NULL) {
            gattc_wrapper_recv_t recv_data;
            for (gattc_wrapper_char_inst_t *char_inst = gl_profile_tab.characteristic; char_inst; char_inst = char_inst->next) {
                if (char_inst->handle == p_data->notify.handle) {
                    recv_data.data = p_data->notify.value;
                    recv_data.len = p_data->notify.value_len;
                    recv_data.gattc_if = gl_profile_tab.gattc_if;
                    recv_data.handle = p_data->notify.handle;
                    recv_data.index = char_inst->index;
                    
                    gattc_recv_callback(recv_data);
                    break;
                }
            }
        }

        break;
    case ESP_GATTC_WRITE_DESCR_EVT:
        if (p_data->write.status != ESP_GATT_OK){
            ESP_LOGE(TAG, "write descr failed, error status = %x", p_data->write.status);
            break;
        }
        ESP_LOGI(TAG, "characteristic notify enable.");
        xEventGroupSetBits(gattc_event_group, GATTC_NOTIFY_ENABLE_BIT);
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
        xEventGroupSetBits(gattc_event_group, GATTC_WRITE_COMP_BIT);

        break;
    case ESP_GATTC_DISCONNECT_EVT:
        ESP_LOGI(TAG, "Disconnected, reason = %d", p_data->disconnect.reason);
        get_service = false;
        find_device = false;
        xEventGroupClearBits(gattc_event_group, GATTC_CONNECTED_BIT | GATTC_NOTIFY_ENABLE_BIT);
        esp_ble_gap_start_scanning(SCAN_ALL_THE_TIME);
        break;
    default:
        break;
    }

}


void gatt_client_wrapper_init()
{
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
    if (ret){
        ESP_LOGE(TAG, "%s gap register failed, error code = %x\n", __func__, ret);
        return;
    }

    // register the callback function to the gattc module
    ret = esp_ble_gattc_register_callback(esp_gattc_cb);
    if(ret){
        ESP_LOGE(TAG, "%s gattc register failed, error code = %x\n", __func__, ret);
        return;
    }
    
    esp_err_t local_mtu_ret = esp_ble_gatt_set_local_mtu(GATTC_WRAPPER_DEFAULT_MTU_SIZE);
    if (local_mtu_ret){
        ESP_LOGE(TAG, "set local  MTU failed, error code = %x", local_mtu_ret);
    }
}


void gattc_wrapper_service_config(gattc_wrapper_svc_config_t *config)
{
    int name_len = strlen(config->server_name);
    if (name_len > sizeof(remote_device_name)) {
        name_len = sizeof(remote_device_name) -1;
    }
    memcpy(remote_device_name, config->server_name, name_len);
    remote_device_name[name_len] = '\0';

    gl_profile_tab.service_uuid = config->service_uuid;
}


void gattc_wrapper_add_interested_char(gattc_wrapper_char_config_t *config)
{
    gattc_wrapper_char_inst_t *char_inst = add_gattc_char_instance(&gl_profile_tab.characteristic);
    if (char_inst != NULL) {
        char_inst->index = config->index;
        char_inst->uuid = config->uuid;
    }
}

void gattc_wrapper_create_default_service()
{
    gattc_wrapper_char_inst_t *char_inst = add_gattc_char_instance(&gl_profile_tab.characteristic);
    if (char_inst != NULL) {
        char_inst->index = GATTC_WRAPPER_DEFAULT_CHAR_INDEX;
        char_inst->uuid.uuid.uuid16 = GATTC_WRAPPER_DEFAULT_CHAR_UUID;
        char_inst->uuid.len = ESP_UUID_LEN_16;
    }
}

void gattc_wrapper_register_callback(gattc_wrapper_recv_cb_t callback)
{
    gattc_recv_callback = callback;

    gattc_event_group = xEventGroupCreate();
    /* register gattc app */
    esp_err_t ret = esp_ble_gattc_app_register(PROFILE_APP_ID);
    if (ret){
        ESP_LOGE(TAG, "ble gattc app register failed");
    }
}


bool gattc_wrapper_wait_status(gattc_wrapper_status_t status, uint32_t wait_time)
{
    EventBits_t bits = 0;
    switch (status) {
    case GATTC_WRAPPER_CONNECTED:
        bits = xEventGroupWaitBits(gattc_event_group, GATTC_CONNECTED_BIT, pdFALSE, pdFALSE, wait_time);
        if (bits & GATTC_CONNECTED_BIT) return true;
        break;
    case GATTC_WRAPPER_NOTIFY_ENABLE:
        bits = xEventGroupWaitBits(gattc_event_group, GATTC_NOTIFY_ENABLE_BIT, pdFALSE, pdFALSE, wait_time);
        if (bits & GATTC_NOTIFY_ENABLE_BIT) return true;
        break;
    case GATTC_WRAPPER_WRITE_COMP:
        bits = xEventGroupWaitBits(gattc_event_group, GATTC_WRITE_COMP_BIT, pdTRUE, pdFALSE, wait_time);
        if (bits & GATTC_WRITE_COMP_BIT) return true;
        break;
    default:
        break;
    }

    return false;
}

