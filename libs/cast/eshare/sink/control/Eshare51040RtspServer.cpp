#include "Eshare51040RtspServer.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QTextStream>
#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkDatagram>

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

    if (!StartUdpServices())
    {
        m_server->close();
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

    StopUdpServices();

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

void Eshare51040RtspServer::OnAudioDataReadyRead()
{
    if (!m_audioDataSocket)
        return;

    while (m_audioDataSocket->hasPendingDatagrams())
    {
        QHostAddress peerAddr;
        quint16 peerPort = 0;

        const qint64 sz = m_audioDataSocket->pendingDatagramSize();
        if (sz <= 0)
            break;

        QByteArray datagram;
        datagram.resize(static_cast<int>(sz));

        const qint64 readLen = m_audioDataSocket->readDatagram(
            datagram.data(),
            datagram.size(),
            &peerAddr,
            &peerPort);

        if (readLen <= 0)
            continue;

        if (readLen < 12)
        {
            emit SigLog(QStringLiteral("[51040S] audio udp too short: from=%1:%2 len=%3")
                            .arg(peerAddr.toString())
                            .arg(peerPort)
                            .arg(readLen));
            continue;
        }

        const uchar* p = reinterpret_cast<const uchar*>(datagram.constData());

        const quint8 version = (p[0] >> 6) & 0x03;
        const quint8 payloadType = p[1] & 0x7F;
        const quint16 seq = ReadBE16(p + 2);
        const quint32 timestamp = ReadBE32(p + 4);
        const quint32 ssrc = ReadBE32(p + 8);

        if (version != 2)
        {
            emit SigLog(QStringLiteral("[51040S] audio udp not rtp-v2: from=%1:%2 len=%3 head=%4")
                            .arg(peerAddr.toString())
                            .arg(peerPort)
                            .arg(readLen)
                            .arg(QString::fromLatin1(datagram.left(16).toHex(' ').toUpper())));
            continue;
        }

        const QByteArray payload = datagram.mid(12);

        if (m_audioRtpDumpFile.isOpen())
            m_audioRtpDumpFile.write(datagram);

        if (m_audioPayloadDumpFile.isOpen())
            m_audioPayloadDumpFile.write(payload);

        if (m_audioPayloadLenDumpFile.isOpen())
        {
            const quint32 payloadLen = static_cast<quint32>(payload.size());

            char lenBuf[4];
            lenBuf[0] = static_cast<char>(payloadLen & 0xFF);
            lenBuf[1] = static_cast<char>((payloadLen >> 8) & 0xFF);
            lenBuf[2] = static_cast<char>((payloadLen >> 16) & 0xFF);
            lenBuf[3] = static_cast<char>((payloadLen >> 24) & 0xFF);

            m_audioPayloadLenDumpFile.write(lenBuf, 4);
            m_audioPayloadLenDumpFile.write(payload);
        }

        if (m_audioDumpCsvFile.isOpen())
        {
            QTextStream ts(&m_audioDumpCsvFile);
            ts << m_audioPacketIndex << ','
               << seq << ','
               << timestamp << ','
               << ssrc << ','
               << readLen << ','
               << payload.size() << '\n';
            ts.flush();
        }

        if (m_audioPacketIndex < 20)
        {
            emit SigLog(QStringLiteral("[51040S] dump rtp[%1]: from=%2:%3 pt=%4 seq=%5 ts=%6 ssrc=0x%7 rtpLen=%8 payloadLen=%9")
                            .arg(m_audioPacketIndex)
                            .arg(peerAddr.toString())
                            .arg(peerPort)
                            .arg(payloadType)
                            .arg(seq)
                            .arg(timestamp)
                            .arg(QString::number(ssrc, 16).rightJustified(8, QLatin1Char('0')))
                            .arg(readLen)
                            .arg(payload.size()));
        }

        ++m_audioPacketIndex;
    }
}

void Eshare51040RtspServer::OnControlReadyRead()
{
    if (!m_controlSocket)
        return;

    while (m_controlSocket->hasPendingDatagrams())
    {
        const qint64 sz = m_controlSocket->pendingDatagramSize();
        if (sz <= 0)
            break;

        QByteArray datagram;
        datagram.resize(static_cast<int>(sz));
        m_controlSocket->readDatagram(datagram.data(), datagram.size());

        // 当前阶段不处理 control 数据
    }
}

void Eshare51040RtspServer::OnMouseReadyRead()
{
    if (!m_mouseSocket)
        return;

    while (m_mouseSocket->hasPendingDatagrams())
    {
        const qint64 sz = m_mouseSocket->pendingDatagramSize();
        if (sz <= 0)
            break;

        QByteArray datagram;
        datagram.resize(static_cast<int>(sz));
        m_mouseSocket->readDatagram(datagram.data(), datagram.size());

        // 当前阶段不处理 mouse 数据
    }
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

bool Eshare51040RtspServer::StartUdpServices()
{
    if (m_audioDataSocket || m_controlSocket || m_mouseSocket)
        return true;

    m_audioDataSocket = new QUdpSocket(this);
    if (!m_audioDataSocket->bind(QHostAddress::AnyIPv4, 0))
    {
        emit SigError(QStringLiteral("[51040S] bind audio data udp failed: %1")
                          .arg(m_audioDataSocket->errorString()));
        StopUdpServices();
        return false;
    }
    m_audioDataPort = static_cast<int>(m_audioDataSocket->localPort());
    connect(m_audioDataSocket, &QUdpSocket::readyRead,
            this, &Eshare51040RtspServer::OnAudioDataReadyRead);

    m_controlSocket = new QUdpSocket(this);
    if (!m_controlSocket->bind(QHostAddress::AnyIPv4, 0))
    {
        emit SigError(QStringLiteral("[51040S] bind control udp failed: %1")
                          .arg(m_controlSocket->errorString()));
        StopUdpServices();
        return false;
    }
    m_controlPort = static_cast<int>(m_controlSocket->localPort());
    connect(m_controlSocket, &QUdpSocket::readyRead,
            this, &Eshare51040RtspServer::OnControlReadyRead);

    m_mouseSocket = new QUdpSocket(this);
    if (!m_mouseSocket->bind(QHostAddress::AnyIPv4, static_cast<quint16>(m_mousePort)))
    {
        emit SigError(QStringLiteral("[51040S] bind mouse udp failed: %1")
                          .arg(m_mouseSocket->errorString()));
        StopUdpServices();
        return false;
    }
    connect(m_mouseSocket, &QUdpSocket::readyRead,
            this, &Eshare51040RtspServer::OnMouseReadyRead);

    if (!OpenAudioDumpFiles())
    {
        StopUdpServices();
        return false;
    }

    emit SigLog(QStringLiteral("[51040S] udp services ready: audioData=%1 control=%2 mouse=%3")
                    .arg(m_audioDataPort)
                    .arg(m_controlPort)
                    .arg(m_mousePort));

    return true;
}

void Eshare51040RtspServer::StopUdpServices()
{
    CloseAudioDumpFiles();

    if (m_audioDataSocket)
    {
        m_audioDataSocket->close();
        m_audioDataSocket->deleteLater();
        m_audioDataSocket = nullptr;
    }

    if (m_controlSocket)
    {
        m_controlSocket->close();
        m_controlSocket->deleteLater();
        m_controlSocket = nullptr;
    }

    if (m_mouseSocket)
    {
        m_mouseSocket->close();
        m_mouseSocket->deleteLater();
        m_mouseSocket = nullptr;
    }

    m_audioDataPort = 0;
    m_controlPort = 0;
    m_mousePort = 51050;
}

bool Eshare51040RtspServer::OpenAudioDumpFiles()
{
    if (m_audioRtpDumpFile.isOpen() &&
        m_audioPayloadDumpFile.isOpen() &&
        m_audioPayloadLenDumpFile.isOpen() &&
        m_audioDumpCsvFile.isOpen())
    {
        return true;
    }

    const QString baseDir = QCoreApplication::applicationDirPath() + "/audio_dump";
    QDir().mkpath(baseDir);

    const QString timeTag = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss");
    const QString dumpDir = baseDir + "/" + timeTag;
    QDir().mkpath(dumpDir);

    m_audioRtpDumpFile.setFileName(dumpDir + "/audio_rtp_dump.bin");
    m_audioPayloadDumpFile.setFileName(dumpDir + "/audio_payload_dump.bin");
    m_audioPayloadLenDumpFile.setFileName(dumpDir + "/audio_payload_len_dump.bin");
    m_audioDumpCsvFile.setFileName(dumpDir + "/audio_rtp_dump.csv");

    if (!m_audioRtpDumpFile.open(QIODevice::WriteOnly) ||
        !m_audioPayloadDumpFile.open(QIODevice::WriteOnly) ||
        !m_audioPayloadLenDumpFile.open(QIODevice::WriteOnly) ||
        !m_audioDumpCsvFile.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        emit SigError(QStringLiteral("[51040S] open audio dump files failed."));
        CloseAudioDumpFiles();
        return false;
    }

    QTextStream ts(&m_audioDumpCsvFile);
    ts << "index,seq,timestamp,ssrc,rtpLen,payloadLen\n";
    ts.flush();

    m_audioPacketIndex = 0;

    emit SigLog(QStringLiteral("[51040S] audio dump dir ready: %1").arg(dumpDir));
    return true;
}

void Eshare51040RtspServer::CloseAudioDumpFiles()
{
    if (m_audioRtpDumpFile.isOpen())
        m_audioRtpDumpFile.close();

    if (m_audioPayloadDumpFile.isOpen())
        m_audioPayloadDumpFile.close();

    if (m_audioPayloadLenDumpFile.isOpen())
        m_audioPayloadLenDumpFile.close();

    if (m_audioDumpCsvFile.isOpen())
        m_audioDumpCsvFile.close();

    m_audioPacketIndex = 0;
}

quint16 Eshare51040RtspServer::ReadBE16(const uchar* p)
{
    return (quint16(p[0]) << 8) | quint16(p[1]);
}

quint32 Eshare51040RtspServer::ReadBE32(const uchar* p)
{
    return (quint32(p[0]) << 24) |
           (quint32(p[1]) << 16) |
           (quint32(p[2]) << 8)  |
           quint32(p[3]);
}

} // namespace WQt::Cast::Eshare