#include "ble_secure.h"
#include "os_api.h"
#include "dev_desc.h"
#include "utility.h"
#include <esp_random.h>
#include <mbedtls/md5.h>
#include <mbedtls/aes.h>
#include <string.h>

namespace ble_secure {

using Rand = std::array<uint8_t, 16>;
using Key = std::array<uint8_t, 16>;

static Rand _rand = {};
static Key _key = {};

static Key gen_aes_key(const Rand rand) {
	Rand random = {};
	Key key = {};
	std::string splice = {};
	splice += {(char*)rand.data(), rand.size()};
	mbedtls_md5((uint8_t*)dev_desc::get_sn().data(), dev_desc::get_sn().size(), random.data());
	splice += {(char*)random.data(), random.size()};
	mbedtls_md5((const uint8_t*)splice.data(), splice.size(), key.data());
	// ESP_LOG_BUFFER_HEX("aes key", s.data(), s.size());
	return key;
}

static IBuf get_aes_key(bool secure) {
	return secure ? IBuf{_key.data(), _key.size()} : IBuf{_rand.data(), _rand.size()};
}

OBuf generate_random() {
	// 生成16字节随机数
	esp_fill_random(_rand.data(), _rand.size());
	_key = gen_aes_key(_rand);
	return {_rand.data(), _rand.size()};
}

OBuf aes_ecb_encrypt(IBuf input, bool secure) {
	auto key = get_aes_key(secure);
	// padding PKCS7
	uint8_t padding_len = 16 - input.size() % 16;
	if (padding_len == 0)
		padding_len = 16;
	//复制一份输入内容，后续在其上追加
	OBuf input_cpy(input); 
	input_cpy.append(padding_len, padding_len);

	// ESP_LOG_BUFFER_HEX("encrypt input", input.data(), input.size());

	OBuf out(input_cpy.size(), '\x00');
	mbedtls_aes_context aes_ctx;
	mbedtls_aes_init(&aes_ctx);
	mbedtls_aes_setkey_enc(&aes_ctx, key.data(), key.size() * 8);
	for (size_t offset = 0; offset < input_cpy.size(); offset += 16)
		mbedtls_aes_crypt_ecb(&aes_ctx, MBEDTLS_AES_ENCRYPT, input_cpy.data() + offset, out.data() + offset);
	mbedtls_aes_free(&aes_ctx);
	// ESP_LOG_BUFFER_HEX("encrypt output", out.data(), out.size());
	return out;
}

OBuf aes_ecb_decrypt(IBuf input, bool secure) {
	auto key = get_aes_key(secure);
	// ESP_LOG_BUFFER_HEX("decrypt input", input.data(), input.size());
	OBuf out(input.size(), '\x00');
	mbedtls_aes_context aes_ctx;
	mbedtls_aes_init(&aes_ctx);
	mbedtls_aes_setkey_enc(&aes_ctx, key.data(), key.size() * 8);
	for (size_t offset = 0; offset < input.size(); offset += 16)
		mbedtls_aes_crypt_ecb(&aes_ctx, MBEDTLS_AES_DECRYPT, input.data() + offset, out.data() + offset);
	mbedtls_aes_free(&aes_ctx);
	// ESP_LOG_BUFFER_HEX("decrypt output", out.data(), out.size());
	return out.substr(0, out.size() - out[out.size() - 1]);
}
	
} // namespace ble_encrypt
