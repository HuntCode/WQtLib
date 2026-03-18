#pragma once

#include <QObject>
#include <QTcpSocket>
#include <QTimer>
#include <QJsonObject>

#include "EshareRtspLiteMessage.h"
#include "EsharePlistExtract.h"

namespace WQt::Cast::Eshare
{

enum class Eshare51040Stage
{
    Idle,
    Connecting,
    WaitingVideoSetupResp,
    WaitingAudioSetupResp,
    RunningOptions,
    Stopping,
    Stopped,
    Error
};

struct Eshare51040OptionsState
{
    bool success = false;
    QString errorText;

    int cseq = -1;
    QString videoAudioHeader;

    QByteArray rawResponse;
    QString responseText;

    QJsonObject json;
    QString jsonPrettyText;

    int framerate = -1;
    int castingWidth = -1;
    int castingHeight = -1;
    int idrReq = -1;
    int bitrate = -1;
    int iInterval = -1;
    int castNum = -1;
    int exclusiveScreen = -1;
    int feature = -1;
};

class Eshare51040RtspClient : public QObject
{
    Q_OBJECT
public:
    explicit Eshare51040RtspClient(QObject* parent = nullptr);

    void Start(const QString& receiverIp,
               quint16 port,
               const QByteArray& videoSetupBody,
               const QByteArray& audioSetupBody,
               int optionsIntervalMs = 1000);

    void Stop();

    Eshare51040Stage Stage() const { return m_stage; }
    Eshare51040PortInfo PortInfo() const { return m_portInfo; }

signals:
    void SigLog(const QString& text);
    void SigSetupReady(const WQt::Cast::Eshare::Eshare51040PortInfo& info);
    void SigOptionsState(const WQt::Cast::Eshare::Eshare51040OptionsState& state);
    void SigStopped();
    void SigError(const QString& errorText);

private slots:
    void OnConnected();
    void OnReadyRead();
    void OnDisconnected();
    void OnSocketError(QAbstractSocket::SocketError);
    void OnOptionsTimer();

private:
    void ResetState();
    void SetStage(Eshare51040Stage stage);

    void SendVideoSetup();
    void SendAudioSetup();
    void SendOptions();
    void SendTeardown();

    QByteArray BuildVideoSetupRequest() const;
    QByteArray BuildAudioSetupRequest() const;
    QByteArray BuildOptionsRequest(int cseq) const;
    QByteArray BuildTeardownRequest(int cseq) const;

    void TryParseMessages();
    void HandleVideoSetupResponse(const RtspLiteMessage& resp, const QByteArray& rawResp);
    void HandleAudioSetupResponse(const RtspLiteMessage& resp, const QByteArray& rawResp);
    void HandleOptionsResponse(const RtspLiteMessage& resp, const QByteArray& rawResp);

private:
    QTcpSocket* m_socket = nullptr;
    QTimer* m_optionsTimer = nullptr;

    QString m_receiverIp;
    quint16 m_port = 51040;
    int m_optionsIntervalMs = 1000;

    QByteArray m_recvBuffer;
    QByteArray m_videoSetupBody;
    QByteArray m_audioSetupBody;

    Eshare51040Stage m_stage = Eshare51040Stage::Idle;
    Eshare51040PortInfo m_portInfo;

    int m_optionCSeq = 0;
    bool m_stopping = false;
};

} // namespace WQt::Cast::Eshare