#pragma once

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QHash>

namespace WQt::Cast::Eshare
{

class EsharePassivePortListener : public QObject
{
    Q_OBJECT
public:
    explicit EsharePassivePortListener(const QString& tag, QObject* parent = nullptr);

    bool Start(const QString& localIp, quint16 port);
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
    QString PeerToString(QTcpSocket* socket) const;
    void CloseAndDeleteSocket(QTcpSocket* socket);

private:
    QString m_tag;
    QTcpServer* m_server = nullptr;
    QString m_localIp;
    quint16 m_port = 0;
    QHash<QTcpSocket*, quint64> m_socketRecvBytes;
};

} // namespace WQt::Cast::Eshare