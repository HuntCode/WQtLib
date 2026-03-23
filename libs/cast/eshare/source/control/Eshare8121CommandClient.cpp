#include "Eshare8121CommandClient.h"

#include <QJsonDocument>
#include <QJsonParseError>

namespace WQt::Cast::Eshare
{

Eshare8121CommandClient::Eshare8121CommandClient(QObject* parent)
    : QObject(parent)
{
    m_socket = new QTcpSocket(this);

    m_idleTimer = new QTimer(this);
    m_idleTimer->setSingleShot(true);

    connect(m_socket, &QTcpSocket::connected,
            this, &Eshare8121CommandClient::OnConnected);
    connect(m_socket, &QTcpSocket::readyRead,
            this, &Eshare8121CommandClient::OnReadyRead);
    connect(m_socket, &QTcpSocket::disconnected,
            this, &Eshare8121CommandClient::OnDisconnected);
    connect(m_socket, &QTcpSocket::errorOccurred,
            this, &Eshare8121CommandClient::OnSocketError);

    connect(m_idleTimer, &QTimer::timeout,
            this, &Eshare8121CommandClient::OnResponseIdleTimeout);
}

void Eshare8121CommandClient::SendGetServerInfo(const QString& receiverIp,
                                                quint16 port,
                                                const QString& clientName,
                                                const QString& clientVersion)
{
    StartCommand(Eshare8121Command::GetServerInfo, receiverIp, port, clientName, clientVersion);
}

void Eshare8121CommandClient::SendDongleConnected(const QString& receiverIp,
                                                  quint16 port,
                                                  const QString& clientName,
                                                  const QString& clientVersion)
{
    StartCommand(Eshare8121Command::DongleConnected, receiverIp, port, clientName, clientVersion);
}

void Eshare8121CommandClient::Abort()
{
    m_idleTimer->stop();

    if (m_socket->state() != QAbstractSocket::UnconnectedState)
    {
        m_finished = true;
        m_socket->abort();
    }
}

void Eshare8121CommandClient::ResetState()
{
    m_lastRequest.clear();
    m_recvBuffer.clear();
    m_finished = false;
    m_responseParsed = false;
    m_pendingResult = Eshare8121CommandResult{};
    m_idleTimer->stop();
}

QString Eshare8121CommandClient::CommandToString(Eshare8121Command command) const
{
    switch (command)
    {
    case Eshare8121Command::GetServerInfo:
        return "getServerInfo";
    case Eshare8121Command::DongleConnected:
        return "dongleConnected";
    default:
        return "unknown";
    }
}

QByteArray Eshare8121CommandClient::BuildRequest(Eshare8121Command command,
                                                 const QString& clientName,
                                                 const QString& clientVersion) const
{
    QByteArray req;
    req += CommandToString(command).toUtf8();
    req += "\r\n";
    req += clientName.toUtf8();
    req += "\r\n";
    req += clientVersion.toUtf8();
    req += "\r\n";
    return req;
}

void Eshare8121CommandClient::StartCommand(Eshare8121Command command,
                                           const QString& receiverIp,
                                           quint16 port,
                                           const QString& clientName,
                                           const QString& clientVersion)
{
    ResetState();

    m_command = command;
    m_receiverIp = receiverIp;
    m_port = port;
    m_lastRequest = BuildRequest(command, clientName, clientVersion);

    emit SigLog(QString("[8121] Connecting to %1:%2, command=%3")
                .arg(receiverIp)
                .arg(port)
                .arg(CommandToString(command)));

    m_socket->connectToHost(receiverIp, port);
}

void Eshare8121CommandClient::OnConnected()
{
    emit SigLog(QString("[8121] >>> SEND (%1 bytes)\n%2")
                .arg(m_lastRequest.size())
                .arg(QString::fromUtf8(m_lastRequest)));

    m_socket->write(m_lastRequest);
    m_socket->flush();
}

bool Eshare8121CommandClient::LooksLikeCompleteJson(const QByteArray& data) const
{
    const QByteArray t = data.trimmed();
    if (!(t.startsWith('{') && t.endsWith('}')))
        return false;

    QJsonParseError jsonError;
    const QJsonDocument doc = QJsonDocument::fromJson(t, &jsonError);
    return jsonError.error == QJsonParseError::NoError && doc.isObject();
}

void Eshare8121CommandClient::OnReadyRead()
{
    m_recvBuffer += m_socket->readAll();

    emit SigLog(QString("[8121] <<< BUFFER APPEND, total=%1 bytes")
                .arg(m_recvBuffer.size()));

    // 如果看起来已经是完整 JSON，可以立刻解析结束
    if (LooksLikeCompleteJson(m_recvBuffer))
    {
        emit SigLog("[8121] Response looks complete JSON, parsing immediately.");
        ParseAndFinish();
        return;
    }

    // 否则等待一个很短的静默窗口，判断响应是否收完
    m_idleTimer->start(50);
}

void Eshare8121CommandClient::OnResponseIdleTimeout()
{
    if (m_finished)
        return;

    if (m_recvBuffer.isEmpty())
        return;

    emit SigLog(QString("[8121] Response idle timeout reached, parsing %1 bytes.")
                .arg(m_recvBuffer.size()));

    ParseAndFinish();
}

void Eshare8121CommandClient::OnSocketError(QAbstractSocket::SocketError)
{
    if (!m_finished)
    {
        FinishWithError(m_socket->errorString());
    }
}

void Eshare8121CommandClient::OnDisconnected()
{
    m_idleTimer->stop();

    if (m_finished)
        return;

    // 如果响应已经解析完了，说明这是我们主动收尾后的断开
    if (m_responseParsed)
    {
        m_finished = true;
        emit SigLog("[8121] Socket disconnected, command finished.");
        emit SigFinished(m_pendingResult);
        return;
    }

    // 如果对端先断开，但我们手里已经有数据，就兜底解析
    if (!m_recvBuffer.isEmpty())
    {
        emit SigLog("[8121] Peer disconnected, parsing buffered response.");
        ParseAndFinish();
        return;
    }

    FinishWithError("Disconnected before receiving response.");
}

void Eshare8121CommandClient::FinishWithError(const QString& text)
{
    if (m_finished)
        return;

    Eshare8121CommandResult result;
    result.success = false;
    result.errorText = text;
    result.command = m_command;
    result.rawRequest = m_lastRequest;
    result.rawResponse = m_recvBuffer;
    result.requestText = QString::fromUtf8(m_lastRequest);
    result.responseText = QString::fromUtf8(m_recvBuffer);

    m_finished = true;
    m_idleTimer->stop();

    emit SigLog(QString("[8121] Error: %1").arg(text));
    emit SigFinished(result);
}

void Eshare8121CommandClient::ParseAndFinish()
{
    if (m_finished || m_responseParsed)
        return;

    m_idleTimer->stop();

    Eshare8121CommandResult result;
    result.success = true;
    result.command = m_command;
    result.rawRequest = m_lastRequest;
    result.rawResponse = m_recvBuffer;
    result.requestText = QString::fromUtf8(m_lastRequest);
    result.responseText = QString::fromUtf8(m_recvBuffer);

    const QByteArray trimmed = m_recvBuffer.trimmed();

    // 1) 尝试按 JSON 解析
    QJsonParseError jsonError;
    const QJsonDocument doc = QJsonDocument::fromJson(trimmed, &jsonError);
    if (jsonError.error == QJsonParseError::NoError && doc.isObject())
    {
        result.hasJsonBody = true;
        result.jsonBody = doc.object();
        result.jsonPrettyText = QString::fromUtf8(doc.toJson(QJsonDocument::Indented));

        if (result.jsonBody.contains("name"))
            result.deviceName = result.jsonBody.value("name").toString();

        if (result.jsonBody.contains("version"))
            result.deviceVersion = QString::number(result.jsonBody.value("version").toInt());

        if (result.jsonBody.contains("webPort"))
            result.webPort = result.jsonBody.value("webPort").toInt(-1);

        if (result.jsonBody.contains("pin"))
            result.pin = result.jsonBody.value("pin").toString();

        if (result.jsonBody.contains("airPlay"))
            result.airPlay = result.jsonBody.value("airPlay").toString();

        if (result.jsonBody.contains("airPlayFeature"))
            result.airPlayFeature = result.jsonBody.value("airPlayFeature").toString();

        if (result.jsonBody.contains("feature"))
            result.feature = result.jsonBody.value("feature").toString();

        if (result.jsonBody.contains("id"))
            result.deviceId = result.jsonBody.value("id").toString();

        emit SigLog(QString("[8121] <<< RECV (%1 bytes)\n%2")
                        .arg(result.rawResponse.size())
                        .arg(result.jsonPrettyText));
    }
    else
    {
        // 2) 普通文本行
        QString text = QString::fromUtf8(trimmed);
        text.replace("\r\n", "\n");
        result.responseLines = text.split('\n', Qt::SkipEmptyParts);

        if (result.responseLines.size() >= 1)
            result.deviceName = result.responseLines[0].trimmed();
        if (result.responseLines.size() >= 2)
            result.deviceVersion = result.responseLines[1].trimmed();

        emit SigLog(QString("[8121] <<< RECV (%1 bytes)\n%2")
                        .arg(result.rawResponse.size())
                        .arg(text));
    }

    // 关键：先缓存结果，等 disconnected 后再发 finished
    m_pendingResult = result;
    m_responseParsed = true;

    if (m_socket->state() == QAbstractSocket::ConnectedState)
    {
        emit SigLog("[8121] Response parsed, disconnecting from host.");
        m_socket->disconnectFromHost();
    }
    else
    {
        // 极端情况：已经断开了，就直接结束
        m_finished = true;
        emit SigFinished(m_pendingResult);
    }
}

} // namespace WQt::Cast::Eshare