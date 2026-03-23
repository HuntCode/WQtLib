#include "Eshare51040RtspServer.h"

#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>

#include <plist/plist.h>

#include "EshareRtspLiteMessage.h"

namespace WQt::Cast::Eshare
{

Eshare51040RtspServer::Eshare51040RtspServer(QObject* parent)
    : QObject(parent)
{
    m_server = new QTcpServer(this);

    connect(m_server, &QTcpServer::newConnection,
            this, &Eshare51040RtspServer::OnNewConnection);
}

bool Eshare51040RtspServer::Start(const QString& localIp, quint16 port)
{
    if (m_server->isListening())
    {
        emit SigLog(QStringLiteral("[51040S] Start ignored: already listening on %1:%2")
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
            emit SigError(QStringLiteral("[51040S] invalid localIp: %1").arg(localIp));
            return false;
        }
    }

    if (!m_server->listen(addr, port))
    {
        emit SigError(QStringLiteral("[51040S] listen failed: %1")
                      .arg(m_server->errorString()));
        return false;
    }

    emit SigLog(QStringLiteral("[51040S] listening on %1:%2")
                .arg(m_server->serverAddress().toString())
                .arg(m_server->serverPort()));
    emit SigStarted(m_server->serverPort());
    return true;
}

void Eshare51040RtspServer::Stop()
{
    const auto sockets = m_recvBuffers.keys();
    for (QTcpSocket* socket : sockets)
    {
        CloseAndDeleteSocket(socket);
    }
    m_recvBuffers.clear();

    if (m_server->isListening())
    {
        emit SigLog(QStringLiteral("[51040S] stop listening."));
        m_server->close();
    }

    emit SigStopped();
}

void Eshare51040RtspServer::OnNewConnection()
{
    while (m_server->hasPendingConnections())
    {
        QTcpSocket* socket = m_server->nextPendingConnection();
        if (!socket)
            continue;

        m_recvBuffers.insert(socket, QByteArray{});

        connect(socket, &QTcpSocket::readyRead,
                this, &Eshare51040RtspServer::OnSocketReadyRead);
        connect(socket, &QTcpSocket::disconnected,
                this, &Eshare51040RtspServer::OnSocketDisconnected);
        connect(socket, &QTcpSocket::errorOccurred,
                this, &Eshare51040RtspServer::OnSocketError);

        emit SigLog(QStringLiteral("[51040S] accepted: %1").arg(PeerToString(socket)));
    }
}

static QString ToReadableRtspMessage(const QByteArray& encoded)
{
    QByteArray tmp = encoded;

    WQt::Cast::Eshare::RtspLiteMessage msg;
    QByteArray raw;
    QString error;
    if (WQt::Cast::Eshare::RtspLiteCodec::TryDecode(tmp, msg, &raw, &error))
    {
        return WQt::Cast::Eshare::RtspLiteCodec::MessageToDebugString(msg);
    }

    return QStringLiteral("<decode failed: %1>\nHEX:\n%2")
        .arg(error, QString::fromUtf8(encoded.toHex(' ')));
}

void Eshare51040RtspServer::OnSocketReadyRead()
{
    auto* socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket)
        return;

    QByteArray& buffer = m_recvBuffers[socket];
    buffer += socket->readAll();

    while (true)
    {
        RtspLiteMessage req;
        QByteArray rawReq;
        QString error;

        if (!RtspLiteCodec::TryDecode(buffer, req, &rawReq, &error))
            break;

        emit SigLog(QStringLiteral("[51040S] <<< RECV from %1 (%2 bytes)\n%3")
                    .arg(PeerToString(socket))
                    .arg(rawReq.size())
                    .arg(RtspLiteCodec::MessageToDebugString(req)));

        bool ok = false;
        const QByteArray respBytes = BuildResponse(rawReq, &ok);
        if (!ok)
        {
            emit SigLog(QStringLiteral("[51040S] ignore unsupported request from %1")
                        .arg(PeerToString(socket)));
            continue;
        }

        emit SigLog(QStringLiteral("[51040S] >>> SEND to %1 (%2 bytes)\n%3")
                        .arg(PeerToString(socket))
                        .arg(respBytes.size())
                        .arg(ToReadableRtspMessage(respBytes)));

        socket->write(respBytes);
        socket->flush();
    }
}

