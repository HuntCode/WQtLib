#pragma once

#include <QObject>
#include <QString>
#include <QFile>

namespace WQt::Cast::Eshare
{

class Eshare8700ProbeServer;
class Eshare8121CommandServer;
class Eshare57395HeartbeatServer;
class Eshare8600CameraServer;
class Eshare51040RtspServer;
class EshareVideoReceiver;
class EshareVideoDepacketizer;
class EsharePassivePortListener;

class EshareSinkSession : public QObject
{
    Q_OBJECT
public:
    explicit EshareSinkSession(QObject* parent = nullptr);

    void Start(const QString& localIp);
    void Stop();

signals:
    void SigLog(const QString& text);
    void SigStarted();
    void SigStopped();
    void SigError(const QString& text);

private:
    QString m_localIp;
    bool m_running = false;

    Eshare8700ProbeServer* m_probe8700 = nullptr;
    Eshare8121CommandServer* m_cmd8121 = nullptr;
    Eshare57395HeartbeatServer* m_heartbeat57395 = nullptr;
    Eshare8600CameraServer* m_camera8600 = nullptr;
    Eshare51040RtspServer* m_rtsp51040 = nullptr;
    EshareVideoReceiver* m_videoReceiver = nullptr;
    EshareVideoDepacketizer* m_videoDepacketizer = nullptr;

    EsharePassivePortListener* m_52020Stub = nullptr;   // 52020
    EsharePassivePortListener* m_52025Stub = nullptr;   // 52025
    EsharePassivePortListener* m_52030Stub = nullptr; // 52030

    QFile m_h264DumpFile;
};

} // namespace WQt::Cast::Eshare