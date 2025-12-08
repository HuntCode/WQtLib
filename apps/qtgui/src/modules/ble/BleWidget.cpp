#include "modules/ble/BleWidget.h"

#include <QPushButton>
#include <QTextEdit>
#include <QHBoxLayout>
#include <QVBoxLayout>

BleWidget::BleWidget(QWidget* parent)
    : QWidget(parent)
{
    m_btnStart = new QPushButton(tr("Start Scan"), this);
    m_btnStop  = new QPushButton(tr("Stop Scan"), this);
    m_btnClear = new QPushButton(tr("Clear Log"), this);

    m_logView = new QTextEdit(this);
    m_logView->setReadOnly(true);

    auto* btnLayout = new QHBoxLayout();
    btnLayout->addWidget(m_btnStart);
    btnLayout->addWidget(m_btnStop);
    btnLayout->addStretch();
    btnLayout->addWidget(m_btnClear);

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(btnLayout);
    layout->addWidget(m_logView, 1);
    setLayout(layout);

    connect(m_btnStart, &QPushButton::clicked,
            this, &BleWidget::onStartScan);
    connect(m_btnStop, &QPushButton::clicked,
            this, &BleWidget::onStopScan);
    connect(m_btnClear, &QPushButton::clicked,
            this, &BleWidget::onClearLog);
}

BleWidget::~BleWidget()
{
    if (m_client) {
        wqt_ble_stop_scan(m_client);
        wqt_ble_destroy(m_client);
        m_client = nullptr;
    }
}

void BleWidget::onStartScan()
{
    if (!m_client) {
        m_client = wqt_ble_create();
        if (!m_client) {
            appendLog(tr("Failed to create BLE client"));
            return;
        }
        wqt_ble_set_log_callback(m_client, &BleWidget::logCallback, this);
    }

    if (wqt_ble_start_scan(m_client) == 0) {
        appendLog(tr("BLE scan started (stub)"));
    } else {
        appendLog(tr("BLE scan start failed"));
    }
}

void BleWidget::onStopScan()
{
    if (!m_client)
        return;

    if (wqt_ble_stop_scan(m_client) == 0) {
        appendLog(tr("BLE scan stopped (stub)"));
    } else {
        appendLog(tr("BLE scan stop failed"));
    }
}

void BleWidget::onClearLog()
{
    m_logView->clear();
}

void BleWidget::appendLog(const QString& text)
{
    m_logView->append(text);
}

void BleWidget::logCallback(void* user_data, int level, const char* message)
{
    auto* self = static_cast<BleWidget*>(user_data);
    if (!self || !message) return;

    QString prefix;
    switch (level) {
    case 0:  prefix = "[INFO] "; break;
    case 1:  prefix = "[WARN] "; break;
    default: prefix = "[ERR ] "; break;
    }

    self->appendLog(prefix + QString::fromUtf8(message));
}
