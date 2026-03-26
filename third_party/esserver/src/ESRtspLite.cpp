#include "ESRtspLite.h"

#include <algorithm>
#include <cctype>
#include <sstream>

namespace hhcast {

namespace {

static std::string Trim(const std::string& value)
{
    size_t begin = 0;
    size_t end = value.size();

    while (begin < end && std::isspace(static_cast<unsigned char>(value[begin]))) {
        ++begin;
    }
    while (end > begin && std::isspace(static_cast<unsigned char>(value[end - 1]))) {
        --end;
    }

    return value.substr(begin, end - begin);
}

static bool IEquals(const std::string& a, const std::string& b)
{
    if (a.size() != b.size()) {
        return false;
    }

    for (size_t i = 0; i < a.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(a[i])) !=
            std::tolower(static_cast<unsigned char>(b[i]))) {
            return false;
        }
    }
    return true;
}

static bool ParseContentLength(const ESRtspLiteMessage& msg, size_t& len)
{
    const std::string value = msg.HeaderValue("Content-Length");
    if (value.empty()) {
        len = 0;
        return true;
    }

    try {
        len = static_cast<size_t>(std::stoul(Trim(value)));
        return true;
    } catch (...) {
        return false;
    }
}

static bool DecodeRaw(const std::string& raw, ESRtspLiteMessage& msg, std::string* error)
{
    msg = ESRtspLiteMessage{};

    const size_t headerEnd = raw.find("\r\n\r\n");
    if (headerEnd == std::string::npos) {
        if (error) *error = "header terminator not found";
        return false;
    }

    const std::string headerPart = raw.substr(0, headerEnd);
    msg.body = raw.substr(headerEnd + 4);

    std::istringstream iss(headerPart);
    std::string line;

    if (!std::getline(iss, line)) {
        if (error) *error = "empty start line";
        return false;
    }

    if (!line.empty() && line.back() == '\r') {
        line.pop_back();
    }
    msg.startLine = line;

    while (std::getline(iss, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        const size_t pos = line.find(':');
        if (pos == std::string::npos) {
            continue;
        }

        ESRtspLiteHeader header;
        header.key = Trim(line.substr(0, pos));
        header.value = Trim(line.substr(pos + 1));
        msg.headers.push_back(header);
    }

    size_t contentLength = 0;
    if (!ParseContentLength(msg, contentLength)) {
        if (error) *error = "invalid Content-Length";
        return false;
    }

    if (msg.body.size() != contentLength) {
        if (error) *error = "body length mismatch";
        return false;
    }

    return true;
}

} // namespace

std::string ESRtspLiteMessage::HeaderValue(const std::string& key) const
{
    for (const auto& header : headers) {
        if (IEquals(header.key, key)) {
            return header.value;
        }
    }
    return "";
}

void ESRtspLiteMessage::SetHeader(const std::string& key, const std::string& value)
{
    for (auto& header : headers) {
        if (IEquals(header.key, key)) {
            header.value = value;
            return;
        }
    }
    headers.push_back({ key, value });
}

bool ESRtspLiteCodec::TryDecode(std::string& buffer,
                                ESRtspLiteMessage& msg,
                                std::string* rawMsg,
                                std::string* error)
{
    const size_t headerEnd = buffer.find("\r\n\r\n");
    if (headerEnd == std::string::npos) {
        return false;
    }

    const std::string headerPart = buffer.substr(0, headerEnd);
    std::istringstream iss(headerPart);
    std::string startLine;
    std::getline(iss, startLine);

    ESRtspLiteMessage headerOnly;
    std::string fakeRaw = buffer.substr(0, headerEnd + 4);
    std::string parseError;
    if (!DecodeRaw(fakeRaw, headerOnly, &parseError)) {
        // fakeRaw body 为空，DecodeRaw 可能因为 Content-Length 不匹配失败
        // 所以这里单独解析 Content-Length
        headerOnly = ESRtspLiteMessage{};
        std::string line;
        std::istringstream hss(headerPart);

        if (!std::getline(hss, line)) {
            if (error) *error = "empty start line";
            return false;
        }
        if (!line.empty() && line.back() == '\r') line.pop_back();
        headerOnly.startLine = line;

        while (std::getline(hss, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            size_t pos = line.find(':');
            if (pos == std::string::npos) continue;
            headerOnly.headers.push_back({ Trim(line.substr(0, pos)), Trim(line.substr(pos + 1)) });
        }
    }

    size_t contentLength = 0;
    if (!ParseContentLength(headerOnly, contentLength)) {
        if (error) *error = "invalid Content-Length";
        return false;
    }

    const size_t totalSize = headerEnd + 4 + contentLength;
    if (buffer.size() < totalSize) {
        return false;
    }

    std::string raw = buffer.substr(0, totalSize);
    if (!DecodeSingle(raw, msg, error)) {
        return false;
    }

    if (rawMsg) {
        *rawMsg = raw;
    }

    buffer.erase(0, totalSize);
    return true;
}

bool ESRtspLiteCodec::DecodeSingle(const std::string& rawMsg,
                                   ESRtspLiteMessage& msg,
                                   std::string* error)
{
    return DecodeRaw(rawMsg, msg, error);
}

std::string ESRtspLiteCodec::Encode(const ESRtspLiteMessage& msg)
{
    std::ostringstream oss;
    oss << msg.startLine << "\r\n";

    bool hasContentLength = false;
    for (const auto& header : msg.headers) {
        if (IEquals(header.key, "Content-Length")) {
            hasContentLength = true;
        }
        oss << header.key << ": " << header.value << "\r\n";
    }

    if (!hasContentLength) {
        oss << "Content-Length: " << msg.body.size() << "\r\n";
    }

    oss << "\r\n";
    oss << msg.body;
    return oss.str();
}

} // namespace hhcast