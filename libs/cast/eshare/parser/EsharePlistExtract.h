#pragma once

#include <QByteArray>
#include <QString>

#include "Eshare51040PortInfo.h"

namespace WQt::Cast::Eshare
{

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