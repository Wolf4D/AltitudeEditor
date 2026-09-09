#include "LeakSuppressionManager.h"
#include "FPMWriter.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>
#include <algorithm>

LeakSuppressionManager::LeakSuppressionManager() {
}

bool LeakSuppressionManager::loadFromMap(const std::shared_ptr<FPSCMap>& map) {
    m_suppressedKeys.clear();
    if (!map) return false;

    if (!map->rawEntries.contains(QStringLiteral("map.leaks.json"))) {
        return false;
    }

    QByteArray data = map->rawEntries.value(QStringLiteral("map.leaks.json"));
    if (data.isEmpty()) return false;

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(data, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        qWarning() << "LeakSuppressionManager: JSON parse error:" << err.errorString();
        return false;
    }

    QJsonObject root = doc.object();
    QJsonArray arr = root.value(QStringLiteral("suppressed")).toArray();
    for (const QJsonValue& val : arr) {
        if (val.isString()) {
            m_suppressedKeys.insert(val.toString());
        } else if (val.isObject()) {
            QString k = val.toObject().value(QStringLiteral("key")).toString();
            if (!k.isEmpty()) {
                m_suppressedKeys.insert(k);
            }
        }
    }

    qDebug() << "LeakSuppressionManager: Loaded" << m_suppressedKeys.size() << "suppressed leaks from map.leaks.json";
    return true;
}

bool LeakSuppressionManager::saveToMap(const std::shared_ptr<FPSCMap>& map, bool autoSaveToFile) {
    if (!map) return false;

    QJsonObject root;
    root[QStringLiteral("version")] = 1;

    QJsonArray arr;
    QStringList sortedKeys = m_suppressedKeys.values();
    std::sort(sortedKeys.begin(), sortedKeys.end());
    for (const QString& k : sortedKeys) {
        arr.append(k);
    }
    root[QStringLiteral("suppressed")] = arr;

    QJsonDocument doc(root);
    QByteArray jsonData = doc.toJson(QJsonDocument::Indented);
    map->rawEntries[QStringLiteral("map.leaks.json")] = jsonData;
    map->isModified = true;

    if (autoSaveToFile && !map->filePath.isEmpty()) {
        bool ok = FPMWriter::saveMap(map, map->filePath, map->password);
        if (ok) {
            qDebug() << "LeakSuppressionManager: Successfully wrote updated .FPM archive with map.leaks.json to" << map->filePath;
        } else {
            qWarning() << "LeakSuppressionManager: Failed to write .FPM to" << map->filePath;
        }
        return ok;
    }

    return true;
}

bool LeakSuppressionManager::isSuppressed(const QString& key) const {
    return m_suppressedKeys.contains(key);
}

bool LeakSuppressionManager::isSuppressed(const PortalLeakWarning& w) const {
    return m_suppressedKeys.contains(w.suppressionKey());
}

void LeakSuppressionManager::suppress(const PortalLeakWarning& w) {
    m_suppressedKeys.insert(w.suppressionKey());
}

void LeakSuppressionManager::unsuppress(const PortalLeakWarning& w) {
    m_suppressedKeys.remove(w.suppressionKey());
}

void LeakSuppressionManager::unsuppressAll() {
    m_suppressedKeys.clear();
}
