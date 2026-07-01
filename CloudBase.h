#pragma once

// 腾讯云开发 CloudBase REST API 客户端 for CutPet
// 使用官方 REST API 直接操作数据库，不需要云函数

#include <windows.h>
#include <winhttp.h>
#include <string>
#include <functional>

#pragma comment(lib, "winhttp.lib")

// ============================================================================
// 腾讯云开发配置
// ============================================================================

// 环境ID
constexpr const wchar_t* CB_ENV_ID = L"cutpet-d2gio3c7bebc3f81e";

// API 端点
constexpr const wchar_t* CB_API_HOST = L"tcb-api.cloud.tencent.com";

// 数据集合名
constexpr const wchar_t* COLLECTION_USERS = L"CutPetUsers";
constexpr const wchar_t* COLLECTION_LEADERBOARD = L"Leaderboard";

// ============================================================================
// 回调类型
// ============================================================================

using HttpCallback = std::function<void(bool success, const std::wstring& response)>;

// ============================================================================
// 腾讯云开发客户端
// ============================================================================

class CloudBaseClient {
public:
    CloudBaseClient() = default;
    ~CloudBaseClient() = default;

    bool Initialize();
    void Shutdown();

    // 注册/登录
    void RegisterOrLogin(const std::wstring& username, HttpCallback callback);

    // 保存玩家数据
    void SavePlayerData(const std::wstring& openid, int level, int currentXP, int totalXP,
                        int happiness, int toysRemaining, HttpCallback callback);

    // 查询玩家数据
    void QueryPlayerData(const std::wstring& openid, HttpCallback callback);

    // 提交排行榜
    void SubmitScore(const std::wstring& openid, int level, int totalXP, HttpCallback callback);

    // 查询排行榜
    void QueryLeaderboard(HttpCallback callback);

    // 每日签到
    void DailySignin(const std::wstring& openid, HttpCallback callback);

private:
    HINTERNET hSession;
    HINTERNET hConnect;
    std::wstring m_openid;
    std::wstring m_sessionToken;

    void SendRequest(const std::wstring& method, const std::wstring& path,
                     const std::wstring& body, HttpCallback callback);
    std::wstring BuildAuthHeader();
    static std::wstring ParseJSONField(const std::wstring& json, const std::wstring& field);
};
