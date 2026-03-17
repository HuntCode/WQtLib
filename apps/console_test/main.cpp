#include <QCoreApplication>
#include <QByteArray>
#include <QString>
#include <QDebug>
#include <QRegularExpression>

// 从 mcrecv://.../s-xxxx 提取并解码 security context
static bool DecodeMcrecvSecurityContext(const QString& mcrecvUri, QByteArray& outSecCtx)
{
    outSecCtx.clear();

    // 1) scheme 校验
    const QString scheme = QStringLiteral("mcrecv://");
    if (!mcrecvUri.startsWith(scheme, Qt::CaseSensitive)) {
        qDebug() << "Unknown scheme:" << mcrecvUri;
        return false;
    }

    // 2) 找到第 3 个 '/'（从 scheme 后开始找）
    int slashPos = scheme.size();  // 9
    int slashCount = 0;
    while (slashCount < 3) {
        slashPos = mcrecvUri.indexOf(QLatin1Char('/'), slashPos + 1);
        if (slashPos < 0) break;
        ++slashCount;
    }
    if (slashPos < 0) {
        qDebug() << "No security context tail segment:" << mcrecvUri;
        return false;
    }

    // 3) thirdSlash 后面的尾段应该是 "s-xxxx"
    const QString tailSegment = mcrecvUri.mid(slashPos + 1);
    if (!tailSegment.startsWith(QStringLiteral("s-"), Qt::CaseSensitive)) {
        qDebug() << "Invalid security context tail segment:" << tailSegment;
        return false;
    }

    // 4) 解码 Base64
    QString base64Text = tailSegment.mid(2); // 去掉 "s-"
    // 防御性：去掉可能的空白（如果你复制粘贴 URI 时换行了）
    base64Text.remove(QRegularExpression(QStringLiteral("\\s+")));

    const QByteArray base64Utf8 = base64Text.toUtf8();
    const QByteArray decoded = QByteArray::fromBase64(base64Utf8);

    if (decoded.isEmpty()) {
        qDebug() << "Base64 decode failed or empty result. base64Len=" << base64Utf8.size();
        return false;
    }

    outSecCtx = decoded;
    return true;
}

static void DumpSecCtx(const QByteArray& secCtx)
{
    qDebug() << "Decoded bytes =" << secCtx.size();

    // 16进制输出（每字节用空格分隔）
    qDebug().noquote() << "Hex(first 256):"
                       << secCtx.left(secCtx.size()).toHex(' ');

    // 粗略提取可打印 ASCII 片段（便于你快速看到里面有没有字符串）
    QByteArray printable;
    for (char c : secCtx) {
        if (c >= 32 && c <= 126) printable.push_back(c);
        else printable.push_back('.');
    }
    qDebug().noquote() << "ASCII view(first 256):" << printable.left(secCtx.size());
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    // 把你抓到的完整 URI 放这里（包含 /s-...）
    const QString uri = QStringLiteral(
        "mcrecv://192.168.9.155:7236/h-00000008/192.168.9.221/"
        "s-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" // 省略
        );

    QByteArray secCtx;
    if (DecodeMcrecvSecurityContext(uri, secCtx)) {
        DumpSecCtx(secCtx);
    } else {
        qDebug() << "No security context decoded.";
    }

    return 0;
}
