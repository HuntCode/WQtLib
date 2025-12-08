#include "windows/MainWindow.h"
#include "ui_MainWindow.h"

#include "panels/MainPanel.h"

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    // 用 MainPanel 作为中央窗口
    m_mainPanel = new MainPanel(this);
    setCentralWidget(m_mainPanel);

    setWindowTitle(tr("WQtLib"));
}

MainWindow::~MainWindow()
{
    delete ui;
}
