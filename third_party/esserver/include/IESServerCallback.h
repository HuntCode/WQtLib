#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace hhcast {

class IESServerCallback {
public:
    virtual ~IESServerCallback() = default;

    virtual void OnConnect(uint32_t streamId, const std::string& name, const std::string& ip) = 0;
    virtual void OnDisconnect(uint32_t streamId) = 0;

    virtual void OnVideoData(uint32_t streamId, const uint8_t* data, size_t size) = 0;
    virtual void OnAudioData(uint32_t streamId, const uint8_t* data, size_t size) = 0;
};

} // namespace hhcast