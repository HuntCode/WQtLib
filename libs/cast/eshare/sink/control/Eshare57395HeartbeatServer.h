#pragma once

#include <QObject>
#include <QHash>
#include <QTcpServer>
#include <QTcpSocket>

namespace WQt::Cast::Eshare
{

class Eshare57395HeartbeatServer : public QObject
{
    Q_OBJECT
public:
    explicit Eshare57395HeartbeatServer(QObject* parent = nullptr);

    bool Start(const QString& localIp, quint16 port = 57395);
    void Stop();

signals:
    void SigLog(const QString& text);
    void SigStarted(quint16 port);
    void SigStopped();
    void SigError(const QString& text);

private slots:
    void OnNewConnection();
    void OnSocketReadyRead();
    void OnSocketDisconnected();
    void OnSocketError(QAbstractSocket::SocketError socketError);

private:
    QByteArray BuildResponse(const QJsonObject& reqObj, bool* ok = nullptr);
    QString PeerToString(QTcpSocket* socket) const;
    void CloseAndDeleteSocket(QTcpSocket* socket);

private:
    QTcpServer* m_server = nullptr;
    QString m_localIp;
    quint16 m_port = 57395;
    QHash<QTcpSocket*, QByteArray> m_recvBuffers;
};

} // namespace WQt::Cast::Eshare