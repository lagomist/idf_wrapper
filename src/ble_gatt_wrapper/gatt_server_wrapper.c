#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_gap_ble_api.h"
#include "esp_bt_main.h"
#include "esp_bt.h"
#include "esp_gatt_common_api.h"
#include "esp_log.h"

#include "gatt_server_wrapper.h"

#define PREPARE_BUF_MAX_SIZE        1024
#define MANUFACTURER_DATA_LEN_MAX   30

#define GATTS_ADV_CONFIG_BIT        (1 << 0)
#define GATTS_SCAN_RSP_BIT          (1 << 1)

/**
 * @brief gatt characteristic instance
 * 
 */
typedef struct gatts_wrapper_char_inst {
    uint8_t index;
    uint16_t handle;
    esp_bt_uuid_t uuid;
    esp_gatt_perm_t perm;
    esp_gatt_char_prop_t property;
    uint16_t descr_handle;
    esp_bt_uuid_t descr_uuid;
    struct gatts_wrapper_char_inst *next;
} gatts_wrapper_char_inst_t;

/* gatt server service profile instance */
struct gatts_profile_inst {
    gatts_wrapper_recv_cb_t recv_cb;
    uint16_t gatts_if;
    uint16_t app_id;
    uint16_t conn_id;
    uint16_t service_handle;
    esp_gatt_srvc_id_t service_id;
    gatts_wrapper_char_inst_t *characteristic;
    struct gatts_profile_inst *next;
};

typedef struct gatts_profile_inst gatts_profile_inst_t;

/* 服务事件处理队列结构体 */
struct gatts_evt_handle_queue {
    gatts_profile_inst_t *profile;
    int event;
};


static const char *TAG = "gatt_server_wrapper";

/* 服务实例配置文件链表头 */
static gatts_profile_inst_t *profile_inst_head = NULL;
static int32_t profile_inst_app = -1;

static uint8_t default_char_value[1] = {0x00};
static uint8_t default_char_ccc[2] = {0x00,0x00};

static uint8_t adv_config_done = 0;
static QueueHandle_t evt_handle_queue = NULL;


static char gap_dev_name[32] = GATTS_WRAPPER_DEFAULT_NAME;
static uint8_t adv_service_uuid128[16] = {
    /* LSB <--------------------------------------------------------------------------------> MSB */
    // first uuid, 16bit, [12],[13] is the value
    0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00,
};

// adv data The length of adv data must be less than 31 bytes
static esp_ble_adv_data_t adv_data = {
    .set_scan_rsp = false,
    .include_name = true,
    .include_txpower = false,
    .min_interval = 0x0006, //slave connection min interval, Time = min_interval * 1.25 msec
    .max_interval = 0x0010, //slave connection max interval, Time = max_interval * 1.25 msec
    .appearance = 0x00,
    .manufacturer_len = 0, //TEST_MANUFACTURER_DATA_LEN,
    .p_manufacturer_data =  NULL, //&test_manufacturer[0],
    .service_data_len = 0,
    .p_service_data = NULL,
    .service_uuid_len = sizeof(adv_service_uuid128),
    .p_service_uuid = adv_service_uuid128,
    .flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
};
// scan response data
static esp_ble_adv_data_t scan_rsp_data = {
    .set_scan_rsp = true,
    .include_name = true,
    .include_txpower = true,
    .appearance = 0x00,
    .manufacturer_len = 0,
    .p_manufacturer_data =  NULL,
    .service_data_len = 0,
    .p_service_data = NULL,
    .service_uuid_len = sizeof(adv_service_uuid128),
    .p_service_uuid = adv_service_uuid128,
    .flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
};

static esp_ble_adv_params_t adv_params = {
    .adv_int_min        = 0x20,
    .adv_int_max        = 0x40,
    .adv_type           = ADV_TYPE_IND,
    .own_addr_type      = BLE_ADDR_TYPE_PUBLIC,
    .channel_map        = ADV_CHNL_ALL,
    .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};


typedef struct {
    uint8_t                 *prepare_buf;
    int                     prepare_len;
} prepare_type_env_t;

static prepare_type_env_t prepare_write_env;


/**
 * @brief add a list node of gatt service profile instance
 * 
 * @param app_id register APP id
 * @return gatts_profile_inst_t* new instance pointer
 */
