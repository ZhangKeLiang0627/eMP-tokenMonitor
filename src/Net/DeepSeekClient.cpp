#include "DeepSeekClient.h"
#include <nlohmann/json.hpp>
#include "../utils/log/log.h"

using namespace Net;
using nlohmann::json;

/* DeepSeek 余额接口常量 */
static const char *DEEPSEEK_HOST = "https://api.deepseek.com";
static const char *DEEPSEEK_BALANCE_PATH = "/user/balance";
static const int CONNECT_TIMEOUT_SEC = 5;
static const int READ_TIMEOUT_SEC = 10;

DeepSeekClient::DeepSeekClient()
{
}

DeepSeekClient::~DeepSeekClient()
{
}

void DeepSeekClient::setApiKey(const std::string &key)
{
    std::lock_guard<std::mutex> lock(_mutex);
    _apiKey = key;
}

/**
 * @brief 创建一个配置好超时的 httplib 客户端
 */
httplib::Client *DeepSeekClient::createClient()
{
    auto *cli = new httplib::Client(DEEPSEEK_HOST);

    // 证书校验默认开启，避免中间人攻击
    cli->enable_server_certificate_verification(true);
    cli->set_connection_timeout(CONNECT_TIMEOUT_SEC, 0);
    cli->set_read_timeout(READ_TIMEOUT_SEC, 0);
    cli->set_write_timeout(READ_TIMEOUT_SEC, 0);
    cli->set_follow_location(true);

    return cli;
}

/**
 * @brief 轻量网络连通性探测
 * @note  只关心能否建立连接并收到响应，不关心业务状态码；
 *        只要能收到 HTTP 响应（哪怕 401），即认为网络是通的。
 */
bool DeepSeekClient::checkNetwork()
{
    std::string key;
    {
        std::lock_guard<std::mutex> lock(_mutex);
        key = _apiKey;
    }

    std::unique_ptr<httplib::Client> cli(createClient());
    cli->set_connection_timeout(3, 0);
    cli->set_read_timeout(3, 0);

    httplib::Headers headers = {
        {"Accept", "application/json"},
    };
    if (!key.empty())
    {
        headers.emplace("Authorization", "Bearer " + key);
    }

    auto res = cli->Get(DEEPSEEK_BALANCE_PATH, headers);

    if (res == nullptr)
    {
        log_debug("[Net] checkNetwork: connect failed, error=%d", (int)res.error());
        return false;
    }

    // 收到任何响应即代表网络可达
    return true;
}

/**
 * @brief 同步拉取余额
 */
FetchResult DeepSeekClient::fetchBalance()
{
    FetchResult result;

    std::string key;
    {
        std::lock_guard<std::mutex> lock(_mutex);
        key = _apiKey;
    }

    if (key.empty())
    {
        result.status = FetchStatus::NO_KEY;
        result.errMsg = "API Key is empty";
        log_warn("[Net] fetchBalance: no api key configured");
        return result;
    }

    std::unique_ptr<httplib::Client> cli(createClient());

    httplib::Headers headers = {
        {"Accept", "application/json"},
        {"Authorization", "Bearer " + key},
    };

    auto res = cli->Get(DEEPSEEK_BALANCE_PATH, headers);

    // 连接失败 / 超时：网络层错误
    if (res == nullptr)
    {
        result.status = FetchStatus::NETWORK_ERROR;
        result.errMsg = httplib::to_string(res.error());
        log_warn("[Net] fetchBalance: network error -> %s", result.errMsg.c_str());
        return result;
    }

    result.httpCode = res->status;

    if (res->status == 200)
    {
        // 解析 JSON（nlohmann/json）
        try
        {
            json j = json::parse(res->body);

            result.balance.is_available = j.value("is_available", false);

            if (j.contains("balance_infos") && j["balance_infos"].is_array())
            {
                for (auto &item : j["balance_infos"])
                {
                    BalanceInfo info;
                    info.currency = item.value("currency", "");
                    info.total_balance = item.value("total_balance", "");
                    info.granted_balance = item.value("granted_balance", "");
                    info.topped_up_balance = item.value("topped_up_balance", "");
                    result.balance.infos.push_back(info);
                }
            }

            result.status = FetchStatus::OK;
            log_info("[Net] fetchBalance: OK, infos=%zu, available=%d",
                     result.balance.infos.size(), result.balance.is_available ? 1 : 0);
        }
        catch (std::exception &e)
        {
            result.status = FetchStatus::PARSE_ERROR;
            result.errMsg = e.what();
            log_error("[Net] fetchBalance: json parse failed -> %s", e.what());
            return result;
        }
    }
    else if (res->status == 401)
    {
        result.status = FetchStatus::AUTH_ERROR;
        result.errMsg = "401 Unauthorized (invalid api key)";
        log_warn("[Net] fetchBalance: 401 unauthorized");
    }
    else
    {
        result.status = FetchStatus::HTTP_ERROR;
        result.errMsg = "HTTP " + std::to_string(res->status);
        log_warn("[Net] fetchBalance: http error %d", res->status);
    }

    return result;
}
