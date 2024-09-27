/*
C++ API wrapper of IDF's ADC driver 
Copyright (C) 2022 Mark Ma
*/

#include "adc.h"
#include "os_api.h"
#include <esp_adc/adc_oneshot.h>
#include <esp_adc/adc_continuous.h>
#include <esp_adc/adc_cali.h>
#include <esp_adc/adc_cali_scheme.h>
#include <esp_log.h>

static const char TAG[] = "adc";

void* ADCOneshot::_adc_handle[2] = {nullptr, nullptr};
void* ADCContinuous::_adc_handle[2] = {nullptr, nullptr};

ADC::ADC(adc_channel_t channel, adc_unit_t unit, adc_atten_t atten)
: _unit(unit), _channel(channel), _atten(atten) {}

int ADC::init() {
	adc_cali_curve_fitting_config_t cali_config = {
		.unit_id = _unit,
		.chan = _channel,
		.atten = _atten,
		.bitwidth = ADC_BITWIDTH_12,
	};
	ESP_ERROR_CHECK(adc_cali_create_scheme_curve_fitting(&cali_config, (adc_cali_handle_t*)&_cali_handle));
	return 0;
}

int ADC::raw_to_voltage(int raw) {
	if (raw < 0)
		return raw;
	int voltage = 0;
	ESP_ERROR_CHECK(adc_cali_raw_to_voltage((adc_cali_handle_t )_cali_handle, raw, &voltage));
	return voltage;
}

int ADCOneshot::init() {
	if (!_adc_handle[_unit]) {
		adc_oneshot_unit_init_cfg_t init_config = {
			.unit_id = _unit,
		};
		ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config, (adc_oneshot_unit_handle_t*)&_adc_handle[_unit]));
	}
	adc_oneshot_chan_cfg_t config = {
		.atten = _atten,
		.bitwidth = ADC_BITWIDTH_12,
	};
	ESP_ERROR_CHECK(adc_oneshot_config_channel((adc_oneshot_unit_handle_t)_adc_handle[_unit], _channel, &config));
	return ADC::init();
}

int ADCOneshot::get_raw() {
	int adc_raw = -1;
	int res = adc_oneshot_read((adc_oneshot_unit_handle_t)_adc_handle[_unit], _channel, &adc_raw);
	if (res != ESP_OK)
		ESP_LOGE(TAG, "adc channel [%d] oneshot err %04X", _channel, res);
	return adc_raw;
}

int ADCContinuous::init(uint32_t buffer_size, uint32_t conv_frame_size, uint32_t sample_freq_hz) {
	if (!_adc_handle[_unit]) {
		adc_continuous_handle_cfg_t adc_config = {
			.max_store_buf_size = buffer_size,
			.conv_frame_size = conv_frame_size,
		};
		ESP_ERROR_CHECK(adc_continuous_new_handle(&adc_config, (adc_continuous_handle_t*)&_adc_handle[_unit]));
	}
    adc_digi_pattern_config_t adc_pattern = {
		.atten = _atten,
		.channel = _channel,
		.unit = _unit,
		.bit_width = ADC_BITWIDTH_12,
	};
	adc_continuous_config_t dig_cfg = {
		.pattern_num = 1,
		.adc_pattern = &adc_pattern,
		// @ref SOC_ADC_SAMPLE_FREQ_THRES_LOW
		// s3 611 - 83333
        .sample_freq_hz = sample_freq_hz,
        .conv_mode = ADC_CONV_SINGLE_UNIT_1,
        .format = ADC_DIGI_OUTPUT_FORMAT_TYPE2,
    };
    ESP_ERROR_CHECK(adc_continuous_config((adc_continuous_handle_t)_adc_handle[_unit], &dig_cfg));
	return ADC::init();
}

void ADCContinuous::start() {
	ESP_ERROR_CHECK(adc_continuous_start((adc_continuous_handle_t)_adc_handle[_unit]));
}

void ADCContinuous::stop() {
	ESP_ERROR_CHECK(adc_continuous_stop((adc_continuous_handle_t)_adc_handle[_unit]));
}

void ADCContinuous::set_cb(Callback cb) {
	adc_continuous_evt_cbs_t cbs = {
        .on_conv_done = +[](adc_continuous_handle_t handle, const adc_continuous_evt_data_t *edata, void *user_data) -> bool IRAM_ATTR {
			return ((Callback)user_data)();
		},
    };
	ESP_ERROR_CHECK(adc_continuous_register_event_callbacks((adc_continuous_handle_t)_adc_handle[_unit], &cbs, (void*)cb));
}

int ADCContinuous::get_raw(adc_digi_output_data_t buff[], uint32_t max_size) {
	uint32_t ret_num = -1;
	int res = adc_continuous_read((adc_continuous_handle_t)_adc_handle[_unit], (uint8_t*)buff, max_size * SOC_ADC_DIGI_RESULT_BYTES, &ret_num, 0);
	if (res != ESP_OK)
		ESP_LOGE(TAG, "adc channel [%d] continuous err %04X", _channel, res);
	return ret_num;
}
