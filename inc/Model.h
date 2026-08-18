#pragma once

#include <string>
#include <thread>
#include <mutex>
#include <atomic>
#include <functional>

#include "View.h"
#include "common_inc.h"
#include "DeepSeekClient.h"

namespace Page
{
    class Model
    {
    public:
        struct Config
        {
            std::string apiKey;      // DeepSeek API Key
            int refreshIntervalSec;  // 余额拉取周期（秒）
        };

    private:
        /* 共享状态（由数据线程写入，LVGL 线程读取） */
        struct State
        {
            bool networkConnected = false;
            Net::FetchStatus fetchStatus = Net::FetchStatus::NETWORK_ERROR;
            int httpCode = 0;
            Net::BalanceResult balance;
            std::string lastUpdateTime; // "HH:MM:SS"
            std::string statusMsg;      // 底部状态消息
        };

        std::mutex _mutex;                        // 互斥量（保护 _state 与 LVGL 对象）
        std::thread _threadLvgl;                  // lvgl 线程
        std::thread _threadDataProc;              // 数据处理（网络）线程
        std::atomic<bool> _threadExitFlag{false}; // 线程退出标志位
        std::atomic<bool> _refreshRequested{false}; // 手动刷新请求标志

        Config _config;
        State _state;
        Net::DeepSeekClient _client;

        View _view;         // View 实例
        lv_timer_t *_timer; // UI 软定时器

    private:
        void threadLvglHandler(void);
        void threadDataProcHandler(void);

        void update(void);
        static void onTimerUpdate(lv_timer_t *timer);

        bool readConfig(void);
        void requestRefresh(void);
        void fetchBalance(void);
        void checkNetwork(void);

        static std::string currentTimeStr(void);

    public:
        Model(std::function<void(void)> exitCb);
        ~Model();
    };
}
