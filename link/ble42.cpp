#include "ble42.h"
#include <esp_gap_ble_api.h>
#include <esp_gatts_api.h>
#include <esp_gatt_common_api.h>
#include <esp_bt_main.h>
#include <esp_log.h>
#include <esp_bt.h>
#include <string.h>
#include <atomic>

namespace ble42 {

static const char TAG[] = "BLE";

#define SPP_DATA_MAX_LEN           (512)

#define SPP_PROFILE_NUM             1
#define SPP_PROFILE_APP_IDX         0
#define ESP_SPP_APP_ID              0x56
#define SPP_SVC_INST_ID	            0

static std::string			_device_name;
static std::string			_sn;
static std::atomic_uint16_t _mtu_size = 0;
static std::atomic_bool		_is_connected = false;
static std::atomic_uint16_t spp_conn_id = 0xffff;
static std::atomic<esp_gatt_if_t> spp_gatts_if = 0xff;

static ConnCallback _connect_cb = nullptr;
static ConnCallback _disconnect_cb = nullptr;
static RecvCallback _spp_cbs[SPP_IDX_NB];
static pfBLERecv _adv_cb = nullptr;

static uint16_t spp_handle_table[SPP_IDX_NB];

static esp_ble_adv_params_t spp_adv_params = {
	#ifdef PROJECT_SMBNA
	.adv_int_min        = 0x100,
	.adv_int_max        = 0x200,
	#else
	.adv_int_min        = 0x20,
	.adv_int_max        = 0x40,
	#endif // 低功耗处理
	.adv_type           = ADV_TYPE_IND,
	.own_addr_type      = BLE_ADDR_TYPE_PUBLIC,
	.channel_map        = ADV_CHNL_ALL,
	.adv_filter_policy  = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

struct gatts_profile_inst {
	esp_gatts_cb_t gatts_cb;
	uint16_t gatts_if;
	uint16_t app_id;
	uint16_t conn_id;
	uint16_t service_handle;
	esp_gatt_srvc_id_t service_id;
	uint16_t char_handle;
	esp_bt_uuid_t char_uuid;
	esp_gatt_perm_t perm;
	esp_gatt_char_prop_t property;
	uint16_t descr_handle;
	esp_bt_uuid_t descr_uuid;
};

static void gatts_profile_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);

/* One gatt-based profile one app_id and one gatts_if, this array will store the gatts_if returned by ESP_GATTS_REG_EVT */
static struct gatts_profile_inst spp_profile_tab[SPP_PROFILE_NUM] = {
	[SPP_PROFILE_APP_IDX] = {
		.gatts_cb = gatts_profile_event_handler,
		.gatts_if = ESP_GATT_IF_NONE,       /* Not get the gatt_if, so initial is ESP_GATT_IF_NONE */
	},
};

/*
*  SPP PROFILE ATTRIBUTES
****************************************************************************************
*/

#define CHAR_DECLARATION_SIZE   (sizeof(uint8_t))
static const uint8_t char_prop_read_write_notify = ESP_GATT_CHAR_PROP_BIT_WRITE_NR|ESP_GATT_CHAR_PROP_BIT_READ|ESP_GATT_CHAR_PROP_BIT_NOTIFY;
static const uint16_t primary_service_uuid = ESP_GATT_UUID_PRI_SERVICE;
static const uint16_t character_declaration_uuid = ESP_GATT_UUID_CHAR_DECLARE;
static const uint8_t  spp_data_receive_val[512] = {0x00};
static const uint8_t  spp_data_notify_ccc[2] = {0x00, 0x00};
/// SPP Service
static const uint16_t spp_service_uuid = 0xABF0;
///SPP Service - data receive characteristic, notify&read&write without response
static const uint16_t spp_data_receive_uuid1 = 0xABF1;
///SPP Service
static const uint16_t spp_production_uuid = 0xABF8;
///SPP Service - data receive characteristic, notify&read&write without response
static const uint16_t spp_data_receive_uuid2 = 0xABF9;
///SPP Service - data notify characteristic, notify&read
static const uint16_t spp_data_notify_uuid = 0x2902;

///Full HRS Database Description - Used to add attributes into the database
static const esp_gatts_attr_db_t spp_gatt_db[SPP_IDX_NB] = {
	//SPP -  Service Declaration
	[SPP_IDX_SVC]                      	=
	{{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&primary_service_uuid, ESP_GATT_PERM_READ,
	sizeof(spp_service_uuid), sizeof(spp_service_uuid), (uint8_t *)&spp_service_uuid}},

	//SPP -  data receive characteristic Declaration
	[SPP_IDX_SPP_CHAR1]            =
	{{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&character_declaration_uuid, ESP_GATT_PERM_READ|ESP_GATT_PERM_WRITE,
	CHAR_DECLARATION_SIZE, CHAR_DECLARATION_SIZE, (uint8_t *)&char_prop_read_write_notify}},

	//SPP -  data receive characteristic Value
	[SPP_IDX_SPP_VAL1]             	=
	{{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&spp_data_receive_uuid1, ESP_GATT_PERM_READ|ESP_GATT_PERM_WRITE,
	SPP_DATA_MAX_LEN, sizeof(spp_data_receive_val), (uint8_t *)spp_data_receive_val}},

	//SPP -  data notify characteristic Value
    [SPP_IDX_SPP_NTY1]   =
    {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&spp_data_notify_uuid, ESP_GATT_PERM_READ|ESP_GATT_PERM_WRITE,
    SPP_DATA_MAX_LEN, sizeof(spp_data_notify_ccc), (uint8_t *)spp_data_notify_ccc}},

