#include "fs_wrapper.h"
#include "esp_vfs.h"
#include "esp_vfs_fat.h"
#include <esp_log.h>
#include <string>
#include <array>
#include <dirent.h>

namespace Wrapper {

namespace FileSystem {

constexpr static const char TAG[] = "Wrapper::FileSystem";

constexpr static char FS_PARTITION[] = "storage";
constexpr static char BASE_PATH[] = "/flash";

// Handle of the wear levelling library instance
static wl_handle_t s_wl_handle = WL_INVALID_HANDLE;

std::string_view get_base_path() {
	return BASE_PATH;
}

int mount() {
    int err = 0;
    // To mount device we need name of device partition, define base_path
    // and allow format partition in case if it is new one and was not formatted before
    const esp_vfs_fat_mount_config_t mount_config = {
        .format_if_mount_failed = true,
        .max_files = 4,
        .allocation_unit_size = CONFIG_WL_SECTOR_SIZE,
        .disk_status_check_enable = false,
        .use_one_fat = false,
    };
    err = esp_vfs_fat_spiflash_mount_rw_wl(BASE_PATH, FS_PARTITION, &mount_config, &s_wl_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount FATFS (%s)", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "Mounting FAT filesystem");
    return err;
}

int unmount() {
    int err = 0;
    err = esp_vfs_fat_spiflash_unmount_rw_wl(BASE_PATH, s_wl_handle);
    ESP_LOGI(TAG, "flash unmounted \n");
    return err;
}

// 格式化文件系统
int format() {
    return esp_vfs_fat_spiflash_format_rw_wl(BASE_PATH, FS_PARTITION);
}

int mkdir(std::string_view path) {
    return ::mkdir(get_full_name(path).c_str(), 0755);
}

// 列出路径下所有文件和文件夹，一般来说文件有后缀，文件夹没有后缀
std::string list(std::string_view path) {
    std::string ret = {};
    DIR *pdir = opendir(path.data());
    struct dirent *pdirent = nullptr;
    if (pdir == nullptr) {
        return {"path error\n"};
    }
    while((pdirent = readdir(pdir)) != nullptr) {
        if (pdirent->d_name[0] == '.') {
            continue;
        }
        ret += pdirent->d_name;
        ret += "\n";
    }
    closedir(pdir);
    return ret;
}


std::string cat(std::string_view path) {
    std::string content = {};
    std::array<char, 512> buf = {};
    FILE *f = fopen(path.data(), "r");
    if (!f) {
        return {"open file %s error", path.data()};
    }
    
    while(!feof(f)) {
        fgets(buf.data(), buf.size(), f);
        content += buf.data();
    }
    fclose(f);
    return content;
}

int remove(std::string_view path) {
    return ::remove(get_full_name(path).c_str());
}


int write(std::string_view name, std::string_view value) {
    FILE* fp = fopen(get_full_name(name).c_str(), "wb");
	if(fp == nullptr)
		return -1;

	int len = fwrite(value.data(), 1, value.size(), fp);
	fclose(fp);
    return len;
}

template<typename T>
int write(std::string_view name, std::vector<T> value) {
    FILE* fp = fopen(get_full_name(name).c_str(), "wb");
	if(fp == nullptr)
		return -1;

	int len = fwrite(value.data(), 1, value.size()*sizeof(T), fp);
	fclose(fp);
    return len;
}

template<typename T>
int read(std::string_view name, std::vector<T>& value) {
    int size = get_file_size(name);
    if (size <= 0 || size % sizeof(T) != 0) {
        return -1;
    }

    FILE* fp = fopen(get_full_name(name).c_str(), "rb");
	if(fp == nullptr)
		return -1;

    value.resize(size / sizeof(T));
	int len = fread(value.data(), 1, size, fp);
	fclose(fp);
    return len;
}


std::string get_full_name(std::string_view name) {
    std::string s = BASE_PATH;
	s += name;
	return s;
}

int get_file_size(std::string_view name) {
    FILE* fp = fopen(get_full_name(name).c_str(), "rb");
	if(fp == nullptr)
		return 0;
	fseek(fp, 0, SEEK_END);
	int size = ftell(fp);
	fseek(fp, 0, SEEK_SET);
    fclose(fp);
	return size;
}


} //namespace FsWrapper

}
