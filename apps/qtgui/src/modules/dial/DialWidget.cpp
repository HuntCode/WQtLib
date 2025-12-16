#include "modules/dial/DialWidget.h"
#include "ui_DialWidget.h"

#include <QPushButton>
#include <QTextEdit>
#include <QScrollBar>
#include <QMutexLocker>

#include "wqt_dial_server.h"

static QString fmtSession(uint32_t sid)
{
    // 方便看：十六进制 + 0 填充
    return QString("0x%1").arg(sid, 8, 16, QChar('0'));
}

DialWidget::DialWidget(QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::DialWidget)
{
    ui->setupUi(this);

    connect(ui->btnStartDial, &QPushButton::clicked, this, &DialWidget::onStartClicked);
    connect(ui->btnStopDial,  &QPushButton::clicked, this, &DialWidget::onStopClicked);
    connect(ui->btnClearLog, &QPushButton::clicked, this, &DialWidget::onClearLogClicked);

    ui->textLog->setReadOnly(true);

    // 直接创建 C++ 封装对象（QObject 子对象）
    m_server = new WQtDialServer(this);

    // 信号：这些会被投递回 Qt 线程（你 wqt_dial_server.cpp 里已经做了 QueuedConnection）
    connect(m_server, &WQtDialServer::youtubeStart, this, &DialWidget::onYoutubeStart);
    connect(m_server, &WQtDialServer::youtubeStop,  this, &DialWidget::onYoutubeStop);
    connect(m_server, &WQtDialServer::youtubeHide,  this, &DialWidget::onYoutubeHide);

    // 同步状态查询：底层在处理 /apps/YouTube 的 status/hide 时会问这个
    // 这里一定要“快 + 线程安全”
    m_server->setYoutubeStatusCallback([this](uint32_t sessionId, bool& canStop) -> bool {
        canStop = true;
        QMutexLocker lk(&m_sessionsMutex);
        return m_activeSessions.contains(sessionId);
    });

    appendLog(tr("[INFO] DialWidget ready. Click Start to run DIAL server."));
}

DialWidget::~DialWidget()
{
    if (m_server) {
        // 先断开同步回调，避免析构期间被底层线程问状态时访问到 this
        m_server->setYoutubeStatusCallback({});

        // 防御性：你 WQtDialServer 析构里也会 stop/uninit，但这里主动做一遍更直观
        m_server->stop();
        m_server->uninit();

        // m_server 是 this 的子对象，会自动 delete
        m_server = nullptr;
    }

    delete ui;
}

void DialWidget::onStartClicked()
{
    if (!m_server) {
        appendLog(tr("[ERR ] m_server is null."));
        return;
    }

    // 你现在第二阶段只想验证流程：friendly_name 随便先给一个固定值即可
    if (!m_server->isInited()) {
        const QString friendlyName = "WQtLib DIAL"; // 之后你可以改成从 UI 输入框读取
        if (!m_server->init(friendlyName)) {
            appendLog(tr("[ERR ] HHDialInit failed (WQtDialServer::init)."));
            return;
        }
        appendLog(tr("[INFO] init OK, friendly_name=%1").arg(friendlyName));
    }

    if (!m_server->start()) {
        appendLog(tr("[ERR ] HHDialStart failed (WQtDialServer::start)."));
        return;
    }

    appendLog(tr("[INFO] DIAL server started."));
}

void DialWidget::onStopClicked()
{
    if (!m_server) {
        appendLog(tr("[WARN] DIAL server not created."));
        return;
    }

    if (!m_server->isStarted()) {
        appendLog(tr("[WARN] DIAL server not started."));
        return;
    }

    m_server->stop();

    // 测试阶段：服务停了就清空会话集合
    {
        QMutexLocker lk(&m_sessionsMutex);
        m_activeSessions.clear();
    }

    appendLog(tr("[INFO] DIAL server stopped."));
}

void DialWidget::onClearLogClicked()
{
    if (!ui || !ui->textLog) return;
    ui->textLog->clear();
}

void DialWidget::onYoutubeStart(uint32_t sessionId, const QString& url)
{
    {
        QMutexLocker lk(&m_sessionsMutex);
        m_activeSessions.insert(sessionId);
    }

    appendLog(tr("[YT  ] start session=%1").arg(fmtSession(sessionId)));
    appendLog(tr("      url=%1").arg(url));
}

void DialWidget::onYoutubeStop(uint32_t sessionId)
{
    {
        QMutexLocker lk(&m_sessionsMutex);
        m_activeSessions.remove(sessionId);
    }

    appendLog(tr("[YT  ] stop  session=%1").arg(fmtSession(sessionId)));
}

void DialWidget::onYoutubeHide(uint32_t sessionId)
{
    // hide 语义你后面可以再定义：这里先只打印，不改变 running 集合
    appendLog(tr("[YT  ] hide  session=%1").arg(fmtSession(sessionId)));
}

void DialWidget::appendLog(const QString& text)
{
    if (!ui || !ui->textLog) return;

    ui->textLog->append(text);

    auto* bar = ui->textLog->verticalScrollBar();
    if (bar) bar->setValue(bar->maximum());
}
