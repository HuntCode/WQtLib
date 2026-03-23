#pragma once

#include <QString>

namespace WQt::Cast::Eshare
{

struct Eshare51040PortInfo
{
    int videoDataPort = -1;
    int audioDataPort = -1;
    int mousePort = -1;
    int controlPort = -1;

    int framerate = -1;
    int castingWidth = -1;
    int castingHeight = -1;

    QString videoFormat;
    QString feature;
};

} // namespace WQt::Cast::Eshare