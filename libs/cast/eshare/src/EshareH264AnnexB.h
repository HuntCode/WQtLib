#pragma once

#include <QByteArray>
#include <QVector>
#include <QString>

namespace WQt::Cast::Eshare
{

struct H264NalUnit
{
    int nalType = -1;
    QByteArray bytes;   // 包含 start code
};

struct H264AccessUnit
{
    QByteArray payload; // 多个 NAL 拼起来，保留 start code
    bool isIdr = false;
};

class EshareH264AnnexB
{
public:
    static QVector<H264NalUnit> ParseNals(const QByteArray& data);

    static bool ExtractConfigPayload(const QVector<H264NalUnit>& nals,
                                     QByteArray& configPayload,
                                     QString* error = nullptr);

    static QVector<H264AccessUnit> BuildAccessUnits(const QVector<H264NalUnit>& nals);

    static bool LoadFileAndBuild(const QString& path,
                                 QByteArray& configPayload,
                                 QVector<H264AccessUnit>& accessUnits,
                                 QString* error = nullptr);

private:
    static int FindStartCode(const QByteArray& data, int from, int* codeLen);
    static bool IsVclNal(int nalType);
    static int ParseFirstMbInSlice(const QByteArray& nalWithStartCode);
};

} // namespace WQt::Cast::Eshare