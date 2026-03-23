#pragma once

#include <QObject>
#include <QTcpSocket>
#include <QTimer>
#include <QVector>

#include "EshareH264AnnexB.h"

namespace WQt::Cast::Eshare
{

class EshareH264FileMirrorSender : public QObject
{
    Q_OBJECT
public:
    explicit EshareH264FileMirrorSender(QObject* parent = nullptr);

    void Start(const QString& receiverIp,
               quint16 videoPort,
               const QString& h264FilePath,
               int fps = 30,
               bool loop = true);

    void Stop();

signals:
    void SigLog(const QString& text);
    void SigConnected();
    void SigStopped();
    void SigError(const QString& text);

private slots:
    void OnConnected();
    void OnDisconnected();
    void OnSocketError(QAbstractSocket::SocketError);
    void OnFrameTimer();

private:
    bool LoadStream(QString* error);
    void SendConfigAndFirstFrame();
    void SendNextFrame();
    bool SendPacket(const QByteArray& packet);

private:
    QTcpSocket* m_socket = nullptr;
    QTimer* m_frameTimer = nullptr;

    QString m_receiverIp;
    quint16 m_videoPort = 0;
    QString m_h264FilePath;

    int m_fps = 30;
    bool m_loop = true;
    bool m_stopping = false;

    QByteArray m_configPayload;
    QVector<H264AccessUnit> m_accessUnits;

    int m_nextFrameIndex = 0;   // accessUnits 索引
    quint64 m_sentFrameCount = 0;
};

} // namespace WQt::Cast::Eshare