#ifndef HTTP_OTA_H
#define HTTP_OTA_H

/**
 * 
 * require components: esp_https_ota app_update
 * 
 * enabled option Allow HTTP for OTA in menuconfig
 * 
 */


typedef enum {
    OTA_FAIL = -1,  // OTA upgrade failed
    OTA_FINISH,     // OTA upgrade finished
    OTA_SAME,       // app version as same as new
} http_ota_wrapper_ret_t;

typedef enum {
    OTA_AUTOMATIC = 1,  // automatic OTA upgrade
    OTA_MANUAL,         // manual OTA upgrade
} http_ota_wrapper_mode_t;


typedef struct {
    char url[256];                  // HTTP固件URL
    http_ota_wrapper_mode_t mode;   // 服务工作模式
    int interval;                   // 自动模式下OTA更新间隔(ms)
} http_ota_wrapper_cfg_t;


/*--------------function declarations-----------*/

/**
 * @brief 等待OTA结束，返回状态
 * 
 * @return http_ota_wrapper_ret_t 
 */
http_ota_wrapper_ret_t http_ota_wrapper_wait_result();

/**
 * @brief 配置OTA服务，确定URL
 * 
 * @param config 
 */
void http_ota_wrapper_service_config(http_ota_wrapper_cfg_t *config);

/**
 * @brief 启动HTTP OTA进程
 * 
 * @return int 
 */
int http_ota_wrapper_process();

/**
 * @brief OTA初始化，创建任务
 * 
 */
void http_ota_wrapper_service_start();

#endif