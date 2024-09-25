#pragma once
#include "bufdef.h"
#include <array>

namespace ble_secure {

OBuf generate_random();
OBuf aes_ecb_encrypt(IBuf input, bool secure);
OBuf aes_ecb_decrypt(IBuf input, bool secure);

	
} // namespace ble_encrypt 
