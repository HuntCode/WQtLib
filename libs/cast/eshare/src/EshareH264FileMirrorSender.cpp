#include "EshareH264FileMirrorSender.h"

#include "EshareVideoPacketizer.h"

namespace WQt::Cast::Eshare
{

EshareH264FileMirrorSender::EshareH264FileMirrorSender(QObject* parent)
    : QObject(parent)
{
    m_socket = new QTcpSocket(this);

    m_frameTimer = new QTimer(this);
    m_frameTimer->setSingleShot(false);

    connect(m_socket, &QTcpSocket::connected,
            this, &EshareH264FileMirrorSender::OnConnected);
    connect(m_socket, &QTcpSocket::disconnected,
            this, &EshareH264FileMirrorSender::OnDisconnected);
    connect(m_socket, &QTcpSocket::errorOccurred,
            this, &EshareH264FileMirrorSender::OnSocketError);

    connect(m_frameTimer, &QTimer::timeout,
            this, &EshareH264FileMirrorSender::OnFrameTimer);
}

void EshareH264FileMirrorSender::Start(const QString& receiverIp,
                                       quint16 videoPort,
                                       const QString& h264FilePath,
                                       int fps,
                                       bool loop)
{
    m_receiverIp = receiverIp;
    m_videoPort = videoPort;
    m_h264FilePath = h264FilePath;
    m_fps = fps > 0 ? fps : 30;
    m_loop = loop;
    m_stopping = false;
    m_nextFrameIndex = 0;
    m_sentFrameCount = 0;
    m_configPayload.clear();
    m_accessUnits.clear();

    QString error;
    if (!LoadStream(&error))
    {
        emit SigError(QString("[H264FILE] %1").arg(error));
        return;
    }

    emit SigLog(QString("[H264FILE] stream prepared: config=%1 bytes, accessUnits=%2, fps=%3")
                .arg(m_configPayload.size())
                .arg(m_accessUnits.size())
                .arg(m_fps));

    emit SigLog(QString("[H264FILE] Connecting to %1:%2")
                .arg(m_receiverIp)
                .arg(m_videoPort));

    m_socket->connectToHost(m_receiverIp, m_videoPort);
}

bool EshareH264FileMirrorSender::LoadStream(QString* error)
{
    return EshareH264AnnexB::LoadFileAndBuild(m_h264FilePath, m_configPayload, m_accessUnits, error);
}

void EshareH264FileMirrorSender::Stop()
{
    m_stopping = true;
    m_frameTimer->stop();

    if (m_socket->state() != QAbstractSocket::UnconnectedState)
    {
        emit SigLog("[H264FILE] Disconnecting...");
        m_socket->disconnectFromHost();
    }
    else
    {
        emit SigStopped();
    }
}

void EshareH264FileMirrorSender::OnConnected()
{
    emit SigLog("[H264FILE] Connected to videoDataPort.");
    emit SigConnected();

    SendConfigAndFirstFrame();

    const int intervalMs = 1000 / (m_fps > 0 ? m_fps : 30);
    m_frameTimer->start(intervalMs > 0 ? intervalMs : 33);
}

void EshareH264FileMirrorSender::SendConfigAndFirstFrame()
{
    if (m_configPayload.isEmpty() || m_accessUnits.isEmpty())
    {
        emit SigError("[H264FILE] Empty config or access units.");
        return;
    }

    // 抓包里 SPS/PPS 和第一帧 IDR 使用同一 timestamp
    const quint64 ts = EshareVideoPacketizer::MakeTimestamp32_32ByFrameIndex(0, m_fps);

    QByteArray configPacket = EshareVideoPacketizer::Pack(
        EshareVideoUnitKind::Config,
        ts,
        m_configPayload
    );

    if (!SendPacket(configPacket))
    {
        emit SigError("[H264FILE] Send config packet failed.");
        return;
    }

    emit SigLog(QString("[H264FILE] sent config packet: payload=%1").arg(m_configPayload.size()));

    // 第一帧立刻发
    const H264AccessUnit& au = m_accessUnits[0];
    QByteArray framePacket = EshareVideoPacketizer::Pack(
        EshareVideoUnitKind::Frame,
        ts,
        au.payload
    );

    if (!SendPacket(framePacket))
    {
        emit SigError("[H264FILE] Send first frame packet failed.");
        return;
    }

    emit SigLog(QString("[H264FILE] sent first frame: payload=%1, idr=%2")
                .arg(au.payload.size())
                .arg(au.isIdr ? 1 : 0));

    m_nextFrameIndex = 1;
    m_sentFrameCount = 1;
}

void EshareH264FileMirrorSender::OnFrameTimer()
{
    SendNextFrame();
}

void EshareH264FileMirrorSender::SendNextFrame()
{
    if (m_accessUnits.isEmpty())
        return;

    if (m_nextFrameIndex >= m_accessUnits.size())
    {
        if (!m_loop)
        {
            emit SigLog("[H264FILE] end of file reached.");
            Stop();
            return;
        }

        emit SigLog("[H264FILE] loop restart.");

        m_nextFrameIndex = 0;
        m_sentFrameCount = 0;
        SendConfigAndFirstFrame();
        return;
    }

    const H264AccessUnit& au = m_accessUnits[m_nextFrameIndex];

    const quint64 ts = EshareVideoPacketizer::MakeTimestamp32_32ByFrameIndex(
        m_sentFrameCount,
        m_fps
    );

    QByteArray framePacket = EshareVideoPacketizer::Pack(
        EshareVideoUnitKind::Frame,
        ts,
        au.payload
    );

    if (!SendPacket(framePacket))
    {
        emit SigError(QString("[H264FILE] Send frame packet failed at index=%1").arg(m_nextFrameIndex));
        return;
    }

    emit SigLog(QString("[H264FILE] sent frame[%1]: payload=%2, idr=%3")
                .arg(m_nextFrameIndex)
                .arg(au.payload.size())
                .arg(au.isIdr ? 1 : 0));

    ++m_nextFrameIndex;
    ++m_sentFrameCount;
}

bool EshareH264FileMirrorSender::SendPacket(const QByteArray& packet)
{
    if (m_socket->state() != QAbstractSocket::ConnectedState)
        return false;

    const qint64 written = m_socket->write(packet);
    m_socket->flush();
    return written == packet.size();
}

void EshareH264FileMirrorSender::OnDisconnected()
{
    m_frameTimer->stop();
    emit SigLog("[H264FILE] Disconnected.");
    emit SigStopped();
}

void EshareH264FileMirrorSender::OnSocketError(QAbstractSocket::SocketError)
{
    if (m_stopping)
        return;

    emit SigError(QString("[H264FILE] %1").arg(m_socket->errorString()));
}

} // namespace WQt::Cast::Eshare