	//SPP -  data receive characteristic Declaration
	[SPP_IDX_SPP_CHAR2]            =
	{{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&character_declaration_uuid, ESP_GATT_PERM_READ|ESP_GATT_PERM_WRITE,
	CHAR_DECLARATION_SIZE, CHAR_DECLARATION_SIZE, (uint8_t *)&char_prop_read_write_notify}},

	//SPP -  data receive characteristic Value
	[SPP_IDX_SPP_VAL2]             	=
	{{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&spp_data_receive_uuid2, ESP_GATT_PERM_READ|ESP_GATT_PERM_WRITE,
	SPP_DATA_MAX_LEN, sizeof(spp_data_receive_val), (uint8_t *)spp_data_receive_val}},

	//SPP -  data notify characteristic Value
    [SPP_IDX_SPP_NTY2]   =
    {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&spp_data_notify_uuid, ESP_GATT_PERM_READ|ESP_GATT_PERM_WRITE,
    SPP_DATA_MAX_LEN, sizeof(spp_data_notify_ccc), (uint8_t *)spp_data_notify_ccc}},
};

static uint8_t find_char_and_desr_index(uint16_t handle) {
	uint8_t error = 0xff;
	for(int i = 0; i < SPP_IDX_NB ; i++) {
		if(handle == spp_handle_table[i])
			return i;
	}
	return error;
}

static void call(uint8_t spp_idx, esp_ble_gatts_cb_param_t* data) {
	auto lambda = [](uint8_t spp_idx) -> const char* {
		switch (spp_idx) {
			case SPP_IDX_SPP_CHAR1: return "SPP_IDX_SPP_CHAR1";
    		case SPP_IDX_SPP_VAL1: return "SPP_IDX_SPP_VAL1";
			case SPP_IDX_SPP_NTY1: return "SPP_IDX_SPP_NTY1";
			case SPP_IDX_SPP_CHAR2: return "SPP_IDX_SPP_CHAR2";
    		case SPP_IDX_SPP_VAL2: return "SPP_IDX_SPP_VAL2";
			case SPP_IDX_SPP_NTY2: return "SPP_IDX_SPP_NTY2";
			default: return "unknown spp";
		}
	};
	if(data->write.is_prep)
		return;
	else if (_spp_cbs[spp_idx])
		_spp_cbs[spp_idx]({data->write.value, data->write.len});
	else
		ESP_LOG_BUFFER_HEX(lambda(spp_idx), data->write.value, data->write.len);
}


