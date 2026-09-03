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
    qint64 normalRamBytes = 0;
    qint64 specularRamBytes = 0;
    qint64 textureDiskBytes = 0;

    QString audioPath;
    qint64 audioSizeBytes = 0;

    qint64 ramPerInstanceBytes = 0;
    qint64 totalTypeRamBytes = 0;

    QStringList warnings;
    QString iconBmpPath;
};

struct SegmentMemoryItem {
    int bankId = 0;
    QString name;
    QString relPath;
    int placedCount = 0;
    int partCount = 0;

    int uniqueMeshCount = 0;
    qint64 meshSizeBytes = 0;

    int uniqueTexCount = 0;
    qint64 diffuseRamBytes = 0;
    qint64 normalRamBytes = 0;
    qint64 specularRamBytes = 0;
    qint64 totalTextureRamBytes = 0;

    qint64 totalTypeRamBytes = 0;
    QString iconBmpPath;
    QStringList warnings;
};

struct MemoryReport {
    // Entities
    int totalPlacedEntities = 0;
    int uniqueEntityTypesCount = 0;
    qint64 totalEntityMeshRamBytes = 0;
    qint64 totalEntityTextureRamBytes = 0;
    qint64 totalEntityNormalRamBytes = 0;
    qint64 totalEntitySpecularRamBytes = 0;
    qint64 totalEntityAudioRamBytes = 0;
    qint64 totalEntityInstanceStructBytes = 0;
    qint64 totalEntityRamBytes = 0;

    // Segments
    int totalPlacedSegmentBlocks = 0;
    int uniqueSegmentTypesCount = 0;
    qint64 totalSegmentMeshRamBytes = 0;
    qint64 totalSegmentDiffuseRamBytes = 0;
    qint64 totalSegmentNormalRamBytes = 0;
    qint64 totalSegmentSpecularRamBytes = 0;
    qint64 totalSegmentTextureRamBytes = 0;
    qint64 totalSegmentRamBytes = 0;

    // Universe, Lightmaps, Engine Baseline
    qint64 universeCsgRamBytes = 0;
    qint64 lightmapsRamBytes = 0;
    qint64 engineBaselineRamBytes = 0;

    // Grand Totals
    qint64 totalEstimatedRamBytes = 0;
    qint64 d3dManagedDuplicateBytes = 0;
    float engineLimitPercent = 0.0f; // Against 1.85 GB (1894 MB)
    QString riskLevel; // "Safe", "Moderate", "Warning", "Critical"

    // Backward compatibility for any code accessing rep.items
    QVector<EntityMemoryItem> items;
    QVector<EntityMemoryItem> entityItems;
    QVector<SegmentMemoryItem> segmentItems;
    QStringList optimizationTips;
};

class MemoryAnalyzer {
public:
    static MemoryReport analyze(std::shared_ptr<FPSCMap> map);
};

#endif // MEMORYANALYZER_H
