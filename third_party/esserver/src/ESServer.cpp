#include "ESServer.h"
#include "ESPortManager.h"
#include "ESSession.h"
#include "ESRtspLite.h"
#include <plist/plist.h>

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

static bool IsBinaryPlistBody(const std::string& body)
{
    return body.size() >= 8 && body.compare(0, 8, "bplist00") == 0;
}

static void PrintPlistXml(plist_t root, const std::string& tag)
{
    if (root == nullptr) {
        return;
    }

    char* xml = nullptr;
    uint32_t len = 0;
    plist_to_xml(root, &xml, &len);

    if (xml && len > 0) {
        std::cout << tag << "\n"
                  << std::string(xml, len) << std::endl;
    }

    if (xml) {
        plist_mem_free(xml);
    }
}

static void TryPrintBinaryPlistXml(const std::string& bin, const std::string& tag)
{
    if (bin.empty()) {
        return;
    }

    plist_t root = nullptr;
    plist_from_bin(bin.data(), static_cast<uint32_t>(bin.size()), &root);
    if (root == nullptr) {
        std::cout << tag << "\n<invalid binary plist>" << std::endl;
        return;
    }

    PrintPlistXml(root, tag);
    plist_free(root);
}

static std::string GetVideoAudioByCSeq(const std::string& cseq)
{
    if (cseq == "0" || cseq == "1") return "0-0";
    if (cseq == "2") return "48-0";
    if (cseq == "3") return "78-0";
    if (cseq == "4") return "108-0";
    return "0-0";
}

static std::string BuildVideoSetupPlist(uint16_t videoPort,
                                        int framerate,
                                        int width,
                                        int height,
                                        const std::string& feature,
                                        const std::string& format)
{
    plist_t root = plist_new_dict();

    plist_t streams = plist_new_array();
    plist_t streamItem = plist_new_dict();
    plist_dict_set_item(streamItem, "type", plist_new_uint(static_cast<uint64_t>(110)));
    plist_dict_set_item(streamItem, "dataPort", plist_new_uint(static_cast<uint64_t>(videoPort)));
    plist_array_append_item(streams, streamItem);
    plist_dict_set_item(root, "streams", streams);

    plist_dict_set_item(root, "feature", plist_new_string(feature.c_str()));
    plist_dict_set_item(root, "Framerate", plist_new_string(std::to_string(framerate).c_str()));
    plist_dict_set_item(root, "casting_win_width", plist_new_string(std::to_string(width).c_str()));
    plist_dict_set_item(root, "casting_win_height", plist_new_string(std::to_string(height).c_str()));
    plist_dict_set_item(root, "format", plist_new_string(format.c_str()));

    PrintPlistXml(root, "[ESServer][TCP][51040] video setup plist xml:");

    char* bin = nullptr;
    uint32_t len = 0;
    plist_to_bin(root, &bin, &len);

    std::string out;
    if (bin && len > 0) {
        out.assign(bin, bin + len);
    }

    if (bin) {
        plist_mem_free(bin);
    }
    plist_free(root);
    return out;
}

static std::string BuildAudioSetupPlist(uint16_t dataPort,
                                        uint16_t controlPort,
                                        uint16_t mousePort)
{
    plist_t root = plist_new_dict();

    plist_t streams = plist_new_array();
    plist_t streamItem = plist_new_dict();
    plist_dict_set_item(streamItem, "type", plist_new_uint(static_cast<uint64_t>(96)));
    plist_dict_set_item(streamItem, "dataPort", plist_new_uint(static_cast<uint64_t>(dataPort)));
    plist_dict_set_item(streamItem, "controlPort", plist_new_uint(static_cast<uint64_t>(controlPort)));
    plist_dict_set_item(streamItem, "mousePort", plist_new_uint(static_cast<uint64_t>(mousePort)));
    plist_array_append_item(streams, streamItem);
    plist_dict_set_item(root, "streams", streams);

    PrintPlistXml(root, "[ESServer][TCP][51040] audio setup plist xml:");

    char* bin = nullptr;
    uint32_t len = 0;
    plist_to_bin(root, &bin, &len);

    std::string out;
    if (bin && len > 0) {
        out.assign(bin, bin + len);
    }

    if (bin) {
        plist_mem_free(bin);
    }
    plist_free(root);
    return out;
}