//AD Structure格式：Length(1B), AD_TYPE(1B), AD_DATA
static void ble_init_adv_data(std::string_view name, std::string_view sn) {
	int idx = 0;
	uint8_t raw_adv_data[ESP_BLE_ADV_DATA_LEN_MAX];
	//AD_TYPE == 0x01，标识设备 LE 物理连接的功能
	raw_adv_data[idx++] = 2;
	raw_adv_data[idx++] = ESP_BLE_AD_TYPE_FLAG;
	/*	bit 0: LE 有限发现模式
		bit 1: LE 普通发现模式
		bit 2: 不支持 BR/EDR
		bit 3: 对 Same Device Capable(Controller) 同时支持 BLE 和 BR/EDR
		bit 4: 对 Same Device Capable(Host) 同时支持 BLE 和 BR/EDR
		bit 5..7: 预留
	*/
	//0x06 == 0b110
	raw_adv_data[idx++] = ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT;

	//AD_TYPE==0x03, Complete List of 16-bit Service Class UUIDs */
	raw_adv_data[idx++] = 3;
	raw_adv_data[idx++] = ESP_BLE_AD_TYPE_16SRV_CMPL;
	raw_adv_data[idx++] = 0xF0;
	raw_adv_data[idx++] = 0xAB;

	// /* Complete Local Name in advertising */
	// 0x0F,0x09, 'E', 'S', 'P', '_', 'S', 'P', 'P', '_', 'S', 'E', 'R','V', 'E', 'R'
	raw_adv_data[idx++] = name.size() + 1;
	raw_adv_data[idx++] = ESP_BLE_AD_TYPE_NAME_CMPL;

	assert(idx + name.size() <= sizeof(raw_adv_data));

	for (auto ch : name)
		raw_adv_data[idx++] = ch;

	if (sn.size() >= 6) {
		assert(idx + 6 <= sizeof(raw_adv_data));
		raw_adv_data[idx++] = 3 + 6;
		raw_adv_data[idx++] = ESP_BLE_AD_MANUFACTURER_SPECIFIC_TYPE;//自定义
		raw_adv_data[idx++] = 0xAA;//公司信息H 后续加入蓝牙联盟后会得到相应ID
		raw_adv_data[idx++] = 0xBB;//公司信息L 后续加入蓝牙联盟后会得到相应ID
		for (auto ch : sn.substr(0, 6))
			raw_adv_data[idx++] = ch;//产品Model
	}

	//The length of adv data must be less than 31 bytes
	ESP_LOGI(TAG, "config raw adv data %s", esp_err_to_name(esp_ble_gap_config_adv_data_raw(raw_adv_data, idx)));
	ESP_LOGI(TAG, "config raw scan rsp data %s", esp_err_to_name(esp_ble_gap_config_scan_rsp_data_raw(raw_adv_data, idx)));
}

static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
	// if(event != 3 && event != 4 && event != 6 && event != 17)
	// 	ESP_LOGW(TAG, "GAP_EVT, event %d", event);
	int len;
	esp_ble_gap_cb_param_t *scan_result = (esp_ble_gap_cb_param_t *)param;
	switch (event) {
	case ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT:
		esp_ble_gap_start_advertising(&spp_adv_params);
		break;
	// case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
		//advertising start complete event to indicate advertising start successfully or failed
		//ESP_LOGW(TAG, "advertising start %s", esp_err_to_name(param->adv_start_cmpl.status));
		// break;
	case ESP_GAP_BLE_SCAN_RESULT_EVT:
		len = scan_result->scan_rst.scan_rsp_len > 0 ? scan_result->scan_rst.scan_rsp_len : param->scan_rst.adv_data_len;
		if(param->scan_rst.ble_adv[5] == 0xAA && param->scan_rst.ble_adv[6] == 0xBB){
			if(_adv_cb != NULL){
				_adv_cb((unsigned char*)&param->scan_rst.ble_adv[7], len-7);
			}
			//esp_log_buffer_hex("DUMP", param->scan_rst.ble_adv, len);
		}
        break;
	default:
		break;
	}
}

