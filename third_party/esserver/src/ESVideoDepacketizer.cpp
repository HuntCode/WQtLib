#include "ESVideoDepacketizer.h"

namespace hhcast {

namespace {
constexpr size_t kEsVideoHeaderSize = 128;
constexpr uint32_t kMaxEsVideoPayloadLen = 8 * 1024 * 1024;
}

ESVideoDepacketizer::ESVideoDepacketizer() = default;
ESVideoDepacketizer::~ESVideoDepacketizer() = default;

void ESVideoDepacketizer::SetCallback(ESVideoUnitCallback callback)
{
    m_callback = std::move(callback);
}

bool ESVideoDepacketizer::PushBytes(const uint8_t* data, size_t size)
{
    if (data == nullptr || size == 0) {
        return false;
    }

    m_inputBytes += static_cast<uint64_t>(size);
    m_buffer.insert(m_buffer.end(), data, data + size);
    ProcessBuffer();
    return true;
}

void ESVideoDepacketizer::Reset()
{
    m_buffer.clear();
    m_inputBytes = 0;
    m_unitCount = 0;
    m_droppedUnitCount = 0;
}

uint64_t ESVideoDepacketizer::GetUnitCount() const
{
    return m_unitCount;
}

uint64_t ESVideoDepacketizer::GetDroppedUnitCount() const
{
    return m_droppedUnitCount;
}

uint64_t ESVideoDepacketizer::GetInputBytes() const
{
    return m_inputBytes;
}

uint32_t ESVideoDepacketizer::ReadLe32(const uint8_t* p)
{
    return  (static_cast<uint32_t>(p[0])      ) |
           (static_cast<uint32_t>(p[1]) <<  8) |
           (static_cast<uint32_t>(p[2]) << 16) |
           (static_cast<uint32_t>(p[3]) << 24);
}

uint64_t ESVideoDepacketizer::ReadLe64(const uint8_t* p)
{
    uint64_t v = 0;
    for (int i = 0; i < 8; ++i) {
        v |= (static_cast<uint64_t>(p[i]) << (8 * i));
    }
    return v;
}

void ESVideoDepacketizer::ProcessBuffer()
{
    while (true) {
        if (m_buffer.size() < kEsVideoHeaderSize) {
            return;
        }

        const uint8_t* base = m_buffer.data();
        const uint32_t payloadLen = ReadLe32(base + 0x00);
        const uint32_t rawKind = ReadLe32(base + 0x04);
        const uint64_t timestamp32_32 = ReadLe64(base + 0x08);

        if (payloadLen > kMaxEsVideoPayloadLen) {
            ++m_droppedUnitCount;
            m_buffer.clear();
            return;
        }

        const size_t totalLen = kEsVideoHeaderSize + static_cast<size_t>(payloadLen);
        if (m_buffer.size() < totalLen) {
            return;
        }

        ESVideoUnit unit;
        unit.payloadLen = payloadLen;
        unit.rawKind = rawKind;
        unit.timestamp32_32 = timestamp32_32;
        unit.payload = base + kEsVideoHeaderSize;
        unit.payloadSize = payloadLen;

        switch (rawKind) {
        case static_cast<uint32_t>(ESVideoUnitKind::Config):
            unit.kind = ESVideoUnitKind::Config;
            break;
        case static_cast<uint32_t>(ESVideoUnitKind::Frame):
            unit.kind = ESVideoUnitKind::Frame;
            break;
        default:
            unit.kind = ESVideoUnitKind::Unknown;
            ++m_droppedUnitCount;
            break;
        }

        ++m_unitCount;

        if (m_callback &&
            (unit.kind == ESVideoUnitKind::Config || unit.kind == ESVideoUnitKind::Frame) &&
            unit.payloadSize > 0)
        {
            m_callback(unit);
        }

        m_buffer.erase(m_buffer.begin(), m_buffer.begin() + static_cast<std::ptrdiff_t>(totalLen));
    }
}

} // namespace hhcast