/*
 C++ wrapper of IDF's PWM API
 Copyright (C) 2022 Mark Ma
*/

#pragma once

#include <stdint.h>

//上电默认0占空比, 此类不线程安全
class PWM {
public:
	void set_duty_cycle(float dc);
	float get_duty_cycle();
	void init(uint8_t channel, int pin, uint32_t freq_hz, float dc = 0);
private:
	uint8_t _channel = 0;
	float _duty_cycle = 0;
};
