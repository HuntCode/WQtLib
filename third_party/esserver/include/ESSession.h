#pragma once

#include <cstdint>
#include <string>

namespace hhcast {

class ESSession {
public:
    explicit ESSession(uint32_t streamId);
    ~ESSession();

    uint32_t GetStreamId() const;

    void SetPeerIp(const std::string& peerIp);
    const std::string& GetPeerIp() const;

    void SetName(const std::string& name);
    const std::string& GetName() const;

private:
    uint32_t m_streamId = 0;
    std::string m_peerIp;
    std::string m_name;
};

} // namespace hhcast