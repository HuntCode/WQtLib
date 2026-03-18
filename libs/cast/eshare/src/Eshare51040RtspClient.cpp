#include "Eshare51040RtspClient.h"

#include <QJsonDocument>
#include <QJsonParseError>

namespace WQt::Cast::Eshare
{

Eshare51040RtspClient::Eshare51040RtspClient(QObject* parent)
    : QObject(parent)
{
    m_socket = new QTcpSocket(this);

    m_optionsTimer = new QTimer(this);
    m_optionsTimer->setSingleShot(false);

    connect(m_socket, &QTcpSocket::connected,
            this, &Eshare51040RtspClient::OnConnected);
    connect(m_socket, &QTcpSocket::readyRead,
            this, &Eshare51040RtspClient::OnReadyRead);
    connect(m_socket, &QTcpSocket::disconnected,
            this, &Eshare51040RtspClient::OnDisconnected);
    connect(m_socket, &QTcpSocket::errorOccurred,
            this, &Eshare51040RtspClient::OnSocketError);

    connect(m_optionsTimer, &QTimer::timeout,
            this, &Eshare51040RtspClient::OnOptionsTimer);
}

void Eshare51040RtspClient::Start(const QString& receiverIp,
                                  quint16 port,
                                  const QByteArray& videoSetupBody,
                                  const QByteArray& audioSetupBody,
                                  int optionsIntervalMs)
{
    ResetState();

    m_receiverIp = receiverIp;
    m_port = port;
    m_videoSetupBody = videoSetupBody;
    m_audioSetupBody = audioSetupBody;
    m_optionsIntervalMs = optionsIntervalMs;

    if (m_videoSetupBody.isEmpty() || m_audioSetupBody.isEmpty())
    {
        SetStage(Eshare51040Stage::Error);
        emit SigError("[51040] video/audio setup body template is empty.");
        return;
    }

    SetStage(Eshare51040Stage::Connecting);
    emit SigLog(QString("[51040] Connecting to %1:%2").arg(receiverIp).arg(port));
    m_socket->connectToHost(receiverIp, port);
}

void Eshare51040RtspClient::Stop()
{
    m_stopping = true;
    m_optionsTimer->stop();

    if (m_socket->state() == QAbstractSocket::ConnectedState)
    {
        SetStage(Eshare51040Stage::Stopping);
        SendTeardown();
        m_socket->disconnectFromHost();
    }
    else
    {
        SetStage(Eshare51040Stage::Stopped);
        emit SigStopped();
    }
}

void Eshare51040RtspClient::ResetState()
{
    m_recvBuffer.clear();
    m_videoSetupBody.clear();
    m_audioSetupBody.clear();
    m_optionsTimer->stop();
    m_stage = Eshare51040Stage::Idle;
    m_portInfo = Eshare51040PortInfo{};
    m_optionCSeq = 0;
    m_stopping = false;
}

void Eshare51040RtspClient::SetStage(Eshare51040Stage stage)
{
    m_stage = stage;
}

void Eshare51040RtspClient::OnConnected()
{
    emit SigLog("[51040] Connected.");
    SendVideoSetup();
}

QByteArray Eshare51040RtspClient::BuildVideoSetupRequest() const
{
    RtspLiteMessage req;
    req.startLine = QString("SETUP rtsp://%1 RTSP/1.0").arg(m_receiverIp);
    req.setHeader("CSeq", "0");
    req.setHeader("VideoAspectRatio", "1");
    req.body = m_videoSetupBody;
    return RtspLiteCodec::Encode(req);
}

QByteArray Eshare51040RtspClient::BuildAudioSetupRequest() const
{
    RtspLiteMessage req;
    req.startLine = QString("SETUP rtsp://%1 RTSP/1.0").arg(m_receiverIp);
    req.setHeader("CSeq", "1");
    req.setHeader("User-Agent", "EShare (EShare 1.0)");
    req.body = m_audioSetupBody;
    return RtspLiteCodec::Encode(req);
}

QByteArray Eshare51040RtspClient::BuildOptionsRequest(int cseq) const
{
    RtspLiteMessage req;
    req.startLine = QString("OPTIONS rtsp://%1 RTSP/1.0").arg(m_receiverIp);
    req.setHeader("CSeq", QString::number(cseq));
    req.setHeader("User-Agent", "EShare (EShare 1.0)");
    req.setHeader("PPTSlideShow", "0");
    req.body.clear();
    return RtspLiteCodec::Encode(req);
}

QByteArray Eshare51040RtspClient::BuildTeardownRequest(int cseq) const
{
    RtspLiteMessage req;
    req.startLine = QString("TEARDOWN rtsp://%1 RTSP/1.0").arg(m_receiverIp);
    req.setHeader("CSeq", QString::number(cseq));
    req.setHeader("User-Agent", "EShare (EShare 1.0)");
    req.setHeader("DongleState", "0");
    req.body = m_videoSetupBody;
    return RtspLiteCodec::Encode(req);
}

void Eshare51040RtspClient::SendVideoSetup()
{
    const QByteArray req = BuildVideoSetupRequest();
    SetStage(Eshare51040Stage::WaitingVideoSetupResp);

    emit SigLog(QString("[51040] >>> SEND VIDEO SETUP (%1 bytes)\n%2")
                .arg(req.size())
                .arg(QString::fromUtf8(req.left(256)) + (req.size() > 256 ? "\n...<omitted>..." : "")));

    m_socket->write(req);
    m_socket->flush();
}

void Eshare51040RtspClient::SendAudioSetup()
{
    const QByteArray req = BuildAudioSetupRequest();
    SetStage(Eshare51040Stage::WaitingAudioSetupResp);

    emit SigLog(QString("[51040] >>> SEND AUDIO SETUP (%1 bytes)\n%2")
                .arg(req.size())
                .arg(QString::fromUtf8(req.left(256)) + (req.size() > 256 ? "\n...<omitted>..." : "")));

    m_socket->write(req);
    m_socket->flush();
}

void Eshare51040RtspClient::SendOptions()
{
    const QByteArray req = BuildOptionsRequest(m_optionCSeq);

    emit SigLog(QString("[51040] >>> SEND OPTIONS cseq=%1 (%2 bytes)\n%3")
                .arg(m_optionCSeq)
                .arg(req.size())
                .arg(QString::fromUtf8(req)));

    m_socket->write(req);
    m_socket->flush();

    ++m_optionCSeq;
}

void Eshare51040RtspClient::SendTeardown()
{
    const QByteArray req = BuildTeardownRequest(m_optionCSeq);

    emit SigLog(QString("[51040] >>> SEND TEARDOWN cseq=%1 (%2 bytes)\n%3")
                .arg(m_optionCSeq)
                .arg(req.size())
                .arg(QString::fromUtf8(req.left(256)) + (req.size() > 256 ? "\n...<omitted>..." : "")));

    m_socket->write(req);
    m_socket->flush();
}

void Eshare51040RtspClient::OnReadyRead()
{
    m_recvBuffer += m_socket->readAll();
    TryParseMessages();
}

void Eshare51040RtspClient::TryParseMessages()
{
    while (true)
    {
        RtspLiteMessage resp;
        QByteArray rawResp;
        QString error;
        if (!RtspLiteCodec::TryDecode(m_recvBuffer, resp, &rawResp, &error))
            break;

        if (m_stage == Eshare51040Stage::WaitingVideoSetupResp)
        {
            HandleVideoSetupResponse(resp, rawResp);
            continue;
        }

        if (m_stage == Eshare51040Stage::WaitingAudioSetupResp)
        {
            HandleAudioSetupResponse(resp, rawResp);
            continue;
        }

        if (m_stage == Eshare51040Stage::RunningOptions)
        {
            HandleOptionsResponse(resp, rawResp);
            continue;
        }
    }
}

void Eshare51040RtspClient::HandleVideoSetupResponse(const RtspLiteMessage& resp, const QByteArray& rawResp)
{
    emit SigLog(QString("[51040] <<< VIDEO SETUP RESP (%1 bytes)\n%2")
                .arg(rawResp.size())
                .arg(RtspLiteCodec::MessageToDebugString(resp)));

    QString plistError;
    const QString xml = EsharePlistExtract::ToXml(resp.body, &plistError);
    if (!xml.isEmpty())
    {
        EsharePlistExtract::ExtractVideoSetupResponse(xml, m_portInfo);

        emit SigLog(QString("[51040] VIDEO SETUP plist parsed: videoDataPort=%1, format=%2, width=%3, height=%4, framerate=%5")
                    .arg(m_portInfo.videoDataPort)
                    .arg(m_portInfo.videoFormat)
                    .arg(m_portInfo.castingWidth)
                    .arg(m_portInfo.castingHeight)
                    .arg(m_portInfo.framerate));
    }
    else
    {
        emit SigLog(QString("[51040] VIDEO SETUP plist parse failed: %1").arg(plistError));
    }

    SendAudioSetup();
}

void Eshare51040RtspClient::HandleAudioSetupResponse(const RtspLiteMessage& resp, const QByteArray& rawResp)
{
    emit SigLog(QString("[51040] <<< AUDIO SETUP RESP (%1 bytes)\n%2")
                .arg(rawResp.size())
                .arg(RtspLiteCodec::MessageToDebugString(resp)));

    QString plistError;
    const QString xml = EsharePlistExtract::ToXml(resp.body, &plistError);
    if (!xml.isEmpty())
    {
        EsharePlistExtract::ExtractAudioSetupResponse(xml, m_portInfo);

        emit SigLog(QString("[51040] AUDIO SETUP plist parsed: audioDataPort=%1, mousePort=%2, controlPort=%3")
                    .arg(m_portInfo.audioDataPort)
                    .arg(m_portInfo.mousePort)
                    .arg(m_portInfo.controlPort));
    }
    else
    {
        emit SigLog(QString("[51040] AUDIO SETUP plist parse failed: %1").arg(plistError));
    }

    SetStage(Eshare51040Stage::RunningOptions);
    emit SigSetupReady(m_portInfo);

    SendOptions();
    m_optionsTimer->start(m_optionsIntervalMs);
}

void Eshare51040RtspClient::HandleOptionsResponse(const RtspLiteMessage& resp, const QByteArray& rawResp)
{
    Eshare51040OptionsState state;
    state.success = true;
    state.rawResponse = rawResp;
    state.responseText = RtspLiteCodec::MessageToDebugString(resp);
    state.videoAudioHeader = resp.headerValue("Video-Audio");

    bool ok = false;
    state.cseq = resp.headerValue("CSeq").toInt(&ok);
    if (!ok) state.cseq = -1;

    QJsonParseError jsonError;
    QJsonDocument doc = QJsonDocument::fromJson(resp.body, &jsonError);
    if (jsonError.error == QJsonParseError::NoError && doc.isObject())
    {
        state.json = doc.object();
        state.jsonPrettyText = QString::fromUtf8(doc.toJson(QJsonDocument::Indented));

        auto toIntStr = [&](const char* key, int def = -1) -> int {
            if (!state.json.contains(key)) return def;
            return state.json.value(key).toString().toInt();
        };

        state.framerate = toIntStr("Framerate");
        state.castingWidth = toIntStr("casting_win_width");
        state.castingHeight = toIntStr("casting_win_height");
        state.idrReq = toIntStr("idr_req");
        state.bitrate = toIntStr("bitrate");
        state.iInterval = toIntStr("i-interval");
        state.castNum = toIntStr("Castnum");
        state.exclusiveScreen = toIntStr("exclusive_screen");
        state.feature = toIntStr("feature");
    }

    emit SigLog(QString("[51040] <<< OPTIONS RESP (%1 bytes)\n%2")
                .arg(rawResp.size())
                .arg(state.responseText));

    emit SigOptionsState(state);
}

void Eshare51040RtspClient::OnOptionsTimer()
{
    if (m_stage == Eshare51040Stage::RunningOptions)
        SendOptions();
}

void Eshare51040RtspClient::OnDisconnected()
{
    m_optionsTimer->stop();

    if (m_stage == Eshare51040Stage::Error)
        return;

    SetStage(Eshare51040Stage::Stopped);
    emit SigLog("[51040] Disconnected.");
    emit SigStopped();
}

void Eshare51040RtspClient::OnSocketError(QAbstractSocket::SocketError)
{
    if (m_stopping)
        return;

    SetStage(Eshare51040Stage::Error);
    emit SigError(QString("[51040] %1").arg(m_socket->errorString()));
}

} // namespace WQt::Cast::Eshare