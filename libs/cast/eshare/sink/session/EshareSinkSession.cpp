#include <QDateTime>
#include <QDir>

#include "EshareSinkSession.h"

#include "Eshare8700ProbeServer.h"
#include "Eshare8121CommandServer.h"
#include "Eshare57395HeartbeatServer.h"
#include "Eshare8600CameraServer.h"
#include "Eshare51040RtspServer.h"
#include "EshareVideoReceiver.h"
#include "EshareVideoDepacketizer.h"
#include "EsharePassivePortListener.h"

namespace WQt::Cast::Eshare
{

EshareSinkSession::EshareSinkSession(QObject* parent)
    : QObject(parent)
{
    m_probe8700 = new Eshare8700ProbeServer(this);
    m_cmd8121 = new Eshare8121CommandServer(this);
    m_heartbeat57395 = new Eshare57395HeartbeatServer(this);
    m_camera8600 = new Eshare8600CameraServer(this);
    m_rtsp51040 = new Eshare51040RtspServer(this);
    m_videoReceiver = new EshareVideoReceiver(this);
    m_videoDepacketizer = new EshareVideoDepacketizer(this);
    m_52020Stub = new EsharePassivePortListener(QStringLiteral("52020S"), this);
    m_52025Stub = new EsharePassivePortListener(QStringLiteral("52025S"), this);
    m_52030Stub = new EsharePassivePortListener(QStringLiteral("52030S"), this);

    connect(m_probe8700, &Eshare8700ProbeServer::SigLog,
            this, &EshareSinkSession::SigLog);

    connect(m_probe8700, &Eshare8700ProbeServer::SigError,
            this, [this](const QString& text) {
                emit SigError(QStringLiteral("[SINK] 8700 failed: %1").arg(text));
            });

    connect(m_cmd8121, &Eshare8121CommandServer::SigLog,
            this, &EshareSinkSession::SigLog);

    connect(m_cmd8121, &Eshare8121CommandServer::SigError,
            this, [this](const QString& text) {
                emit SigError(QStringLiteral("[SINK] 8121 failed: %1").arg(text));
            });

    connect(m_heartbeat57395, &Eshare57395HeartbeatServer::SigLog,
            this, &EshareSinkSession::SigLog);

    connect(m_heartbeat57395, &Eshare57395HeartbeatServer::SigError,
            this, [this](const QString& text) {
                emit SigError(QStringLiteral("[SINK] 57395 failed: %1").arg(text));
            });

    connect(m_camera8600, &Eshare8600CameraServer::SigLog,
            this, &EshareSinkSession::SigLog);

    connect(m_camera8600, &Eshare8600CameraServer::SigError,
            this, [this](const QString& text) {
                emit SigError(QStringLiteral("[SINK] 8600 failed: %1").arg(text));
            });

    connect(m_rtsp51040, &Eshare51040RtspServer::SigLog,
            this, &EshareSinkSession::SigLog);

    connect(m_rtsp51040, &Eshare51040RtspServer::SigError,
            this, [this](const QString& text) {
                emit SigError(QStringLiteral("[SINK] 51040 failed: %1").arg(text));
            });

    connect(m_videoReceiver, &EshareVideoReceiver::SigLog,
            this, &EshareSinkSession::SigLog);

    connect(m_videoReceiver, &EshareVideoReceiver::SigError,
            this, [this](const QString& text) {
                emit SigError(QStringLiteral("[SINK] 51030 failed: %1").arg(text));
            });

    connect(m_videoReceiver, &EshareVideoReceiver::SigVideoData,
           m_videoDepacketizer, &EshareVideoDepacketizer::PushBytes);

    connect(m_videoDepacketizer, &EshareVideoDepacketizer::SigLog,
            this, &EshareSinkSession::SigLog);

    connect(m_videoDepacketizer, &EshareVideoDepacketizer::SigUnitReady,
            this, [this](const EshareVideoUnit& unit) {
                if (!m_h264DumpFile.isOpen())
                    return;

                if (unit.kind == EshareVideoUnitKind::Config ||
                    unit.kind == EshareVideoUnitKind::Frame)
                {
                    m_h264DumpFile.write(unit.payload);
                    m_h264DumpFile.flush();
                }
            });

    connect(m_52020Stub, &EsharePassivePortListener::SigLog,
            this, &EshareSinkSession::SigLog);
    connect(m_52025Stub, &EsharePassivePortListener::SigLog,
            this, &EshareSinkSession::SigLog);
    connect(m_52030Stub, &EsharePassivePortListener::SigLog,
            this, &EshareSinkSession::SigLog);

    connect(m_52020Stub, &EsharePassivePortListener::SigError,
            this, [this](const QString& text) { emit SigError(QStringLiteral("[SINK] %1").arg(text)); });
    connect(m_52025Stub, &EsharePassivePortListener::SigError,
            this, [this](const QString& text) { emit SigError(QStringLiteral("[SINK] %1").arg(text)); });
    connect(m_52030Stub, &EsharePassivePortListener::SigError,
            this, [this](const QString& text) { emit SigError(QStringLiteral("[SINK] %1").arg(text)); });
}

void EshareSinkSession::Start(const QString& localIp)
{
    if (m_running)
    {
        emit SigLog(QStringLiteral("[SINK] Start ignored: already running."));
        return;
    }

    m_localIp = localIp;
    m_running = true;

    emit SigLog(QStringLiteral("[SINK] session starting, localIp=%1").arg(m_localIp));

    if (m_h264DumpFile.isOpen())
    {
        m_h264DumpFile.close();
    }

    const QString h264FileName = QStringLiteral("eshare_recv_%1.h264")
                                     .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss")));

    m_h264DumpFile.setFileName(h264FileName);

    if (!m_h264DumpFile.open(QIODevice::WriteOnly))
    {
        m_running = false;
        emit SigError(QStringLiteral("[SINK] failed to open h264 dump file: %1")
                          .arg(m_h264DumpFile.errorString()));
        return;
    }

    emit SigLog(QStringLiteral("[SINK] h264 dump file opened: %1")
                    .arg(QDir::toNativeSeparators(m_h264DumpFile.fileName())));

    if (!m_probe8700->Start(localIp, 8700))
    {
        m_running = false;
        emit SigError(QStringLiteral("[SINK] failed to start 8700 server."));
        return;
    }

    if (!m_cmd8121->Start(localIp, 8121))
    {
        m_probe8700->Stop();
        m_running = false;
        emit SigError(QStringLiteral("[SINK] failed to start 8121 server."));
        return;
    }

    if (!m_heartbeat57395->Start(localIp, 57395))
    {
        m_cmd8121->Stop();
        m_probe8700->Stop();
        m_running = false;
        emit SigError(QStringLiteral("[SINK] failed to start 57395 server."));
        return;
    }

    if (!m_camera8600->Start(localIp, 8600))
    {
        m_heartbeat57395->Stop();
        m_cmd8121->Stop();
        m_probe8700->Stop();
        m_running = false;
        emit SigError(QStringLiteral("[SINK] failed to start 8600 server."));
        return;
    }

    if (!m_videoReceiver->Start(localIp, 51030, false))
    {
        m_camera8600->Stop();
        m_heartbeat57395->Stop();
        m_cmd8121->Stop();
        m_probe8700->Stop();
        m_running = false;
        emit SigError(QStringLiteral("[SINK] failed to start 51030 video receiver."));
        return;
    }

    if (!m_rtsp51040->Start(localIp, 51040))
    {
        m_videoReceiver->Stop();
        m_camera8600->Stop();
        m_heartbeat57395->Stop();
        m_cmd8121->Stop();
        m_probe8700->Stop();
        m_running = false;
        emit SigError(QStringLiteral("[SINK] failed to start 51040 server."));
        return;
    }

    if (!m_52020Stub->Start(localIp, 52020))
    {
        m_rtsp51040->Stop();
        m_videoReceiver->Stop();
        m_camera8600->Stop();
        m_heartbeat57395->Stop();
        m_cmd8121->Stop();
        m_probe8700->Stop();
        m_running = false;
        emit SigError(QStringLiteral("[SINK] failed to start 52010 audio stub."));
        return;
    }

    if (!m_52025Stub->Start(localIp, 52025))
    {
        m_52020Stub->Stop();
        m_videoReceiver->Stop();
        m_camera8600->Stop();
        m_heartbeat57395->Stop();
        m_cmd8121->Stop();
        m_probe8700->Stop();
        m_running = false;
        emit SigError(QStringLiteral("[SINK] failed to start 52020 mouse stub."));
        return;
    }

    if (!m_52030Stub->Start(localIp, 52030))
    {
        m_52025Stub->Stop();
        m_52020Stub->Stop();
        m_videoReceiver->Stop();
        m_camera8600->Stop();
        m_heartbeat57395->Stop();
        m_cmd8121->Stop();
        m_probe8700->Stop();
        m_running = false;
        emit SigError(QStringLiteral("[SINK] failed to start 52030 control stub."));
        return;
    }

    emit SigLog(QStringLiteral("[SINK] session started, localIp=%1").arg(m_localIp));
    emit SigStarted();
}

void EshareSinkSession::Stop()
{
    if (!m_running)
    {
        emit SigLog(QStringLiteral("[SINK] Stop ignored: not running."));
        return;
    }

    m_probe8700->Stop();
    m_cmd8121->Stop();
    m_heartbeat57395->Stop();
    m_camera8600->Stop();
    m_videoReceiver->Stop();
    m_rtsp51040->Stop();
    m_52020Stub->Stop();
    m_52025Stub->Stop();
    m_52030Stub->Stop();

    if (m_h264DumpFile.isOpen())
    {
        emit SigLog(QStringLiteral("[SINK] h264 dump file closed: %1, size=%2 bytes")
                        .arg(QDir::toNativeSeparators(m_h264DumpFile.fileName()))
                        .arg(m_h264DumpFile.size()));
        m_h264DumpFile.close();
    }

    m_running = false;

    emit SigLog(QStringLiteral("[SINK] session stopped."));
    emit SigStopped();
}

} // namespace WQt::Cast::Eshare