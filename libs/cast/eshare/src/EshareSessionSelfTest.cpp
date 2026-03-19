#include <QCoreApplication>
#include <QDebug>
#include <QMetaObject>
#include <QFile>

#include <iostream>
#include <string>
#include <thread>

#include "EshareSessionClient.h"
#include "EsharePlistExtract.h"

using namespace WQt::Cast::Eshare;

#ifndef ESHARE_TESTDATA_DIR
#define ESHARE_TESTDATA_DIR "."
#endif

static void DumpLoadedPlist(const QString& tag, const QByteArray& data)
{
    qDebug().noquote() << "";
    qDebug().noquote() << "[PLIST] =====" << tag << "=====";
    qDebug().noquote() << "[PLIST] size =" << data.size();

    if (data.isEmpty())
    {
        qDebug().noquote() << "[PLIST] empty";
        return;
    }

    QString error;
    const QString xml = EsharePlistExtract::ToXml(data, &error);
    if (!xml.isEmpty())
    {
        qDebug().noquote() << xml;
    }
    else
    {
        qDebug().noquote() << "[PLIST] parse failed:" << error;
        qDebug().noquote() << "[PLIST] HEX:";
        qDebug().noquote() << data.toHex(' ');
    }
}

static QByteArray LoadBinaryFile(const QString& path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
    {
        qWarning().noquote() << "[FILE] open failed:" << path << "," << f.errorString();
        return {};
    }

    QByteArray data = f.readAll();
    qDebug().noquote() << "[FILE] loaded:" << path << ", size =" << data.size();
    return data;
}

static const char* PhaseToString(EshareSessionPhase phase)
{
    switch (phase)
    {
    case EshareSessionPhase::Idle: return "Idle";
    case EshareSessionPhase::Probing8700: return "Probing8700";
    case EshareSessionPhase::Starting8121GetServerInfo: return "Starting8121GetServerInfo";
    case EshareSessionPhase::Starting8121DongleConnected: return "Starting8121DongleConnected";
    case EshareSessionPhase::Starting57395: return "Starting57395";
    case EshareSessionPhase::Running57395: return "Running57395";
    case EshareSessionPhase::Starting8600: return "Starting8600";
    case EshareSessionPhase::Starting51040: return "Starting51040";
    case EshareSessionPhase::Running51040: return "Running51040";
    case EshareSessionPhase::ReadyForNextStage: return "ReadyForNextStage";
    case EshareSessionPhase::Stopping: return "Stopping";
    case EshareSessionPhase::Stopped: return "Stopped";
    case EshareSessionPhase::Error: return "Error";
    default: return "Unknown";
    }
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    auto* session = new EshareSessionClient(&app);

    QObject::connect(session, &EshareSessionClient::SigLog,
                     [](const QString& text) {
                         qDebug().noquote() << text;
                     });

    QObject::connect(session, &EshareSessionClient::SigPhaseChanged,
                     [](EshareSessionPhase phase) {
                         qDebug() << "[SESSION] phase =" << PhaseToString(phase);
                     });

    QObject::connect(session, &EshareSessionClient::SigReadyForNextStep,
                     []() {
                         qDebug() << "[SESSION] 8700 + 8121 + 57395 + 8600 (+ maybe 51040) are ready.";
                     });

    QObject::connect(session, &EshareSessionClient::SigError,
                     [&app](const QString& errorText) {
                         qDebug().noquote() << "[SESSION] ERROR:" << errorText;
                         app.quit();
                     });

    QObject::connect(session, &EshareSessionClient::SigStopped,
                     [&app]() {
                         qDebug() << "[SESSION] stopped.";
                         app.quit();
                     });

    // 这里就是新增的部分：读取 51040 的两个 request body 模板
    const QString baseDir = QStringLiteral(ESHARE_TESTDATA_DIR);
    const QByteArray videoBody = LoadBinaryFile(baseDir + "/51040_video_setup_body.bin");
    const QByteArray audioBody = LoadBinaryFile(baseDir + "/51040_audio_setup_body.bin");

    DumpLoadedPlist("51040_video_setup_body.bin", videoBody);
    DumpLoadedPlist("51040_audio_setup_body.bin", audioBody);

    session->Set51040RequestBodies(videoBody, audioBody);

    session->Start("192.168.9.141");

    qDebug().noquote() << "";
    qDebug().noquote() << "[INPUT] Type 'q' and press Enter to stop.";

    std::thread inputThread([session]() {
        std::string line;
        while (std::getline(std::cin, line))
        {
            if (line == "q" || line == "Q")
            {
                QMetaObject::invokeMethod(session, [session]() {
                    session->Stop();
                }, Qt::QueuedConnection);
                break;
            }
        }
    });

    inputThread.detach();

    return app.exec();
}