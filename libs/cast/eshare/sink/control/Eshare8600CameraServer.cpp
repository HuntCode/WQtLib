#include "Eshare8600CameraServer.h"

#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>

namespace WQt::Cast::Eshare
{

Eshare8600CameraServer::Eshare8600CameraServer(QObject* parent)
    : QObject(parent)
{
    m_server = new QTcpServer(this);

    connect(m_server, &QTcpServer::newConnection,
            this, &Eshare8600CameraServer::OnNewConnection);
}

bool Eshare8600CameraServer::Start(const QString& localIp, quint16 port)
{
    if (m_server->isListening())
    {
        emit SigLog(QStringLiteral("[8600S] Start ignored: already listening on %1:%2")
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
            emit SigError(QStringLiteral("[8600S] invalid localIp: %1").arg(localIp));
            return false;
        }
    }

    if (!m_server->listen(addr, port))
    {
        emit SigError(QStringLiteral("[8600S] listen failed: %1")
                      .arg(m_server->errorString()));
        return false;
    }

    emit SigLog(QStringLiteral("[8600S] listening on %1:%2")
                .arg(m_server->serverAddress().toString())
                .arg(m_server->serverPort()));
    emit SigStarted(m_server->serverPort());
    return true;
}

void Eshare8600CameraServer::Stop()
{
    const auto sockets = m_recvBuffers.keys();
    for (QTcpSocket* socket : sockets)
    {
        CloseAndDeleteSocket(socket);
    }
    m_recvBuffers.clear();

    if (m_server->isListening())
    {
        emit SigLog(QStringLiteral("[8600S] stop listening."));
        m_server->close();
    }

    emit SigStopped();
}

void Eshare8600CameraServer::OnNewConnection()
{
    while (m_server->hasPendingConnections())
    {
        QTcpSocket* socket = m_server->nextPendingConnection();
        if (!socket)
            continue;

        m_recvBuffers.insert(socket, QByteArray{});

        connect(socket, &QTcpSocket::readyRead,
                this, &Eshare8600CameraServer::OnSocketReadyRead);
        connect(socket, &QTcpSocket::disconnected,
                this, &Eshare8600CameraServer::OnSocketDisconnected);
        connect(socket, &QTcpSocket::errorOccurred,
                this, &Eshare8600CameraServer::OnSocketError);

        emit SigLog(QStringLiteral("[8600S] accepted: %1").arg(PeerToString(socket)));
    }
}

void Eshare8600CameraServer::OnSocketReadyRead()
{
    auto* socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket)
        return;

    QByteArray& buffer = m_recvBuffers[socket];
    buffer += socket->readAll();

    emit SigLog(QStringLiteral("[8600S] <<< BUFFER APPEND from %1, total=%2 bytes")
                .arg(PeerToString(socket))
                .arg(buffer.size()));

    // 8600 命令都很短，以 CRLF 结束，按行处理即可
    while (true)
    {
        int pos = buffer.indexOf("\r\n");
        int sepLen = 2;
        if (pos < 0)
        {
            pos = buffer.indexOf('\n');
            sepLen = 1;
        }

        if (pos < 0)
            break;

        QByteArray rawReq = buffer.left(pos + sepLen);
        buffer.remove(0, pos + sepLen);

        emit SigLog(QStringLiteral("[8600S] <<< RECV from %1 (%2 bytes)\n%3")
                    .arg(PeerToString(socket))
                    .arg(rawReq.size())
                    .arg(QString::fromUtf8(rawReq)));

        bool ok = false;
        const QByteArray respBytes = BuildResponse(rawReq, &ok);
        if (!ok)
        {
            emit SigLog(QStringLiteral("[8600S] ignore unsupported command from %1")
                        .arg(PeerToString(socket)));
            continue;
        }

        emit SigLog(QStringLiteral("[8600S] >>> SEND to %1 (%2 bytes)\n%3")
                    .arg(PeerToString(socket))
                    .arg(respBytes.size())
                    .arg(QString::fromUtf8(respBytes)));

        socket->write(respBytes);
        socket->flush();
    }
}

void Eshare8600CameraServer::OnSocketDisconnected()
{
    auto* socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket)
        return;

    emit SigLog(QStringLiteral("[8600S] disconnected: %1").arg(PeerToString(socket)));

    m_recvBuffers.remove(socket);
    socket->deleteLater();
}

void Eshare8600CameraServer::OnSocketError(QAbstractSocket::SocketError socketError)
{
    Q_UNUSED(socketError);

    auto* socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket)
        return;

    if (socket->error() == QAbstractSocket::RemoteHostClosedError)
    {
        emit SigLog(QStringLiteral("[8600S] remote closed: %1").arg(PeerToString(socket)));
        return;
    }

    emit SigLog(QStringLiteral("[8600S] socket error from %1: %2")
                .arg(PeerToString(socket))
                .arg(socket->errorString()));
}

QByteArray Eshare8600CameraServer::BuildResponse(const QByteArray& rawRequest, bool* ok)
{
    if (ok)
        *ok = true;

    QString cmd = QString::fromUtf8(rawRequest).trimmed();

    if (cmd.compare(QStringLiteral("CameraAvailabilityCheck"), Qt::CaseInsensitive) == 0)
    {
        // sender 侧会直接 toInt()，所以返回纯数字文本即可
        return QByteArray("0\r\n");
    }

    if (cmd.compare(QStringLiteral("CameraStateCheck"), Qt::CaseInsensitive) == 0)
    {
        QJsonObject obj;
        obj.insert(QStringLiteral("replayStateCheck"), QStringLiteral("1"));
        obj.insert(QStringLiteral("currentID"), 0);
        obj.insert(QStringLiteral("count"), 0);

        QByteArray resp = QJsonDocument(obj).toJson(QJsonDocument::Compact);
        resp += "\r\n";
        return resp;
    }

    if (ok)
        *ok = false;

    return {};
}

QString Eshare8600CameraServer::PeerToString(QTcpSocket* socket) const
{
    if (!socket)
        return QStringLiteral("<null>");

    return QStringLiteral("%1:%2")
        .arg(socket->peerAddress().toString())
        .arg(socket->peerPort());
}

void Eshare8600CameraServer::CloseAndDeleteSocket(QTcpSocket* socket)
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