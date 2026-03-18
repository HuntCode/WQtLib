#pragma once

#include <QObject>
#include <QString>

#include "Eshare8700ProbeClient.h"
#include "Eshare57395HeartbeatClient.h"
#include "Eshare8121CommandClient.h"
#include "Eshare8600CameraClient.h"

namespace WQt::Cast::Eshare
{

enum class EshareSessionPhase
{
    Idle,
    Probing8700,
    Starting8121GetServerInfo,
    Starting8121DongleConnected,
    Starting57395,
    Running57395,
    Starting8600,
    ReadyForNextStage,
    Stopping,
    Stopped,
    Error
};

class EshareSessionClient : public QObject
{
    Q_OBJECT
public:
    explicit EshareSessionClient(QObject* parent = nullptr);

    void Start(const QString& receiverIp);
    void Stop();

    EshareSessionPhase Phase() const { return m_phase; }

signals:
    void SigLog(const QString& text);
    void SigPhaseChanged(WQt::Cast::Eshare::EshareSessionPhase phase);
    void SigReadyForNextStep();
    void SigStopped();
    void SigError(const QString& errorText);

private slots:
    void On8700Finished(const WQt::Cast::Eshare::Eshare8700ProbeResult& result);
    void On57395ClientInfoAck(const WQt::Cast::Eshare::Eshare57395State& state);
    void On57395HeartbeatState(const WQt::Cast::Eshare::Eshare57395State& state);
    void On57395Stopped();

    void On8121Finished(const WQt::Cast::Eshare::Eshare8121CommandResult& result);

    void On8600Availability(const WQt::Cast::Eshare::Eshare8600CameraResult& result);
    void On8600State(const WQt::Cast::Eshare::Eshare8600CameraResult& result);
    void On8600Ready();
    void On8600Stopped();

private:
    void SetPhase(EshareSessionPhase phase);
    void EmitLog(const QString& text);

    void Start8121GetServerInfo();
    void Start8121DongleConnected();
    void Start57395();

    void Start8600();

private:
    QString m_receiverIp;
    EshareSessionPhase m_phase = EshareSessionPhase::Idle;

    Eshare8700ProbeClient* m_probe8700 = nullptr;
    Eshare57395HeartbeatClient* m_heartbeat57395 = nullptr;
    Eshare8121CommandClient* m_cmd8121 = nullptr;
    Eshare8600CameraClient* m_camera8600 = nullptr;

    QString m_clientName = "HuntCode";
    QString m_clientVersion = "EShare ES06T-DP_U_v4.1.251122";

    QString m_deviceName;
    QString m_deviceVersion;
    int m_webPort = -1;
    QString m_pin;
    QString m_feature;
    QString m_deviceId;
};

} // namespace WQt::Cast::Eshare