static void gatts_profile_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param) {
	esp_ble_gatts_cb_param_t *p_data = (esp_ble_gatts_cb_param_t *) param;
	switch (event) {
		case ESP_GATTS_REG_EVT:
			ble_init_adv_data(_device_name, _sn);
			esp_ble_gatts_create_attr_tab(spp_gatt_db, gatts_if, SPP_IDX_NB, SPP_SVC_INST_ID);
			break;
		case ESP_GATTS_READ_EVT:
			find_char_and_desr_index(p_data->read.handle);
			break;
		case ESP_GATTS_WRITE_EVT:
			call(find_char_and_desr_index(p_data->write.handle), p_data);
			break;
		case ESP_GATTS_EXEC_WRITE_EVT:
			ESP_LOGI(TAG, "ESP_GATTS_EXEC_WRITE_EVT");
			break;
		case ESP_GATTS_MTU_EVT:
			_mtu_size = p_data->mtu.mtu;
			ESP_LOGI(TAG, "mtu size change to %d", p_data->mtu.mtu);
			break;
		case ESP_GATTS_CONF_EVT:
			break;
		case ESP_GATTS_UNREG_EVT:
			break;
		case ESP_GATTS_DELETE_EVT:
			break;
		case ESP_GATTS_START_EVT:
			break;
		case ESP_GATTS_STOP_EVT:
			break;
		case ESP_GATTS_CONNECT_EVT:
			_is_connected = true;
			spp_conn_id = p_data->connect.conn_id;
			spp_gatts_if = gatts_if;
			ESP_LOGI(TAG, "connected, id %d, interface type %d, addr %02X-%02X-%02X-%02X-%02X-%02X",
				p_data->connect.conn_id,
				gatts_if,
				p_data->connect.remote_bda[0],
				p_data->connect.remote_bda[1],
				p_data->connect.remote_bda[2],
				p_data->connect.remote_bda[3],
				p_data->connect.remote_bda[4],
				p_data->connect.remote_bda[5]
			);
			if (_connect_cb)
				_connect_cb();
			break;
		case ESP_GATTS_DISCONNECT_EVT:
			_is_connected = false;
			ESP_LOGI(TAG, "disconnected, gap start advertising");
			esp_ble_gap_start_advertising(&spp_adv_params);
			if (_disconnect_cb)
				_disconnect_cb();
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
		case ESP_GATTS_CREAT_ATTR_TAB_EVT:{
			ESP_LOGI(TAG, "The number handle =%x",param->add_attr_tab.num_handle);
			if (param->add_attr_tab.status != ESP_GATT_OK) {
				ESP_LOGE(TAG, "Create attribute table failed, error code=0x%x", param->add_attr_tab.status);
			}
			else if (param->add_attr_tab.num_handle != SPP_IDX_NB) {
				ESP_LOGE(TAG, "Create attribute table abnormally, num_handle (%d) doesn't equal to HRS_IDX_NB(%d)", param->add_attr_tab.num_handle, SPP_IDX_NB);
			}
			else {
				memcpy(spp_handle_table, param->add_attr_tab.handles, sizeof(spp_handle_table));
				esp_ble_gatts_start_service(spp_handle_table[SPP_IDX_SVC]);
			}
			break;
		}
		default:
			break;
	}
}


static void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param) {
	/* If event is register event, store the gatts_if for each profile */
	if (event == ESP_GATTS_REG_EVT) {
		if (param->reg.status == ESP_GATT_OK) {
			spp_profile_tab[SPP_PROFILE_APP_IDX].gatts_if = gatts_if;
		}
		else {
			ESP_LOGI(TAG, "Reg app failed, app_id %04x, status %d",param->reg.app_id, param->reg.status);
			return;
		}
	}

	for (int idx = 0; idx < SPP_PROFILE_NUM; idx++) {
		if (gatts_if == ESP_GATT_IF_NONE || /* ESP_GATT_IF_NONE, not specify a certain gatt_if, need to call every profile cb function */
				gatts_if == spp_profile_tab[idx].gatts_if) {
			if (spp_profile_tab[idx].gatts_cb) {
				spp_profile_tab[idx].gatts_cb(event, gatts_if, param);
			}
		}
	}
}

uint16_t mtu_size() {
	return _mtu_size;
}

int send(Attr attr, IBuf data) {
	if (data.size() > mtu_size()) {
		ESP_LOGE(TAG, "msg size %u is bigger than mtu size %u", data.size(), mtu_size());
		return -1;
	}
	auto err = esp_ble_gatts_send_indicate(spp_gatts_if, spp_conn_id, spp_handle_table[attr], data.size(), (uint8_t*)data.data(), false);
	return err == ESP_OK ? 0 : -2;
}

bool connected() {
	return _is_connected;
}

