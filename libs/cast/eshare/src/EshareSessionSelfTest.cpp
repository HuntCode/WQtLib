#include <QCoreApplication>
#include <QDebug>
#include <QMetaObject>

#include <iostream>
#include <string>
#include <thread>

#include "EshareSessionClient.h"

using namespace WQt::Cast::Eshare;

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
                         qDebug() << "[SESSION] 8700 + 8121 + 57395 + 8600 are ready, next step can integrate 51040.";
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

    session->Start("192.168.9.141");

    qDebug().noquote() << "";
    qDebug().noquote() << "[INPUT] Type 'q' and press Enter to stop.";

    // 后台线程阻塞等待控制台输入
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