#include "MemoryAnalyzer.h"
#include "AssetManager.h"
#include <QFileInfo>
#include <algorithm>

static const qint64 FPSC_32BIT_LIMIT_BYTES = 1850LL * 1024LL * 1024LL; // 1.85 GB

MemoryReport MemoryAnalyzer::analyze(std::shared_ptr<FPSCMap> map) {
    MemoryReport rep;
    if (!map) return rep;

    rep.totalPlacedEntities = map->placedEntities.size();

    // Count instances per bank ID
    QMap<int, int> instanceCounts;
    for (const auto& ent : map->placedEntities) {
        instanceCounts[ent.bankIndex]++;
    }

    rep.uniqueEntityTypesCount = map->entityProfiles.size();

    for (auto it = map->entityProfiles.begin(); it != map->entityProfiles.end(); ++it) {
        int bankId = it.key();
        const auto& prof = it.value();
        if (!prof) continue;

        EntityMemoryItem item;
        item.bankId = bankId;
        item.name = prof->name;
        item.relPath = prof->relPath;
        item.category = prof->category;
        item.instanceCount = instanceCounts.value(bankId, 0);
        item.iconBmpPath = prof->iconBmpPath;

        item.modelPath = prof->modelPath;
        item.meshSizeBytes = prof->meshSizeBytes;

        item.texturePath = prof->texturePath;
        item.texWidth = prof->texWidth;
        item.texHeight = prof->texHeight;
        item.textureRamBytes = prof->diffuseSizeBytes;
        item.textureDiskBytes = AssetManager::instance().getFileSizeBytes(prof->texturePath);

        item.audioPath = prof->soundSet.isEmpty() ? prof->soundSet1 : prof->soundSet;
        item.audioSizeBytes = prof->audioSizeBytes;

        // Base memory for the asset data (loaded once into RAM / VRAM)
        qint64 baseAssetRam = item.meshSizeBytes + item.textureRamBytes + prof->otherTexturesSizeBytes + item.audioSizeBytes;

        // Per-instance overhead: ~2.5 KB for state, matrix, AI node, ODE body
        qint64 perInstanceOverhead = 2560;
        item.ramPerInstanceBytes = (item.instanceCount > 0) ? (baseAssetRam / item.instanceCount + perInstanceOverhead) : baseAssetRam;
        item.totalTypeRamBytes = baseAssetRam + (static_cast<qint64>(item.instanceCount) * perInstanceOverhead);

        // Warning heuristics
        if (item.textureRamBytes >= 16LL * 1024LL * 1024LL) {
            item.warnings.append(QString("Heavy Texture: %1x%2 (%3 MB RAM)")
                .arg(item.texWidth).arg(item.texHeight)
                .arg(item.textureRamBytes / (1024.0 * 1024.0), 0, 'f', 1));
        }
        if (item.meshSizeBytes >= 3LL * 1024LL * 1024LL) {
            item.warnings.append(QString("Heavy Mesh: %1 MB")
                .arg(item.meshSizeBytes / (1024.0 * 1024.0), 0, 'f', 1));
        }
        if (item.instanceCount >= 40) {
            item.warnings.append(QString("High count: %1 instances").arg(item.instanceCount));
        }

        // Accumulate totals
        rep.totalMeshRamBytes += item.meshSizeBytes;
        rep.totalTextureRamBytes += item.textureRamBytes;
        rep.totalAudioRamBytes += item.audioSizeBytes;
        rep.totalInstanceStructBytes += (static_cast<qint64>(item.instanceCount) * perInstanceOverhead);
        rep.totalEstimatedRamBytes += item.totalTypeRamBytes;

        rep.items.append(item);
    }

    // Sort items by total RAM descending
    std::sort(rep.items.begin(), rep.items.end(), [](const EntityMemoryItem& a, const EntityMemoryItem& b) {
        return a.totalTypeRamBytes > b.totalTypeRamBytes;
    });

    // D3D9 Managed Pool duplicate estimate
    rep.d3dManagedDuplicateBytes = rep.totalTextureRamBytes + rep.totalMeshRamBytes;

    rep.engineLimitPercent = (static_cast<float>(rep.totalEstimatedRamBytes) / FPSC_32BIT_LIMIT_BYTES) * 100.0f;

    if (rep.totalEstimatedRamBytes < 400LL * 1024LL * 1024LL) {
        rep.riskLevel = QStringLiteral("Safe (Low Memory)");
    } else if (rep.totalEstimatedRamBytes < 1000LL * 1024LL * 1024LL) {
        rep.riskLevel = QStringLiteral("Moderate (Normal Level)");
    } else if (rep.totalEstimatedRamBytes < 1500LL * 1024LL * 1024LL) {
        rep.riskLevel = QStringLiteral("Warning (High RAM)");
    } else {
        rep.riskLevel = QStringLiteral("Critical (Risk of OOM Crash!)");
    }

    // Generate Optimization Advice
    int uncompressedTgaCount = 0;
    qint64 potentialDdsSavings = 0;
    for (const auto& itm : rep.items) {
        if (itm.texturePath.toLower().endsWith(".tga") && itm.textureRamBytes > 2LL * 1024LL * 1024LL) {
            uncompressedTgaCount++;
            potentialDdsSavings += static_cast<qint64>(itm.textureRamBytes * 0.75); // 75% savings with DXT1/5
        }
    }

    if (uncompressedTgaCount > 0) {
        rep.optimizationTips.append(QString("Convert %1 uncompressed TGA textures to DDS (DXT1/DXT5). Estimated RAM savings: %2 MB.")
            .arg(uncompressedTgaCount)
            .arg(potentialDdsSavings / (1024.0 * 1024.0), 0, 'f', 1));
    }

    if (rep.totalPlacedEntities > 200) {
        rep.optimizationTips.append(QString("High entity count (%1). Consider using 'instance object' instead of 'clone object' or dynamic entity activation zones.")
            .arg(rep.totalPlacedEntities));
    }

    if (rep.totalMeshRamBytes > 100LL * 1024LL * 1024LL) {
        rep.optimizationTips.append(QString("3D Mesh data is heavy (%1 MB). Switch collision mode to primitive colliders (Box/Sphere) in FPE to reduce ODE physics tree RAM.")
            .arg(rep.totalMeshRamBytes / (1024.0 * 1024.0), 0, 'f', 1));
    }

    return rep;
}