static std::string BuildOptionsJson(int framerate, int width, int height)
{
    std::ostringstream oss;
    oss << "{"
        << "\"Framerate\":\"" << framerate << "\","
        << "\"casting_win_width\":\"" << width << "\","
        << "\"casting_win_height\":\"" << height << "\","
        << "\"idr_req\":\"1\","
        << "\"bitrate\":\"8000000\","
        << "\"i-interval\":\"60\","
        << "\"Castnum\":\"1\","
        << "\"exclusive_screen\":\"0\""
        << "}";
    return oss.str();
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

void ESServer::OnTcpData(uint16_t localPort,
                         const std::string& peerIp,
                         const uint8_t* data,
                         size_t size)
{
    if (data == nullptr || size == 0) {
        return;
    }

    std::cout << "[ESServer][TCP][" << localPort << "] recv "
              << size << " bytes from " << peerIp << std::endl;

    if (localPort != 51030) {
        return;
    }

    const uint32_t streamId = IPToStreamID(peerIp);
    if (streamId == 0) {
        std::cout << "[ESServer][TCP][51030] invalid peer ip: "
                  << peerIp << std::endl;
        return;
    }

    auto session = GetSession(streamId);
    if (!session) {
        std::cout << "[ESServer][TCP][51030] session not found, drop video, streamId="
                  << streamId << ", peerIp=" << peerIp << std::endl;
        return;
    }

    session->InputVideoTcpData(data, size);
}

void ESServer::OnUdpData(uint16_t localPort,
                         const std::string& peerIp,
                         const uint8_t* data,
                         size_t size)
{
    if (data == nullptr || size == 0) {
        return;
    }

    std::cout << "[ESServer][UDP][" << localPort << "] recv "
              << size << " bytes from " << peerIp << std::endl;

    if (m_portManager == nullptr) {
        return;
    }

    const uint16_t dataPort = m_portManager->GetDataPort();
    const uint16_t controlPort = m_portManager->GetControlPort();
    const uint16_t mousePort = m_portManager->GetMousePort();

    if (localPort == dataPort) {
        const uint32_t streamId = IPToStreamID(peerIp);
        if (streamId == 0) {
            std::cout << "[ESServer][UDP][" << localPort
                      << "] invalid peer ip: " << peerIp << std::endl;
            return;
        }

        auto session = GetSession(streamId);
        if (!session) {
            std::cout << "[ESServer][UDP][" << localPort
                      << "] session not found, drop audio, streamId="
                      << streamId << ", peerIp=" << peerIp << std::endl;
            return;
        }

        session->InputAudioUdpDatagram(data, size);
        return;
    }

    if (localPort == controlPort) {
        std::cout << "[ESServer][UDP][" << localPort
                  << "] control data ignored for now." << std::endl;
        return;
    }

    if (localPort == mousePort) {
        std::cout << "[ESServer][UDP][" << localPort
                  << "] mouse data ignored for now." << std::endl;
        return;
    }
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
                << "\"castState\":1"
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

    if (localPort == 51040) {
        ESRtspLiteMessage req;
        std::string error;
        if (!ESRtspLiteCodec::DecodeSingle(request, req, &error)) {
            std::cout << "[ESServer][TCP][51040] decode failed: " << error << std::endl;
            return "";
        }

        if (IsBinaryPlistBody(req.body)) {
            TryPrintBinaryPlistXml(req.body, "[ESServer][TCP][51040] recv plist xml:");
        }

        ESRtspLiteMessage resp;
        const std::string cseq = req.HeaderValue("CSeq");

        const bool isSetup =
            req.startLine.rfind("SETUP ", 0) == 0 ||
            req.startLine.rfind("setup ", 0) == 0;
        const bool isOptions =
            req.startLine.rfind("OPTIONS ", 0) == 0 ||
            req.startLine.rfind("options ", 0) == 0;
        const bool isTeardown =
            req.startLine.rfind("TEARDOWN ", 0) == 0 ||
            req.startLine.rfind("teardown ", 0) == 0;

        if (isSetup) {
            const bool isVideoSetup =
                (cseq == "0") || !req.HeaderValue("VideoAspectRatio").empty();

            resp.startLine = "RTSP/1.0 200 OK";
            if (!cseq.empty()) {
                resp.SetHeader("CSeq", cseq);
            }
            resp.SetHeader("Content-Type", "null");

            if (isVideoSetup) {
                resp.body = BuildVideoSetupPlist(
                    m_portManager->GetVideoPort(),
                    30,
                    3840,
                    2160,
                    "1",
                    "video:h264");
            } else {
                resp.body = BuildAudioSetupPlist(
                    m_portManager->GetDataPort(),
                    m_portManager->GetControlPort(),
                    m_portManager->GetMousePort());
            }

            std::string response = ESRtspLiteCodec::Encode(resp);
            std::cout << "[ESServer][TCP][51040] response to " << peerIp
                      << " (" << (isVideoSetup ? "video setup" : "audio setup") << ")\n";
            return response;
        }

        if (isOptions) {
            resp.startLine = "RTSP/1.0 200 OK";
            if (!cseq.empty()) {
                resp.SetHeader("CSeq", cseq);
            }
            resp.SetHeader("Video-Audio", GetVideoAudioByCSeq(cseq));
            resp.body =
                "{\"Framerate\":\"30\","
                "\"idr_req\":\"0\","
                "\"casting_win_height\":\"2160\","
                "\"feature\":\"1\","
                "\"Castnum\":\"1\","
                "\"casting_win_width\":\"3840\","
                "\"exclusive_screen\":\"0\","
                "\"bitrate\":\"0\","
                "\"i-interval\":\"0\"}";

            std::string response = ESRtspLiteCodec::Encode(resp);
            std::cout << "[ESServer][TCP][51040] options response to " << peerIp << std::endl;
            return response;
        }

        if (isTeardown) {
            resp.startLine = "RTSP/1.0 200 OK";
            if (!cseq.empty()) {
                resp.SetHeader("CSeq", cseq);
            }
            resp.body.clear();

            std::string response = ESRtspLiteCodec::Encode(resp);
            std::cout << "[ESServer][TCP][51040] teardown response to " << peerIp << std::endl;
            return response;
        }

        std::cout << "[ESServer][TCP][51040] unsupported request:\n" << request << std::endl;
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

    session->SetVideoCallback(
        [this](uint32_t cbStreamId,
               const uint8_t* data,
               size_t size,
               const ESVideoUnit& unit) {
            (void)unit;

            if (m_callback && data != nullptr && size > 0) {
                m_callback->OnVideoData(cbStreamId, data, size);
            }
        });

    session->SetAudioCallback(
        [this](uint32_t cbStreamId,
               const uint8_t* data,
               size_t size,
               const ESAudioPayloadInfo& info) {
            (void)info;

            if (m_callback && data != nullptr && size > 0) {
                m_callback->OnAudioData(cbStreamId, data, size);
            }
        });

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