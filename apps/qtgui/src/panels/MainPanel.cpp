#include "panels/MainPanel.h"

#include <QListWidget>
#include <QStackedWidget>
#include <QHBoxLayout>

#include "modules/dial/DialWidget.h"
#include "modules/ble/BleWidget.h"

MainPanel::MainPanel(QWidget* parent)
    : QWidget(parent)
{
    m_moduleList = new QListWidget(this);
    m_stack      = new QStackedWidget(this);

    // 左侧列表
    m_moduleList->addItem(tr("DIAL"));
    m_moduleList->addItem(tr("Bluetooth"));

    m_dialWidget = new DialWidget(this);
    m_bleWidget  = new BleWidget(this);

    m_stack->addWidget(m_dialWidget); // index 0
    m_stack->addWidget(m_bleWidget);  // index 1

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->addWidget(m_moduleList);
    layout->addWidget(m_stack, 1);
    setLayout(layout);

    connect(m_moduleList, &QListWidget::currentRowChanged,
            m_stack, &QStackedWidget::setCurrentIndex);

    m_moduleList->setCurrentRow(0);
}

MainPanel::~MainPanel() = default;
