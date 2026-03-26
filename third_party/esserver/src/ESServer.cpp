#include "ESServer.h"
#include "ESPortManager.h"
#include "ESSession.h"

#include <iostream>
#include <sstream>

namespace hhcast {

namespace {

static std::string Trim(const std::string& value)
{
    size_t begin = 0;
    size_t end = value.size();

    while (begin < end && (value[begin] == ' ' || value[begin] == '\t' || value[begin] == '\r' || value[begin] == '\n')) {
        ++begin;
    }

    while (end > begin && (value[end - 1] == ' ' || value[end - 1] == '\t' || value[end - 1] == '\r' || value[end - 1] == '\n')) {
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
}

std::string ESServer::HandleTcpRequest(uint16_t localPort, const std::string& peerIp, const std::string& request)
{
    std::cout << "[ESServer][TCP][" << localPort << "] request from " << peerIp << ":\n"
              << request << std::endl;

    if (localPort == 8700) {
        const std::string cseq = GetHeaderValue(request, "CSeq");

        if (request.find("OPTIONS") != std::string::npos &&
            request.find("RTSP/1.0") != std::string::npos) {
            const std::string body = R"({"byom_tx_avalible":"0"})";
            const std::string response = BuildRtspResponse(200, "OK", cseq, body);

            std::cout << "[ESServer][TCP][8700] response to " << peerIp << ":\n"
                      << response << std::endl;

            return response;
        }

        const std::string body = "unsupported request";
        const std::string response = BuildRtspResponse(400, "Bad Request", cseq, body);

        std::cout << "[ESServer][TCP][8700] unsupported request from " << peerIp << std::endl;
        std::cout << "[ESServer][TCP][8700] response to " << peerIp << ":\n"
                  << response << std::endl;

        return response;
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