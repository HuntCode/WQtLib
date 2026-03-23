#include "Eshare57395HeartbeatServer.h"

#include <QHostAddress>
#include <QJsonObject>

#include "EshareJsonLineCodec.h"

namespace WQt::Cast::Eshare
{

Eshare57395HeartbeatServer::Eshare57395HeartbeatServer(QObject* parent)
    : QObject(parent)
{
    m_server = new QTcpServer(this);

    connect(m_server, &QTcpServer::newConnection,
            this, &Eshare57395HeartbeatServer::OnNewConnection);
}

bool Eshare57395HeartbeatServer::Start(const QString& localIp, quint16 port)
{
    if (m_server->isListening())
    {
        emit SigLog(QStringLiteral("[57395S] Start ignored: already listening on %1:%2")
                    .arg(m_server->serverAddress().toString())
                    .arg(m_server->serverPort()));
        return true;
    }

    m_localIp = localIp;
    m_port = port;

    QHostAddress addr = QHostAddress::AnyIPv4;
    if (!localIp.isEmpty() && localIp != QStringLiteral("0.0.0.0"))
    {
        addr = QHostAddress(localIp);
        if (addr.isNull())
        {
            emit SigError(QStringLiteral("[57395S] invalid localIp: %1").arg(localIp));
            return false;
        }
    }

    if (!m_server->listen(addr, port))
    {
        emit SigError(QStringLiteral("[57395S] listen failed: %1")
                      .arg(m_server->errorString()));
        return false;
    }

    emit SigLog(QStringLiteral("[57395S] listening on %1:%2")
                .arg(m_server->serverAddress().toString())
                .arg(m_server->serverPort()));
    emit SigStarted(m_server->serverPort());
    return true;
}

void Eshare57395HeartbeatServer::Stop()
{
    const auto sockets = m_recvBuffers.keys();
    for (QTcpSocket* socket : sockets)
    {
        CloseAndDeleteSocket(socket);
    }
    m_recvBuffers.clear();

    if (m_server->isListening())
    {
        emit SigLog(QStringLiteral("[57395S] stop listening."));
        m_server->close();
    }

    emit SigStopped();
}

void Eshare57395HeartbeatServer::OnNewConnection()
{
    while (m_server->hasPendingConnections())
    {
        QTcpSocket* socket = m_server->nextPendingConnection();
        if (!socket)
            continue;

        m_recvBuffers.insert(socket, QByteArray{});

        connect(socket, &QTcpSocket::readyRead,
                this, &Eshare57395HeartbeatServer::OnSocketReadyRead);
        connect(socket, &QTcpSocket::disconnected,
                this, &Eshare57395HeartbeatServer::OnSocketDisconnected);
        connect(socket, &QTcpSocket::errorOccurred,
                this, &Eshare57395HeartbeatServer::OnSocketError);

        emit SigLog(QStringLiteral("[57395S] accepted: %1").arg(PeerToString(socket)));
    }
}

void Eshare57395HeartbeatServer::OnSocketReadyRead()
{
    auto* socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket)
        return;

    QByteArray& buffer = m_recvBuffers[socket];
    buffer += socket->readAll();

    while (true)
    {
        QByteArray rawLine;
        QJsonObject obj;
        QString error;

        if (!JsonLineCodec::TryDecode(buffer, &rawLine, obj, &error))
            break;

        emit SigLog(QStringLiteral("[57395S] <<< RECV from %1 (%2 bytes)\n%3")
                    .arg(PeerToString(socket))
                    .arg(rawLine.size())
                    .arg(JsonLineCodec::ToPrettyString(obj)));

        bool ok = false;
        const QByteArray respBytes = BuildResponse(obj, &ok);
        if (!ok)
        {
            emit SigLog(QStringLiteral("[57395S] ignore unsupported json from %1")
                        .arg(PeerToString(socket)));
            continue;
        }

        emit SigLog(QStringLiteral("[57395S] >>> SEND to %1 (%2 bytes)\n%3")
                    .arg(PeerToString(socket))
                    .arg(respBytes.size())
                    .arg(QString::fromUtf8(respBytes)));

        socket->write(respBytes);
        socket->flush();
    }
}

void Eshare57395HeartbeatServer::OnSocketDisconnected()
{
    auto* socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket)
        return;

    emit SigLog(QStringLiteral("[57395S] disconnected: %1").arg(PeerToString(socket)));

    m_recvBuffers.remove(socket);
    socket->deleteLater();
}

void Eshare57395HeartbeatServer::OnSocketError(QAbstractSocket::SocketError socketError)
{
    Q_UNUSED(socketError);

    auto* socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket)
        return;

    if (socket->error() == QAbstractSocket::RemoteHostClosedError)
    {
        emit SigLog(QStringLiteral("[57395S] remote closed: %1").arg(PeerToString(socket)));
        return;
    }

    emit SigLog(QStringLiteral("[57395S] socket error from %1: %2")
                .arg(PeerToString(socket))
                .arg(socket->errorString()));
}

QByteArray Eshare57395HeartbeatServer::BuildResponse(const QJsonObject& reqObj, bool* ok)
{
    if (ok)
        *ok = true;

    QJsonObject resp;

    // 第一条通常是 client-info
    if (reqObj.contains(QStringLiteral("clientName")))
    {
        resp.insert(QStringLiteral("replyHeartbeat"), 0);
        resp.insert(QStringLiteral("castState"), 1);
        resp.insert(QStringLiteral("mirrorMode"), 1);
        resp.insert(QStringLiteral("castMode"), 1);
        resp.insert(QStringLiteral("multiScreen"), 0);
        resp.insert(QStringLiteral("isModerator"), 0);
        resp.insert(QStringLiteral("radioMode"), 0);
        return JsonLineCodec::Encode(resp);
    }

    // 后续是 heartbeat
    if (reqObj.contains(QStringLiteral("heartbeat")))
    {
        const int hb = reqObj.value(QStringLiteral("heartbeat")).toInt(-1);

        resp.insert(QStringLiteral("replyHeartbeat"), hb);
        resp.insert(QStringLiteral("castState"), 1);
        resp.insert(QStringLiteral("mirrorMode"), 1);
        resp.insert(QStringLiteral("castMode"), 1);
        resp.insert(QStringLiteral("multiScreen"), 0);
        resp.insert(QStringLiteral("isModerator"), 0);
        resp.insert(QStringLiteral("radioMode"), 0);
        return JsonLineCodec::Encode(resp);
    }

    if (ok)
        *ok = false;

    return {};
}

QString Eshare57395HeartbeatServer::PeerToString(QTcpSocket* socket) const
{
    if (!socket)
        return QStringLiteral("<null>");

    return QStringLiteral("%1:%2")
        .arg(socket->peerAddress().toString())
        .arg(socket->peerPort());
}

void Eshare57395HeartbeatServer::CloseAndDeleteSocket(QTcpSocket* socket)
{
    if (!socket)
        return;

    socket->disconnect(this);

    if (socket->state() != QAbstractSocket::UnconnectedState)
    {
        socket->abort();
    }

    socket->deleteLater();
}

} // namespace WQt::Cast::Eshare