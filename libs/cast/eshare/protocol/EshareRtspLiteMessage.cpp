#include "EshareRtspLiteMessage.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QStringList>

#include <plist/plist.h>

namespace WQt::Cast::Eshare
{

QString RtspLiteMessage::headerValue(const QString& key) const
{
    return headers.value(key);
}

void RtspLiteMessage::setHeader(const QString& key, const QString& value)
{
    headers.insert(key, value);
}

QByteArray RtspLiteCodec::Encode(const RtspLiteMessage& msg)
{
    QByteArray out;
    out += msg.startLine.toUtf8();
    out += "\r\n";

    QMap<QString, QString> headers = msg.headers;
    if (!headers.contains("Content-Length"))
    {
        headers.insert("Content-Length", QString::number(msg.body.size()));
    }

    for (auto it = headers.begin(); it != headers.end(); ++it)
    {
        out += it.key().toUtf8();
        out += ": ";
        out += it.value().toUtf8();
        out += "\r\n";
    }

    out += "\r\n";
    out += msg.body;
    return out;
}

int RtspLiteCodec::FindHeaderEnd(const QByteArray& buffer)
{
    int pos = buffer.indexOf("\r\n\r\n");
    if (pos >= 0)
        return pos + 4;

    pos = buffer.indexOf("\n\n");
    if (pos >= 0)
        return pos + 2;

    return -1;
}

bool RtspLiteCodec::TryDecode(QByteArray& buffer,
                              RtspLiteMessage& outMsg,
                              QByteArray* rawMessage,
                              QString* error)
{
    const int headerEnd = FindHeaderEnd(buffer);
    if (headerEnd < 0)
        return false;

    QByteArray headerPart = buffer.left(headerEnd);
    QList<QByteArray> lines = headerPart.split('\n');

    if (lines.isEmpty())
    {
        if (error) *error = "Empty RTSP-like header.";
        return false;
    }

    auto trimLine = [](QByteArray line) -> QByteArray {
        return line.trimmed();
    };

    outMsg = RtspLiteMessage{};
    outMsg.startLine = QString::fromUtf8(trimLine(lines.first()));

    int contentLength = 0;

    for (int i = 1; i < lines.size(); ++i)
    {
        QByteArray line = trimLine(lines[i]);
        if (line.isEmpty())
            continue;

        int colon = line.indexOf(':');
        if (colon <= 0)
            continue;

        QString key = QString::fromUtf8(line.left(colon)).trimmed();
        QString value = QString::fromUtf8(line.mid(colon + 1)).trimmed();
        outMsg.headers.insert(key, value);

        if (key.compare("Content-Length", Qt::CaseInsensitive) == 0)
        {
            bool ok = false;
            int len = value.toInt(&ok);
            if (ok && len >= 0)
                contentLength = len;
        }
    }

    if (buffer.size() < headerEnd + contentLength)
    {
        return false;
    }

    const int totalLen = headerEnd + contentLength;
    if (rawMessage)
    {
        *rawMessage = buffer.left(totalLen);
    }

    outMsg.body = buffer.mid(headerEnd, contentLength);
    buffer.remove(0, totalLen);
    return true;
}

bool RtspLiteCodec::IsLikelyTextBody(const QByteArray& body)
{
    if (body.isEmpty())
        return true;

    int printable = 0;
    for (unsigned char c : body)
    {
        if (c == '\r' || c == '\n' || c == '\t' || (c >= 32 && c <= 126))
            ++printable;
    }

    // 简单启发式：大多数可打印字符就视为文本
    return printable * 100 / body.size() >= 85;
}

bool RtspLiteCodec::IsBinaryPlist(const QByteArray& body)
{
    return body.startsWith("bplist00");
}

QString RtspLiteCodec::BinaryPlistToXml(const QByteArray& body, QString* error)
{
    plist_t root = nullptr;
    plist_err_t err = plist_from_bin(body.constData(),
                                     static_cast<uint32_t>(body.size()),
                                     &root);
    if (err != PLIST_ERR_SUCCESS || !root)
    {
        if (error) *error = "plist_from_bin failed.";
        return {};
    }

    char* xml = nullptr;
    uint32_t xmlLen = 0;
    plist_to_xml(root, &xml, &xmlLen);

    QString text;
    if (xml && xmlLen > 0)
    {
        text = QString::fromUtf8(xml, static_cast<int>(xmlLen));
    }

    if (xml)
        plist_mem_free(xml);
    plist_free(root);

    return text;
}

QString RtspLiteCodec::BodyToReadableString(const QByteArray& body)
{
    if (body.isEmpty())
        return "<empty>";

    // 1) JSON
    QJsonParseError jsonError;
    QJsonDocument jsonDoc = QJsonDocument::fromJson(body, &jsonError);
    if (jsonError.error == QJsonParseError::NoError)
    {
        return QString::fromUtf8(jsonDoc.toJson(QJsonDocument::Indented));
    }

    // 2) binary plist -> XML
    if (IsBinaryPlist(body))
    {
        QString plistError;
        QString xml = BinaryPlistToXml(body, &plistError);
        if (!xml.isEmpty())
            return xml;

        return QString("<binary plist parse failed: %1>\nHEX:\n%2")
            .arg(plistError, QString::fromUtf8(body.toHex(' ')));
    }

    // 3) 普通文本
    if (IsLikelyTextBody(body))
    {
        return QString::fromUtf8(body);
    }

    // 4) 其它二进制，输出 hex
    return QString("HEX:\n%1").arg(QString::fromUtf8(body.toHex(' ')));
}

QString RtspLiteCodec::MessageToDebugString(const RtspLiteMessage& msg)
{
    QStringList lines;
    lines << msg.startLine;

    for (auto it = msg.headers.begin(); it != msg.headers.end(); ++it)
    {
        lines << QString("%1: %2").arg(it.key(), it.value());
    }

    lines << "";

    if (!msg.body.isEmpty())
        lines << BodyToReadableString(msg.body);
    else
        lines << "<empty>";

    return lines.join("\n");
}

} // namespace WQt::Cast::Eshare