static gatts_profile_inst_t *add_gatts_profile_instance(uint16_t app_id)
{  
    gatts_profile_inst_t *new_inst = (gatts_profile_inst_t *)malloc(sizeof(gatts_profile_inst_t));  
    if (new_inst == NULL) return NULL;
    if (profile_inst_head == NULL) {
        profile_inst_head = new_inst;
    } else {
        gatts_profile_inst_t *current;
        current = profile_inst_head;
        while (current->next != NULL) {
            current = current->next;
        }
        current->next = new_inst;
    }

    // 设置节点信息
    memset(new_inst, 0, sizeof(gatts_profile_inst_t));
    new_inst->app_id = app_id;
    new_inst->gatts_if = ESP_GATT_IF_NONE;
    new_inst->next = NULL;
    return new_inst;
}


/**
 * @brief delete a list node of gatt service profile instance
 * 
 * @param app_id 
 */
static void delete_gatts_profile_instance(uint16_t app_id)
{
    if (profile_inst_head == NULL) return;
    gatts_profile_inst_t *current, *prev;
    current = profile_inst_head;
    // 判断是否为第一个节点
    if (current != NULL && current->app_id == app_id) {
        profile_inst_head = current->next;
        free(current);
        return;
    }
    while (current->next != NULL) {
        prev = current;
        current = current->next;
        if (current != NULL && current->app_id == app_id) {
            prev->next = current->next;

            /* release characteristic list */
            while (current->characteristic != NULL) {
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
 * @brief 通过特征UUID查找profile的特征信息
 * 
 * @param instance 特征信息列表
 * @param uuid 目标特征UUID
 * @return gatts_wrapper_char_inst_t* 查找信息结果
 */
static gatts_wrapper_char_inst_t *find_gatts_profile_char_instance(gatts_wrapper_char_inst_t *instance, esp_bt_uuid_t *uuid)
{
    gatts_wrapper_char_inst_t *char_inst = NULL;
    for (char_inst = instance; char_inst; char_inst = char_inst->next) {
        if (char_inst->uuid.len == uuid->len) {
            if (char_inst->uuid.len == ESP_UUID_LEN_16) {
                if (char_inst->uuid.uuid.uuid16 == uuid->uuid.uuid16) {
                    break;
                }
            } else if (char_inst->uuid.len == ESP_UUID_LEN_32) {
                if (char_inst->uuid.uuid.uuid32 == uuid->uuid.uuid32) {
                    break;
                }
            } else if (char_inst->uuid.len == ESP_UUID_LEN_128) {
                if (memcmp(char_inst->uuid.uuid.uuid128, uuid->uuid.uuid128, ESP_UUID_LEN_128) == 0) {
                    break;
                }
            }
        }
    }

    return char_inst;
}


/**
 * @brief 添加一个特征实例
 * 
 * @param inst_list 特征实例链表头
 */
static gatts_wrapper_char_inst_t *add_gatts_char_instance(gatts_wrapper_char_inst_t **inst_list) 
{
    gatts_wrapper_char_inst_t *new_char = (gatts_wrapper_char_inst_t *)malloc(sizeof(gatts_wrapper_char_inst_t));  
    if (new_char == NULL) return NULL;
    if (*inst_list == NULL) {
        *inst_list = new_char;
    } else {
        gatts_wrapper_char_inst_t *current;
        current = *inst_list;
        while (current->next != NULL) {
            current = current->next;
        }
        current->next = new_char;
    }

    memset(new_char, 0, sizeof(gatts_wrapper_char_inst_t));
    new_char->next = NULL;
    return new_char;
}





static void prepare_write_event_env(esp_gatt_if_t gatts_if, prepare_type_env_t *prepare_write_env, esp_ble_gatts_cb_param_t *param){
    esp_gatt_status_t status = ESP_GATT_OK;
    if (prepare_write_env->prepare_buf == NULL) {
        prepare_write_env->prepare_buf = (uint8_t *)malloc(PREPARE_BUF_MAX_SIZE * sizeof(uint8_t));
        prepare_write_env->prepare_len = 0;
        if (prepare_write_env->prepare_buf == NULL) {
            ESP_LOGE(TAG, "%s, Gatt_server prep no mem", __func__);
            status = ESP_GATT_NO_RESOURCES;
        }
    } else {
        if(param->write.offset > PREPARE_BUF_MAX_SIZE) {
            status = ESP_GATT_INVALID_OFFSET;
        } else if ((param->write.offset + param->write.len) > PREPARE_BUF_MAX_SIZE) {
            status = ESP_GATT_INVALID_ATTR_LEN;
        }
    }
    /*send response when param->write.need_rsp is true */
    if (param->write.need_rsp){
        esp_gatt_rsp_t *gatt_rsp = (esp_gatt_rsp_t *)malloc(sizeof(esp_gatt_rsp_t));
        if (gatt_rsp != NULL){
            gatt_rsp->attr_value.len = param->write.len;
            gatt_rsp->attr_value.handle = param->write.handle;
            gatt_rsp->attr_value.offset = param->write.offset;
            gatt_rsp->attr_value.auth_req = ESP_GATT_AUTH_REQ_NONE;
            memcpy(gatt_rsp->attr_value.value, param->write.value, param->write.len);
            esp_err_t response_err = esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, status, gatt_rsp);
            if (response_err != ESP_OK){
               ESP_LOGE(TAG, "Send response error");
            }
            free(gatt_rsp);
        }else{
            ESP_LOGE(TAG, "%s, malloc failed", __func__);
        }
    }
    if (status != ESP_GATT_OK){
        return;
    }
    memcpy(prepare_write_env->prepare_buf + prepare_write_env->prepare_len,
           param->write.value,
           param->write.len);
    prepare_write_env->prepare_len += param->write.len;
}

static void exec_write_event_env(prepare_type_env_t *prepare_write_env, esp_ble_gatts_cb_param_t *param){
    if (param->exec_write.exec_write_flag == ESP_GATT_PREP_WRITE_EXEC){

    }else{
        ESP_LOGI(TAG,"ESP_GATT_PREP_WRITE_CANCEL");
    }
    if (prepare_write_env->prepare_buf) {
        free(prepare_write_env->prepare_buf);
        prepare_write_env->prepare_buf = NULL;
    }
    prepare_write_env->prepare_len = 0;
}

static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    switch (event) {
    case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
        adv_config_done &= (~GATTS_ADV_CONFIG_BIT);
        if (adv_config_done == 0) {
            esp_ble_gap_start_advertising(&adv_params);
        }
        break;
    case ESP_GAP_BLE_SCAN_RSP_DATA_SET_COMPLETE_EVT:
        adv_config_done &= (~GATTS_SCAN_RSP_BIT);
        if (adv_config_done == 0) {
            esp_ble_gap_start_advertising(&adv_params);
        }
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
 * @brief GATT service profile 处理
 * 
 * @param event GATT Server callback function events
 * @param gatts_if Gatt interface type, different application on GATT client use different gatt_if
 * @param param Gatt server callback parameters union
 */
static void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
{
    gatts_profile_inst_t *profile_inst = NULL;
    for (profile_inst = profile_inst_head; profile_inst; profile_inst = profile_inst->next) {
        if (profile_inst->gatts_if == gatts_if) {
            break;
        }
    }
    if (profile_inst == NULL && event != ESP_GATTS_REG_EVT) return;

    switch (event) {
    case ESP_GATTS_REG_EVT: {
        if (param->reg.status == ESP_GATT_OK) {
            for (profile_inst = profile_inst_head; profile_inst; profile_inst = profile_inst->next) {
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
        if (profile_inst->app_id == profile_inst_app) {
            esp_err_t set_dev_name_ret = esp_ble_gap_set_device_name(gap_dev_name);
            if (set_dev_name_ret){
                ESP_LOGE(TAG, "set device name failed, error code = %x", set_dev_name_ret);
            }

            //config adv data
            esp_err_t ret = esp_ble_gap_config_adv_data(&adv_data);
            if (ret){
                ESP_LOGE(TAG, "config adv data failed, error code = %x", ret);
            }
            adv_config_done |= GATTS_ADV_CONFIG_BIT;
            //config scan response data
            ret = esp_ble_gap_config_adv_data(&scan_rsp_data);
            if (ret){
                ESP_LOGE(TAG, "config scan response data failed, error code = %x", ret);
            }
            adv_config_done |= GATTS_SCAN_RSP_BIT;
        }

        /* 特征计数 创建服务 */
        uint16_t num_handle = 1;    // Service Declaration
        for (gatts_wrapper_char_inst_t *char_inst = profile_inst->characteristic; char_inst; char_inst = char_inst->next) {
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
        ESP_LOGD(TAG, "ESP_GATTS_READ_EVT");
        uint16_t length = 0;
        const uint8_t *prf_char;
        esp_ble_gatts_get_attr_value(param->read.handle,  &length, &prf_char);
        esp_gatt_rsp_t rsp = {0};
        rsp.attr_value.handle = param->read.handle;
        rsp.attr_value.len = length;
        memcpy(rsp.attr_value.value, prf_char, length);
        esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, ESP_GATT_OK, &rsp);
        break;
    }
    case ESP_GATTS_WRITE_EVT: {
        if (!param->write.is_prep) {
            /* send response when param->write.need_rsp is true*/
            if (param->write.need_rsp){
                esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, ESP_GATT_OK, NULL);
            }
            /* 遍历特征列表 判断写入特征描述 */
            for (gatts_wrapper_char_inst_t *char_inst = profile_inst->characteristic; char_inst; char_inst = char_inst->next) {
                if (char_inst->descr_handle == param->write.handle) {
                    esp_ble_gatts_set_attr_value(param->write.handle, param->write.len, param->write.value);
                    break;
                } else if (char_inst->handle == param->write.handle) {
                    // 接收数据送入回调函数
                    if (profile_inst->recv_cb != NULL) {
                        gatts_wrapper_recv_t info = {0};
                        info.index = char_inst->index;
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
        } else {
            /* handle prepare write */
            prepare_write_event_env(gatts_if, &prepare_write_env, param);
        }
        break;
    }
    case ESP_GATTS_EXEC_WRITE_EVT:
        ESP_LOGI(TAG,"ESP_GATTS_EXEC_WRITE_EVT");
        esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, ESP_GATT_OK, NULL);
        // 接收数据送入回调函数
        for (gatts_wrapper_char_inst_t *char_inst = profile_inst->characteristic; char_inst; char_inst = char_inst->next) {
            if (char_inst->handle == param->write.handle) {
                if (profile_inst->recv_cb != NULL) {
                    gatts_wrapper_recv_t info = {0};
                    info.index = char_inst->index;
                    info.gatts_if = gatts_if;
                    info.conn_id = param->write.conn_id;
                    info.data = prepare_write_env.prepare_buf;
                    info.len = prepare_write_env.prepare_len;
                    info.handle = param->write.handle;

                    profile_inst->recv_cb(info);
                }
                break;
            }
        }
        
        exec_write_event_env(&prepare_write_env, param);
        break;
    case ESP_GATTS_MTU_EVT:
        if (profile_inst->app_id == profile_inst_app) {
            ESP_LOGI(TAG, "ESP_GATTS_MTU_EVT, MTU %d", param->mtu.mtu);
        }
        break;
    case ESP_GATTS_UNREG_EVT:
        break;
    case ESP_GATTS_CREATE_EVT: {
        profile_inst->service_handle = param->create.service_handle;

        esp_ble_gatts_start_service(profile_inst->service_handle);

        struct gatts_evt_handle_queue handle;
        handle.event = ESP_GATTS_CREATE_EVT;
        handle.profile = profile_inst;
        xQueueSend(evt_handle_queue, &handle, 10 / portTICK_PERIOD_MS);
        
        break;
    }
    case ESP_GATTS_ADD_INCL_SRVC_EVT:
        break;
    case ESP_GATTS_ADD_CHAR_EVT: {
        /* 查找特征UUID 保存特征句柄 */
        gatts_wrapper_char_inst_t *char_inst = NULL;
        char_inst = find_gatts_profile_char_instance(profile_inst->characteristic, &param->add_char.char_uuid);
        if (char_inst != NULL) {
            char_inst->handle = param->add_char.attr_handle;
        }

        struct gatts_evt_handle_queue handle;
        handle.event = ESP_GATTS_ADD_CHAR_EVT;
        handle.profile = profile_inst;
        xQueueSend(evt_handle_queue, &handle, 10 / portTICK_PERIOD_MS);
      
        break;
    }
    case ESP_GATTS_ADD_CHAR_DESCR_EVT: {
        for (gatts_wrapper_char_inst_t *char_inst = profile_inst->characteristic; char_inst; char_inst = char_inst->next) {
            if (char_inst->descr_handle == 0 && char_inst->descr_uuid.len != 0) {
                char_inst->descr_handle = param->add_char_descr.attr_handle;
                break;
            }
        }
        struct gatts_evt_handle_queue handle;
        handle.event = ESP_GATTS_ADD_CHAR_DESCR_EVT;
        handle.profile = profile_inst;
        xQueueSend(evt_handle_queue, &handle, 10 / portTICK_PERIOD_MS);
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
        if (profile_inst->app_id == profile_inst_app) {
            esp_ble_conn_update_params_t conn_params = {0};
            memcpy(conn_params.bda, param->connect.remote_bda, sizeof(esp_bd_addr_t));
            /* For the IOS system, please reference the apple official documents about the ble connection parameters restrictions. */
            conn_params.latency = 0;
            conn_params.max_int = 0x20;    // max_int = 0x20*1.25ms = 40ms
            conn_params.min_int = 0x10;    // min_int = 0x10*1.25ms = 20ms
            conn_params.timeout = 400;    // timeout = 400*10ms = 4000ms
            //start sent the update connection parameters to the peer device.
            esp_ble_gap_update_conn_params(&conn_params);
        }
        profile_inst->conn_id = param->connect.conn_id;
        break;
    }
    case ESP_GATTS_DISCONNECT_EVT:
        if (profile_inst->app_id == profile_inst_app) {
            ESP_LOGI(TAG, "ESP_GATTS_DISCONNECT_EVT, disconnect reason 0x%x", param->disconnect.reason);
            esp_ble_gap_start_advertising(&adv_params);
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



static void gatts_evt_handle_task(void *pvParameters)
{
    struct gatts_evt_handle_queue handle;

    esp_attr_value_t default_attr_value = {
        .attr_max_len = GATTS_WRAPPER_VAL_LEN_MAX,
        .attr_len     = sizeof(default_char_value),
        .attr_value   = default_char_value,
    };
    /* 客户端配置描述符值定义 */
    esp_attr_value_t descr_value = {
        .attr_max_len = sizeof (uint16_t),
        .attr_len = sizeof(default_char_ccc),
        .attr_value = default_char_ccc,
    };

    while (1) {
        xQueueReceive(evt_handle_queue, &handle, portMAX_DELAY);
        switch (handle.event) {
        case ESP_GATTS_CREATE_EVT:
            /* 遍历特征列表 添加特征 */
            for (gatts_wrapper_char_inst_t *char_inst = handle.profile->characteristic; char_inst; char_inst = char_inst->next) {
                if (char_inst->handle == 0) {
                    esp_ble_gatts_add_char(handle.profile->service_handle, &char_inst->uuid,
                                            char_inst->perm, char_inst->property, &default_attr_value, NULL);
                    break;
                }
            }
            break;
        case ESP_GATTS_ADD_CHAR_EVT:
            /* 遍历特征列表 添加特征或描述符 */
            for (gatts_wrapper_char_inst_t *char_inst = handle.profile->characteristic; char_inst; char_inst = char_inst->next) {
                if (char_inst->handle == 0) {
                    esp_ble_gatts_add_char(handle.profile->service_handle, &char_inst->uuid,
                                            char_inst->perm, char_inst->property, &default_attr_value, NULL);
                    break;
                }
                if (char_inst->descr_handle == 0 && char_inst->descr_uuid.len != 0) {
                    esp_ble_gatts_add_char_descr(handle.profile->service_handle, &char_inst->descr_uuid,
                                            ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE, &descr_value, NULL);
                    break;
                }
            }
            break;
        case ESP_GATTS_ADD_CHAR_DESCR_EVT:
            /* 遍历特征列表 添加下一个特征 */
            for (gatts_wrapper_char_inst_t *char_inst = handle.profile->characteristic; char_inst; char_inst = char_inst->next) {
                if (char_inst->handle == 0) {
                    esp_ble_gatts_add_char(handle.profile->service_handle, &char_inst->uuid,
                                            char_inst->perm, char_inst->property, &default_attr_value, NULL);
                    break;
                }
            }
            break;
        default:
            break;
        }
    }
}


void gatts_wrapper_gap_config(gatts_wrapper_gap_config_t *config)
{
    int name_len = strlen(config->dev_name);
    if (name_len > sizeof(gap_dev_name)) {
        name_len = sizeof(gap_dev_name) -1;
    }
    memcpy(gap_dev_name, config->dev_name, name_len);
    gap_dev_name[name_len] = '\0';

    if (config->adv_uuid != 0) {
        *(uint16_t *)&adv_service_uuid128[12] = config->adv_uuid;
    }
    if (config->adv_len > 0 && config->adv_len <= MANUFACTURER_DATA_LEN_MAX) {
        adv_data.p_manufacturer_data = config->adv_data;
        adv_data.manufacturer_len = config->adv_len;
    }
}

void gatt_server_wrapper_init()
{
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

    esp_err_t local_mtu_ret = esp_ble_gatt_set_local_mtu(GATTS_WRAPPER_DEFAULT_MTU_SIZE);
    if (local_mtu_ret){
        ESP_LOGE(TAG, "set local  MTU failed, error code = %x", local_mtu_ret);
    }

    
    evt_handle_queue = xQueueCreate(1, sizeof(struct gatts_evt_handle_queue));
    if (evt_handle_queue == NULL) {
        ESP_LOGE(TAG, "evt_handle_queue create failed!");
    }
    xTaskCreate(gatts_evt_handle_task, "gatts_handle_task", 3 * 1024, NULL, 8, NULL);
}


void gatts_wrapper_add_service(uint16_t app_id, esp_bt_uuid_t *service_uuid)
{
    /* 记录第一次调用的APP ID 以处理GAP事件 */
    if (profile_inst_app == -1) {
        profile_inst_app = app_id;
    }
    /* 创建服务配置文件实例 */
    gatts_profile_inst_t *new_inst = add_gatts_profile_instance(app_id);
    if (new_inst != NULL) {
        /*  服务配置 */
        new_inst->service_id.is_primary = true;
        new_inst->service_id.id.inst_id = 0x00;
        new_inst->service_id.id.uuid.len = service_uuid->len;
        new_inst->service_id.id.uuid.uuid = service_uuid->uuid;
        
    }
}


void gatt_server_wrapper_delete_service(uint16_t app_id)
{
    gatts_profile_inst_t *current = NULL;
    for (current = profile_inst_head; current; current = current->next) {
        if (current->app_id == app_id) {
            esp_ble_gatts_app_unregister(current->gatts_if);
            delete_gatts_profile_instance(app_id);
            break;
        }
    }
}

void gatts_wrapper_service_add_char(uint16_t app_id, gatts_wrapper_char_config_t *config)
{
    gatts_profile_inst_t *current = NULL;
    for (current = profile_inst_head; current; current = current->next) {
        if (current->app_id == app_id) {
            /* 添加特征配置 */
            gatts_wrapper_char_inst_t *char_inst = add_gatts_char_instance(&current->characteristic);
            if (char_inst != NULL) {
                char_inst->index = config->index;
                char_inst->perm = config->perm;
                char_inst->property = config->property;
                memcpy(&char_inst->uuid, &config->uuid, sizeof(esp_bt_uuid_t));
                if (config->property & ESP_GATT_CHAR_PROP_BIT_NOTIFY) {
                    /* 描述符配置 */
                    char_inst->descr_uuid.len = ESP_UUID_LEN_16;
                    char_inst->descr_uuid.uuid.uuid16 = ESP_GATT_UUID_CHAR_CLIENT_CONFIG;
                }
            }
            break;
        }
    }
}

void gatts_wrapper_service_register_callback(uint16_t app_id, gatts_wrapper_recv_cb_t callback)
{
    gatts_profile_inst_t *current = NULL;
    for (current = profile_inst_head; current; current = current->next) {
        if (current->app_id == app_id) {
            /* 接收回调配置 */
            current->recv_cb = callback;
            break;
        }
    }
    /* register gatts app */
    if (esp_ble_gatts_app_register(app_id)) {
        ESP_LOGE(TAG, "esp_ble_gatts_app_register failed!");
    }
}

void gatts_wrapper_create_default_service()
{
    esp_bt_uuid_t service = {
        .len = ESP_UUID_LEN_16,
        .uuid.uuid16 = GATTS_WRAPPER_DEFAULT_SVC_UUID,
    };
    gatts_wrapper_add_service(GATTS_WRAPPER_DEFAULT_APP_ID, &service);

    gatts_wrapper_char_config_t char_config = {
        .index = GATTS_WRAPPER_DEFAULT_CHAR_INDEX,
        .perm = ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
        .property = ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_WRITE | ESP_GATT_CHAR_PROP_BIT_NOTIFY,
        .uuid.len = ESP_UUID_LEN_16,
        .uuid.uuid.uuid16 = GATTS_WRAPPER_DEFAULT_CHAR_UUID,
    };
    gatts_wrapper_service_add_char(GATTS_WRAPPER_DEFAULT_APP_ID, &char_config);
}

void gatts_wrapper_default_service_register_callback(gatts_wrapper_recv_cb_t callback)
{
    gatts_wrapper_service_register_callback(GATTS_WRAPPER_DEFAULT_APP_ID, callback);
}