#pragma once

#include <QObject>
#include <QFile>
#include <QHash>
#include <QTcpServer>
#include <QTcpSocket>

namespace WQt::Cast::Eshare
{

class EshareVideoReceiver : public QObject
{
    Q_OBJECT
public:
    explicit EshareVideoReceiver(QObject* parent = nullptr);

    bool Start(const QString& localIp, quint16 port = 51030, bool dumpToFile = true);
    void Stop();

signals:
    void SigLog(const QString& text);
    void SigStarted(quint16 port);
    void SigStopped();
    void SigError(const QString& text);

    void SigClientConnected(const QString& peer);
    void SigClientDisconnected(const QString& peer);
    void SigVideoData(const QByteArray& data);

private slots:
    void OnNewConnection();
    void OnSocketReadyRead();
    void OnSocketDisconnected();
    void OnSocketError(QAbstractSocket::SocketError socketError);

private:
    QString PeerToString(QTcpSocket* socket) const;
    void CloseAndDeleteSocket(QTcpSocket* socket);

private:
    QTcpServer* m_server = nullptr;
    QString m_localIp;
    quint16 m_port = 51030;

    bool m_dumpToFile = true;
    QFile m_dumpFile;

    QHash<QTcpSocket*, quint64> m_socketRecvBytes;
    quint64 m_totalRecvBytes = 0;
};

} // namespace WQt::Cast::Eshare