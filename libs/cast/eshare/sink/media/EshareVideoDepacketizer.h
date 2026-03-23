#pragma once

#include <QObject>
#include <QByteArray>

namespace WQt::Cast::Eshare
{

enum class EshareVideoUnitKind : quint32
{
    Config = 0x00000100,
    Frame  = 0x00000101,
};

struct EshareVideoUnit
{
    quint32 payloadLen = 0;
    EshareVideoUnitKind kind = EshareVideoUnitKind::Frame;
    quint64 timestamp32_32 = 0;
    QByteArray payload;
};

class EshareVideoDepacketizer : public QObject
{
    Q_OBJECT
public:
    explicit EshareVideoDepacketizer(QObject* parent = nullptr);

    void PushBytes(const QByteArray& data);
    void Reset();

signals:
    void SigLog(const QString& text);
    void SigUnitReady(const WQt::Cast::Eshare::EshareVideoUnit& unit);
    void SigError(const QString& text);

private:
    static quint32 ReadLe32(const QByteArray& data, int offset);
    static quint64 ReadLe64(const QByteArray& data, int offset);

private:
    QByteArray m_buffer;
};

} // namespace WQt::Cast::Eshare