void Eshare51040RtspServer::OnSocketDisconnected()
{
    auto* socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket)
        return;

    emit SigLog(QStringLiteral("[51040S] disconnected: %1").arg(PeerToString(socket)));

    m_recvBuffers.remove(socket);
    socket->deleteLater();
}

void Eshare51040RtspServer::OnSocketError(QAbstractSocket::SocketError socketError)
{
    Q_UNUSED(socketError);

    auto* socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket)
        return;

    if (socket->error() == QAbstractSocket::RemoteHostClosedError)
    {
        emit SigLog(QStringLiteral("[51040S] remote closed: %1").arg(PeerToString(socket)));
        return;
    }

    emit SigLog(QStringLiteral("[51040S] socket error from %1: %2")
                .arg(PeerToString(socket))
                .arg(socket->errorString()));
}

QByteArray Eshare51040RtspServer::BuildResponse(const QByteArray& rawRequest, bool* ok)
{
    if (ok)
        *ok = true;

    QByteArray tmp = rawRequest;

    RtspLiteMessage req;
    QByteArray rawMsg;
    QString error;
    const bool decoded = RtspLiteCodec::TryDecode(tmp, req, &rawMsg, &error);
    if (!decoded)
    {
        if (ok)
            *ok = false;
        return {};
    }

    RtspLiteMessage resp;
    const QString cseq = req.headerValue(QStringLiteral("CSeq"));

    const bool isSetup = req.startLine.startsWith(QStringLiteral("SETUP "), Qt::CaseInsensitive);
    const bool isOptions = req.startLine.startsWith(QStringLiteral("OPTIONS "), Qt::CaseInsensitive);
    const bool isTeardown = req.startLine.startsWith(QStringLiteral("TEARDOWN "), Qt::CaseInsensitive);

    if (isSetup)
    {
        // sender 侧当前顺序是：CSeq=0 的 video setup，CSeq=1 的 audio setup
        const bool isVideoSetup =
            (cseq == QStringLiteral("0")) ||
            !req.headerValue(QStringLiteral("VideoAspectRatio")).isEmpty();

        resp.startLine = QStringLiteral("RTSP/1.0 200 OK");
        if (!cseq.isEmpty())
            resp.setHeader(QStringLiteral("CSeq"), cseq);
        resp.setHeader(QStringLiteral("Content-Type"),
                       QStringLiteral("application/x-apple-binary-plist"));

        if (isVideoSetup)
        {
            resp.body = BuildVideoSetupPlist();
        }
        else
        {
            resp.body = BuildAudioSetupPlist();
        }

        return RtspLiteCodec::Encode(resp);
    }

    if (isOptions)
    {
        resp.startLine = QStringLiteral("RTSP/1.0 200 OK");
        if (!cseq.isEmpty())
            resp.setHeader(QStringLiteral("CSeq"), cseq);

        // sender 侧会读取这个头
        resp.setHeader(QStringLiteral("Video-Audio"), QStringLiteral("1"));
        resp.setHeader(QStringLiteral("Content-Type"), QStringLiteral("application/json"));
        resp.body = BuildOptionsJson();
        return RtspLiteCodec::Encode(resp);
    }

    if (isTeardown)
    {
        resp.startLine = QStringLiteral("RTSP/1.0 200 OK");
        if (!cseq.isEmpty())
            resp.setHeader(QStringLiteral("CSeq"), cseq);
        resp.body.clear();
        return RtspLiteCodec::Encode(resp);
    }

    if (ok)
        *ok = false;
    return {};
}

