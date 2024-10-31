#pragma once

#include <string>

namespace Wrapper {

namespace OTA {

enum class Status : uint8_t {
	IDLE,
    BEGIN,
	PROCESSING,
	SAMENESS,
	SUCCESS,
	FAILURE
};


void start(std::string_view url);
void abort();

Status status();
int getSize();
uint8_t getPercentage();

}

}
