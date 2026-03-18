#include "EsharePlistExtract.h"

#include <QRegularExpression>
#include <plist/plist.h>

namespace WQt::Cast::Eshare
{

QString EsharePlistExtract::ToXml(const QByteArray& binPlist, QString* error)
{
    plist_t root = nullptr;
    plist_err_t err = plist_from_bin(binPlist.constData(),
                                     static_cast<uint32_t>(binPlist.size()),
                                     &root);
    if (err != PLIST_ERR_SUCCESS || !root)
    {
        if (error) *error = "plist_from_bin failed";
        return {};
    }

    char* xml = nullptr;
    uint32_t xmlLen = 0;
    plist_to_xml(root, &xml, &xmlLen);

    QString text;
    if (xml && xmlLen > 0)
        text = QString::fromUtf8(xml, static_cast<int>(xmlLen));

    if (xml)
        plist_mem_free(xml);
    plist_free(root);

    return text;
}

int EsharePlistExtract::ExtractInt(const QString& xml, const QString& key, int defaultValue)
{
    const QString pattern =
        QString(R"(<key>\s*%1\s*</key>\s*<(?:integer|string)>\s*([^<]+)\s*</(?:integer|string)>)")
            .arg(QRegularExpression::escape(key));

    QRegularExpression re(pattern, QRegularExpression::DotMatchesEverythingOption);
    QRegularExpressionMatch m = re.match(xml);
    if (!m.hasMatch())
        return defaultValue;

    bool ok = false;
    int v = m.captured(1).trimmed().toInt(&ok);
    return ok ? v : defaultValue;
}

QString EsharePlistExtract::ExtractString(const QString& xml, const QString& key)
{
    const QString pattern =
        QString(R"(<key>\s*%1\s*</key>\s*<string>\s*([^<]+)\s*</string>)")
            .arg(QRegularExpression::escape(key));

    QRegularExpression re(pattern, QRegularExpression::DotMatchesEverythingOption);
    QRegularExpressionMatch m = re.match(xml);
    if (!m.hasMatch())
        return {};

    return m.captured(1).trimmed();
}

void EsharePlistExtract::ExtractVideoSetupResponse(const QString& xml, Eshare51040PortInfo& outInfo)
{
    outInfo.videoDataPort = ExtractInt(xml, "dataPort", outInfo.videoDataPort);
    outInfo.framerate = ExtractInt(xml, "Framerate", outInfo.framerate);
    outInfo.castingWidth = ExtractInt(xml, "casting_win_width", outInfo.castingWidth);
    outInfo.castingHeight = ExtractInt(xml, "casting_win_height", outInfo.castingHeight);

    QString format = ExtractString(xml, "format");
    if (!format.isEmpty())
        outInfo.videoFormat = format;

    QString feature = ExtractString(xml, "feature");
    if (!feature.isEmpty())
        outInfo.feature = feature;
}

void EsharePlistExtract::ExtractAudioSetupResponse(const QString& xml, Eshare51040PortInfo& outInfo)
{
    outInfo.audioDataPort = ExtractInt(xml, "dataPort", outInfo.audioDataPort);
    outInfo.mousePort = ExtractInt(xml, "mousePort", outInfo.mousePort);
    outInfo.controlPort = ExtractInt(xml, "controlPort", outInfo.controlPort);
}

} // namespace WQt::Cast::Eshare