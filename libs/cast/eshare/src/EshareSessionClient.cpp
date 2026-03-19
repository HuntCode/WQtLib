#include "EshareSessionClient.h"

namespace WQt::Cast::Eshare
{

EshareSessionClient::EshareSessionClient(QObject* parent)
    : QObject(parent)
{
    m_probe8700 = new Eshare8700ProbeClient(this);
    m_heartbeat57395 = new Eshare57395HeartbeatClient(this);
    m_cmd8121 = new Eshare8121CommandClient(this);
    m_camera8600 = new Eshare8600CameraClient(this);
    m_rtsp51040 = new Eshare51040RtspClient(this);
    m_h264FileSender = new EshareH264FileMirrorSender(this);

    connect(m_probe8700, &Eshare8700ProbeClient::SigLog,
            this, &EshareSessionClient::SigLog);
    connect(m_probe8700, &Eshare8700ProbeClient::SigFinished,
            this, &EshareSessionClient::On8700Finished);

    connect(m_heartbeat57395, &Eshare57395HeartbeatClient::SigLog,
            this, &EshareSessionClient::SigLog);
    connect(m_heartbeat57395, &Eshare57395HeartbeatClient::SigClientInfoAck,
            this, &EshareSessionClient::On57395ClientInfoAck);
    connect(m_heartbeat57395, &Eshare57395HeartbeatClient::SigHeartbeatState,
            this, &EshareSessionClient::On57395HeartbeatState);
    connect(m_heartbeat57395, &Eshare57395HeartbeatClient::SigStopped,
            this, &EshareSessionClient::On57395Stopped);

    connect(m_cmd8121, &Eshare8121CommandClient::SigLog,
            this, &EshareSessionClient::SigLog);
    connect(m_cmd8121, &Eshare8121CommandClient::SigFinished,
            this, &EshareSessionClient::On8121Finished);

    connect(m_camera8600, &Eshare8600CameraClient::SigLog,
            this, &EshareSessionClient::SigLog);
    connect(m_camera8600, &Eshare8600CameraClient::SigAvailability,
            this, &EshareSessionClient::On8600Availability);
    connect(m_camera8600, &Eshare8600CameraClient::SigState,
            this, &EshareSessionClient::On8600State);
    connect(m_camera8600, &Eshare8600CameraClient::SigReady,
            this, &EshareSessionClient::On8600Ready);
    connect(m_camera8600, &Eshare8600CameraClient::SigStopped,
            this, &EshareSessionClient::On8600Stopped);
    connect(m_camera8600, &Eshare8600CameraClient::SigError,
            this, [this](const QString& text) {
                SetPhase(EshareSessionPhase::Error);
                emit SigError(QString("[SESSION] 8600 failed: %1").arg(text));
            });

    connect(m_rtsp51040, &Eshare51040RtspClient::SigLog,
            this, &EshareSessionClient::SigLog);
    connect(m_rtsp51040, &Eshare51040RtspClient::SigSetupReady,
            this, &EshareSessionClient::On51040SetupReady);
    connect(m_rtsp51040, &Eshare51040RtspClient::SigOptionsState,
            this, &EshareSessionClient::On51040OptionsState);
    connect(m_rtsp51040, &Eshare51040RtspClient::SigStopped,
            this, &EshareSessionClient::On51040Stopped);
    connect(m_rtsp51040, &Eshare51040RtspClient::SigError,
            this, [this](const QString& text) {
                SetPhase(EshareSessionPhase::Error);
                emit SigError(QString("[SESSION] 51040 failed: %1").arg(text));
            });

    connect(m_h264FileSender, &EshareH264FileMirrorSender::SigLog,
            this, &EshareSessionClient::SigLog);

    connect(m_h264FileSender, &EshareH264FileMirrorSender::SigError,
            this, [this](const QString& text) {
                SetPhase(EshareSessionPhase::Error);
                emit SigError(QString("[SESSION] H264 file sender failed: %1").arg(text));
            });

    connect(m_h264FileSender, &EshareH264FileMirrorSender::SigStopped,
            this, &EshareSessionClient::OnH264FileSenderStopped);
}

void EshareSessionClient::Start(const QString& receiverIp)
{
    if (m_phase != EshareSessionPhase::Idle &&
        m_phase != EshareSessionPhase::Stopped &&
        m_phase != EshareSessionPhase::Error)
    {
        EmitLog("[SESSION] Start ignored: session is already running.");
        return;
    }

    m_receiverIp = receiverIp;
    m_deviceName.clear();
    m_deviceVersion.clear();
    m_webPort = -1;
    m_pin.clear();
    m_feature.clear();
    m_deviceId.clear();

    EmitLog(QString("[SESSION] Start session, receiver=%1").arg(receiverIp));
    SetPhase(EshareSessionPhase::Probing8700);

    m_probe8700->Probe(receiverIp, 8700, 0);
}

void EshareSessionClient::Stop()
{
    EmitLog("[SESSION] Stop requested.");
    SetPhase(EshareSessionPhase::Stopping);

    m_probe8700->Abort();
    m_cmd8121->Abort();
    m_heartbeat57395->Stop();
    m_camera8600->Stop();
    m_rtsp51040->Stop();
    m_h264FileSender->Stop();
}

void EshareSessionClient::Set51040RequestBodies(const QByteArray& videoSetupBody,
                                                const QByteArray& audioSetupBody)
{
    m_videoSetupBody51040 = videoSetupBody;
    m_audioSetupBody51040 = audioSetupBody;
}

void EshareSessionClient::SetTestH264FilePath(const QString& path)
{
    m_testH264FilePath = path;
}

void EshareSessionClient::On8700Finished(const Eshare8700ProbeResult& result)
{
    if (!result.success)
    {
        SetPhase(EshareSessionPhase::Error);
        emit SigError(QString("[SESSION] 8700 probe failed: %1").arg(result.errorText));
        return;
    }

    EmitLog(QString("[SESSION] 8700 ok, byom_tx_avalible=%1")
            .arg(result.hasByomTxAvailable ? QString::number(result.byomTxAvailable) : "N/A"));

    // 修正后的顺序：8700 之后先走 8121
    Start8121GetServerInfo();
}

void EshareSessionClient::Start8121GetServerInfo()
{
    SetPhase(EshareSessionPhase::Starting8121GetServerInfo);
    EmitLog("[SESSION] Start 8121 getServerInfo.");
    m_cmd8121->SendGetServerInfo(m_receiverIp, 8121, m_clientName, m_clientVersion);
}

void EshareSessionClient::Start8121DongleConnected()
{
    SetPhase(EshareSessionPhase::Starting8121DongleConnected);
    EmitLog("[SESSION] Start 8121 dongleConnected.");
    m_cmd8121->SendDongleConnected(m_receiverIp, 8121, m_clientName, m_clientVersion);
}

void EshareSessionClient::Start57395()
{
    SetPhase(EshareSessionPhase::Starting57395);

    EmitLog("[SESSION] Start 57395 heartbeat channel.");

    Eshare57395ClientInfo info;
    info.clientName = m_clientName;
    info.clientType = 3;
    info.isTouchable = 1;
    info.reportClientInfo = "N";
    info.versionCode = 2;
    info.versionName = m_clientVersion;

    m_heartbeat57395->Start(m_receiverIp, 57395, info, 1000);
}

void EshareSessionClient::Start8600()
{
    SetPhase(EshareSessionPhase::Starting8600);
    EmitLog("[SESSION] Start 8600 camera checks.");
    m_camera8600->Start(m_receiverIp, 8600);
}

void EshareSessionClient::Start51040()
{
    if (m_videoSetupBody51040.isEmpty() || m_audioSetupBody51040.isEmpty())
    {
        EmitLog("[SESSION] 51040 request bodies are empty, skip 51040 for now.");
        SetPhase(EshareSessionPhase::ReadyForNextStage);
        emit SigReadyForNextStep();
        return;
    }

    SetPhase(EshareSessionPhase::Starting51040);
    EmitLog("[SESSION] Start 51040 RTSP-like control channel.");

    m_rtsp51040->Start(m_receiverIp, 51040,
                       m_videoSetupBody51040,
                       m_audioSetupBody51040,
                       1000);
}

void EshareSessionClient::On8121Finished(const Eshare8121CommandResult& result)
{
    if (!result.success)
    {
        SetPhase(EshareSessionPhase::Error);
        emit SigError(QString("[SESSION] 8121 command failed: %1").arg(result.errorText));
        return;
    }

    if (result.command == Eshare8121Command::GetServerInfo)
    {
        m_deviceName = result.deviceName;
        m_deviceVersion = result.deviceVersion;
        m_webPort = result.webPort;
        m_pin = result.pin;
        m_feature = result.feature;
        m_deviceId = result.deviceId;

        EmitLog(QString("[SESSION] 8121 getServerInfo ok: deviceName=%1, deviceVersion=%2, webPort=%3, pin=%4, feature=%5, deviceId=%6")
                .arg(m_deviceName)
                .arg(m_deviceVersion)
                .arg(m_webPort)
                .arg(m_pin)
                .arg(m_feature)
                .arg(m_deviceId));

        Start8121DongleConnected();
        return;
    }

    if (result.command == Eshare8121Command::DongleConnected)
    {
        EmitLog(QString("[SESSION] 8121 dongleConnected ok: deviceName=%1, deviceVersion=%2")
                .arg(result.deviceName)
                .arg(result.deviceVersion));

        // 修正后的顺序：8121 两条都完成后，再启动 57395
        Start57395();
        return;
    }
}

void EshareSessionClient::On57395ClientInfoAck(const Eshare57395State& state)
{
    if (!state.success)
    {
        SetPhase(EshareSessionPhase::Error);
        emit SigError(QString("[SESSION] 57395 client-info failed: %1").arg(state.errorText));
        return;
    }

    SetPhase(EshareSessionPhase::Running57395);

    EmitLog(QString("[SESSION] 57395 client-info ack ok, castState=%1")
            .arg(state.castState));

    Start8600();
}

void EshareSessionClient::On57395HeartbeatState(const Eshare57395State& state)
{
    if (!state.success)
    {
        SetPhase(EshareSessionPhase::Error);
        emit SigError(QString("[SESSION] 57395 heartbeat failed: %1").arg(state.errorText));
        return;
    }

    EmitLog(QString("[SESSION] 57395 heartbeat reply=%1 castState=%2 mirrorMode=%3 castMode=%4")
            .arg(state.replyHeartbeat)
            .arg(state.castState)
            .arg(state.mirrorMode)
            .arg(state.castMode));
}

void EshareSessionClient::On57395Stopped()
{
    if (m_phase == EshareSessionPhase::Stopping)
    {
        SetPhase(EshareSessionPhase::Stopped);
        emit SigStopped();
        return;
    }

    if (m_phase != EshareSessionPhase::Error)
    {
        SetPhase(EshareSessionPhase::Stopped);
        emit SigStopped();
    }
}

void EshareSessionClient::On8600Availability(const Eshare8600CameraResult& result)
{
    if (!result.success)
    {
        SetPhase(EshareSessionPhase::Error);
        emit SigError("[SESSION] 8600 availability failed.");
        return;
    }

    EmitLog(QString("[SESSION] 8600 availability=%1")
                .arg(result.hasAvailability ? QString::number(result.availability) : "N/A"));
}

void EshareSessionClient::On8600State(const Eshare8600CameraResult& result)
{
    if (!result.success)
    {
        SetPhase(EshareSessionPhase::Error);
        emit SigError("[SESSION] 8600 state check failed.");
        return;
    }

    EmitLog(QString("[SESSION] 8600 state: replayStateCheck=%1 currentID=%2 count=%3")
                .arg(result.replayStateCheck)
                .arg(result.currentID)
                .arg(result.count));
}

void EshareSessionClient::On8600Ready()
{
    EmitLog("[SESSION] 8600 camera checks completed.");
    Start51040();
}

void EshareSessionClient::On8600Stopped()
{
    EmitLog("[SESSION] 8600 stopped.");
}

void EshareSessionClient::On51040SetupReady(const Eshare51040PortInfo& info)
{
    SetPhase(EshareSessionPhase::Running51040);

    EmitLog(QString("[SESSION] 51040 setup ready: videoDataPort=%1, audioDataPort=%2, mousePort=%3, controlPort=%4, width=%5, height=%6, framerate=%7, format=%8")
                .arg(info.videoDataPort)
                .arg(info.audioDataPort)
                .arg(info.mousePort)
                .arg(info.controlPort)
                .arg(info.castingWidth)
                .arg(info.castingHeight)
                .arg(info.framerate)
                .arg(info.videoFormat));

    if (!m_testH264FilePath.isEmpty() && info.videoDataPort > 0)
    {
        EmitLog(QString("[SESSION] Start H264 file sender: port=%1, file=%2")
                    .arg(info.videoDataPort)
                    .arg(m_testH264FilePath));

        m_h264FileSender->Start(m_receiverIp,
                                static_cast<quint16>(info.videoDataPort),
                                m_testH264FilePath,
                                30,
                                true);
    }
}

void EshareSessionClient::On51040OptionsState(const Eshare51040OptionsState& state)
{
    if (!state.success)
    {
        SetPhase(EshareSessionPhase::Error);
        emit SigError("[SESSION] 51040 options state failed.");
        return;
    }

    EmitLog(QString("[SESSION] 51040 options: cseq=%1 Video-Audio=%2 width=%3 height=%4 framerate=%5 idr_req=%6 bitrate=%7 i-interval=%8")
                .arg(state.cseq)
                .arg(state.videoAudioHeader)
                .arg(state.castingWidth)
                .arg(state.castingHeight)
                .arg(state.framerate)
                .arg(state.idrReq)
                .arg(state.bitrate)
                .arg(state.iInterval));
}

void EshareSessionClient::On51040Stopped()
{
    EmitLog("[SESSION] 51040 stopped.");
}

void EshareSessionClient::OnH264FileSenderStopped()
{
    EmitLog("[SESSION] H264 file sender stopped.");
}

void EshareSessionClient::SetPhase(EshareSessionPhase phase)
{
    if (m_phase == phase)
        return;

    m_phase = phase;
    emit SigPhaseChanged(phase);
}

void EshareSessionClient::EmitLog(const QString& text)
{
    emit SigLog(text);
}

} // namespace WQt::Cast::Eshare