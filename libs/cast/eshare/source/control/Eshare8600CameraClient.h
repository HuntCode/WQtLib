#pragma once

#include <QObject>
#include <QTcpSocket>
#include <QTimer>
#include <QJsonObject>

namespace WQt::Cast::Eshare
{

enum class Eshare8600Stage
{
    Idle,
    Connecting,
    WaitingAvailabilityReply,
    WaitingStateReply,
    Ready,
    Error
};

struct Eshare8600CameraResult
{
    bool success = false;
    QString errorText;

    QByteArray rawRequest;
    QByteArray rawResponse;

    QString requestText;
    QString responseText;

    bool hasAvailability = false;
    int availability = -1;

    bool hasStateJson = false;
    QJsonObject stateJson;
    QString statePrettyText;

    QString replayStateCheck;
    int currentID = -1;
    int count = -1;
};

class Eshare8600CameraClient : public QObject
{
    Q_OBJECT
public:
    explicit Eshare8600CameraClient(QObject* parent = nullptr);

    void Start(const QString& receiverIp, quint16 port = 8600);
    void Stop();

    Eshare8600Stage Stage() const { return m_stage; }

signals:
    void SigLog(const QString& text);
    void SigAvailability(const WQt::Cast::Eshare::Eshare8600CameraResult& result);
    void SigState(const WQt::Cast::Eshare::Eshare8600CameraResult& result);
    void SigReady();
    void SigStopped();
    void SigError(const QString& errorText);

private slots:
    void OnConnected();
    void OnReadyRead();
    void OnDisconnected();
    void OnSocketError(QAbstractSocket::SocketError);
    void OnIdleTimeout();

private:
    void SetStage(Eshare8600Stage stage);
    void ResetState();

    void SendAvailabilityCheck();
    void SendStateCheck();

    void TryParseBufferedResponse();
    void FinishWithError(const QString& text);

    static QByteArray BuildCommand(const QByteArray& cmd);
    static QString NormalizeText(const QByteArray& data);

private:
    QTcpSocket* m_socket = nullptr;
    QTimer* m_idleTimer = nullptr;

    QString m_receiverIp;
    quint16 m_port = 8600;

    Eshare8600Stage m_stage = Eshare8600Stage::Idle;

    QByteArray m_recvBuffer;
    QByteArray m_lastRequest;
    bool m_stopping = false;
};

} // namespace WQt::Cast::Eshare