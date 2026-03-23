#include "EshareVideoReceiver.h"

#include <QDateTime>
#include <QDir>
#include <QHostAddress>

namespace WQt::Cast::Eshare
{

EshareVideoReceiver::EshareVideoReceiver(QObject* parent)
    : QObject(parent)
{
    m_server = new QTcpServer(this);

    connect(m_server, &QTcpServer::newConnection,
            this, &EshareVideoReceiver::OnNewConnection);
}

bool EshareVideoReceiver::Start(const QString& localIp, quint16 port, bool dumpToFile)
{
    if (m_server->isListening())
    {
        emit SigLog(QStringLiteral("[51030R] Start ignored: already listening on %1:%2")
                    .arg(m_server->serverAddress().toString())
                    .arg(m_server->serverPort()));
        return true;
    }

    m_localIp = localIp;
    m_port = port;
    m_dumpToFile = dumpToFile;
    m_totalRecvBytes = 0;
    m_socketRecvBytes.clear();

    if (m_dumpToFile)
    {
        const QString fileName = QStringLiteral("eshare_video_dump_%1.bin")
            .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss")));

        m_dumpFile.setFileName(fileName);
        if (!m_dumpFile.open(QIODevice::WriteOnly))
        {
            emit SigError(QStringLiteral("[51030R] open dump file failed: %1")
                          .arg(m_dumpFile.errorString()));
            return false;
        }

        emit SigLog(QStringLiteral("[51030R] dump file opened: %1")
                    .arg(QDir::toNativeSeparators(m_dumpFile.fileName())));
    }

    QHostAddress addr = QHostAddress::AnyIPv4;
    if (!localIp.isEmpty() && localIp != QStringLiteral("0.0.0.0"))
    {
        addr = QHostAddress(localIp);
        if (addr.isNull())
        {
            emit SigError(QStringLiteral("[51030R] invalid localIp: %1").arg(localIp));
            if (m_dumpFile.isOpen())
                m_dumpFile.close();
            return false;
        }
    }

    if (!m_server->listen(addr, port))
    {
        emit SigError(QStringLiteral("[51030R] listen failed: %1")
                      .arg(m_server->errorString()));
        if (m_dumpFile.isOpen())
            m_dumpFile.close();
        return false;
    }

    emit SigLog(QStringLiteral("[51030R] listening on %1:%2")
                .arg(m_server->serverAddress().toString())
                .arg(m_server->serverPort()));
    emit SigStarted(m_server->serverPort());
    return true;
}

void EshareVideoReceiver::Stop()
{
    const auto sockets = m_socketRecvBytes.keys();
    for (QTcpSocket* socket : sockets)
    {
        CloseAndDeleteSocket(socket);
    }
    m_socketRecvBytes.clear();

    if (m_server->isListening())
    {
        emit SigLog(QStringLiteral("[51030R] stop listening."));
        m_server->close();
    }

    if (m_dumpFile.isOpen())
    {
        emit SigLog(QStringLiteral("[51030R] dump file closed: %1, total=%2 bytes")
                    .arg(QDir::toNativeSeparators(m_dumpFile.fileName()))
                    .arg(m_totalRecvBytes));
        m_dumpFile.close();
    }

    emit SigStopped();
}

void EshareVideoReceiver::OnNewConnection()
{
    while (m_server->hasPendingConnections())
    {
        QTcpSocket* socket = m_server->nextPendingConnection();
        if (!socket)
            continue;

        m_socketRecvBytes.insert(socket, 0);

        connect(socket, &QTcpSocket::readyRead,
                this, &EshareVideoReceiver::OnSocketReadyRead);
        connect(socket, &QTcpSocket::disconnected,
                this, &EshareVideoReceiver::OnSocketDisconnected);
        connect(socket, &QTcpSocket::errorOccurred,
                this, &EshareVideoReceiver::OnSocketError);

        const QString peer = PeerToString(socket);
        emit SigLog(QStringLiteral("[51030R] accepted: %1").arg(peer));
        emit SigClientConnected(peer);
    }
}

void EshareVideoReceiver::OnSocketReadyRead()
{
    auto* socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket)
        return;

    const QByteArray data = socket->readAll();
    if (data.isEmpty())
        return;

    m_totalRecvBytes += static_cast<quint64>(data.size());
    m_socketRecvBytes[socket] += static_cast<quint64>(data.size());

    if (m_dumpToFile && m_dumpFile.isOpen())
    {
        const qint64 written = m_dumpFile.write(data);
        if (written != data.size())
        {
            emit SigLog(QStringLiteral("[51030R] dump write mismatch: want=%1, written=%2")
                        .arg(data.size())
                        .arg(written));
        }
        m_dumpFile.flush();
    }

    emit SigLog(QStringLiteral("[51030R] <<< DATA from %1, chunk=%2 bytes, peerTotal=%3, allTotal=%4")
                .arg(PeerToString(socket))
                .arg(data.size())
                .arg(m_socketRecvBytes.value(socket))
                .arg(m_totalRecvBytes));

    emit SigVideoData(data);
}

void EshareVideoReceiver::OnSocketDisconnected()
{
    auto* socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket)
        return;

    const QString peer = PeerToString(socket);
    const quint64 peerBytes = m_socketRecvBytes.value(socket);

    emit SigLog(QStringLiteral("[51030R] disconnected: %1, peerTotal=%2 bytes")
                .arg(peer)
                .arg(peerBytes));
    emit SigClientDisconnected(peer);

    m_socketRecvBytes.remove(socket);
    socket->deleteLater();
}

void EshareVideoReceiver::OnSocketError(QAbstractSocket::SocketError socketError)
{
    Q_UNUSED(socketError);

    auto* socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket)
        return;

    if (socket->error() == QAbstractSocket::RemoteHostClosedError)
    {
        emit SigLog(QStringLiteral("[51030R] remote closed: %1").arg(PeerToString(socket)));
        return;
    }

    emit SigLog(QStringLiteral("[51030R] socket error from %1: %2")
                .arg(PeerToString(socket))
                .arg(socket->errorString()));
}

QString EshareVideoReceiver::PeerToString(QTcpSocket* socket) const
{
    if (!socket)
        return QStringLiteral("<null>");

    return QStringLiteral("%1:%2")
        .arg(socket->peerAddress().toString())
        .arg(socket->peerPort());
}

void EshareVideoReceiver::CloseAndDeleteSocket(QTcpSocket* socket)
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