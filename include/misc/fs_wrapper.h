#pragma once

#include <string_view>
#include <vector>

namespace Wrapper {

namespace FileSystem {

/**
 * @brief File system wrapper 文件操作需用绝对路径
 * 
 */

int mkdir(std::string_view path);
std::string list(std::string_view path);
std::string cat(std::string_view path);
int remove(std::string_view path);

int get_file_size(std::string_view name);
std::string get_full_name(std::string_view name);

int write(std::string_view name, std::string_view value);

template<typename T>
int write(std::string_view name, std::vector<T> value);
template<typename T>
int read(std::string_view name, std::vector<T>& value);

int read_at(std::string_view name, uint8_t* buf, size_t offset, size_t len);

namespace Flash {

std::string get_base_path();
int mount();
int unmount();
int format();

} /* namespace Flash */

namespace SdCard { 

bool is_mounted();

std::string get_base_path();
// SPI
int mount(uint8_t spi_port, int spi_cs);

// sdmmc 1-wire
int mount(int clk, int cmd, int d0);

int unmount();
int format();

} /* namespace SdCard */


} // namespace FS

} /* namespace Wrapper */
