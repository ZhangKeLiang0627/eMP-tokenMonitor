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
 * @brief 退出回调函数（由 UI 退出按钮触发，运行在 LVGL 线程）
 * @note  不在回调里 delete model：Model 析构会 join 数据线程（可能阻塞在网络
 *        调用上），且此时 detached 的 LVGL 线程仍在执行 lv_task_handler，
 *        并发析构会触发 use-after-free / double-free，表现为随机闪退。
 *        统一由 exit(0) 结束进程，资源交由内核回收。
 */
static void exitCallback(void)
{
    log_info("[Sys] exit requested, exiting ...");
    exit(0);
}
