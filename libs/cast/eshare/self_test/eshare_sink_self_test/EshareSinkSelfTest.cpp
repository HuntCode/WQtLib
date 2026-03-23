#include <QCoreApplication>
#include <QDebug>
#include <QMetaObject>

#include <iostream>
#include <string>
#include <thread>

#include "EshareSinkSession.h"

using namespace WQt::Cast::Eshare;

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    EshareSinkSession session;

    QObject::connect(&session, &EshareSinkSession::SigLog,
                     [](const QString& text) {
                         qDebug().noquote() << text;
                     });

    QObject::connect(&session, &EshareSinkSession::SigStarted,
                     []() {
                         qDebug().noquote() << "[SINK_TEST] sink session started.";
                     });

    QObject::connect(&session, &EshareSinkSession::SigStopped,
                     [&app]() {
                         qDebug().noquote() << "[SINK_TEST] sink session stopped.";
                         app.quit();
                     });

    QObject::connect(&session, &EshareSinkSession::SigError,
                     [](const QString& text) {
                         qDebug().noquote() << text;
                     });

    session.Start(QStringLiteral("0.0.0.0"));

    qDebug().noquote() << "";
    qDebug().noquote() << "[INPUT] Type 'q' and press Enter to stop.";

    std::thread inputThread([&session]() {
        std::string line;
        while (std::getline(std::cin, line))
        {
            if (line == "q" || line == "Q")
            {
                QMetaObject::invokeMethod(&session, [&session]() {
                    session.Stop();
                }, Qt::QueuedConnection);
                break;
            }
        }
    });

    inputThread.detach();

    return app.exec();
}