QByteArray Eshare51040RtspServer::BuildVideoSetupPlist() const
{
    plist_t root = plist_new_dict();

    // streams = [ { type=110, dataPort=51030 } ]
    plist_t streams = plist_new_array();
    plist_t streamItem = plist_new_dict();

    plist_dict_set_item(streamItem, "type",
                        plist_new_uint(static_cast<uint64_t>(110)));
    plist_dict_set_item(streamItem, "dataPort",
                        plist_new_uint(static_cast<uint64_t>(m_videoDataPort)));

    plist_array_append_item(streams, streamItem);
    plist_dict_set_item(root, "streams", streams);

    // 其余字段按抓包里的类型来
    plist_dict_set_item(root, "feature",
                        plist_new_string(m_feature.toUtf8().constData()));

    plist_dict_set_item(root, "Framerate",
                        plist_new_string(QString::number(m_framerate).toUtf8().constData()));

    plist_dict_set_item(root, "casting_win_width",
                        plist_new_string(QString::number(m_castingWidth).toUtf8().constData()));

    plist_dict_set_item(root, "casting_win_height",
                        plist_new_string(QString::number(m_castingHeight).toUtf8().constData()));

    plist_dict_set_item(root, "format",
                        plist_new_string(m_videoFormat.toUtf8().constData()));

    char* bin = nullptr;
    uint32_t len = 0;
    plist_to_bin(root, &bin, &len);

    QByteArray out;
    if (bin && len > 0)
        out = QByteArray(bin, static_cast<int>(len));

    if (bin)
        plist_mem_free(bin);
    plist_free(root);

    return out;
}

QByteArray Eshare51040RtspServer::BuildAudioSetupPlist() const
{
    plist_t root = plist_new_dict();

    // streams = [ { type=96, dataPort=..., controlPort=..., mousePort=... } ]
    plist_t streams = plist_new_array();
    plist_t streamItem = plist_new_dict();

    plist_dict_set_item(streamItem, "type",
                        plist_new_uint(static_cast<uint64_t>(96)));
    plist_dict_set_item(streamItem, "dataPort",
                        plist_new_uint(static_cast<uint64_t>(m_audioDataPort)));
    plist_dict_set_item(streamItem, "controlPort",
                        plist_new_uint(static_cast<uint64_t>(m_controlPort)));
    plist_dict_set_item(streamItem, "mousePort",
                        plist_new_uint(static_cast<uint64_t>(m_mousePort)));

    plist_array_append_item(streams, streamItem);
    plist_dict_set_item(root, "streams", streams);

    char* bin = nullptr;
    uint32_t len = 0;
    plist_to_bin(root, &bin, &len);

    QByteArray out;
    if (bin && len > 0)
        out = QByteArray(bin, static_cast<int>(len));

    if (bin)
        plist_mem_free(bin);
    plist_free(root);

    return out;
}

QByteArray Eshare51040RtspServer::BuildOptionsJson() const
{
    QJsonObject obj;

    // sender 侧这里是 value.toString().toInt()，所以这些值必须放字符串
    obj.insert(QStringLiteral("Framerate"), QString::number(m_framerate));
    obj.insert(QStringLiteral("casting_win_width"), QString::number(m_castingWidth));
    obj.insert(QStringLiteral("casting_win_height"), QString::number(m_castingHeight));
    obj.insert(QStringLiteral("idr_req"), QStringLiteral("1"));
    obj.insert(QStringLiteral("bitrate"), QStringLiteral("8000000"));
    obj.insert(QStringLiteral("i-interval"), QStringLiteral("60"));
    obj.insert(QStringLiteral("Castnum"), QStringLiteral("1"));
    obj.insert(QStringLiteral("exclusive_screen"), QStringLiteral("0"));
    obj.insert(QStringLiteral("feature"), QStringLiteral("0"));

    return QJsonDocument(obj).toJson(QJsonDocument::Compact);
}

QString Eshare51040RtspServer::PeerToString(QTcpSocket* socket) const
{
    if (!socket)
        return QStringLiteral("<null>");

    return QStringLiteral("%1:%2")
        .arg(socket->peerAddress().toString())
        .arg(socket->peerPort());
}

void Eshare51040RtspServer::CloseAndDeleteSocket(QTcpSocket* socket)
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