#include "HAL.h"

#include <atomic>

#ifdef __x86_64__
#include "../libs/lv_drivers/sdl/sdl.h"
#include "../libs/lv_drivers/indev/mouse.h"
#endif

/* Signal handler */
void signalExitCallback(int signal);
/* set Signal Callback */
void install_signal_handler(void);
/* LVGL tick get */
uint32_t custom_tick_get(void);

/**
 * @brief 硬件抽象层初始化
 */
void HAL::Init(void)
{
    /* LittlevGL init */
    lv_init();
    lv_png_init();

#ifdef __aarch64__
// 64位ARM架构 / AArch64
#elif defined(__arm__)
    // 32位ARM架构 / AArch32
    // Linux frame buffer device init
    uint32_t rotated = LV_DISP_ROT_NONE;
    sunxifb_init(rotated);

    // A buffer for LittlevGL to draw the screen's content
    static uint32_t width, height;
    sunxifb_get_sizes(&width, &height);

    static lv_color_t *buf;
    buf = (lv_color_t *)sunxifb_alloc(width * height * sizeof(lv_color_t), (char *)"lv_examples");

    if (buf == NULL)
    {
        sunxifb_exit();
        log_error("malloc draw buffer fail\n");
        return;
    }

    // Initialize a descriptor for the buffer
    static lv_disp_draw_buf_t disp_buf;
    lv_disp_draw_buf_init(&disp_buf, buf, NULL, width * height);

    // Initialize and register a display driver
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.draw_buf = &disp_buf;
    disp_drv.flush_cb = sunxifb_flush;
    disp_drv.hor_res = width;
    disp_drv.ver_res = height;
    disp_drv.rotated = rotated;
#ifndef USE_SUNXIFB_G2D_ROTATE
    if (rotated != LV_DISP_ROT_NONE)
        disp_drv.sw_rotate = 1;
#endif
    lv_disp_drv_register(&disp_drv);

    evdev_init();
    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = evdev_read;
    lv_indev_t *evdev_indev = lv_indev_drv_register(&indev_drv);

#elif defined(__x86_64__)
// x86_64架构 / 电脑
#define DISP_BUF_SIZE (480 * 480)
    // sdl_init
    sdl_init();

    /* A small buffer for LittlevGL to draw the screen's content */
    static lv_color_t buf[DISP_BUF_SIZE];

    /* Initialize a descriptor for the buffer */
    static lv_disp_draw_buf_t disp_buf;
    lv_disp_draw_buf_init(&disp_buf, buf, NULL, DISP_BUF_SIZE);

    /* Initialize and register a display driver */
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.draw_buf = &disp_buf;
    disp_drv.flush_cb = sdl_display_flush;
    disp_drv.hor_res = SDL_HOR_RES;
    disp_drv.ver_res = SDL_VER_RES;
    lv_disp_drv_register(&disp_drv);

    mouse_init();
    static lv_indev_drv_t indev_drv_1;
    lv_indev_drv_init(&indev_drv_1); /* Basic initialization */
    indev_drv_1.type = LV_INDEV_TYPE_POINTER;
    indev_drv_1.read_cb = mouse_read;
    lv_indev_t *mouse_indev = lv_indev_drv_register(&indev_drv_1);
#endif

    // 注册退出回调函数
    install_signal_handler();
}

/**
 * @brief 系统退出回调函数
 * @param signal
 */
void signalExitCallback(int signal)
{
    // 信号会递交给进程内任意一个未阻塞该信号的线程，多个线程可能并发进入
    // 本回调。用 atomic_flag 保证退出流程只执行一次，否则并发调用 exit(0)
    // 会导致 glibc exit handlers 双重执行（double-free），表现为随机闪退。
    static std::atomic_flag exitFlag = ATOMIC_FLAG_INIT;
    if (exitFlag.test_and_set())
        return;

    log_warn("[Sys] Got signal %d, exiting ...\n", signal);

#ifdef __aarch64__
// 64位ARM架构 / AArch64
#elif defined(__arm__)
    // 32位ARM架构 / AArch32
    sunxifb_free((void **)&lv_disp_get_default()->driver->draw_buf->buf1, (char *)"lv_examples");
    sunxifb_exit();
#elif defined(__x86_64__)
// x86 / 电脑
#endif

    exit(0);
}

/**
 * @brief 系统退出信号绑定
 */
void install_signal_handler(void)
{
    signal(SIGBUS, signalExitCallback);
    signal(SIGFPE, signalExitCallback);
    signal(SIGHUP, signalExitCallback);
    signal(SIGILL, signalExitCallback);
    signal(SIGINT, signalExitCallback);
    signal(SIGIOT, signalExitCallback);
    signal(SIGPIPE, signalExitCallback);
    signal(SIGQUIT, signalExitCallback);
    signal(SIGSEGV, signalExitCallback);
    signal(SIGSYS, signalExitCallback);
    signal(SIGTERM, signalExitCallback);
    signal(SIGTRAP, signalExitCallback);
    signal(SIGUSR1, signalExitCallback);
    signal(SIGUSR2, signalExitCallback);
}

/* Set in lv_conf.h as `LV_TICK_CUSTOM_SYS_TIME_EXPR` */
uint32_t custom_tick_get(void)
{
    static uint64_t start_ms = 0;
    if (start_ms == 0)
    {
        struct timeval tv_start;
        gettimeofday(&tv_start, NULL);
        start_ms = ((uint64_t)tv_start.tv_sec * 1000000 + (uint64_t)tv_start.tv_usec) / 1000;
    }

    struct timeval tv_now;
    gettimeofday(&tv_now, NULL);
    uint64_t now_ms;
    now_ms = ((uint64_t)tv_now.tv_sec * 1000000 + (uint64_t)tv_now.tv_usec) / 1000;

    uint32_t time_ms = now_ms - start_ms;
    return time_ms;
}
