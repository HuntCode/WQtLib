#include "EshareJsonLineCodec.h"

#include <QJsonDocument>
#include <QJsonParseError>

namespace WQt::Cast::Eshare
{

QByteArray JsonLineCodec::Encode(const QJsonObject& obj)
{
    QByteArray line = QJsonDocument(obj).toJson(QJsonDocument::Compact);
    line += '\n';
    return line;
}

bool JsonLineCodec::TryDecode(QByteArray& buffer,
                              QByteArray* rawLine,
                              QJsonObject& obj,
                              QString* error)
{
    const int lfPos = buffer.indexOf('\n');
    if (lfPos < 0)
        return false;

    QByteArray line = buffer.left(lfPos);
    buffer.remove(0, lfPos + 1);

    if (!line.isEmpty() && line.endsWith('\r'))
        line.chop(1);

    if (rawLine)
        *rawLine = line;

    QJsonParseError jsonError;
    const QJsonDocument doc = QJsonDocument::fromJson(line, &jsonError);
    if (jsonError.error != QJsonParseError::NoError || !doc.isObject())
    {
        if (error)
        {
            *error = QString("Invalid JSON line: %1").arg(jsonError.errorString());
        }
        return false;
    }

    obj = doc.object();
    return true;
}

QString JsonLineCodec::ToPrettyString(const QJsonObject& obj)
{
    return QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Indented));
}

} // namespace WQt::Cast::Eshare