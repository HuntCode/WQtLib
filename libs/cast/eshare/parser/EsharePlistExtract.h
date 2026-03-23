#pragma once

#include <QByteArray>
#include <QString>

namespace WQt::Cast::Eshare
{

struct Eshare51040PortInfo
{
    int videoDataPort = -1;
    int audioDataPort = -1;
    int mousePort = -1;
    int controlPort = -1;

    int framerate = -1;
    int castingWidth = -1;
    int castingHeight = -1;

    QString videoFormat;
    QString feature;
};

class EsharePlistExtract
{
public:
    static QString ToXml(const QByteArray& binPlist, QString* error = nullptr);

    static int ExtractInt(const QString& xml, const QString& key, int defaultValue = -1);
    static QString ExtractString(const QString& xml, const QString& key);

    static void ExtractVideoSetupResponse(const QString& xml, Eshare51040PortInfo& outInfo);
    static void ExtractAudioSetupResponse(const QString& xml, Eshare51040PortInfo& outInfo);
};

} // namespace WQt::Cast::Eshare