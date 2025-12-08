#pragma once

#include <QWidget>

extern "C" {
#include "wqt_ble.h"
}

class QPushButton;
class QTextEdit;

class BleWidget : public QWidget
{
    Q_OBJECT
public:
    explicit BleWidget(QWidget* parent = nullptr);
    ~BleWidget() override;

private slots:
    void onStartScan();
    void onStopScan();
    void onClearLog();

private:
    void appendLog(const QString& text);
    static void logCallback(void* user_data, int level, const char* message);

private:
    WqtBleClient* m_client = nullptr;

    QPushButton* m_btnStart = nullptr;
    QPushButton* m_btnStop  = nullptr;
    QPushButton* m_btnClear = nullptr;
    QTextEdit*   m_logView  = nullptr;
};
