#pragma once

#include <QByteArray>
#include <QMap>
#include <QString>

namespace WQt::Cast::Eshare
{

struct RtspLiteMessage
{
    QString startLine;
    QMap<QString, QString> headers;
    QByteArray body;

    QString headerValue(const QString& key) const;
    void setHeader(const QString& key, const QString& value);
};

class RtspLiteCodec
{
public:
    static QByteArray Encode(const RtspLiteMessage& msg);

    // 成功时：
    // 1. outMsg 返回解析结果
    // 2. rawMessage 返回原始完整报文
    // 3. 从 buffer 中移除已消费字节
    static bool TryDecode(QByteArray& buffer,
                          RtspLiteMessage& outMsg,
                          QByteArray* rawMessage = nullptr,
                          QString* error = nullptr);

    static QString MessageToDebugString(const RtspLiteMessage& msg);
    static QString BodyToReadableString(const QByteArray& body);

private:
    static int FindHeaderEnd(const QByteArray& buffer);
    static bool IsLikelyTextBody(const QByteArray& body);
    static bool IsBinaryPlist(const QByteArray& body);
    static QString BinaryPlistToXml(const QByteArray& body, QString* error = nullptr);
};

} // namespace WQt::Cast::Eshare