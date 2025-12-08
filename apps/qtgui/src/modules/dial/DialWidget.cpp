#include "modules/dial/DialWidget.h"
#include "ui_DialWidget.h"

#include <QPushButton>
#include <QTextEdit>
#include <QScrollBar>

DialWidget::DialWidget(QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::DialWidget)
{
    ui->setupUi(this);

    // 连接按钮信号
    connect(ui->btnStartDial, &QPushButton::clicked,
            this, &DialWidget::onStartClicked);
    connect(ui->btnStopDial, &QPushButton::clicked,
            this, &DialWidget::onStopClicked);
    connect(ui->btnClearLog, &QPushButton::clicked,
            this, &DialWidget::onClearLogClicked);

    // 日志区只读（在 ui 里应该已经勾了，这里再保险一下）
    ui->textLog->setReadOnly(true);

    appendLog(tr("DIAL widget initialized (stub implementation)."));
}

DialWidget::~DialWidget()
{
    if (m_server) {
        wqt_dial_server_stop(m_server);
        wqt_dial_server_destroy(m_server);
        m_server = nullptr;
    }
    delete ui;
}

void DialWidget::onStartClicked()
{
    if (!m_server) {
        // 先创建 server（目前是桩实现，后面再换成 dial-reference 真正逻辑）
        m_server = wqt_dial_server_create(
            "WQtLib DIAL",
            "dummy-uuid-1234",  // 以后可以换成真实 UUID
            56789                // 本地 HTTP 端口占位
            );

        if (!m_server) {
            appendLog(tr("[ERR ] Failed to create DIAL server instance."));
            return;
        }

        wqt_dial_server_set_log_callback(
            m_server,
            &DialWidget::logCallback,
            this
            );
    }

    int rc = wqt_dial_server_start(m_server);
    if (rc == 0) {
        appendLog(tr("[INFO] DIAL server started."));
    } else {
        appendLog(tr("[ERR ] DIAL server start failed, rc=%1").arg(rc));
    }
}

void DialWidget::onStopClicked()
{
    if (!m_server) {
        appendLog(tr("[WARN] DIAL server is not running."));
        return;
    }

    int rc = wqt_dial_server_stop(m_server);
    if (rc == 0) {
        appendLog(tr("[INFO] DIAL server stopped."));
    } else {
        appendLog(tr("[ERR ] DIAL server stop failed, rc=%1").arg(rc));
    }
}

void DialWidget::onClearLogClicked()
{
    if (!ui || !ui->textLog)
        return;

    ui->textLog->clear();
}

void DialWidget::appendLog(const QString& text)
{
    if (!ui || !ui->textLog)
        return;

    ui->textLog->append(text);

    // 自动滚到底部，方便看最新日志
    auto* bar = ui->textLog->verticalScrollBar();
    if (bar)
        bar->setValue(bar->maximum());
}

/* static */
void DialWidget::logCallback(void* user_data, int level, const char* message)
{
    auto* self = static_cast<DialWidget*>(user_data);
    if (!self || !message)
        return;

    QString prefix;
    switch (level) {
    case 0:  prefix = "[INFO] "; break;
    case 1:  prefix = "[WARN] "; break;
    default: prefix = "[ERR ] "; break;
    }

    self->appendLog(prefix + QString::fromUtf8(message));
}
