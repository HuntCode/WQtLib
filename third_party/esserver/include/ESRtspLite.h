#pragma once

#include <string>
#include <vector>

namespace hhcast {

struct ESRtspLiteHeader {
    std::string key;
    std::string value;
};

struct ESRtspLiteMessage {
    std::string startLine;
    std::vector<ESRtspLiteHeader> headers;
    std::string body;

    std::string HeaderValue(const std::string& key) const;
    void SetHeader(const std::string& key, const std::string& value);
};

class ESRtspLiteCodec {
public:
    // 从 buffer 中尝试解析一条完整消息；成功则消费 buffer 前面的消息字节
    static bool TryDecode(std::string& buffer,
                          ESRtspLiteMessage& msg,
                          std::string* rawMsg = nullptr,
                          std::string* error = nullptr);

    // 解码一条完整原始消息，不消费外部 buffer
    static bool DecodeSingle(const std::string& rawMsg,
                             ESRtspLiteMessage& msg,
                             std::string* error = nullptr);

    static std::string Encode(const ESRtspLiteMessage& msg);
};

} // namespace hhcast