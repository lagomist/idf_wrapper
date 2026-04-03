#pragma once

#include <cstdint>

namespace Wrapper {

class ADC {
public:
    enum Atten {
        ADC_ATTEN_DB_0   = 0,  ///<No input attenuation, ADC can measure up to approx.
        ADC_ATTEN_DB_2_5 = 1,  ///<The input voltage of ADC will be attenuated extending the range of measurement by about 2.5 dB
        ADC_ATTEN_DB_6   = 2,  ///<The input voltage of ADC will be attenuated extending the range of measurement by about 6 dB
        ADC_ATTEN_DB_12  = 3,  ///<The input voltage of ADC will be attenuated extending the range of measurement by about 12 dB
    };
	ADC(uint8_t channel, uint8_t unit = 0, Atten atten = ADC_ATTEN_DB_12);
	// mv
	int rawToVoltage(int raw);
protected:
	uint8_t _unit;
	uint8_t _channel;
	Atten _atten = ADC_ATTEN_DB_12;
private:
	void* _cali_handle = nullptr;
};

class ADCOneshot : public ADC {
public:
	ADCOneshot(uint8_t channel, uint8_t unit = 0, Atten atten = ADC_ATTEN_DB_12);
	int getRaw();
	int getVoltage() { return rawToVoltage(getRaw()); }
private:
	static void* _adc_handle[2];
};

class ADCContinuous : public ADC {
public:
    struct DigiOutputData {
        union {
            struct {
                uint32_t data:          12; /*!<ADC real output data info. Resolution: 12 bit. */
                uint32_t reserved12:    1;  /*!<Reserved12. */
                uint32_t channel:       4;  /*!<ADC channel index info.
                                                If (channel < ADC_CHANNEL_MAX), The data is valid.
                                                If (channel > ADC_CHANNEL_MAX), The data is invalid. */
                uint32_t unit:          1;  /*!<ADC unit index info. 0: ADC1; 1: ADC2.  */
                uint32_t reserved17_31: 14; /*!<Reserved17. */
            } type2;                        /*!<When the configured output format is 12bit. */
            uint32_t val;                   /*!<Raw data value */
        };
    };
	using ADC::ADC;
    using Callback = bool (*)(void);
	int init(uint32_t buffer_size, uint32_t conv_frame_size, uint32_t sample_freq_hz);
	void start();
	void stop();
	void setEvtCallback(Callback cb);
	int getRaw(DigiOutputData buff[], uint32_t max_size);
	int getVoltage(DigiOutputData buf);
private:
	static void* _adc_handle[2];
};

}
