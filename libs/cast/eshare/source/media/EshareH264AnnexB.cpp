#include "EshareH264AnnexB.h"

#include <QFile>

namespace WQt::Cast::Eshare
{
namespace
{

class BitReader
{
public:
    explicit BitReader(const QByteArray& data)
        : m_data(data)
    {
    }

    int ReadBit()
    {
        if (m_bytePos >= m_data.size())
            return -1;

        const unsigned char b = static_cast<unsigned char>(m_data[m_bytePos]);
        const int bit = (b >> (7 - m_bitPos)) & 0x01;

        ++m_bitPos;
        if (m_bitPos == 8)
        {
            m_bitPos = 0;
            ++m_bytePos;
        }

        return bit;
    }

    quint32 ReadBits(int n)
    {
        quint32 v = 0;
        for (int i = 0; i < n; ++i)
        {
            int bit = ReadBit();
            if (bit < 0)
                return v;
            v = (v << 1) | static_cast<quint32>(bit);
        }
        return v;
    }

    quint32 ReadUE()
    {
        int leadingZeroBits = 0;
        while (true)
        {
            int bit = ReadBit();
            if (bit < 0)
                return 0;
            if (bit == 1)
                break;
            ++leadingZeroBits;
        }

        if (leadingZeroBits == 0)
            return 0;

        const quint32 suffix = ReadBits(leadingZeroBits);
        return ((1u << leadingZeroBits) - 1u) + suffix;
    }

private:
    QByteArray m_data;
    int m_bytePos = 0;
    int m_bitPos = 0;
};

static QByteArray RemoveEmulationPrevention(const QByteArray& src)
{
    QByteArray out;
    out.reserve(src.size());

    for (int i = 0; i < src.size(); ++i)
    {
        if (i >= 2 &&
            static_cast<unsigned char>(src[i - 2]) == 0x00 &&
            static_cast<unsigned char>(src[i - 1]) == 0x00 &&
            static_cast<unsigned char>(src[i]) == 0x03)
        {
            continue;
        }
        out.push_back(src[i]);
    }

    return out;
}

static int DetectStartCodeLen(const QByteArray& nal)
{
    if (nal.size() >= 4 &&
        static_cast<unsigned char>(nal[0]) == 0x00 &&
        static_cast<unsigned char>(nal[1]) == 0x00 &&
        static_cast<unsigned char>(nal[2]) == 0x00 &&
        static_cast<unsigned char>(nal[3]) == 0x01)
    {
        return 4;
    }

    if (nal.size() >= 3 &&
        static_cast<unsigned char>(nal[0]) == 0x00 &&
        static_cast<unsigned char>(nal[1]) == 0x00 &&
        static_cast<unsigned char>(nal[2]) == 0x01)
    {
        return 3;
    }

    return 0;
}

} // anonymous namespace

int EshareH264AnnexB::FindStartCode(const QByteArray& data, int from, int* codeLen)
{
    for (int i = from; i + 3 < data.size(); ++i)
    {
        if (static_cast<unsigned char>(data[i]) == 0x00 &&
            static_cast<unsigned char>(data[i + 1]) == 0x00)
        {
            if (static_cast<unsigned char>(data[i + 2]) == 0x01)
            {
                if (codeLen) *codeLen = 3;
                return i;
            }

            if (i + 4 < data.size() &&
                static_cast<unsigned char>(data[i + 2]) == 0x00 &&
                static_cast<unsigned char>(data[i + 3]) == 0x01)
            {
                if (codeLen) *codeLen = 4;
                return i;
            }
        }
    }
    return -1;
}

bool EshareH264AnnexB::IsVclNal(int nalType)
{
    return nalType == 1 || nalType == 5;
}

QVector<H264NalUnit> EshareH264AnnexB::ParseNals(const QByteArray& data)
{
    QVector<H264NalUnit> out;

    int pos = 0;
    while (true)
    {
        int codeLen1 = 0;
        int start = FindStartCode(data, pos, &codeLen1);
        if (start < 0)
            break;

        int nalStart = start + codeLen1;
        if (nalStart >= data.size())
            break;

        int codeLen2 = 0;
        int next = FindStartCode(data, nalStart, &codeLen2);
        int end = (next >= 0) ? next : data.size();

        QByteArray nalBytes = data.mid(start, end - start);
        unsigned char nalHeader = static_cast<unsigned char>(data[nalStart]);
        int nalType = nalHeader & 0x1F;

        H264NalUnit unit;
        unit.nalType = nalType;
        unit.bytes = nalBytes;
        out.push_back(unit);

        pos = end;
    }

    return out;
}

bool EshareH264AnnexB::ExtractConfigPayload(const QVector<H264NalUnit>& nals,
                                            QByteArray& configPayload,
                                            QString* error)
{
    QByteArray sps;
    QByteArray pps;

    for (const auto& nal : nals)
    {
        if (nal.nalType == 7 && sps.isEmpty())
            sps = nal.bytes;
        else if (nal.nalType == 8 && pps.isEmpty())
            pps = nal.bytes;

        if (!sps.isEmpty() && !pps.isEmpty())
            break;
    }

    if (sps.isEmpty() || pps.isEmpty())
    {
        if (error) *error = "SPS/PPS not found in Annex B stream.";
        return false;
    }

    configPayload = sps + pps;
    return true;
}

int EshareH264AnnexB::ParseFirstMbInSlice(const QByteArray& nalWithStartCode)
{
    const int scLen = DetectStartCodeLen(nalWithStartCode);
    if (scLen <= 0 || nalWithStartCode.size() <= scLen + 1)
        return -1;

    // 跳过 start code 和 nal header
    QByteArray sliceData = nalWithStartCode.mid(scLen + 1);
    QByteArray rbsp = RemoveEmulationPrevention(sliceData);

    BitReader br(rbsp);
    return static_cast<int>(br.ReadUE()); // first_mb_in_slice
}

QVector<H264AccessUnit> EshareH264AnnexB::BuildAccessUnits(const QVector<H264NalUnit>& nals)
{
    QVector<H264AccessUnit> out;

    QByteArray prefixNals;
    QByteArray currentPayload;
    bool currentHasVcl = false;
    bool currentIsIdr = false;

    auto flushCurrent = [&]() {
        if (!currentPayload.isEmpty())
        {
            H264AccessUnit au;
            au.payload = currentPayload;
            au.isIdr = currentIsIdr;
            out.push_back(au);
        }
        currentPayload.clear();
        currentHasVcl = false;
        currentIsIdr = false;
    };

    for (const auto& nal : nals)
    {
        const int t = nal.nalType;

        // SPS/PPS 单独走 config，不进入 frame access units
        if (t == 7 || t == 8)
            continue;

        // AUD：通常表示新的 access unit 边界
        if (t == 9)
        {
            if (currentHasVcl)
                flushCurrent();

            prefixNals.clear();
            prefixNals += nal.bytes;
            continue;
        }

        // SEI 或其他非 VCL，挂到下一个 access unit 前缀上
        if (!IsVclNal(t))
        {
            if (currentHasVcl)
                currentPayload += nal.bytes;
            else
                prefixNals += nal.bytes;
            continue;
        }

        // VCL
        const int firstMb = ParseFirstMbInSlice(nal.bytes);
        const bool startsNewAu = (currentHasVcl && firstMb == 0);

        if (startsNewAu)
            flushCurrent();

        if (!currentHasVcl)
        {
            currentPayload += prefixNals;
            prefixNals.clear();
            currentHasVcl = true;
            currentIsIdr = false;
        }

        currentPayload += nal.bytes;
        if (t == 5)
            currentIsIdr = true;
    }

    flushCurrent();
    return out;
}

bool EshareH264AnnexB::LoadFileAndBuild(const QString& path,
                                        QByteArray& configPayload,
                                        QVector<H264AccessUnit>& accessUnits,
                                        QString* error)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
    {
        if (error) *error = QString("Open file failed: %1").arg(f.errorString());
        return false;
    }

    const QByteArray data = f.readAll();
    const QVector<H264NalUnit> nals = ParseNals(data);

    if (nals.isEmpty())
    {
        if (error) *error = "No Annex B NAL units parsed.";
        return false;
    }

    if (!ExtractConfigPayload(nals, configPayload, error))
        return false;

    accessUnits = BuildAccessUnits(nals);
    if (accessUnits.isEmpty())
    {
        if (error) *error = "No access units built from Annex B stream.";
        return false;
    }

    return true;
}

} // namespace WQt::Cast::Eshare