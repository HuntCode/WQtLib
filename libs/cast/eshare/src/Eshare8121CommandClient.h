#pragma once

#include <QObject>
#include <QTcpSocket>
#include <QTimer>
#include <QJsonObject>
#include <QStringList>

namespace WQt::Cast::Eshare
{

enum class Eshare8121Command
{
    GetServerInfo,
    DongleConnected
};

struct Eshare8121CommandResult
{
    bool success = false;
    QString errorText;

    Eshare8121Command command = Eshare8121Command::GetServerInfo;

    QByteArray rawRequest;
    QByteArray rawResponse;

    QString requestText;
    QString responseText;

    bool hasJsonBody = false;
    QJsonObject jsonBody;
    QString jsonPrettyText;

    QStringList responseLines;

    QString deviceName;
    QString deviceVersion;
    int webPort = -1;
    QString pin;
    QString airPlay;
    QString airPlayFeature;
    QString feature;
    QString deviceId;
};

class Eshare8121CommandClient : public QObject
{
    Q_OBJECT
public:
    explicit Eshare8121CommandClient(QObject* parent = nullptr);

    void SendGetServerInfo(const QString& receiverIp,
                           quint16 port,
                           const QString& clientName,
                           const QString& clientVersion);

    void SendDongleConnected(const QString& receiverIp,
                             quint16 port,
                             const QString& clientName,
                             const QString& clientVersion);

    void Abort();

signals:
    void SigLog(const QString& text);
    void SigFinished(const WQt::Cast::Eshare::Eshare8121CommandResult& result);

private slots:
    void OnConnected();
    void OnReadyRead();
    void OnSocketError(QAbstractSocket::SocketError socketError);
    void OnDisconnected();
    void OnResponseIdleTimeout();

private:
    void ResetState();
    void StartCommand(Eshare8121Command command,
                      const QString& receiverIp,
                      quint16 port,
                      const QString& clientName,
                      const QString& clientVersion);

    QByteArray BuildRequest(Eshare8121Command command,
                            const QString& clientName,
                            const QString& clientVersion) const;

    QString CommandToString(Eshare8121Command command) const;
    void FinishWithError(const QString& text);
    void ParseAndFinish();
    bool LooksLikeCompleteJson(const QByteArray& data) const;

private:
    QTcpSocket* m_socket = nullptr;
    QTimer* m_idleTimer = nullptr;

    QString m_receiverIp;
    quint16 m_port = 8121;
    Eshare8121Command m_command = Eshare8121Command::GetServerInfo;

    QByteArray m_lastRequest;
    QByteArray m_recvBuffer;

    bool m_finished = false;
    bool m_responseParsed = false;
    Eshare8121CommandResult m_pendingResult;
};

} // namespace WQt::Cast::Eshare