#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

namespace hhcast {

enum class ESVideoUnitKind : uint32_t {
    Unknown = 0,
    Config  = 0x00000100,
    Frame   = 0x00000101,
};

struct ESVideoUnit {
    uint32_t payloadLen = 0;
    uint32_t rawKind = 0;
    ESVideoUnitKind kind = ESVideoUnitKind::Unknown;
    uint64_t timestamp32_32 = 0;
    const uint8_t* payload = nullptr;
    size_t payloadSize = 0;
};

using ESVideoUnitCallback = std::function<void(const ESVideoUnit& unit)>;

class ESVideoDepacketizer {
public:
    ESVideoDepacketizer();
    ~ESVideoDepacketizer();

    void SetCallback(ESVideoUnitCallback callback);

    bool PushBytes(const uint8_t* data, size_t size);
    void Reset();

    uint64_t GetUnitCount() const;
    uint64_t GetDroppedUnitCount() const;
    uint64_t GetInputBytes() const;

private:
    static uint32_t ReadLe32(const uint8_t* p);
    static uint64_t ReadLe64(const uint8_t* p);
    void ProcessBuffer();

private:
    std::vector<uint8_t> m_buffer;
    ESVideoUnitCallback m_callback;

    uint64_t m_inputBytes = 0;
    uint64_t m_unitCount = 0;
    uint64_t m_droppedUnitCount = 0;
};

} // namespace hhcast