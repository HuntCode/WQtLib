#include "Eshare8700ProbeClient.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

namespace WQt::Cast::Eshare
{

Eshare8700ProbeClient::Eshare8700ProbeClient(QObject* parent)
    : QObject(parent)
{
    m_socket = new QTcpSocket(this);

    connect(m_socket, &QTcpSocket::connected,
            this, &Eshare8700ProbeClient::OnConnected);
    connect(m_socket, &QTcpSocket::readyRead,
            this, &Eshare8700ProbeClient::OnReadyRead);
    connect(m_socket, &QTcpSocket::disconnected,
            this, &Eshare8700ProbeClient::OnDisconnected);
    connect(m_socket, &QTcpSocket::errorOccurred,
            this, &Eshare8700ProbeClient::OnSocketError);
}

void Eshare8700ProbeClient::Probe(const QString& receiverIp, quint16 port, int cseq)
{
    ResetState();

    m_receiverIp = receiverIp;
    m_port = port;
    m_cseq = cseq;

    emit SigLog(QString("[8700] Connecting to %1:%2").arg(receiverIp).arg(port));
    m_socket->connectToHost(receiverIp, port);
}

void Eshare8700ProbeClient::Abort()
{
    if (m_socket->state() != QAbstractSocket::UnconnectedState)
    {
        m_finished = true;
        m_socket->abort();
    }
}

void Eshare8700ProbeClient::ResetState()
{
    m_recvBuffer.clear();
    m_lastRequest.clear();
    m_finished = false;
}

QByteArray Eshare8700ProbeClient::BuildOptionsRequest(const QString& receiverIp, int cseq) const
{
    RtspLiteMessage req;
    req.startLine = QString("OPTIONS rtsp://%1 RTSP/1.0").arg(receiverIp);
    req.setHeader("CSeq", QString::number(cseq));
    req.setHeader("Version", "20231026");
    req.body.clear();
    return RtspLiteCodec::Encode(req);
}

void Eshare8700ProbeClient::OnConnected()
{
    m_lastRequest = BuildOptionsRequest(m_receiverIp, m_cseq);

    emit SigLog(QString("[8700] >>> SEND (%1 bytes)\n%2")
                .arg(m_lastRequest.size())
                .arg(QString::fromUtf8(m_lastRequest)));

    m_socket->write(m_lastRequest);
    m_socket->flush();
}

void Eshare8700ProbeClient::OnReadyRead()
{
    m_recvBuffer += m_socket->readAll();
    TryParseResponse();
}

void Eshare8700ProbeClient::TryParseResponse()
{
    if (m_finished)
        return;

    RtspLiteMessage resp;
    QByteArray rawResp;
    QString error;

    if (!RtspLiteCodec::TryDecode(m_recvBuffer, resp, &rawResp, &error))
        return;

    Eshare8700ProbeResult result;
    result.rawRequest = m_lastRequest;
    result.rawResponse = rawResp;
    result.requestText = QString::fromUtf8(m_lastRequest);
    result.responseText = RtspLiteCodec::MessageToDebugString(resp);
    result.responseBodyReadable = RtspLiteCodec::BodyToReadableString(resp.body);
    result.statusText = resp.startLine;

    // 解析状态行：RTSP/1.0 200 OK
    QRegularExpression re(R"(^RTSP/\d+\.\d+\s+(\d+)\s*(.*)$)");
    QRegularExpressionMatch match = re.match(resp.startLine);
    if (match.hasMatch())
    {
        result.statusCode = match.captured(1).toInt();
        result.statusText = match.captured(2).trimmed();
    }

    bool ok = false;
    result.cseq = resp.headerValue("CSeq").toInt(&ok);
    if (!ok)
        result.cseq = -1;

    QJsonParseError jsonError;
    QJsonDocument doc = QJsonDocument::fromJson(resp.body, &jsonError);
    if (jsonError.error == QJsonParseError::NoError && doc.isObject())
    {
        result.jsonBody = doc.object();

        if (result.jsonBody.contains("byom_tx_avalible"))
        {
            result.hasByomTxAvailable = true;
            result.byomTxAvailable = result.jsonBody.value("byom_tx_avalible").toString().toInt();
        }
    }

    result.success = (result.statusCode == 200);

    emit SigLog(QString("[8700] <<< RECV (%1 bytes)\n%2")
                .arg(rawResp.size())
                .arg(result.responseText));

    emit SigLog(QString("[8700] Parsed: status=%1 cseq=%2 byom_tx_avalible=%3")
                .arg(result.statusCode)
                .arg(result.cseq)
                .arg(result.hasByomTxAvailable ? QString::number(result.byomTxAvailable) : "N/A"));

    m_finished = true;
    emit SigFinished(result);

    m_socket->disconnectFromHost();
}

void Eshare8700ProbeClient::FinishWithError(const QString& text)
{
    if (m_finished)
        return;

    Eshare8700ProbeResult result;
    result.success = false;
    result.errorText = text;
    result.rawRequest = m_lastRequest;
    result.requestText = QString::fromUtf8(m_lastRequest);

    m_finished = true;

    emit SigLog(QString("[8700] Error: %1").arg(text));
    emit SigFinished(result);
}

void Eshare8700ProbeClient::OnSocketError(QAbstractSocket::SocketError)
{
    FinishWithError(m_socket->errorString());
}

void Eshare8700ProbeClient::OnDisconnected()
{
    if (!m_finished && m_recvBuffer.isEmpty())
    {
        FinishWithError("Disconnected before complete response.");
    }
}

} // namespace WQt::Cast::Eshare