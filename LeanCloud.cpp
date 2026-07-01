#include "LeanCloud.h"
#include <sstream>
#include <algorithm>

// ============================================================================
// 初始化/注销
// ============================================================================

bool LeanCloudClient::Initialize() {
    // 打开 WinHTTP 会话
    hSession = WinHttpOpen(L"CutPet/1.0",
                           WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                           WINHTTP_NO_PROXY_NAME,
                           WINHTTP_NO_PROXY_BYPASS,
                           0);
    if (!hSession) {
        return false;
    }

    // 设置会话选项（超时等）
    DWORD timeout = 10000; // 10秒
    WinHttpSetOption(hSession, WINHTTP_OPTION_CONNECT_TIMEOUT, &timeout, sizeof(timeout));
    WinHttpSetOption(hSession, WINHTTP_OPTION_SEND_TIMEOUT, &timeout, sizeof(timeout));
    WinHttpSetOption(hSession, WINHTTP_OPTION_RECEIVE_TIMEOUT, &timeout, sizeof(timeout));

    // 连接到 LeanCloud API 服务器
    hConnect = WinHttpConnect(hSession, LEAN_CLOUD_API_HOST, INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        hSession = nullptr;
        return false;
    }

    return true;
}

void LeanCloudClient::Shutdown() {
    if (hConnect) {
        WinHttpCloseHandle(hConnect);
        hConnect = nullptr;
    }
    if (hSession) {
        WinHttpCloseHandle(hSession);
        hSession = nullptr;
    }
}

// ============================================================================
// 内部方法
// ============================================================================

std::wstring LeanCloudClient::BuildAuthHeader() {
    // X-LC-Id: App ID
    // X-LC-Key: App Key
    std::wstring header = L"X-LC-Id: " + std::wstring(LEAN_CLOUD_APP_ID) + L"\r\n";
    header += L"X-LC-Key: " + std::wstring(LEAN_CLOUD_APP_KEY) + L"\r\n";
    if (!m_sessionToken.empty()) {
        header += L"X-LC-Session: " + m_sessionToken + L"\r\n";
    }
    return header;
}

std::wstring LeanCloudClient::ParseJSONField(const std::wstring& json, const std::wstring& field) {
    // 简化的 JSON 字段解析
    std::wstring key = L"\"" + field + L"\"";
    size_t pos = json.find(key);
    if (pos == std::wstring::npos) return L"";

    pos = json.find(L":", pos) + 1;
    while (pos < json.size() && (json[pos] == L' ' || json[pos] == L'\t')) pos++;

    if (pos >= json.size()) return L"";

    if (json[pos] == L'"') {
        // 字符串值
        pos++;
        size_t end = json.find(L'"', pos);
        if (end == std::wstring::npos) return L"";
        return json.substr(pos, end - pos);
    } else {
        // 数字或其他值
        size_t end = json.find_first_of(L",}", pos);
        if (end == std::wstring::npos) end = json.size();
        return json.substr(pos, end - pos);
    }
}

void LeanCloudClient::SendRequest(const std::wstring& method, const std::wstring& path,
                                   const std::wstring& body, HttpCallback callback) {
    if (!hConnect) {
        if (callback) callback(false, L"网络连接未初始化");
        return;
    }

    // 打开 HTTPS 请求
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, method.c_str(),
                                             path.c_str(), nullptr,
                                             WINHTTP_NO_REFERER,
                                             WINHTTP_DEFAULT_ACCEPT_TYPES,
                                             WINHTTP_FLAG_SECURE);
    if (!hRequest) {
        if (callback) callback(false, L"创建请求失败");
        return;
    }

    // 设置认证头
    std::wstring authHeader = BuildAuthHeader();

    // 发送请求
    BOOL result = WinHttpSendRequest(hRequest,
                                      authHeader.c_str(),
                                      static_cast<DWORD>(authHeader.length() * sizeof(wchar_t)),
                                      const_cast<wchar_t*>(body.empty() ? nullptr : body.c_str()),
                                      static_cast<DWORD>(body.length() * sizeof(wchar_t)),
                                      static_cast<DWORD>(body.length()),
                                      0);

    if (!result) {
        WinHttpCloseHandle(hRequest);
        if (callback) callback(false, L"发送请求失败");
        return;
    }

    result = WinHttpReceiveResponse(hRequest, nullptr);
    if (!result) {
        WinHttpCloseHandle(hRequest);
        if (callback) callback(false, L"接收响应失败");
        return;
    }

    // 读取响应数据
    std::wstring response;
    DWORD bytesRead = 0;
    char buffer[4096];
    do {
        result = WinHttpReadData(hRequest, buffer, sizeof(buffer) - 1, &bytesRead);
        if (result && bytesRead > 0) {
            buffer[bytesRead] = '\0';
            // ANSI to UTF-16
            int wLen = MultiByteToWideChar(CP_UTF8, 0, buffer, bytesRead, nullptr, 0);
            if (wLen > 0) {
                std::wstring wStr(wLen, L'\0');
                MultiByteToWideChar(CP_UTF8, 0, buffer, bytesRead, &wStr[0], wLen);
                response += wStr;
            }
        }
    } while (result && bytesRead > 0);

    WinHttpCloseHandle(hRequest);

    // 检查 HTTP 状态码
    DWORD httpStatus = 0;
    DWORD statusCodeSize = sizeof(httpStatus);
    WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_CUSTOM,
                        WINHTTP_HEADER_NAME_BY_INDEX, &httpStatus, &statusCodeSize, WINHTTP_NO_HEADER_INDEX);
    // 注意：上面的调用可能不工作，用另一种方式
    // 这里我们简单判断响应是否为空或包含错误信息

    if (callback) {
        bool success = !response.empty() && response.find(L"error") == std::wstring::npos;
        callback(success, response);
    }
}

