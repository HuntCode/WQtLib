#pragma once

#include <QByteArray>
#include <QtGlobal>

namespace WQt::Cast::Eshare
{

enum class EshareVideoUnitKind : quint32
{
    Config = 0x00000100,
    Frame  = 0x00000101,
};

class EshareVideoPacketizer
{
public:
    static QByteArray Pack(EshareVideoUnitKind kind,
                           quint64 timestamp32_32,
                           const QByteArray& payload);

    static quint64 MakeTimestamp32_32ByFrameIndex(quint64 frameIndex, int fps);

private:
    static void WriteLe32(QByteArray& out, int offset, quint32 value);
    static void WriteLe64(QByteArray& out, int offset, quint64 value);
};

} // namespace WQt::Cast::Eshare