#include "ESServer.h"
#include "ESPortManager.h"
#include "ESSession.h"

#include <cctype>
#include <iostream>
#include <sstream>
#include <vector>

namespace hhcast {

namespace {

static std::string Trim(const std::string& value)
{
    size_t begin = 0;
    size_t end = value.size();

    while (begin < end && (value[begin] == ' ' || value[begin] == '\t' ||
                           value[begin] == '\r' || value[begin] == '\n')) {
        ++begin;
    }

    while (end > begin && (value[end - 1] == ' ' || value[end - 1] == '\t' ||
                           value[end - 1] == '\r' || value[end - 1] == '\n')) {
        --end;
    }

    return value.substr(begin, end - begin);
}

static std::string GetHeaderValue(const std::string& request, const std::string& key)
{
    std::istringstream iss(request);
    std::string line;
    const std::string prefix = key + ":";

    while (std::getline(iss, line)) {
        line = Trim(line);
        if (line.rfind(prefix, 0) == 0) {
            return Trim(line.substr(prefix.size()));
        }
    }

    return "";
}

static std::string BuildRtspResponse(int statusCode,
                                     const std::string& statusText,
                                     const std::string& cseq,
                                     const std::string& body)
{
    std::ostringstream oss;
    oss << "RTSP/1.0 " << statusCode << " " << statusText << "\r\n";
    if (!cseq.empty()) {
        oss << "CSeq: " << cseq << "\r\n";
    }
    oss << "Content-Length: " << body.size() << "\r\n";
    oss << "\r\n";
    oss << body;
    return oss.str();
}

static std::vector<std::string> SplitNonEmptyLines(const std::string& text)
{
    std::vector<std::string> lines;
    std::istringstream iss(text);
    std::string line;

    while (std::getline(iss, line)) {
        line = Trim(line);
        if (!line.empty()) {
            lines.push_back(line);
        }
    }

    return lines;
}

static uint32_t IPToStreamID(const std::string& ip)
{
    std::istringstream iss(ip);
    std::string token;
    uint32_t parts[4] = { 0 };
    int index = 0;

    while (std::getline(iss, token, '.')) {
        if (index >= 4) {
            return 0;
        }

        token = Trim(token);
        if (token.empty()) {
            return 0;
        }

        for (char ch : token) {
            if (!std::isdigit(static_cast<unsigned char>(ch))) {
                return 0;
            }
        }

        int value = std::stoi(token);
        if (value < 0 || value > 255) {
            return 0;
        }

        parts[index++] = static_cast<uint32_t>(value);
    }

    if (index != 4) {
        return 0;
    }

    return (parts[0] << 24) | (parts[1] << 16) | (parts[2] << 8) | parts[3];
}

static bool GetJsonStringValue(const std::string& json, const std::string& key, std::string& value)
{
    const std::string pattern = "\"" + key + "\"";
    size_t pos = json.find(pattern);
    if (pos == std::string::npos) {
        return false;
    }

    pos = json.find(':', pos + pattern.size());
    if (pos == std::string::npos) {
        return false;
    }

    ++pos;
    while (pos < json.size() && std::isspace(static_cast<unsigned char>(json[pos]))) {
        ++pos;
    }

    if (pos >= json.size() || json[pos] != '"') {
        return false;
    }

    ++pos;
    size_t end = json.find('"', pos);
    if (end == std::string::npos) {
        return false;
    }

    value = json.substr(pos, end - pos);
    return true;
}

static bool GetJsonIntValue(const std::string& json, const std::string& key, int& value)
{
    const std::string pattern = "\"" + key + "\"";
    size_t pos = json.find(pattern);
    if (pos == std::string::npos) {
        return false;
    }

    pos = json.find(':', pos + pattern.size());
    if (pos == std::string::npos) {
        return false;
    }

    ++pos;
    while (pos < json.size() && std::isspace(static_cast<unsigned char>(json[pos]))) {
        ++pos;
    }

    size_t end = pos;
    if (end < json.size() && (json[end] == '-' || json[end] == '+')) {
        ++end;
    }

    while (end < json.size() && std::isdigit(static_cast<unsigned char>(json[end]))) {
        ++end;
    }

    if (end == pos) {
        return false;
    }

    value = std::stoi(json.substr(pos, end - pos));
    return true;
}

} // namespace

ESServer::ESServer()
{
    m_portManager = std::make_unique<ESPortManager>();
    m_portManager->SetServer(this);
}

ESServer::~ESServer()
{
    StopServer();
}

int ESServer::StartServer()
{
    if (m_running.load()) {
        return 0;
    }

    int ret = m_portManager->Start();
    if (ret != 0) {
        return ret;
    }

    m_running = true;
    std::cout << "[ESServer] started" << std::endl;
    return 0;
}

int ESServer::StopServer()
{
    if (!m_running.load()) {
        return 0;
    }

    m_portManager->Stop();
    ClearSessions();

    m_running = false;
    std::cout << "[ESServer] stopped" << std::endl;
    return 0;
}

void ESServer::SetCallback(std::shared_ptr<IESServerCallback> callback)
{
    m_callback = callback;
}

bool ESServer::IsRunning() const
{
    return m_running.load();
}

void ESServer::OnTcpConnected(uint16_t localPort, const std::string& peerIp)
{
    std::cout << "[ESServer][TCP][" << localPort << "] connected: " << peerIp << std::endl;
}

void ESServer::OnTcpDisconnected(uint16_t localPort, const std::string& peerIp)
{
    std::cout << "[ESServer][TCP][" << localPort << "] disconnected: " << peerIp << std::endl;

    if (localPort == 57395) {
        uint32_t streamId = IPToStreamID(peerIp);
        if (streamId == 0) {
            std::cout << "[ESServer][TCP][57395] invalid peer ip: " << peerIp << std::endl;
            return;
        }

        auto session = GetSession(streamId);
        if (session) {
            if (m_callback) {
                m_callback->OnDisconnect(streamId);
            }
            RemoveSession(streamId);
        }
    }
}

void ESServer::OnUdpData(uint16_t localPort, const std::string& peerIp, const uint8_t* data, size_t size)
{
    (void)data;
    std::cout << "[ESServer][UDP][" << localPort << "] recv " << size
              << " bytes from " << peerIp << std::endl;
}

std::string ESServer::HandleTcpRequest(uint16_t localPort, const std::string& peerIp, const std::string& request)
{
    std::cout << "[ESServer][TCP][" << localPort << "] request from " << peerIp << ":\n"
              << request << std::endl;

    if (localPort == 8700) {
        const std::string cseq = GetHeaderValue(request, "CSeq");

        std::string response;
        if (request.find("OPTIONS") != std::string::npos &&
            request.find("RTSP/1.0") != std::string::npos) {
            const std::string body = R"({"byom_tx_avalible":"0"})";
            response = BuildRtspResponse(200, "OK", cseq, body);
        } else {
            response = BuildRtspResponse(400, "Bad Request", cseq, "unsupported request");
        }

        std::cout << "[ESServer][TCP][8700] response to " << peerIp << ":\n"
                  << response << std::endl;

        return response;
    }

    if (localPort == 8121) {
        std::vector<std::string> lines = SplitNonEmptyLines(request);
        if (lines.size() < 3) {
            std::cout << "[ESServer][TCP][8121] request lines not enough, wait more data" << std::endl;
            return "";
        }

        const std::string& cmd = lines[0];
        const std::string& senderName = lines[1];
        const std::string& senderVersion = lines[2];

        std::cout << "[ESServer][TCP][8121] cmd=" << cmd
                  << ", senderName=" << senderName
                  << ", senderVersion=" << senderVersion << std::endl;

        std::string response;
        if (cmd == "getServerInfo") {
            response =
                "{\"feature\":\"0x3001bf\","
                "\"name\":\"Newline-7465\","
                "\"version\":20260113,"
                "\"pin\":\"27115282\","
                "\"airPlay\":\"CD:49:0D:D4:41:A1\","
                "\"airPlayFeature\":\"0x527FFFF6,0x1E\","
                "\"webPort\":8000,"
                "\"rotation\":0,"
                "\"id\":\"EC74CD34EFEA\"}";
        }
        else if (cmd == "dongleConnected") {
            response = "Newline-7465\n3.0.1.320\n";
        }
        else {
            response = "unsupported\n";
        }

        std::cout << "[ESServer][TCP][8121] response to " << peerIp << ":\n"
                  << response << std::endl;

        return response;
    }

    if (localPort == 57395) {
        uint32_t streamId = IPToStreamID(peerIp);
        if (streamId == 0) {
            std::cout << "[ESServer][TCP][57395] invalid peer ip: " << peerIp << std::endl;
            return "";
        }

        const std::string text = Trim(request);

        if (text.find("\"clientName\"") != std::string::npos &&
            text.find("\"clientType\"") != std::string::npos) {
            std::string clientName;
            if (!GetJsonStringValue(text, "clientName", clientName)) {
                std::cout << "[ESServer][TCP][57395] clientName not found" << std::endl;
                return "";
            }

            auto session = GetSession(streamId);
            bool isNewSession = false;
            if (!session) {
                session = CreateSession(streamId);
                isNewSession = true;
            }

            session->SetPeerIp(peerIp);
            session->SetName(clientName);

            if (isNewSession && m_callback) {
                m_callback->OnConnect(streamId, session->GetName(), session->GetPeerIp());
            }

            std::string response =
                "{\"boardExists\":0,"
                "\"flavor\":\"eshareall\","
                "\"macAddress\":\"EC74CD34EFEA\","
                "\"replyClientInfo\":\"N\","
                "\"rotation\":0,"
                "\"serverHttpPort\":8000,"
                "\"versionName\":\"v7.7.0113\","
                "\"deviceName\":\"Newline-7465\","
                "\"supportMirror\":1,"
                "\"versionCode\":20260113,"
                "\"platform\":\"\\u003cRockchip3588\\u003e\"}";

            std::cout << "[ESServer][TCP][57395] client info response to " << peerIp << ":\n"
                      << response << std::endl;

            return response;
        }

        if (text.find("\"heartbeat\"") != std::string::npos) {
            auto session = GetSession(streamId);
            if (!session) {
                std::cout << "[ESServer][TCP][57395] heartbeat received but session not found, streamId="
                          << streamId << std::endl;
                return "";
            }

            int heartbeat = 0;
            if (!GetJsonIntValue(text, "heartbeat", heartbeat)) {
                std::cout << "[ESServer][TCP][57395] heartbeat value not found" << std::endl;
                return "";
            }

            std::ostringstream oss;
            oss << "{"
                << "\"isModerator\":0,"
                << "\"multiScreen\":1,"
                << "\"radioMode\":1,"
                << "\"castMode\":1,"
                << "\"replyHeartbeat\":" << heartbeat << ","
                << "\"mirrorMode\":1,"
                << "\"castState\":0"
                << "}";

            std::string response = oss.str();

            std::cout << "[ESServer][TCP][57395] heartbeat response to " << peerIp << ":\n"
                      << response << std::endl;

            return response;
        }

        std::cout << "[ESServer][TCP][57395] unsupported request" << std::endl;
        return "";
    }

    if (localPort == 8600) {
        const std::string text = Trim(request);

        if (text == "CameraAvailabilityCheck") {
            std::string response = "0";
            std::cout << "[ESServer][TCP][8600] response to " << peerIp << ":\n"
                      << response << std::endl;
            return response;
        }

        if (text == "CameraStateCheck") {
            std::string response = "{\"replayStateCheck\":\"N\",\"currentID\":0,\"count\":0}";
            std::cout << "[ESServer][TCP][8600] response to " << peerIp << ":\n"
                      << response << std::endl;
            return response;
        }

        std::cout << "[ESServer][TCP][8600] unsupported request: " << text << std::endl;
        return "";
    }

    return "";
}

std::shared_ptr<ESSession> ESServer::GetSession(uint32_t streamId)
{
    auto it = m_sessions.find(streamId);
    if (it != m_sessions.end()) {
        return it->second;
    }

    return nullptr;
}

std::shared_ptr<ESSession> ESServer::CreateSession(uint32_t streamId)
{
    auto session = GetSession(streamId);
    if (session) {
        return session;
    }

    session = std::make_shared<ESSession>(streamId);
    m_sessions[streamId] = session;
    return session;
}

void ESServer::RemoveSession(uint32_t streamId)
{
    m_sessions.erase(streamId);
}

void ESServer::ClearSessions()
{
    m_sessions.clear();
}

} // namespace hhcast