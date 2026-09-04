#pragma once

#include <QString>
#include "BuildNumber.h"

namespace VersionInfo {
    inline const QString AppName     = QStringLiteral("Altitude Editor");
    inline const QString BaseVersion = QStringLiteral("0.9.0b");
    inline const int     BuildNumber = BUILD_NUMBER;
    inline const QString Version     = QStringLiteral("0.9.0b (build %1)").arg(BUILD_NUMBER);
    inline const QString AppSubtitle = QStringLiteral("FPS Creator map editor");
    inline const QString Developer   = QStringLiteral("Ivan Klenov (aka NavY LiK)");
    inline const QString Studio      = QStringLiteral("Madness Studio");

    inline QString fullTitle() {
        return QString("%1 v%2 - %3").arg(AppName, Version, AppSubtitle);
    }

    inline QString shortTitle() {
        return QString("%1 v%2").arg(AppName, Version);
    }

    inline QString mapTitle(const QString& mapFileName, bool isModified = false) {
        return QString("%1 v%2 - [%3%4]").arg(AppName, Version, mapFileName, isModified ? QStringLiteral("*") : QStringLiteral(""));
    }
}
