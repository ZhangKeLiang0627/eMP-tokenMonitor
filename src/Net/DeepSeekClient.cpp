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
    std::lock_guard<std::mutex> lock(_mutex);
    delete _client;
    _client = nullptr;
}

void DeepSeekClient::setApiKey(const std::string &key)
{
    std::lock_guard<std::mutex> lock(_mutex);
    _apiKey = key;
}

/**
 * @brief 获取（必要时创建）复用的 httplib 客户端
 * @note  客户端单实例复用：SSL_CTX 只创建一次、keep-alive 连接可复用，
 *        避免频繁创建/销毁 SSL 上下文在旧版 OpenSSL（如 1.1.0i）下
 *        每次连接泄漏内存，长期运行触发 OOM 被内核杀死的问题。
 */
httplib::Client *DeepSeekClient::getClient()
{
    std::lock_guard<std::mutex> lock(_mutex);
    if (_client == nullptr)
    {
        _client = new httplib::Client(DEEPSEEK_HOST);

        // 证书校验默认开启，避免中间人攻击
        _client->enable_server_certificate_verification(true);
        _client->set_connection_timeout(CONNECT_TIMEOUT_SEC, 0);
        _client->set_read_timeout(READ_TIMEOUT_SEC, 0);
        _client->set_write_timeout(READ_TIMEOUT_SEC, 0);
        _client->set_follow_location(true);
        // 关键：httplib 客户端默认 keep_alive_=false 且请求头带 "Connection: close"，
        // 每次请求都会新建 TLS 连接（板端 OpenSSL 1.1.0i 每次建连泄漏内存）。
        // 显式开启 keep-alive，配合 httplib.h 中 is_ssl_peer_could_be_closed 的
        // 兼容补丁，让连接真正复用。
        _client->set_keep_alive(true);
    }
    return _client;
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

    httplib::Client *cli = getClient();
    // 复用客户端：超时统一用创建时的配置（5s 连接 / 10s 读写），不再单独覆盖

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

    httplib::Client *cli = getClient();

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
