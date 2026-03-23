#include "EshareVideoDepacketizer.h"

namespace WQt::Cast::Eshare
{

EshareVideoDepacketizer::EshareVideoDepacketizer(QObject* parent)
    : QObject(parent)
{
}

void EshareVideoDepacketizer::Reset()
{
    m_buffer.clear();
}

quint32 EshareVideoDepacketizer::ReadLe32(const QByteArray& data, int offset)
{
    return  (static_cast<quint32>(static_cast<unsigned char>(data[offset + 0]))      ) |
           (static_cast<quint32>(static_cast<unsigned char>(data[offset + 1])) <<  8) |
           (static_cast<quint32>(static_cast<unsigned char>(data[offset + 2])) << 16) |
           (static_cast<quint32>(static_cast<unsigned char>(data[offset + 3])) << 24);
}

quint64 EshareVideoDepacketizer::ReadLe64(const QByteArray& data, int offset)
{
    quint64 v = 0;
    for (int i = 0; i < 8; ++i)
    {
        v |= (static_cast<quint64>(static_cast<unsigned char>(data[offset + i])) << (8 * i));
    }
    return v;
}

void EshareVideoDepacketizer::PushBytes(const QByteArray& data)
{
    if (data.isEmpty())
        return;

    m_buffer += data;

    while (true)
    {
        if (m_buffer.size() < 128)
            return;

        const quint32 payloadLen = ReadLe32(m_buffer, 0x00);
        const quint32 kindRaw = ReadLe32(m_buffer, 0x04);
        const quint64 ts = ReadLe64(m_buffer, 0x08);

        const qint64 totalLen = 128ll + static_cast<qint64>(payloadLen);
        if (totalLen < 128)
        {
            emit SigError(QStringLiteral("[VDEP] invalid totalLen"));
            m_buffer.clear();
            return;
        }

        if (m_buffer.size() < totalLen)
            return;

        EshareVideoUnit unit;
        unit.payloadLen = payloadLen;
        unit.kind = static_cast<EshareVideoUnitKind>(kindRaw);
        unit.timestamp32_32 = ts;
        unit.payload = m_buffer.mid(128, payloadLen);

        emit SigLog(QStringLiteral("[VDEP] unit ready: kind=0x%1 payload=%2 ts=0x%3")
                    .arg(QString::number(kindRaw, 16))
                    .arg(payloadLen)
                    .arg(QString::number(ts, 16)));

        emit SigUnitReady(unit);

        m_buffer.remove(0, static_cast<int>(totalLen));
    }
}

} // namespace WQt::Cast::Eshare