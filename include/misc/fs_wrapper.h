#pragma once

#include <string_view>
#include <vector>

namespace Wrapper {

namespace FileSystem {

/**
 * @brief 挂载文件系统 
 *
 * @note 挂载littlefs文件系统
 *
 * @param 
 * 
 * @return
 *      - 0 on success
 *      - Other failure
 */
int mount();

/**
 * @brief 卸载文件系统 
 *
 * @note 卸载littlefs文件系统
 *
 * @param 
 * 
 * @return
 *      - 0 on success
 *      - Other failure
 */
int unmount();

int format();
int mkdir(std::string_view path);
std::string list(std::string_view path);
std::string cat(std::string_view path);
int remove(std::string_view path);

int get_file_size(std::string_view name);
std::string_view get_base_path();
std::string get_full_name(std::string_view name);

int write(std::string_view name, std::string_view value);

template<typename T>
int write(std::string_view name, std::vector<T> value);
template<typename T>
int read(std::string_view name, std::vector<T>& value);

} // namespace FS

}
