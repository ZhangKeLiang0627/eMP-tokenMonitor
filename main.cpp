#include "../inc/common_inc.h"
#include "Model.h"
#include "TimeSync.h"

static Page::Model *model;

static void exitCallback(void);

int main(int argc, char *argv[])
{
    // log init
    logger_init("/mnt/UDISK/logs/", "eMP_tokenMonitor.log", false, 1024 * 1024 * 1, 10);
    log_info("[Sys] eMP_tokenMonitor begin!");

#ifdef __arm__
    // T113-S3 无 RTC，默认时钟为 1970，会令 HTTPS 证书校验失败。
    // 在启动 LVGL（tick 基于系统时钟）之前先对时，避免时钟跳变影响 LVGL。
    Net::syncSystemTime();
#endif

    // Init HAL
    HAL::Init();

    // model 初始化
    model = new Page::Model(exitCallback);

    while (1)
    {
        usleep(10 * 1000 * 1000);
    }

    return 0;
}

/**
 * @brief 退出回调函数
 */
static void exitCallback(void)
{
    delete model;
    exit(0);
}