void register_recv_cb(Attr attr, RecvCallback cb) {
	_spp_cbs[attr] = cb;
}

void register_connect_cb(ConnCallback cb) {
	_connect_cb = cb;
}

void register_disconnect_cb(ConnCallback cb) {
	_disconnect_cb = cb;
}

void deauth() {
	if (!_is_connected)
		return;
	esp_ble_gatts_close(spp_gatts_if, spp_conn_id);
}

void init(std::string_view host_name, std::string_view sn) {
	esp_err_t ret;
	esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();

	//前两字符作为广播帧尾
	_sn = sn;
	//作为广播帧中间
	_device_name = host_name;

	ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));

	ret = esp_bt_controller_init(&bt_cfg);
	if (ret != ESP_OK) {
		ESP_LOGE(TAG, "init controller failed: %s", esp_err_to_name(ret));
		return;
	}

	ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
	if (ret != ESP_OK) {
		ESP_LOGE(TAG, "enable controller failed: %s", esp_err_to_name(ret));
		return;
	}

	#if ESP_IDF_VERSION >= 0x00050200U
	esp_bluedroid_config_t bluedroid_cfg = BT_BLUEDROID_INIT_CONFIG_DEFAULT();
	ret = esp_bluedroid_init_with_cfg(&bluedroid_cfg);
	#else
	ret = esp_bluedroid_init();
	#endif
	if (ret != ESP_OK) {
		ESP_LOGE(TAG, "init bluetooth failed: %s", esp_err_to_name(ret));
		return;
	}
	ret = esp_bluedroid_enable();
	if (ret != ESP_OK) {
		ESP_LOGE(TAG, "enable bluetooth failed: %s", esp_err_to_name(ret));
		return;
	}

	ESP_LOGI(TAG, "bluetooth init");

	esp_ble_gatts_register_callback(gatts_event_handler);
	esp_ble_gap_register_callback(gap_event_handler);
	esp_ble_gatts_app_register(ESP_SPP_APP_ID);
	esp_ble_gatt_set_local_mtu(SPP_DATA_MAX_LEN);
	esp_ble_gap_set_device_name(_device_name.data());

}

void deinit() {
	esp_bluedroid_disable();
	esp_bluedroid_deinit();
	esp_bt_controller_disable();
	esp_bt_controller_deinit();
}

/*
void beacon_start_recv(pfBLERecv cb){//开启接收
	static esp_ble_scan_params_t ble_scan_params = {
		.scan_type              = BLE_SCAN_TYPE_PASSIVE,
		.own_addr_type          = BLE_ADDR_TYPE_PUBLIC,
		.scan_filter_policy     = BLE_SCAN_FILTER_ALLOW_ALL,
		.scan_interval          = 0x50,
		.scan_window            = 0x30,
		.scan_duplicate         = BLE_SCAN_DUPLICATE_DISABLE
	};

	esp_err_t scan_ret = esp_ble_gap_set_scan_params(&ble_scan_params);
    if (scan_ret){
        ESP_LOGE("ERROR", "set scan params error, error code = %x", scan_ret);
    }

    uint32_t duration = 0;
    esp_ble_gap_start_scanning(duration);
	_adv_cb = cb;
}
*/

void beacon_update(unsigned char* value, int len){
	int idx = 0;
	uint8_t raw_adv_data[31];
	// /* Flags */
	// 0x02,0x01,0x06,
	raw_adv_data[idx++] = 0x02;
	raw_adv_data[idx++] = 0x01;
	raw_adv_data[idx++] = 0x06;
	if(len + 7 > sizeof(raw_adv_data)){
		ESP_LOGE("ERROR", "adv len exceed 31 max");
		return;
	}
	raw_adv_data[idx++] = len + 3;
	raw_adv_data[idx++] = 0xFF;
	raw_adv_data[idx++] = 0xAA;
	raw_adv_data[idx++] = 0xBB;
	memcpy(&raw_adv_data[idx], value, len);
	if(!_is_connected){
		esp_ble_gap_stop_advertising();
		esp_ble_gap_config_adv_data_raw(raw_adv_data, len + 7);
	}

}

// void beacon_stop_recv(){//关闭接收
// 	esp_ble_gap_stop_scanning();
// }

}
