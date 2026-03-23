#include "Eshare8121CommandServer.h"

#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>

namespace WQt::Cast::Eshare
{

Eshare8121CommandServer::Eshare8121CommandServer(QObject* parent)
    : QObject(parent)
{
    m_server = new QTcpServer(this);

    connect(m_server, &QTcpServer::newConnection,
            this, &Eshare8121CommandServer::OnNewConnection);
}

bool Eshare8121CommandServer::Start(const QString& localIp, quint16 port)
{
    if (m_server->isListening())
    {
        emit SigLog(QStringLiteral("[8121S] Start ignored: already listening on %1:%2")
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
            emit SigError(QStringLiteral("[8121S] invalid localIp: %1").arg(localIp));
            return false;
        }
    }

    if (!m_server->listen(addr, port))
    {
        emit SigError(QStringLiteral("[8121S] listen failed: %1")
                      .arg(m_server->errorString()));
        return false;
    }

    emit SigLog(QStringLiteral("[8121S] listening on %1:%2")
                .arg(m_server->serverAddress().toString())
                .arg(m_server->serverPort()));
    emit SigStarted(m_server->serverPort());
    return true;
}

void Eshare8121CommandServer::Stop()
{
    const auto sockets = m_recvBuffers.keys();
    for (QTcpSocket* socket : sockets)
    {
        CloseAndDeleteSocket(socket);
    }
    m_recvBuffers.clear();

    if (m_server->isListening())
    {
        emit SigLog(QStringLiteral("[8121S] stop listening."));
        m_server->close();
    }

    emit SigStopped();
}

void Eshare8121CommandServer::OnNewConnection()
{
    while (m_server->hasPendingConnections())
    {
        QTcpSocket* socket = m_server->nextPendingConnection();
        if (!socket)
            continue;

        m_recvBuffers.insert(socket, QByteArray{});

        connect(socket, &QTcpSocket::readyRead,
                this, &Eshare8121CommandServer::OnSocketReadyRead);
        connect(socket, &QTcpSocket::disconnected,
                this, &Eshare8121CommandServer::OnSocketDisconnected);
        connect(socket, &QTcpSocket::errorOccurred,
                this, &Eshare8121CommandServer::OnSocketError);

        emit SigLog(QStringLiteral("[8121S] accepted: %1").arg(PeerToString(socket)));
    }
}

void Eshare8121CommandServer::OnSocketReadyRead()
{
    auto* socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket)
        return;

    QByteArray& buffer = m_recvBuffers[socket];
    buffer += socket->readAll();

    emit SigLog(QStringLiteral("[8121S] <<< BUFFER APPEND from %1, total=%2 bytes")
                .arg(PeerToString(socket))
                .arg(buffer.size()));

    // 8121 sender 请求固定是 3 行，最后有 CRLF，简单按行数兜底即可
    QByteArray normalized = buffer;
    normalized.replace("\r\n", "\n");

    const QList<QByteArray> lines = normalized.split('\n');
    int nonEmptyCount = 0;
    for (const QByteArray& line : lines)
    {
        if (!line.trimmed().isEmpty())
            ++nonEmptyCount;
    }

    if (nonEmptyCount < 3)
        return;

    emit SigLog(QStringLiteral("[8121S] <<< RECV from %1 (%2 bytes)\n%3")
                .arg(PeerToString(socket))
                .arg(buffer.size())
                .arg(QString::fromUtf8(buffer)));

    const QByteArray respBytes = BuildResponse(buffer);

    emit SigLog(QStringLiteral("[8121S] >>> SEND to %1 (%2 bytes)\n%3")
                .arg(PeerToString(socket))
                .arg(respBytes.size())
                .arg(QString::fromUtf8(respBytes)));

    socket->write(respBytes);
    socket->flush();
    socket->disconnectFromHost();
}

void Eshare8121CommandServer::OnSocketDisconnected()
{
    auto* socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket)
        return;

    emit SigLog(QStringLiteral("[8121S] disconnected: %1").arg(PeerToString(socket)));

    m_recvBuffers.remove(socket);
    socket->deleteLater();
}

void Eshare8121CommandServer::OnSocketError(QAbstractSocket::SocketError socketError)
{
    Q_UNUSED(socketError);

    auto* socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket)
        return;

    if (socket->error() == QAbstractSocket::RemoteHostClosedError)
    {
        emit SigLog(QStringLiteral("[8121S] remote closed: %1").arg(PeerToString(socket)));
        return;
    }

    emit SigLog(QStringLiteral("[8121S] socket error from %1: %2")
                .arg(PeerToString(socket))
                .arg(socket->errorString()));
}

QByteArray Eshare8121CommandServer::BuildResponse(const QByteArray& rawRequest)
{
    QByteArray normalized = rawRequest;
    normalized.replace("\r\n", "\n");

    const QString text = QString::fromUtf8(normalized);
    const QStringList lines = text.split('\n', Qt::SkipEmptyParts);

    const QString command = (lines.size() >= 1) ? lines[0].trimmed() : QString{};
    const QString clientName = (lines.size() >= 2) ? lines[1].trimmed() : QString{};
    const QString clientVersion = (lines.size() >= 3) ? lines[2].trimmed() : QString{};

    Q_UNUSED(clientName);
    Q_UNUSED(clientVersion);

    if (command.compare(QStringLiteral("getServerInfo"), Qt::CaseInsensitive) == 0)
    {
        QJsonObject obj;
        obj.insert(QStringLiteral("name"), QStringLiteral("WQtLib EShare Sink"));
        obj.insert(QStringLiteral("version"), 20260113);
        obj.insert(QStringLiteral("webPort"), 8000);
        obj.insert(QStringLiteral("pin"), QStringLiteral("27115282"));
        obj.insert(QStringLiteral("airPlay"), QStringLiteral("CD:49:0D:D4:41:A1"));
        obj.insert(QStringLiteral("airPlayFeature"), QStringLiteral("0x527FFFF6,0x1E"));
        obj.insert(QStringLiteral("feature"), QStringLiteral("0x3001bf"));
        obj.insert(QStringLiteral("id"), QStringLiteral("EC74CD34EFEA"));
        obj.insert(QStringLiteral("rotation"), 0);
        return QJsonDocument(obj).toJson(QJsonDocument::Compact);
    }

    if (command.compare(QStringLiteral("dongleConnected"), Qt::CaseInsensitive) == 0)
    {
        QByteArray resp;
        resp += "WQtLib EShare Sink\r\n";
        resp += "3.0.1.320\r\n";
        return resp;
    }

    QJsonObject obj;
    obj.insert(QStringLiteral("error"), QStringLiteral("unknown command"));
    obj.insert(QStringLiteral("command"), command);
    return QJsonDocument(obj).toJson(QJsonDocument::Compact);
}

QString Eshare8121CommandServer::PeerToString(QTcpSocket* socket) const
{
    if (!socket)
        return QStringLiteral("<null>");

    return QStringLiteral("%1:%2")
        .arg(socket->peerAddress().toString())
        .arg(socket->peerPort());
}

void Eshare8121CommandServer::CloseAndDeleteSocket(QTcpSocket* socket)
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