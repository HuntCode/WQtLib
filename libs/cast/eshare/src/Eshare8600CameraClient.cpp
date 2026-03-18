#include "Eshare8600CameraClient.h"

#include <QJsonDocument>
#include <QJsonParseError>

namespace WQt::Cast::Eshare
{

Eshare8600CameraClient::Eshare8600CameraClient(QObject* parent)
    : QObject(parent)
{
    m_socket = new QTcpSocket(this);

    m_idleTimer = new QTimer(this);
    m_idleTimer->setSingleShot(true);

    connect(m_socket, &QTcpSocket::connected,
            this, &Eshare8600CameraClient::OnConnected);
    connect(m_socket, &QTcpSocket::readyRead,
            this, &Eshare8600CameraClient::OnReadyRead);
    connect(m_socket, &QTcpSocket::disconnected,
            this, &Eshare8600CameraClient::OnDisconnected);
    connect(m_socket, &QTcpSocket::errorOccurred,
            this, &Eshare8600CameraClient::OnSocketError);

    connect(m_idleTimer, &QTimer::timeout,
            this, &Eshare8600CameraClient::OnIdleTimeout);
}

void Eshare8600CameraClient::Start(const QString& receiverIp, quint16 port)
{
    ResetState();

    m_receiverIp = receiverIp;
    m_port = port;

    SetStage(Eshare8600Stage::Connecting);
    emit SigLog(QString("[8600] Connecting to %1:%2").arg(receiverIp).arg(port));
    m_socket->connectToHost(receiverIp, port);
}

void Eshare8600CameraClient::Stop()
{
    m_stopping = true;
    m_idleTimer->stop();

    if (m_socket->state() != QAbstractSocket::UnconnectedState)
    {
        emit SigLog("[8600] Disconnecting...");
        m_socket->disconnectFromHost();
    }
    else
    {
        SetStage(Eshare8600Stage::Idle);
        emit SigStopped();
    }
}

void Eshare8600CameraClient::ResetState()
{
    m_recvBuffer.clear();
    m_lastRequest.clear();
    m_stopping = false;
    m_idleTimer->stop();
    m_stage = Eshare8600Stage::Idle;
}

void Eshare8600CameraClient::SetStage(Eshare8600Stage stage)
{
    m_stage = stage;
}

QByteArray Eshare8600CameraClient::BuildCommand(const QByteArray& cmd)
{
    QByteArray out = cmd;
    out += "\r\n";
    return out;
}

QString Eshare8600CameraClient::NormalizeText(const QByteArray& data)
{
    QString text = QString::fromUtf8(data).trimmed();
    text.replace("\r\n", "\n");
    return text;
}

void Eshare8600CameraClient::OnConnected()
{
    emit SigLog("[8600] Connected.");
    SendAvailabilityCheck();
}

void Eshare8600CameraClient::SendAvailabilityCheck()
{
    m_recvBuffer.clear();
    m_lastRequest = BuildCommand("CameraAvailabilityCheck");
    SetStage(Eshare8600Stage::WaitingAvailabilityReply);

    emit SigLog(QString("[8600] >>> SEND (%1 bytes)\n%2")
                .arg(m_lastRequest.size())
                .arg(QString::fromUtf8(m_lastRequest)));

    m_socket->write(m_lastRequest);
    m_socket->flush();
}

void Eshare8600CameraClient::SendStateCheck()
{
    m_recvBuffer.clear();
    m_lastRequest = BuildCommand("CameraStateCheck");
    SetStage(Eshare8600Stage::WaitingStateReply);

    emit SigLog(QString("[8600] >>> SEND (%1 bytes)\n%2")
                .arg(m_lastRequest.size())
                .arg(QString::fromUtf8(m_lastRequest)));

    m_socket->write(m_lastRequest);
    m_socket->flush();
}

void Eshare8600CameraClient::OnReadyRead()
{
    m_recvBuffer += m_socket->readAll();

    emit SigLog(QString("[8600] <<< BUFFER APPEND, total=%1 bytes")
                .arg(m_recvBuffer.size()));

    // 8600 返回都很短，走静默窗口即可
    m_idleTimer->start(50);
}

void Eshare8600CameraClient::OnIdleTimeout()
{
    TryParseBufferedResponse();
}

void Eshare8600CameraClient::TryParseBufferedResponse()
{
    if (m_recvBuffer.isEmpty())
        return;

    if (m_stage == Eshare8600Stage::WaitingAvailabilityReply)
    {
        Eshare8600CameraResult result;
        result.success = true;
        result.rawRequest = m_lastRequest;
        result.rawResponse = m_recvBuffer;
        result.requestText = QString::fromUtf8(m_lastRequest);
        result.responseText = NormalizeText(m_recvBuffer);

        bool ok = false;
        int value = result.responseText.toInt(&ok);
        if (ok)
        {
            result.hasAvailability = true;
            result.availability = value;
        }

        emit SigLog(QString("[8600] <<< RECV (%1 bytes)\n%2")
                    .arg(result.rawResponse.size())
                    .arg(result.responseText));

        emit SigAvailability(result);

        // 第一版最小流程：availability 后立刻查一次 state
        SendStateCheck();
        return;
    }

    if (m_stage == Eshare8600Stage::WaitingStateReply)
    {
        Eshare8600CameraResult result;
        result.success = true;
        result.rawRequest = m_lastRequest;
        result.rawResponse = m_recvBuffer;
        result.requestText = QString::fromUtf8(m_lastRequest);
        result.responseText = NormalizeText(m_recvBuffer);

        QJsonParseError jsonError;
        const QJsonDocument doc = QJsonDocument::fromJson(m_recvBuffer.trimmed(), &jsonError);
        if (jsonError.error == QJsonParseError::NoError && doc.isObject())
        {
            result.hasStateJson = true;
            result.stateJson = doc.object();
            result.statePrettyText = QString::fromUtf8(doc.toJson(QJsonDocument::Indented));

            if (result.stateJson.contains("replayStateCheck"))
                result.replayStateCheck = result.stateJson.value("replayStateCheck").toString();

            if (result.stateJson.contains("currentID"))
                result.currentID = result.stateJson.value("currentID").toInt(-1);

            if (result.stateJson.contains("count"))
                result.count = result.stateJson.value("count").toInt(-1);
        }

        emit SigLog(QString("[8600] <<< RECV (%1 bytes)\n%2")
                    .arg(result.rawResponse.size())
                    .arg(result.hasStateJson ? result.statePrettyText : result.responseText));

        emit SigState(result);

        SetStage(Eshare8600Stage::Ready);
        emit SigReady();
        return;
    }
}

void Eshare8600CameraClient::FinishWithError(const QString& text)
{
    SetStage(Eshare8600Stage::Error);
    emit SigLog(QString("[8600] Error: %1").arg(text));
    emit SigError(text);
}

void Eshare8600CameraClient::OnDisconnected()
{
    m_idleTimer->stop();

    if (m_stopping)
    {
        SetStage(Eshare8600Stage::Idle);
        emit SigStopped();
        return;
    }

    // 如果还没进入 Ready 就断开，视为异常
    if (m_stage != Eshare8600Stage::Ready)
    {
        FinishWithError("Disconnected before camera checks completed.");
        return;
    }

    emit SigLog("[8600] Disconnected.");
    SetStage(Eshare8600Stage::Idle);
    emit SigStopped();
}

void Eshare8600CameraClient::OnSocketError(QAbstractSocket::SocketError)
{
    if (m_stopping)
        return;

    FinishWithError(m_socket->errorString());
}

} // namespace WQt::Cast::Eshare