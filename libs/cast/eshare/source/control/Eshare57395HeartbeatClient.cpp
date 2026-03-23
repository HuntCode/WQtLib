#include "Eshare57395HeartbeatClient.h"
#include "EshareJsonLineCodec.h"

#include <QJsonDocument>

namespace WQt::Cast::Eshare
{

Eshare57395HeartbeatClient::Eshare57395HeartbeatClient(QObject* parent)
    : QObject(parent)
{
    m_socket = new QTcpSocket(this);
    m_heartbeatTimer = new QTimer(this);
    m_heartbeatTimer->setSingleShot(false);

    connect(m_socket, &QTcpSocket::connected,
            this, &Eshare57395HeartbeatClient::OnConnected);
    connect(m_socket, &QTcpSocket::readyRead,
            this, &Eshare57395HeartbeatClient::OnReadyRead);
    connect(m_socket, &QTcpSocket::disconnected,
            this, &Eshare57395HeartbeatClient::OnDisconnected);
    connect(m_socket, &QTcpSocket::errorOccurred,
            this, &Eshare57395HeartbeatClient::OnSocketError);

    connect(m_heartbeatTimer, &QTimer::timeout,
            this, &Eshare57395HeartbeatClient::OnHeartbeatTimeout);
}

void Eshare57395HeartbeatClient::Start(const QString& receiverIp,
                                       quint16 port,
                                       const Eshare57395ClientInfo& clientInfo,
                                       int heartbeatIntervalMs)
{
    ResetState();

    m_receiverIp = receiverIp;
    m_port = port;
    m_clientInfo = clientInfo;
    m_heartbeatIntervalMs = heartbeatIntervalMs;

    emit SigLog(QString("[57395] Connecting to %1:%2").arg(receiverIp).arg(port));
    m_socket->connectToHost(receiverIp, port);
}

void Eshare57395HeartbeatClient::Stop()
{
    m_stopping = true;
    m_heartbeatTimer->stop();

    if (m_socket->state() != QAbstractSocket::UnconnectedState)
    {
        emit SigLog("[57395] Disconnecting...");
        m_socket->disconnectFromHost();
    }
    else
    {
        emit SigStopped();
    }
}

void Eshare57395HeartbeatClient::ResetState()
{
    m_recvBuffer.clear();
    m_nextHeartbeat = 1;
    m_clientInfoAcked = false;
    m_stopping = false;
    m_heartbeatTimer->stop();
}

void Eshare57395HeartbeatClient::OnConnected()
{
    emit SigLog("[57395] Connected.");
    SendClientInfo();
}

void Eshare57395HeartbeatClient::SendClientInfo()
{
    QJsonObject obj;
    obj.insert("clientName", m_clientInfo.clientName);
    obj.insert("clientType", m_clientInfo.clientType);
    obj.insert("isTouchable", m_clientInfo.isTouchable);
    obj.insert("reportClientInfo", m_clientInfo.reportClientInfo);
    obj.insert("versionCode", m_clientInfo.versionCode);
    obj.insert("versionName", m_clientInfo.versionName);

    const QByteArray payload = JsonLineCodec::Encode(obj);

    emit SigLog(QString("[57395] >>> SEND client-info (%1 bytes)\n%2")
                .arg(payload.size())
                .arg(QString::fromUtf8(payload)));

    m_socket->write(payload);
    m_socket->flush();
}

void Eshare57395HeartbeatClient::SendHeartbeat()
{
    QJsonObject obj;
    obj.insert("heartbeat", m_nextHeartbeat);

    const QByteArray payload = JsonLineCodec::Encode(obj);

    emit SigLog(QString("[57395] >>> SEND heartbeat=%1 (%2 bytes)\n%3")
                .arg(m_nextHeartbeat)
                .arg(payload.size())
                .arg(QString::fromUtf8(payload)));

    m_socket->write(payload);
    m_socket->flush();

    ++m_nextHeartbeat;
}

void Eshare57395HeartbeatClient::OnHeartbeatTimeout()
{
    SendHeartbeat();
}

void Eshare57395HeartbeatClient::OnReadyRead()
{
    m_recvBuffer += m_socket->readAll();
    TryParseIncoming();
}

void Eshare57395HeartbeatClient::TryParseIncoming()
{
    while (true)
    {
        QByteArray rawLine;
        QJsonObject obj;
        QString error;

        if (!JsonLineCodec::TryDecode(m_recvBuffer, &rawLine, obj, &error))
            break;

        Eshare57395State state = BuildStateFromJson(rawLine, obj);

        emit SigLog(QString("[57395] <<< RECV (%1 bytes)\n%2")
                    .arg(rawLine.size())
                    .arg(state.prettyText));

        if (!m_clientInfoAcked)
        {
            m_clientInfoAcked = true;
            state.success = true;
            emit SigClientInfoAck(state);

            emit SigLog(QString("[57395] Client-info ACK received. Heartbeat timer started: %1 ms")
                        .arg(m_heartbeatIntervalMs));
            m_heartbeatTimer->start(m_heartbeatIntervalMs);
        }
        else
        {
            state.success = true;
            emit SigHeartbeatState(state);
        }
    }
}

Eshare57395State Eshare57395HeartbeatClient::BuildStateFromJson(const QByteArray& rawLine,
                                                                const QJsonObject& obj) const
{
    Eshare57395State state;
    state.success = true;
    state.rawLine = rawLine;
    state.lineText = QString::fromUtf8(rawLine);
    state.prettyText = JsonLineCodec::ToPrettyString(obj);
    state.json = obj;

    if (obj.contains("replyHeartbeat"))
        state.replyHeartbeat = obj.value("replyHeartbeat").toInt(-1);

    if (obj.contains("castState"))
        state.castState = obj.value("castState").toInt(-1);

    if (obj.contains("mirrorMode"))
        state.mirrorMode = obj.value("mirrorMode").toInt(-1);

    if (obj.contains("castMode"))
        state.castMode = obj.value("castMode").toInt(-1);

    if (obj.contains("multiScreen"))
        state.multiScreen = obj.value("multiScreen").toInt(-1);

    if (obj.contains("isModerator"))
        state.isModerator = obj.value("isModerator").toInt(-1);

    if (obj.contains("radioMode"))
        state.radioMode = obj.value("radioMode").toInt(-1);

    return state;
}

void Eshare57395HeartbeatClient::FinishWithError(const QString& text)
{
    Eshare57395State state;
    state.success = false;
    state.errorText = text;

    emit SigLog(QString("[57395] Error: %1").arg(text));

    if (!m_clientInfoAcked)
        emit SigClientInfoAck(state);
    else
        emit SigHeartbeatState(state);
}

void Eshare57395HeartbeatClient::OnSocketError(QAbstractSocket::SocketError)
{
    if (m_stopping)
        return;

    FinishWithError(m_socket->errorString());
}

void Eshare57395HeartbeatClient::OnDisconnected()
{
    m_heartbeatTimer->stop();

    if (!m_stopping)
    {
        emit SigLog("[57395] Disconnected.");
    }

    emit SigStopped();
}

} // namespace WQt::Cast::Eshare