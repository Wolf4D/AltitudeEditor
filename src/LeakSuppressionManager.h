#pragma once

#include <QString>
#include <QSet>
#include <memory>
#include "FPSCData.h"
#include "PortalLeakAnalyzer.h"

class LeakSuppressionManager {
public:
    LeakSuppressionManager();

    // Load suppressed warnings from map->rawEntries["map.leaks.json"]
    bool loadFromMap(const std::shared_ptr<FPSCMap>& map);

    // Save current suppressed warnings into map->rawEntries["map.leaks.json"]
    // If autoSaveToFile is true and map has a valid filePath, writes the updated .FPM archive to disk
    bool saveToMap(const std::shared_ptr<FPSCMap>& map, bool autoSaveToFile = false);

    bool isSuppressed(const QString& key) const;
    bool isSuppressed(const PortalLeakWarning& w) const;

    void suppress(const PortalLeakWarning& w);
    void unsuppress(const PortalLeakWarning& w);
    void unsuppressAll();

    int suppressedCount() const { return m_suppressedKeys.size(); }
    const QSet<QString>& suppressedKeys() const { return m_suppressedKeys; }

private:
    QSet<QString> m_suppressedKeys;
};
