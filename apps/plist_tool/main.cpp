#include <QCoreApplication>
#include <QCommandLineParser>
#include <QCommandLineOption>
#include <QFile>
#include <QTextStream>
#include <QString>
#include <QStringList>
#include <QByteArray>
#include <QRegularExpression>

#include <plist/plist.h>

#include <cstdint>

static QString PlistFormatToString(plist_format_t fmt)
{
    switch (fmt) {
    case PLIST_FORMAT_NONE:     return "none";
    case PLIST_FORMAT_XML:      return "xml";
    case PLIST_FORMAT_BINARY:   return "binary";
    case PLIST_FORMAT_JSON:     return "json";
    case PLIST_FORMAT_OSTEP:    return "openstep";
    case PLIST_FORMAT_PRINT:    return "print";
    case PLIST_FORMAT_LIMD:     return "limd";
    case PLIST_FORMAT_PLUTIL:   return "plutil";
    default:                    return "unknown";
    }
}

static bool ReadTextFile(const QString& path, QString& outText, QString& err)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        err = QString("打开文本文件失败: %1").arg(path);
        return false;
    }
    outText = QString::fromUtf8(f.readAll());
    return true;
}

static bool ReadBinaryFile(const QString& path, QByteArray& outData, QString& err)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        err = QString("打开二进制文件失败: %1").arg(path);
        return false;
    }
    outData = f.readAll();
    return true;
}

// 解析 Wireshark / 人工整理后的 hex 文本。
// 支持 token 形式：
//   62 70 6C 69
//   0x62 0x70 0x6C
//   \x62 \x70 \x6C
// 会自动忽略：
//   偏移列（如 0000 / 0010）
//   ASCII 列（如 bplist00）
//   其他无关字符
static bool ParseHexByteTokens(const QString& text, QByteArray& outData, QString& err)
{
    outData.clear();

    const QStringList rawTokens = text.split(QRegularExpression(R"([\s,;|]+)"),
                                             Qt::SkipEmptyParts);

    QRegularExpression byteRe(R"(^(?:0x|\\x)?([0-9A-Fa-f]{2})$)");
    int accepted = 0;

    for (const QString& tk : rawTokens) {
        QRegularExpressionMatch m = byteRe.match(tk);
        if (!m.hasMatch()) {
            continue;
        }

        bool ok = false;
        int v = m.captured(1).toInt(&ok, 16);
        if (!ok) {
            continue;
        }

        outData.append(static_cast<char>(v & 0xFF));
        ++accepted;
    }

    if (accepted == 0) {
        err = "没有解析出任何十六进制字节，请确认输入是 plist 的 payload 字节而不是整段协议文本。";
        return false;
    }

    return true;
}

static bool ConvertPlistBytesToXml(const QByteArray& input,
                                   QString& outXml,
                                   QString& outDetectedFormat,
                                   QString& err)
{
    if (input.isEmpty()) {
        err = "输入字节为空。";
        return false;
    }

    plist_t root = nullptr;
    plist_format_t detectedFormat = PLIST_FORMAT_NONE;

    plist_err_t rc = plist_from_memory(
        input.constData(),
        static_cast<uint32_t>(input.size()),
        &root,
        &detectedFormat
    );

    if (rc != PLIST_ERR_SUCCESS || !root) {
        err = QString("plist_from_memory 失败，rc=%1，输入大小=%2 bytes")
                  .arg(static_cast<int>(rc))
                  .arg(input.size());
        return false;
    }

    char* xmlBuf = nullptr;
    uint32_t xmlLen = 0;

    rc = plist_to_xml(root, &xmlBuf, &xmlLen);
    if (rc != PLIST_ERR_SUCCESS || !xmlBuf) {
        plist_free(root);
        err = QString("plist_to_xml 失败，rc=%1").arg(static_cast<int>(rc));
        return false;
    }

    outDetectedFormat = PlistFormatToString(detectedFormat);
    outXml = QString::fromUtf8(xmlBuf, static_cast<int>(xmlLen));

    plist_mem_free(xmlBuf);
    plist_free(root);
    return true;
}

static bool WriteTextFile(const QString& path, const QString& text, QString& err)
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        err = QString("写出文件失败: %1").arg(path);
        return false;
    }

    QTextStream ts(&f);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    ts.setCodec("UTF-8");
#else
    ts.setEncoding(QStringConverter::Utf8);
#endif
    ts << text;
    return true;
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName("plist_tool");
    QCoreApplication::setApplicationVersion("0.1.0");

    QCommandLineParser parser;
    parser.setApplicationDescription("Convert plist bytes captured from Wireshark into XML.");
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption hexFileOpt(
        QStringList() << "hex-file",
        "十六进制文本文件路径（如 Wireshark 导出的 payload 文本）",
        "path"
    );

    QCommandLineOption binFileOpt(
        QStringList() << "bin-file",
        "二进制 plist 文件路径",
        "path"
    );

    QCommandLineOption outOpt(
        QStringList() << "o" << "out",
        "输出 xml 文件路径；不填则打印到控制台",
        "path"
    );

    parser.addOption(hexFileOpt);
    parser.addOption(binFileOpt);
    parser.addOption(outOpt);

    parser.process(app);

    const bool hasHexFile = parser.isSet(hexFileOpt);
    const bool hasBinFile = parser.isSet(binFileOpt);

    if ((hasHexFile ? 1 : 0) + (hasBinFile ? 1 : 0) != 1) {
        QTextStream(stderr)
            << "必须二选一:\n"
            << "  --hex-file <path>\n"
            << "  --bin-file <path>\n";
        return 1;
    }

    QString err;
    QByteArray inputBytes;

    if (hasHexFile) {
        QString hexText;
        if (!ReadTextFile(parser.value(hexFileOpt), hexText, err)) {
            QTextStream(stderr) << err << '\n';
            return 2;
        }

        if (!ParseHexByteTokens(hexText, inputBytes, err)) {
            QTextStream(stderr) << err << '\n';
            return 3;
        }
    } else {
        if (!ReadBinaryFile(parser.value(binFileOpt), inputBytes, err)) {
            QTextStream(stderr) << err << '\n';
            return 4;
        }
    }

    QString xml;
    QString detectedFormat;
    if (!ConvertPlistBytesToXml(inputBytes, xml, detectedFormat, err)) {
        QTextStream(stderr) << err << '\n';
        return 5;
    }

    QTextStream(stdout) << "[info] input bytes = " << inputBytes.size() << '\n';
    QTextStream(stdout) << "[info] detected plist format = " << detectedFormat << '\n';

    if (parser.isSet(outOpt)) {
        if (!WriteTextFile(parser.value(outOpt), xml, err)) {
            QTextStream(stderr) << err << '\n';
            return 6;
        }
        QTextStream(stdout) << "[info] xml written to: " << parser.value(outOpt) << '\n';
    } else {
        QTextStream(stdout) << xml << '\n';
    }

    return 0;
}