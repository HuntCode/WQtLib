#include "EshareVideoPacketizer.h"

namespace WQt::Cast::Eshare
{

void EshareVideoPacketizer::WriteLe32(QByteArray& out, int offset, quint32 value)
{
    out[offset + 0] = static_cast<char>( value        & 0xFF);
    out[offset + 1] = static_cast<char>((value >> 8 ) & 0xFF);
    out[offset + 2] = static_cast<char>((value >> 16) & 0xFF);
    out[offset + 3] = static_cast<char>((value >> 24) & 0xFF);
}

void EshareVideoPacketizer::WriteLe64(QByteArray& out, int offset, quint64 value)
{
    for (int i = 0; i < 8; ++i)
        out[offset + i] = static_cast<char>((value >> (i * 8)) & 0xFF);
}

QByteArray EshareVideoPacketizer::Pack(EshareVideoUnitKind kind,
                                       quint64 timestamp32_32,
                                       const QByteArray& payload)
{
    QByteArray out;
    out.resize(128 + payload.size());
    out.fill('\0');

    // 0x00~0x03: payload length
    WriteLe32(out, 0x00, static_cast<quint32>(payload.size()));

    // 0x04~0x07: unit kind
    WriteLe32(out, 0x04, static_cast<quint32>(kind));

    // 0x08~0x0F: media timestamp (32.32 fixed-point)
    WriteLe64(out, 0x08, timestamp32_32);

    // 0x10~0x7F: reserved = 0

    memcpy(out.data() + 128, payload.constData(), payload.size());
    return out;
}

quint64 EshareVideoPacketizer::MakeTimestamp32_32ByFrameIndex(quint64 frameIndex, int fps)
{
    if (fps <= 0)
        fps = 30;

    const double seconds = static_cast<double>(frameIndex) / static_cast<double>(fps);
    return static_cast<quint64>(seconds * 4294967296.0); // 2^32
}

} // namespace WQt::Cast::Eshare