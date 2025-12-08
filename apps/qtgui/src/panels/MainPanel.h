#pragma once

#include <QWidget>

class QListWidget;
class QStackedWidget;
class DialWidget;
class BleWidget;

class MainPanel : public QWidget
{
    Q_OBJECT
public:
    explicit MainPanel(QWidget* parent = nullptr);
    ~MainPanel() override;

private:
    QListWidget*   m_moduleList = nullptr;
    QStackedWidget* m_stack     = nullptr;

    DialWidget*    m_dialWidget = nullptr;
    BleWidget*     m_bleWidget  = nullptr;
};
