#pragma once

#include <QMetaType>

namespace WQt::Cast::Eshare
{

enum class EshareSessionPhase
{
    Idle,
    Probing8700,
    Starting8121GetServerInfo,
    Starting8121DongleConnected,
    Starting57395,
    Running57395,
    Starting8600,
    Starting51040,
    Running51040,
    ReadyForNextStage,
    Stopping,
    Stopped,
    Error
};

} // namespace WQt::Cast::Eshare

Q_DECLARE_METATYPE(WQt::Cast::Eshare::EshareSessionPhase)