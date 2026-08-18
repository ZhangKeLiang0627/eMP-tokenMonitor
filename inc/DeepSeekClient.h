#pragma once

#include <string>
#include <vector>
#include <mutex>

#include "../libs/cpp-httplib/httplib.h"

namespace Net
{
    /* 单条余额信息（对应 balance_infos 数组元素） */
    struct BalanceInfo
    {
        std::string currency;          // 币种：CNY / USD
        std::string total_balance;     // 总的可用余额（赠金 + 充值）
        std::string granted_balance;   // 未过期的赠金余额
        std::string topped_up_balance; // 充值余额
    };

    /* 余额查询结果 */
    struct BalanceResult
    {
        bool is_available = false;     // 当前账户是否有余额可供 API 调用
        std::vector<BalanceInfo> infos;
    };

    /* 拉取状态枚举 */
    enum class FetchStatus
    {
        OK = 0,          // 成功
        NO_KEY,          // 未配置 API Key
        NETWORK_ERROR,   // 网络未连接 / 连接失败
        HTTP_ERROR,      // 其它 HTTP 错误（5xx 等）
        AUTH_ERROR,      // 401 鉴权失败（Key 无效）
        PARSE_ERROR,     // JSON 解析失败
    };

    /* 拉取结果 */
    struct FetchResult
    {
        FetchStatus status = FetchStatus::NETWORK_ERROR;
        int httpCode = 0;
        BalanceResult balance;
        std::string errMsg;
    };

    /**
     * @brief DeepSeek 余额查询客户端（基于 cpp-httplib）
     * @note  官方接口：GET https://api.deepseek.com/user/balance
     *        请求头：Accept: application/json
     *               Authorization: Bearer <API_KEY>
     */
    class DeepSeekClient
    {
    public:
        DeepSeekClient();
        ~DeepSeekClient();

        void setApiKey(const std::string &key);

        /**
         * @brief 轻量网络连通性探测
         * @return true-网络可达（能连上 api.deepseek.com），false-不可达
         */
        bool checkNetwork();

        /**
         * @brief 同步拉取余额
         * @return FetchResult 拉取结果
         */
        FetchResult fetchBalance();

    private:
        httplib::Client *createClient();

        std::string _apiKey;
        std::mutex _mutex;
    };
}
