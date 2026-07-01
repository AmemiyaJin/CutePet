#pragma once

// LeanCloud REST API client for CutPet
// Free tier: 1GB storage, 100K requests/month

#include <windows.h>
#include <winhttp.h>
#include <string>
#include <functional>

#pragma comment(lib, "winhttp.lib")

// ============================================================================
// LeanCloud 配置（你需要去 leancloud.cn 注册后替换）
// ============================================================================

// 去 https://leancloud.cn 注册免费账号
// 创建应用后，在这里填入你的 App ID 和 App Key
constexpr const wchar_t* LEAN_CLOUD_APP_ID = L"你的App ID";
constexpr const wchar_t* LEAN_CLOUD_APP_KEY = L"你的App Key";
constexpr const wchar_t* LEAN_CLOUD_API_HOST = L"api.leancloud.cn";

// 数据表名
constexpr const wchar_t* TABLE_USERS = L"CutPetUsers";
constexpr const wchar_t* TABLE_LEADERBOARD = L"Leaderboard";

// ============================================================================
// 回调类型
// ============================================================================

using HttpCallback = std::function<void(bool success, const std::wstring& response)>;

// ============================================================================
// LeanCloud REST API 客户端
// ============================================================================

class LeanCloudClient {
public:
    LeanCloudClient() = default;
    ~LeanCloudClient() = default;

    // 禁止拷贝
    LeanCloudClient(const LeanCloudClient&) = delete;
    LeanCloudClient& operator=(const LeanCloudClient&) = delete;

    // 初始化
    bool Initialize();

    // 注销（释放资源）
    void Shutdown();

    // 用户注册
    void SignUp(const std::wstring& username, const std::wstring& password, HttpCallback callback);

    // 用户登录
    void Login(const std::wstring& username, const std::wstring& password, HttpCallback callback);

    // 保存玩家数据（等级、经验等）
    void SavePlayerData(const std::wstring& objectId, int level, int currentXP, int totalXP,
                        int happiness, int toysRemaining, HttpCallback callback);

    // 查询玩家数据
    void QueryPlayerData(const std::wstring& objectId, HttpCallback callback);

    // 提交排行榜
    void SubmitScore(const std::wstring& objectId, int level, int totalXP, HttpCallback callback);

    // 查询排行榜（前10名）
    void QueryLeaderboard(HttpCallback callback);

    // 每日签到
    void DailySignin(const std::wstring& objectId, HttpCallback callback);

private:
    HINTERNET hSession;
    HINTERNET hConnect;

    // 发送 HTTP 请求（内部方法）
    void SendRequest(const std::wstring& method, const std::wstring& path,
                     const std::wstring& body, HttpCallback callback);

    // 构建认证头
    std::wstring BuildAuthHeader();

    // 解析 JSON 响应（简化版）
    static std::wstring ParseJSONField(const std::wstring& json, const std::wstring& field);

    // 获取当前 Object ID（登录后保存）
    std::wstring m_objectId;
    std::wstring m_sessionToken;
};
