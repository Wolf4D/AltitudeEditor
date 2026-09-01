#ifndef MEMORYANALYZER_H
#define MEMORYANALYZER_H

#include "FPSCData.h"
#include <QString>
#include <QVector>
#include <memory>

struct EntityMemoryItem {
    int bankId = 0;
    QString name;
    QString relPath;
    EntityCategory category = EntityCategory::Unknown;
    int instanceCount = 0;

    QString modelPath;
    qint64 meshSizeBytes = 0;

    QString texturePath;
    int texWidth = 0, texHeight = 0;
    qint64 textureRamBytes = 0;
    qint64 textureDiskBytes = 0;

    QString audioPath;
    qint64 audioSizeBytes = 0;

    qint64 ramPerInstanceBytes = 0;
    qint64 totalTypeRamBytes = 0;

    QStringList warnings;
    QString iconBmpPath;
};

struct MemoryReport {
    int totalPlacedEntities = 0;
    int uniqueEntityTypesCount = 0;

    qint64 totalMeshRamBytes = 0;
    qint64 totalTextureRamBytes = 0;
    qint64 totalAudioRamBytes = 0;
    qint64 totalInstanceStructBytes = 0;
    qint64 totalEstimatedRamBytes = 0;
    qint64 d3dManagedDuplicateBytes = 0;

    float engineLimitPercent = 0.0f; // Against 1.85 GB (1894 MB)
    QString riskLevel; // "Safe", "Moderate", "High", "Critical"

    QVector<EntityMemoryItem> items;
    QStringList optimizationTips;
};

class MemoryAnalyzer {
public:
    static MemoryReport analyze(std::shared_ptr<FPSCMap> map);
};

#endif // MEMORYANALYZER_H
