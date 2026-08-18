#include "Model.h"

#include <fstream>
#include <ctime>
#include <chrono>

#include <nlohmann/json.hpp>
#include "../utils/log/log.h"
#include "ResourcePool.h"
#include "TimeSync.h"

using nlohmann::json;

#define CONFIG_FILE "./config/eMP_tokenMonitor.json"
#define NETWORK_PROBE_INTERVAL_SEC 3   // 轻量网络探测周期
#define MIN_REFRESH_INTERVAL_SEC 5     // 最小拉取周期
#define TIME_SYNC_INTERVAL_SEC 60      // 网络对时周期（ARM 无 RTC 板子）

using namespace Page;

/**
 * @brief Model 构造函数
 * @param exitCb 退出回调
 */
Model::Model(std::function<void(void)> exitCb)
{
    _threadExitFlag = false;
    _refreshRequested = false;

    // 设置 UI 回调函数
    Operations uiOpts = {0};
    uiOpts.exitCb = exitCb;
    uiOpts.refreshCb = std::bind(&Model::requestRefresh, this);

    /* 初始化资源池 */
    ResourcePool::Init();

    /* 创建 UI */
    _view.create(uiOpts);

    // 这里设置一个 1000ms 的软定时器，用于在 onTimerUpdate 里 update
    _timer = lv_timer_create(onTimerUpdate, 1000, this);
    update();

    // 创建 lvgl 处理线程，传递 this 指针
    _threadLvgl = std::thread([](Model *pThis)
                              { pThis->threadLvglHandler(); }, this);
    _threadLvgl.detach();

    // 创建 data 处理（网络）线程，传递 this 指针
    _threadDataProc = std::thread([](Model *pThis)
                                  { pThis->threadDataProcHandler(); }, this);

    // 若设置了 EMP_AUTOSHOT 环境变量，则延时自动截图（用于无显示环境验证 / 生成文档图）
    const char *autoShot = getenv("EMP_AUTOSHOT");
    if (autoShot != nullptr)
    {
        int sec = atoi(autoShot);
        if (sec <= 0)
            sec = 3;
        lv_timer_t *shotTimer = lv_timer_create([](lv_timer_t *timer)
                                                 {
            Model *pThis = (Model *)timer->user_data;
            pThis->_view.screenshot(); },
                                                 sec * 1000, this);
        lv_timer_set_repeat_count(shotTimer, 1);
        log_info("[Model] auto screenshot scheduled in %ds", sec);
    }
}

Model::~Model()
{
    _threadExitFlag = true;

    // 等待线程退出，回收资源
    if (_threadDataProc.joinable())
    {
        log_info("[Model] joining _threadDataProc...");
        _threadDataProc.join();
        log_info("[Model] _threadDataProc joined");
    }

    lv_timer_del(_timer);
    _view.release();

    log_info("[Model] ~Model exit!");
}

/**
 * @brief 定时器更新函数（运行在 LVGL 线程）
 */
void Model::onTimerUpdate(lv_timer_t *timer)
{
    Model *instance = (Model *)timer->user_data;
    instance->update();
}

/**
 * @brief 更新 UI 等事务（运行在 LVGL 线程，此时 _mutex 已由 lv_task_handler 持有）
 */
void Model::update(void)
{
    bool connected = _state.networkConnected;
    Net::FetchStatus status = _state.fetchStatus;
    std::string lastUpdate = _state.lastUpdateTime;
    std::string msg = _state.statusMsg;

    _view.setNetwork(connected);
    _view.setLastUpdate(lastUpdate.c_str());
    _view.setStatusMessage(msg.c_str());

    // 仅在成功时刷新余额数字，其它情况保留上一次的显示
    if (status == Net::FetchStatus::OK && !_state.balance.infos.empty())
    {
        const Net::BalanceInfo &info = _state.balance.infos[0];
        _view.setBalanceText(info.total_balance.c_str());
        _view.setCurrencyText(info.currency.c_str());
        _view.setGrantedText(info.granted_balance.c_str());
        _view.setToppedText(info.topped_up_balance.c_str());
        _view.setAvailable(_state.balance.is_available);
    }
}

/**
 * @brief LVGL 处理线程
 */
void Model::threadLvglHandler(void)
{
    while (!_threadExitFlag)
    {
        std::unique_lock<std::mutex> lock(_mutex);
        uint32_t ms = lv_task_handler();
        lock.unlock();

        std::this_thread::sleep_for(std::chrono::milliseconds(ms));
    }
    log_info("[Model] threadLvglHandler exit!");
}

/**
 * @brief data 处理线程（网络探测 + 余额拉取）
 */