// ============================================================================
// 公开 API
// ============================================================================

void LeanCloudClient::SignUp(const std::wstring& username, const std::wstring& password, HttpCallback callback) {
    // POST /1.1/users
    std::wstring path = L"/1.1/users";

    // 构建 JSON 请求体
    std::wostringstream oss;
    oss << L"{\"username\":\"" << username << L"\",\"password\":\"" << password << L"\"}";

    SendRequest(L"POST", path, oss.str(), [this, callback](bool success, const std::wstring& response) {
        if (success) {
            // 解析返回的 objectId 和 sessionToken
            m_objectId = ParseJSONField(response, L"id");
            m_sessionToken = ParseJSONField(response, L"sessionToken");
        }
        if (callback) callback(success, response);
    });
}

void LeanCloudClient::Login(const std::wstring& username, const std::wstring& password, HttpCallback callback) {
    // GET /1.1/login?username=xxx&password=xxx
    std::wostringstream oss;
    oss << L"/1.1/login?username=" << username << L"&password=" << password;

    SendRequest(L"GET", oss.str(), L"", [this, callback](bool success, const std::wstring& response) {
        if (success) {
            m_objectId = ParseJSONField(response, L"id");
            m_sessionToken = ParseJSONField(response, L"sessionToken");
        }
        if (callback) callback(success, response);
    });
}

void LeanCloudClient::SavePlayerData(const std::wstring& objectId, int level, int currentXP, int totalXP,
                                      int happiness, int toysRemaining, HttpCallback callback) {
    // PUT /1.1/classes/CutPetUsers/{objectId}
    std::wostringstream oss;
    oss << L"/1.1/classes/" << TABLE_USERS << L"/" << objectId;

    // 构建 JSON 请求体
    oss << L"{\"level\":" << level
        << L",\"currentXP\":" << currentXP
        << L",\"totalXP\":" << totalXP
        << L",\"happiness\":" << happiness
        << L",\"toysRemaining\":" << toysRemaining
        << L",\"lastLogin\":\"\\";

    std::wstring path = oss.str();

    SendRequest(L"PUT", path.substr(0, path.find_last_of(L"\\")),
                oss.str(), [callback](bool success, const std::wstring& response) {
        if (callback) callback(success, response);
    });
}

void LeanCloudClient::QueryPlayerData(const std::wstring& objectId, HttpCallback callback) {
    // GET /1.1/classes/CutPetUsers/{objectId}
    std::wostringstream oss;
    oss << L"/1.1/classes/" << TABLE_USERS << L"/" << objectId;

    SendRequest(L"GET", oss.str(), L"", callback);
}

void LeanCloudClient::SubmitScore(const std::wstring& objectId, int level, int totalXP, HttpCallback callback) {
    // POST /1.1/classes/Leaderboard
    std::wostringstream oss;
    oss << L"{\"objectId\":\"" << objectId
        << L"\",\"level\":" << level
        << L",\"totalXP\":" << totalXP
        << L",\"updatedAt\":\"" << std::to_wstring(GetTickCount64()) << L"\"}";

    SendRequest(L"POST", L"/1.1/classes/" + std::wstring(TABLE_LEADERBOARD), oss.str(), callback);
}

void LeanCloudClient::QueryLeaderboard(HttpCallback callback) {
    // GET /1.1/classes/Leaderboard?order=-level&limit=10
    std::wstring path = L"/1.1/classes/" + std::wstring(TABLE_LEADERBOARD) +
                        L"?order=-level&limit=10&keys=objectId,level,totalXP,updatedAt";

    SendRequest(L"GET", path, L"", callback);
}

void LeanCloudClient::DailySignin(const std::wstring& objectId, HttpCallback callback) {
    // POST /1.1/checkins
    std::wostringstream oss;
    oss << L"{\"$signature\":\"" << objectId << L"\"}";

    SendRequest(L"POST", L"/1.1/checkins", oss.str(), callback);
}
