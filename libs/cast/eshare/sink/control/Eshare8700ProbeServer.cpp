#include "Eshare8700ProbeServer.h"

#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>

#include "EshareRtspLiteMessage.h"

namespace WQt::Cast::Eshare
{

Eshare8700ProbeServer::Eshare8700ProbeServer(QObject* parent)
    : QObject(parent)
{
    m_server = new QTcpServer(this);

    connect(m_server, &QTcpServer::newConnection,
            this, &Eshare8700ProbeServer::OnNewConnection);
}

bool Eshare8700ProbeServer::Start(const QString& localIp, quint16 port)
{
    if (m_server->isListening())
    {
        emit SigLog(QStringLiteral("[8700S] Start ignored: already listening on %1:%2")
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
            emit SigError(QStringLiteral("[8700S] invalid localIp: %1").arg(localIp));
            return false;
        }
    }

    if (!m_server->listen(addr, port))
    {
        emit SigError(QStringLiteral("[8700S] listen failed: %1")
                      .arg(m_server->errorString()));
        return false;
    }

    emit SigLog(QStringLiteral("[8700S] listening on %1:%2")
                .arg(m_server->serverAddress().toString())
                .arg(m_server->serverPort()));
    emit SigStarted(m_server->serverPort());
    return true;
}

void Eshare8700ProbeServer::Stop()
{
    const auto sockets = m_recvBuffers.keys();
    for (QTcpSocket* socket : sockets)
    {
        CloseAndDeleteSocket(socket);
    }
    m_recvBuffers.clear();

    if (m_server->isListening())
    {
        emit SigLog(QStringLiteral("[8700S] stop listening."));
        m_server->close();
    }

    emit SigStopped();
}

void Eshare8700ProbeServer::OnNewConnection()
{
    while (m_server->hasPendingConnections())
    {
        QTcpSocket* socket = m_server->nextPendingConnection();
        if (!socket)
            continue;

        m_recvBuffers.insert(socket, QByteArray{});

        connect(socket, &QTcpSocket::readyRead,
                this, &Eshare8700ProbeServer::OnSocketReadyRead);
        connect(socket, &QTcpSocket::disconnected,
                this, &Eshare8700ProbeServer::OnSocketDisconnected);
        connect(socket, &QTcpSocket::errorOccurred,
                this, &Eshare8700ProbeServer::OnSocketError);

        emit SigLog(QStringLiteral("[8700S] accepted: %1").arg(PeerToString(socket)));
    }
}

void Eshare8700ProbeServer::OnSocketReadyRead()
{
    auto* socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket)
        return;

    QByteArray& buffer = m_recvBuffers[socket];
    buffer += socket->readAll();

    RtspLiteMessage req;
    QByteArray rawReq;
    QString error;

    while (RtspLiteCodec::TryDecode(buffer, req, &rawReq, &error))
    {
        emit SigLog(QStringLiteral("[8700S] <<< RECV from %1 (%2 bytes)\n%3")
                    .arg(PeerToString(socket))
                    .arg(rawReq.size())
                    .arg(QString::fromUtf8(rawReq)));

        emit SigLog(QStringLiteral("[8700S] Parsed request:\n%1")
                    .arg(RtspLiteCodec::MessageToDebugString(req)));

        const QByteArray respBytes = BuildResponse(rawReq);

        emit SigLog(QStringLiteral("[8700S] >>> SEND to %1 (%2 bytes)\n%3")
                    .arg(PeerToString(socket))
                    .arg(respBytes.size())
                    .arg(QString::fromUtf8(respBytes)));

        socket->write(respBytes);
        socket->flush();

        socket->disconnectFromHost();
        return;
    }

    if (!error.isEmpty())
    {
        emit SigLog(QStringLiteral("[8700S] decode note: %1").arg(error));
    }
}

void Eshare8700ProbeServer::OnSocketDisconnected()
{
    auto* socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket)
        return;

    emit SigLog(QStringLiteral("[8700S] disconnected: %1").arg(PeerToString(socket)));

    m_recvBuffers.remove(socket);
    socket->deleteLater();
}

void Eshare8700ProbeServer::OnSocketError(QAbstractSocket::SocketError socketError)
{
    Q_UNUSED(socketError);

    auto* socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket)
        return;

    // RemoteHostClosedError 在这里通常是正常收尾，不当成失败
    if (socket->error() == QAbstractSocket::RemoteHostClosedError)
    {
        emit SigLog(QStringLiteral("[8700S] remote closed: %1").arg(PeerToString(socket)));
        return;
    }

    emit SigLog(QStringLiteral("[8700S] socket error from %1: %2")
                .arg(PeerToString(socket))
                .arg(socket->errorString()));
}

QByteArray Eshare8700ProbeServer::BuildResponse(const QByteArray& rawRequest)
{
    QByteArray tmp = rawRequest;

    RtspLiteMessage req;
    QByteArray rawMsg;
    QString error;
    const bool ok = RtspLiteCodec::TryDecode(tmp, req, &rawMsg, &error);

    RtspLiteMessage resp;

    if (!ok)
    {
        resp.startLine = QStringLiteral("RTSP/1.0 400 Bad Request");
        resp.setHeader(QStringLiteral("Content-Type"), QStringLiteral("application/json"));

        QJsonObject body;
        body.insert(QStringLiteral("error"), QStringLiteral("decode failed"));
        resp.body = QJsonDocument(body).toJson(QJsonDocument::Compact);
        return RtspLiteCodec::Encode(resp);
    }

    const bool isOptions = req.startLine.startsWith(QStringLiteral("OPTIONS "), Qt::CaseInsensitive);

    if (isOptions)
    {
        resp.startLine = QStringLiteral("RTSP/1.0 200 OK");
    }
    else
    {
        resp.startLine = QStringLiteral("RTSP/1.0 405 Method Not Allowed");
    }

    const QString cseq = req.headerValue(QStringLiteral("CSeq"));
    if (!cseq.isEmpty())
    {
        resp.setHeader(QStringLiteral("CSeq"), cseq);
    }

    resp.setHeader(QStringLiteral("Version"), QStringLiteral("20231026"));
    resp.setHeader(QStringLiteral("Content-Type"), QStringLiteral("application/json"));

    QJsonObject body;
    body.insert(QStringLiteral("byom_tx_avalible"), QStringLiteral("0"));
    resp.body = QJsonDocument(body).toJson(QJsonDocument::Compact);

    return RtspLiteCodec::Encode(resp);
}

QString Eshare8700ProbeServer::PeerToString(QTcpSocket* socket) const
{
    if (!socket)
        return QStringLiteral("<null>");

    return QStringLiteral("%1:%2")
        .arg(socket->peerAddress().toString())
        .arg(socket->peerPort());
}

void Eshare8700ProbeServer::CloseAndDeleteSocket(QTcpSocket* socket)
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