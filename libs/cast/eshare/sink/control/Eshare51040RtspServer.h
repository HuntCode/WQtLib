#pragma once

#include <QObject>
#include <QHash>
#include <QTcpServer>
#include <QTcpSocket>

namespace WQt::Cast::Eshare
{

class Eshare51040RtspServer : public QObject
{
    Q_OBJECT
public:
    explicit Eshare51040RtspServer(QObject* parent = nullptr);

    bool Start(const QString& localIp, quint16 port = 51040);
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
    QByteArray BuildResponse(const QByteArray& rawRequest, bool* ok = nullptr);
    QByteArray BuildVideoSetupPlist() const;
    QByteArray BuildAudioSetupPlist() const;
    QByteArray BuildOptionsJson() const;
    QString PeerToString(QTcpSocket* socket) const;
    void CloseAndDeleteSocket(QTcpSocket* socket);

private:
    QTcpServer* m_server = nullptr;
    QString m_localIp;
    quint16 m_port = 51040;
    QHash<QTcpSocket*, QByteArray> m_recvBuffers;

    // M5 固定策略：视频端口先锁到 51030，其他先给占位端口
    int m_videoDataPort = 51030;
    int m_audioDataPort = 38643;
    int m_mousePort = 38407;
    int m_controlPort = 51050;

    int m_framerate = 30;
    int m_castingWidth = 3840;
    int m_castingHeight = 2160;
    QString m_videoFormat = QStringLiteral("video:h264");
    QString m_feature = QStringLiteral("1");
};

} // namespace WQt::Cast::Eshare