#pragma once

#include <QWidget>

extern "C" {
#include "wqt_dial_server.h"
}

QT_BEGIN_NAMESPACE
namespace Ui { class DialWidget; }
QT_END_NAMESPACE

class DialWidget : public QWidget
{
    Q_OBJECT
public:
    explicit DialWidget(QWidget* parent = nullptr);
    ~DialWidget() override;

private slots:
    void onStartClicked();
    void onStopClicked();
    void onClearLogClicked();

private:
    void appendLog(const QString& text);
    static void logCallback(void* user_data, int level, const char* message);

private:
    Ui::DialWidget* ui = nullptr;
    WqtDialServer*  m_server = nullptr;
};
