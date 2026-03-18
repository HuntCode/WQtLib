#pragma once

#include <QObject>
#include <QTcpSocket>
#include <QTimer>
#include <QJsonObject>

namespace WQt::Cast::Eshare
{

struct Eshare57395ClientInfo
{
    QString clientName = "HuntCode";
    int clientType = 3;
    int isTouchable = 1;
    QString reportClientInfo = "N";
    int versionCode = 2;
    QString versionName = "EShare ES06T-DP_U_v4.1.251122";
};

struct Eshare57395State
{
    bool success = false;
    QString errorText;

    QByteArray rawLine;
    QString lineText;
    QString prettyText;
    QJsonObject json;

    int replyHeartbeat = -1;
    int castState = -1;
    int mirrorMode = -1;
    int castMode = -1;
    int multiScreen = -1;
    int isModerator = -1;
    int radioMode = -1;
};

class Eshare57395HeartbeatClient : public QObject
{
    Q_OBJECT
public:
    explicit Eshare57395HeartbeatClient(QObject* parent = nullptr);

    void Start(const QString& receiverIp,
               quint16 port,
               const Eshare57395ClientInfo& clientInfo,
               int heartbeatIntervalMs = 1000);

    void Stop();

signals:
    void SigLog(const QString& text);
    void SigClientInfoAck(const WQt::Cast::Eshare::Eshare57395State& state);
    void SigHeartbeatState(const WQt::Cast::Eshare::Eshare57395State& state);
    void SigStopped();

private slots:
    void OnConnected();
    void OnReadyRead();
    void OnHeartbeatTimeout();
    void OnSocketError(QAbstractSocket::SocketError socketError);
    void OnDisconnected();

private:
    void ResetState();
    void FinishWithError(const QString& text);
    void SendClientInfo();
    void SendHeartbeat();
    void TryParseIncoming();
    Eshare57395State BuildStateFromJson(const QByteArray& rawLine, const QJsonObject& obj) const;

private:
    QTcpSocket* m_socket = nullptr;
    QTimer* m_heartbeatTimer = nullptr;

    QByteArray m_recvBuffer;

    QString m_receiverIp;
    quint16 m_port = 57395;
    int m_heartbeatIntervalMs = 1000;

    Eshare57395ClientInfo m_clientInfo;
    int m_nextHeartbeat = 1;
    bool m_clientInfoAcked = false;
    bool m_stopping = false;
};

} // namespace WQt::Cast::Eshare