void Model::threadDataProcHandler(void)
{
    // 读取配置文件
    if (readConfig() != true)
    {
        log_warn("[Model] readConfig failed, use default config");
    }

    _client.setApiKey(_config.apiKey);

#ifdef __arm__
    // 无 RTC 板子：启动即对时一次，之后每 60s 周期对时（校正漂移 / 网络恢复后补对时）
    uint32_t timeSyncCountdown = TIME_SYNC_INTERVAL_SEC;
    Net::syncSystemTime();
#endif

    // 启动后立即拉取一次
    fetchBalance();

    uint32_t countdown = (uint32_t)_config.refreshIntervalSec;

    while (!_threadExitFlag)
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        if (_threadExitFlag)
            break;

#ifdef __arm__
        // 周期网络对时（60s）
        if (--timeSyncCountdown == 0)
        {
            timeSyncCountdown = TIME_SYNC_INTERVAL_SEC;
            Net::syncSystemTime();
        }
#endif

        // 手动刷新请求优先处理
        if (_refreshRequested)
        {
            _refreshRequested = false;
            log_info("[Model] manual refresh triggered");
            fetchBalance();
            countdown = (uint32_t)_config.refreshIntervalSec;
            continue;
        }

        // 到达周期 → 拉取余额
        if (--countdown == 0)
        {
            countdown = (uint32_t)_config.refreshIntervalSec;
            fetchBalance();
        }
        // 其余时刻做轻量网络探测，快速刷新网络状态
        else if ((countdown % NETWORK_PROBE_INTERVAL_SEC) == 0)
        {
            checkNetwork();
        }
    }

    log_info("[Model] threadDataProcHandler exit!");
}

/**
 * @brief 手动刷新（由 UI 刷新按钮触发）
 */
void Model::requestRefresh(void)
{
    _refreshRequested = true;
}

/**
 * @brief 拉取余额并更新共享状态
 */
void Model::fetchBalance(void)
{
    Net::FetchResult r = _client.fetchBalance();

    std::lock_guard<std::mutex> lock(_mutex);

    _state.fetchStatus = r.status;
    _state.httpCode = r.httpCode;
    _state.balance = r.balance;
    _state.networkConnected = (r.status != Net::FetchStatus::NETWORK_ERROR);
    _state.lastUpdateTime = currentTimeStr();

    switch (r.status)
    {
    case Net::FetchStatus::OK:
        _state.statusMsg = "OK";
        break;
    case Net::FetchStatus::NO_KEY:
        _state.statusMsg = "No API Key";
        break;
    case Net::FetchStatus::NETWORK_ERROR:
        _state.statusMsg = r.errMsg.empty() ? "Network Error" : r.errMsg;
        break;
    case Net::FetchStatus::HTTP_ERROR:
        _state.statusMsg = "HTTP " + std::to_string(r.httpCode);
        break;
    case Net::FetchStatus::AUTH_ERROR:
        _state.statusMsg = "401 Unauthorized";
        break;
    case Net::FetchStatus::PARSE_ERROR:
        _state.statusMsg = "Parse Error";
        break;
    default:
        _state.statusMsg = "Unknown";
        break;
    }
}

/**
 * @brief 轻量网络探测
 */
void Model::checkNetwork(void)
{
    bool ok = _client.checkNetwork();

    std::lock_guard<std::mutex> lock(_mutex);
    _state.networkConnected = ok;
}

/**
 * @brief 读取配置信息
 * @return true-读取成功  false-读取失败（使用缺省值）
 */
bool Model::readConfig(void)
{
    // 缺省值
    _config.apiKey = "";
    _config.refreshIntervalSec = 30;

    std::ifstream file(CONFIG_FILE);
    if (file.is_open() != true)
    {
        log_warn("[Model] open \"%s\" failed! Use default config.", CONFIG_FILE);
        return false;
    }

    try
    {
        json j;
        file >> j;

        if (j.contains("api_key") && j["api_key"].is_string())
            _config.apiKey = j["api_key"].get<std::string>();

        if (j.contains("refresh_interval_sec") && j["refresh_interval_sec"].is_number())
        {
            int v = j["refresh_interval_sec"].get<int>();
            if (v < MIN_REFRESH_INTERVAL_SEC)
                v = MIN_REFRESH_INTERVAL_SEC;
            _config.refreshIntervalSec = v;
        }
    }
    catch (std::exception &e)
    {
        log_warn("[Model] parse \"%s\" failed: %s", CONFIG_FILE, e.what());
        return false;
    }

    log_info("[Model] config loaded: refresh_interval=%ds, key_len=%zu",
             _config.refreshIntervalSec, _config.apiKey.length());

    return true;
}

/**
 * @brief 获取当前时间字符串 "HH:MM:SS"
 */
std::string Model::currentTimeStr(void)
{
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm local{};
    localtime_r(&t, &local);

    char buf[16];
    snprintf(buf, sizeof(buf), "%02d:%02d:%02d", local.tm_hour, local.tm_min, local.tm_sec);
    return std::string(buf);
}
