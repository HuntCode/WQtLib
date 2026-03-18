#pragma once

#include <QObject>
#include <QTcpSocket>
#include <QJsonObject>

#include "EshareRtspLiteMessage.h"

namespace WQt::Cast::Eshare
{

struct Eshare8700ProbeResult
{
    bool success = false;

    int statusCode = -1;
    int cseq = -1;

    bool hasByomTxAvailable = false;
    int byomTxAvailable = -1;

    QString statusText;
    QString errorText;

    QByteArray rawRequest;
    QByteArray rawResponse;

    QString requestText;
    QString responseText;
    QString responseBodyReadable;

    QJsonObject jsonBody;
};

class Eshare8700ProbeClient : public QObject
{
    Q_OBJECT
public:
    explicit Eshare8700ProbeClient(QObject* parent = nullptr);

    void Probe(const QString& receiverIp, quint16 port = 8700, int cseq = 0);
    void Abort();

signals:
    void SigFinished(const WQt::Cast::Eshare::Eshare8700ProbeResult& result);
    void SigLog(const QString& text);

private slots:
    void OnConnected();
    void OnReadyRead();
    void OnSocketError(QAbstractSocket::SocketError socketError);
    void OnDisconnected();

private:
    void ResetState();
    void FinishWithError(const QString& text);
    void TryParseResponse();
    QByteArray BuildOptionsRequest(const QString& receiverIp, int cseq) const;

private:
    QTcpSocket* m_socket = nullptr;
    QByteArray m_recvBuffer;
    QByteArray m_lastRequest;

    QString m_receiverIp;
    quint16 m_port = 8700;
    int m_cseq = 0;
    bool m_finished = false;
};

} // namespace WQt::Cast::Eshare