#include "modules/dial/DialWidget.h"
#include "ui_DialWidget.h"

#include "wqt_dial_server.h"   // 注意：现在是 C++ 类，不要再 extern "C"

#include <QPushButton>
#include <QTextEdit>
#include <QScrollBar>
#include <QDateTime>
#include <QVBoxLayout>
#include <QUrl>

#include <QtWebEngineWidgets/QWebEngineView>
#include <QtWebEngineCore/QWebEngineProfile>
#include <QtWebEngineCore/QWebEngineSettings>

static QString nowTag()
{
    return QDateTime::currentDateTime().toString("HH:mm:ss.zzz");
}

DialWidget::DialWidget(QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::DialWidget)
{
    ui->setupUi(this);

    connect(ui->btnStartDial, &QPushButton::clicked, this, &DialWidget::onStartClicked);
    connect(ui->btnStopDial,  &QPushButton::clicked, this, &DialWidget::onStopClicked);
    connect(ui->btnClearLog,  &QPushButton::clicked, this, &DialWidget::onClearLogClicked);

    ui->textLog->setReadOnly(true);

    ensureWebView();

    // 创建 DIAL server（QObject，挂到 DialWidget 下面自动析构）
    m_server = new WQtDialServer(this);

    // Qt 信号：已经在 wqt_dial_server.cpp 里做了 QueuedConnection 投递到 Qt 线程
    connect(m_server, &WQtDialServer::youtubeStart, this, &DialWidget::onYoutubeStart);
    connect(m_server, &WQtDialServer::youtubeStop,  this, &DialWidget::onYoutubeStop);
    connect(m_server, &WQtDialServer::youtubeHide,  this, &DialWidget::onYoutubeHide);

    // 状态回调：必须同步/快速/线程安全（别直接碰 UI）
    m_server->setYoutubeStatusCallback([this](uint32_t sessionId, bool& canStop) -> bool {
        canStop = true;
        std::lock_guard<std::mutex> lk(m_sessionsMu);
        return (m_runningSessions.find(sessionId) != m_runningSessions.end());
    });

    appendLog(QString("[%1] DialWidget ready.").arg(nowTag()));
}

DialWidget::~DialWidget()
{
    // m_server 是 QObject child，会自动析构；它自己的析构里也会 stop/uninit（双保险）
    delete ui;
}

void DialWidget::ensureWebView()
{
    if (m_view) return;

    // ui 里 videoContainer 是 native=true 且没 layout，需要我们自己补一个 layout 再塞控件进去
    if (!ui->videoContainer->layout()) {
        auto* lay = new QVBoxLayout(ui->videoContainer);
        lay->setContentsMargins(0, 0, 0, 0);
        lay->setSpacing(0);
        ui->videoContainer->setLayout(lay);
    }

    m_view = new QWebEngineView(ui->videoContainer);
    ui->videoContainer->layout()->addWidget(m_view);

    // 让 YouTube TV 更像“客厅端”
    m_view->page()->profile()->setHttpUserAgent(
        "Mozilla/5.0 (PlayStation 3; 3.55) AppleWebKit/531.22.8 "
        "(KHTML, like Gecko) Version/4.0 Safari/531.22.8");

    // 测试阶段尽量别被手势策略卡住（后面你可以再收紧）
    m_view->settings()->setAttribute(QWebEngineSettings::PlaybackRequiresUserGesture, false);

    m_view->setUrl(QUrl("about:blank"));
}

void DialWidget::onStartClicked()
{
    if (!m_server) return;

    // 先 init（你 libs/dial 的 init() 已经决定：init 后不热改参数）
    if (!m_server->isInited()) {
        const bool ok = m_server->init("WQtLib DIAL");
        if (!ok) {
            appendLog(QString("[%1] [ERR ] DIAL init failed.").arg(nowTag()));
            return;
        }
        appendLog(QString("[%1] [INFO] DIAL inited.").arg(nowTag()));
    }

    if (m_server->start()) {
        appendLog(QString("[%1] [INFO] DIAL started.").arg(nowTag()));
    } else {
        appendLog(QString("[%1] [ERR ] DIAL start failed.").arg(nowTag()));
    }
}

void DialWidget::onStopClicked()
{
    if (!m_server) return;

    m_server->stop();

    {
        std::lock_guard<std::mutex> lk(m_sessionsMu);
        m_runningSessions.clear();
    }

    appendLog(QString("[%1] [INFO] DIAL stopped.").arg(nowTag()));
}

void DialWidget::onClearLogClicked()
{
    ui->textLog->clear();
}

void DialWidget::onYoutubeStart(uint32_t sessionId, const QString& url)
{
    {
        std::lock_guard<std::mutex> lk(m_sessionsMu);
        m_runningSessions.insert(sessionId);
    }

    appendLog(QString("[%1] [YT  ] start sid=%2 url=%3")
                  .arg(nowTag()).arg(sessionId).arg(url));

    ensureWebView();
    m_view->setUrl(QUrl(url));
}

void DialWidget::onYoutubeStop(uint32_t sessionId)
{
    {
        std::lock_guard<std::mutex> lk(m_sessionsMu);
        m_runningSessions.erase(sessionId);
    }

    appendLog(QString("[%1] [YT  ] stop  sid=%2").arg(nowTag()).arg(sessionId));
}

void DialWidget::onYoutubeHide(uint32_t sessionId)
{
    appendLog(QString("[%1] [YT  ] hide  sid=%2").arg(nowTag()).arg(sessionId));
}

void DialWidget::appendLog(const QString& text)
{
    ui->textLog->append(text);
    if (auto* bar = ui->textLog->verticalScrollBar()) {
        bar->setValue(bar->maximum());
    }
}
