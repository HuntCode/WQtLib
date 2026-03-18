#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QString>

namespace WQt::Cast::Eshare
{

class JsonLineCodec
{
public:
    static QByteArray Encode(const QJsonObject& obj);

    // 从 buffer 中尝试取出一条以 '\n' 结尾的 JSON
    // 成功时：
    // 1. rawLine 返回原始一行（不含末尾 '\n'）
    // 2. obj 返回解析结果
    // 3. 从 buffer 中移除已消费字节
    static bool TryDecode(QByteArray& buffer,
                          QByteArray* rawLine,
                          QJsonObject& obj,
                          QString* error = nullptr);

    static QString ToPrettyString(const QJsonObject& obj);
};

} // namespace WQt::Cast::Eshare