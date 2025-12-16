#pragma once

#include <QWidget>
#include <QSet>
#include <QMutex>

QT_BEGIN_NAMESPACE
namespace Ui { class DialWidget; }
QT_END_NAMESPACE

class WQtDialServer;

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

    void onYoutubeStart(uint32_t sessionId, const QString& url);
    void onYoutubeStop(uint32_t sessionId);
    void onYoutubeHide(uint32_t sessionId);

private:
    void appendLog(const QString& text);

private:
    Ui::DialWidget* ui = nullptr;
    WQtDialServer*  m_server = nullptr;

    // 仅用于测试：用 sessionId 维护一个“运行中集合”，提供给 status 回调同步查询
    QMutex m_sessionsMutex;
    QSet<uint32_t> m_activeSessions;
};
