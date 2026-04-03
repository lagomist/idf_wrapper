#include "adc_wrapper.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_continuous.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_log.h"

namespace Wrapper {

constexpr static const char TAG[] = "Wrapper::adc";

void* ADCOneshot::_adc_handle[2] = {nullptr, nullptr};
void* ADCContinuous::_adc_handle[2] = {nullptr, nullptr};

ADC::ADC(uint8_t channel, uint8_t unit, Atten atten)
: _unit(unit), _channel(channel), _atten(atten) {
	adc_cali_curve_fitting_config_t cali_config = {
		.unit_id = (adc_unit_t  )_unit,
		.chan = (adc_channel_t )_channel,
		.atten = (adc_atten_t ) _atten,
		.bitwidth = ADC_BITWIDTH_12,
	};
	ESP_ERROR_CHECK(adc_cali_create_scheme_curve_fitting(&cali_config, (adc_cali_handle_t*)&_cali_handle));
}

int ADC::rawToVoltage(int raw) {
	if (raw < 0) {
		return raw;
    }
	int voltage = 0;
	adc_cali_raw_to_voltage((adc_cali_handle_t )_cali_handle, raw, &voltage);
	return voltage;
}

ADCOneshot::ADCOneshot(uint8_t channel, uint8_t unit, Atten atten)
: ADC(channel, unit, atten) {
	if (!_adc_handle[_unit]) {
		adc_oneshot_unit_init_cfg_t init_config = {
			.unit_id = (adc_unit_t )_unit,
		};
		ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config, (adc_oneshot_unit_handle_t*)&_adc_handle[_unit]));
	}
	adc_oneshot_chan_cfg_t config = {
		.atten = (adc_atten_t )_atten,
		.bitwidth = ADC_BITWIDTH_12,
	};
	ESP_ERROR_CHECK(adc_oneshot_config_channel((adc_oneshot_unit_handle_t)_adc_handle[_unit], (adc_channel_t )_channel, &config));
}

int ADCOneshot::getRaw() {
	int adc_raw = -1;
	int res = adc_oneshot_read((adc_oneshot_unit_handle_t)_adc_handle[_unit], (adc_channel_t )_channel, &adc_raw);
	if (res != ESP_OK) {
		ESP_LOGE(TAG, "adc channel [%d] oneshot err %04X", _channel, res);
    }
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
	return 0;
}

void ADCContinuous::start() {
	ESP_ERROR_CHECK(adc_continuous_start((adc_continuous_handle_t)_adc_handle[_unit]));
}

void ADCContinuous::stop() {
	ESP_ERROR_CHECK(adc_continuous_stop((adc_continuous_handle_t)_adc_handle[_unit]));
}

void ADCContinuous::setEvtCallback(Callback cb) {
	adc_continuous_evt_cbs_t cbs = {
        .on_conv_done = +[](adc_continuous_handle_t handle, const adc_continuous_evt_data_t *edata, void *user_data) -> bool IRAM_ATTR {
			return ((Callback)user_data)();
		},
    };
	ESP_ERROR_CHECK(adc_continuous_register_event_callbacks((adc_continuous_handle_t)_adc_handle[_unit], &cbs, (void*)cb));
}

int ADCContinuous::getRaw(DigiOutputData buff[], uint32_t max_size) {
	uint32_t ret_num = 0;
	int res = adc_continuous_read((adc_continuous_handle_t)_adc_handle[_unit], (uint8_t*)buff, max_size * SOC_ADC_DIGI_RESULT_BYTES, &ret_num, 0);
	if (res != ESP_OK) {
		ESP_LOGE(TAG, "adc channel [%d] continuous err %04X", _channel, res);
    }
	return ret_num;
}

int ADCContinuous::getVoltage(DigiOutputData buf) {
    return ADC::rawToVoltage(buf.type2.data);
}

}
