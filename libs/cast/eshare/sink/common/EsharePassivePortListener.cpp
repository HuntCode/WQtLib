#include "EsharePassivePortListener.h"

#include <QHostAddress>

namespace WQt::Cast::Eshare
{

EsharePassivePortListener::EsharePassivePortListener(const QString& tag, QObject* parent)
    : QObject(parent), m_tag(tag)
{
    m_server = new QTcpServer(this);

    connect(m_server, &QTcpServer::newConnection,
            this, &EsharePassivePortListener::OnNewConnection);
}

bool EsharePassivePortListener::Start(const QString& localIp, quint16 port)
{
    if (m_server->isListening())
    {
        emit SigLog(QStringLiteral("[%1] Start ignored: already listening on %2:%3")
                    .arg(m_tag)
                    .arg(m_server->serverAddress().toString())
                    .arg(m_server->serverPort()));
        return true;
    }

    m_localIp = localIp;
    m_port = port;
    m_socketRecvBytes.clear();

    QHostAddress addr = QHostAddress::AnyIPv4;
    if (!localIp.isEmpty() && localIp != QStringLiteral("0.0.0.0"))
    {
        addr = QHostAddress(localIp);
        if (addr.isNull())
        {
            emit SigError(QStringLiteral("[%1] invalid localIp: %2").arg(m_tag, localIp));
            return false;
        }
    }

    if (!m_server->listen(addr, port))
    {
        emit SigError(QStringLiteral("[%1] listen failed: %2")
                      .arg(m_tag, m_server->errorString()));
        return false;
    }

    emit SigLog(QStringLiteral("[%1] listening on %2:%3")
                .arg(m_tag)
                .arg(m_server->serverAddress().toString())
                .arg(m_server->serverPort()));
    emit SigStarted(m_server->serverPort());
    return true;
}

void EsharePassivePortListener::Stop()
{
    const auto sockets = m_socketRecvBytes.keys();
    for (QTcpSocket* socket : sockets)
    {
        CloseAndDeleteSocket(socket);
    }
    m_socketRecvBytes.clear();

    if (m_server->isListening())
    {
        emit SigLog(QStringLiteral("[%1] stop listening.").arg(m_tag));
        m_server->close();
    }

    emit SigStopped();
}

void EsharePassivePortListener::OnNewConnection()
{
    while (m_server->hasPendingConnections())
    {
        QTcpSocket* socket = m_server->nextPendingConnection();
        if (!socket)
            continue;

        m_socketRecvBytes.insert(socket, 0);

        connect(socket, &QTcpSocket::readyRead,
                this, &EsharePassivePortListener::OnSocketReadyRead);
        connect(socket, &QTcpSocket::disconnected,
                this, &EsharePassivePortListener::OnSocketDisconnected);
        connect(socket, &QTcpSocket::errorOccurred,
                this, &EsharePassivePortListener::OnSocketError);

        emit SigLog(QStringLiteral("[%1] accepted: %2").arg(m_tag, PeerToString(socket)));
    }
}

void EsharePassivePortListener::OnSocketReadyRead()
{
    auto* socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket)
        return;

    const QByteArray data = socket->readAll();
    if (data.isEmpty())
        return;

    m_socketRecvBytes[socket] += static_cast<quint64>(data.size());

    QString printable = QString::fromUtf8(data);
    bool looksText = true;
    for (char c : data)
    {
        const unsigned char uc = static_cast<unsigned char>(c);
        if (!(uc == '\r' || uc == '\n' || uc == '\t' || (uc >= 32 && uc <= 126)))
        {
            looksText = false;
            break;
        }
    }

    emit SigLog(QStringLiteral("[%1] <<< DATA from %2, chunk=%3 bytes, peerTotal=%4\n%5")
                .arg(m_tag)
                .arg(PeerToString(socket))
                .arg(data.size())
                .arg(m_socketRecvBytes.value(socket))
                .arg(looksText
                        ? printable
                        : QStringLiteral("HEX:\n%1").arg(QString::fromUtf8(data.toHex(' ')))));
}

void EsharePassivePortListener::OnSocketDisconnected()
{
    auto* socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket)
        return;

    emit SigLog(QStringLiteral("[%1] disconnected: %2, peerTotal=%3 bytes")
                .arg(m_tag)
                .arg(PeerToString(socket))
                .arg(m_socketRecvBytes.value(socket)));

    m_socketRecvBytes.remove(socket);
    socket->deleteLater();
}

void EsharePassivePortListener::OnSocketError(QAbstractSocket::SocketError socketError)
{
    Q_UNUSED(socketError);

    auto* socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket)
        return;

    if (socket->error() == QAbstractSocket::RemoteHostClosedError)
    {
        emit SigLog(QStringLiteral("[%1] remote closed: %2").arg(m_tag, PeerToString(socket)));
        return;
    }

    emit SigLog(QStringLiteral("[%1] socket error from %2: %3")
                .arg(m_tag)
                .arg(PeerToString(socket))
                .arg(socket->errorString()));
}

QString EsharePassivePortListener::PeerToString(QTcpSocket* socket) const
{
    if (!socket)
        return QStringLiteral("<null>");

    return QStringLiteral("%1:%2")
        .arg(socket->peerAddress().toString())
        .arg(socket->peerPort());
}

void EsharePassivePortListener::CloseAndDeleteSocket(QTcpSocket* socket)
{
    if (!socket)
        return;

    socket->disconnect(this);

    if (socket->state() != QAbstractSocket::UnconnectedState)
        socket->abort();

    socket->deleteLater();
}

} // namespace WQt::Cast::Eshare