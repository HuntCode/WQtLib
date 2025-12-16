#pragma once

#include <QWidget>
#include <QString>
#include <cstdint>
#include <mutex>
#include <unordered_set>

QT_BEGIN_NAMESPACE
namespace Ui { class DialWidget; }
QT_END_NAMESPACE

class QWebEngineView;
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
    void ensureWebView(); // 把 QWebEngineView 塞进 videoContainer

private:
    Ui::DialWidget* ui = nullptr;

    WQtDialServer*  m_server = nullptr;
    QWebEngineView* m_view = nullptr;

    // 用于 status 回调：DIAL 的 status/hide 是“网络线程”语境，得线程安全
    std::mutex m_sessionsMu;
    std::unordered_set<uint32_t> m_runningSessions;
};
