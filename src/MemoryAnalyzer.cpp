#include "MemoryAnalyzer.h"
#include "AssetManager.h"
#include <QFileInfo>
#include <QSet>
#include <QDir>
#include <algorithm>

static const qint64 FPSC_32BIT_LIMIT_BYTES = 1850LL * 1024LL * 1024LL; // 1.85 GB

static void inspectTextureWithMaps(const QString& texPath, qint64& outDiff, qint64& outNorm, qint64& outSpec, int& outW, int& outH) {
    outDiff = 0; outNorm = 0; outSpec = 0; outW = 0; outH = 0;
    if (texPath.isEmpty()) return;
    QString clean = QString(texPath).replace('\\', '/').toLower();
    int w = 0, h = 0; qint64 r = 0, d = 0;
    if (AssetManager::instance().getTextureMetrics(clean, w, h, r, d)) {
        outDiff = r;
        outW = w;
        outH = h;
    }

    int dot = clean.lastIndexOf('.');
    if (dot > 0) {
        QString stem = clean.left(dot);
        QString ext = clean.mid(dot);

        // Check normal map variants
        QStringList normCandidates = {
            stem + "_n" + ext,
            stem + "_n2" + ext,
            QString(stem).replace("_d2", "_n2") + ext,
            QString(stem).replace("_d2", "_n") + ext,
            QString(stem).replace("_d", "_n") + ext
        };
        for (const auto& nc : normCandidates) {
            if (nc != clean && AssetManager::instance().getFileSizeBytes(nc) > 0) {
                int nw = 0, nh = 0; qint64 nr = 0, nd = 0;
                AssetManager::instance().getTextureMetrics(nc, nw, nh, nr, nd);
                outNorm = nr;
                break;
            }
        }

        // Check specular and illumination map variants
        QStringList specCandidates = {
            stem + "_s" + ext,
            stem + "_s2" + ext,
            stem + "_i" + ext,
            QString(stem).replace("_d2", "_s2") + ext,
            QString(stem).replace("_d2", "_s") + ext,
            QString(stem).replace("_d", "_s") + ext
        };
        for (const auto& sc : specCandidates) {
            if (sc != clean && AssetManager::instance().getFileSizeBytes(sc) > 0) {
                int sw = 0, sh = 0; qint64 sr = 0, sd = 0;
                AssetManager::instance().getTextureMetrics(sc, sw, sh, sr, sd);
                outSpec = sr;
                break;
            }
        }
    }
}

MemoryReport MemoryAnalyzer::analyze(std::shared_ptr<FPSCMap> map) {
    MemoryReport rep;
    if (!map) return rep;

    // =========================================================================
    // 1. SEGMENTS (ROOMS, CORRIDORS, ARCHITECTURE)
    // =========================================================================
    // Count placed blocks per segment bank ID in the 3D grid
    QMap<int, int> segPlacedCounts;
    for (int l = 0; l < map->gridBlocks.size(); ++l) {
        for (int y = 0; y < map->gridBlocks[l].size(); ++y) {
            for (int x = 0; x < map->gridBlocks[l][y].size(); ++x) {
                int segId = map->gridBlocks[l][y][x];
                if (segId > 0) {
                    segPlacedCounts[segId]++;
                    rep.totalPlacedSegmentBlocks++;
                }
            }
        }
    }

    rep.uniqueSegmentTypesCount = map->segments.size();

    QSet<QString> uniqueSegMeshes;
    QSet<QString> uniqueSegTextures;

    for (auto it = map->segments.begin(); it != map->segments.end(); ++it) {
        int segId = it.key();
        const auto& seg = it.value();
        if (!seg) continue;

        SegmentMemoryItem item;
        item.bankId = segId;
        item.name = seg->name;
        item.relPath = seg->relPath;
        item.placedCount = segPlacedCounts.value(segId, 0);
        item.partCount = seg->parts.size();

        // Icon thumbnail (.bmp next to .fps)
        if (!seg->relPath.isEmpty()) {
            QString bmpRel = seg->relPath;
            int dot = bmpRel.lastIndexOf('.');
            if (dot > 0) bmpRel = bmpRel.left(dot) + ".bmp";
            if (AssetManager::instance().getFileSizeBytes(bmpRel) > 0) {
                item.iconBmpPath = bmpRel;
            }
        }

        QSet<QString> itemMeshes;
        QSet<QString> itemTextures;

        for (const auto& p : seg->parts) {
            if (!p.meshName.isEmpty()) {
                QString m = QString(p.meshName).replace('\\', '/').toLower();
                if (!itemMeshes.contains(m)) {
                    itemMeshes.insert(m);
                    qint64 mBytes = AssetManager::instance().getFileSizeBytes(m);
                    item.meshSizeBytes += static_cast<qint64>(mBytes * 1.5); // D3D Vertex + Index Buffers
                    if (!uniqueSegMeshes.contains(m)) {
                        uniqueSegMeshes.insert(m);
                        rep.totalSegmentMeshRamBytes += static_cast<qint64>(mBytes * 1.5);
                    }
                }
            }

            if (!p.texture.isEmpty()) {
                QString t = QString(p.texture).replace('\\', '/').toLower();
                if (!itemTextures.contains(t)) {
                    itemTextures.insert(t);
                    qint64 diff = 0, norm = 0, spec = 0;
                    int tw = 0, th = 0;
                    inspectTextureWithMaps(t, diff, norm, spec, tw, th);

                    item.diffuseRamBytes += diff;
                    item.normalRamBytes += norm;
                    item.specularRamBytes += spec;
                    item.totalTextureRamBytes += (diff + norm + spec);

                    if (!uniqueSegTextures.contains(t)) {
                        uniqueSegTextures.insert(t);
                        rep.totalSegmentDiffuseRamBytes += diff;
                        rep.totalSegmentNormalRamBytes += norm;
                        rep.totalSegmentSpecularRamBytes += spec;
                        rep.totalSegmentTextureRamBytes += (diff + norm + spec);
                    }
                }
            }
        }

        item.uniqueMeshCount = itemMeshes.size();
        item.uniqueTexCount = itemTextures.size();
        item.totalTypeRamBytes = item.meshSizeBytes + item.totalTextureRamBytes;

        // Warnings for heavy segments
        if (item.totalTextureRamBytes >= 16LL * 1024LL * 1024LL) {
            item.warnings.append(QString("Heavy Textures: %1 MB RAM")
                .arg(item.totalTextureRamBytes / (1024.0 * 1024.0), 0, 'f', 1));
        }
        if (item.placedCount >= 100) {
            item.warnings.append(QString("High usage: %1 blocks").arg(item.placedCount));
        }

        rep.segmentItems.append(item);
    }

    // Sort segment items descending by total RAM
    std::sort(rep.segmentItems.begin(), rep.segmentItems.end(), [](const SegmentMemoryItem& a, const SegmentMemoryItem& b) {
        return a.totalTypeRamBytes > b.totalTypeRamBytes;
    });

    rep.totalSegmentRamBytes = rep.totalSegmentMeshRamBytes + rep.totalSegmentTextureRamBytes;

    // =========================================================================
    // 2. ENTITIES (CHARACTERS, PROPS, LIGHTS, WEAPONS)
    // =========================================================================
    rep.totalPlacedEntities = map->placedEntities.size();
    rep.uniqueEntityTypesCount = map->entityProfiles.size();

    QMap<int, int> entCounts;
    for (const auto& ent : map->placedEntities) {
        entCounts[ent.bankIndex]++;
    }

    QSet<QString> uniqueEntMeshes;
    QSet<QString> uniqueEntTextures;
    QSet<QString> uniqueEntAudio;

    for (auto it = map->entityProfiles.begin(); it != map->entityProfiles.end(); ++it) {
        int bankId = it.key();
        const auto& prof = it.value();
        if (!prof) continue;

        EntityMemoryItem item;
        item.bankId = bankId;
        item.name = prof->name;
        item.relPath = prof->relPath;
        item.category = prof->category;
        item.instanceCount = entCounts.value(bankId, 0);
        item.iconBmpPath = prof->iconBmpPath;

        // Model (.X)
        item.modelPath = prof->modelPath;
        if (!prof->modelPath.isEmpty()) {
            float meshMultiplier = prof->isCharacter ? 2.0f : 1.5f;
            item.meshSizeBytes = static_cast<qint64>(prof->meshSizeBytes * meshMultiplier);

            QString m = QString(prof->modelPath).replace('\\', '/').toLower();
            if (!uniqueEntMeshes.contains(m)) {
                uniqueEntMeshes.insert(m);
                rep.totalEntityMeshRamBytes += item.meshSizeBytes;
            }
        }

        // Textures (Diffuse + Normal + Specular)
        item.texturePath = prof->texturePath;
        auto inspectEntTex = [&](const QString& tPath) {
            if (tPath.isEmpty()) return;
            QString t = QString(tPath).replace('\\', '/').toLower();
            qint64 diff = 0, norm = 0, spec = 0;
            int tw = 0, th = 0;
            inspectTextureWithMaps(t, diff, norm, spec, tw, th);

            item.texWidth = tw;
            item.texHeight = th;
            item.textureRamBytes += diff;
            item.normalRamBytes += norm;
            item.specularRamBytes += spec;
            item.textureDiskBytes += AssetManager::instance().getFileSizeBytes(t);

            if (!uniqueEntTextures.contains(t)) {
                uniqueEntTextures.insert(t);
                rep.totalEntityTextureRamBytes += diff;
                rep.totalEntityNormalRamBytes += norm;
                rep.totalEntitySpecularRamBytes += spec;
            }
        };

        inspectEntTex(prof->texturePath);
        inspectEntTex(prof->altTexturePath);

        // Audio sounds
        item.audioPath = prof->soundSet.isEmpty() ? prof->soundSet1 : prof->soundSet;
        auto addAudio = [&](const QString& snd) {
            if (snd.isEmpty()) return;
            QString a = QString(snd).replace('\\', '/').toLower();
            qint64 sz = AssetManager::instance().getFileSizeBytes(a);
            item.audioSizeBytes += sz;
            if (!uniqueEntAudio.contains(a)) {
                uniqueEntAudio.insert(a);
                rep.totalEntityAudioRamBytes += sz;
            }
        };
        addAudio(prof->soundSet);
        addAudio(prof->soundSet1);

        // In DarkBasic Pro (FPSC-Game.exe), CLONE OBJECT duplicates vertex buffers,
        // bone hierarchies, ODE physics dynamic bodies, AI state and shader material stages.
        // Dynamic characters: ~1.7 MB per instance
        // Static scenery/props: ~800 KB per instance
        qint64 perInstanceOverhead = prof->isCharacter ? 1730150 : 819200;
        qint64 baseAssetRam = item.meshSizeBytes + item.textureRamBytes + item.normalRamBytes + item.specularRamBytes + item.audioSizeBytes;
        item.ramPerInstanceBytes = (item.instanceCount > 0) ? (baseAssetRam / item.instanceCount + perInstanceOverhead) : baseAssetRam;
        item.totalTypeRamBytes = baseAssetRam + (static_cast<qint64>(item.instanceCount) * perInstanceOverhead);
        rep.totalEntityInstanceStructBytes += (static_cast<qint64>(item.instanceCount) * perInstanceOverhead);

        // Warning heuristics
        qint64 totalTex = item.textureRamBytes + item.normalRamBytes + item.specularRamBytes;
        if (totalTex >= 12LL * 1024LL * 1024LL) {
            item.warnings.append(QString("Heavy Texture: %1x%2 (%3 MB RAM)")
                .arg(item.texWidth).arg(item.texHeight)
                .arg(totalTex / (1024.0 * 1024.0), 0, 'f', 1));
        }
        if (item.meshSizeBytes >= 3LL * 1024LL * 1024LL) {
            item.warnings.append(QString("Heavy Mesh: %1 MB")
                .arg(item.meshSizeBytes / (1024.0 * 1024.0), 0, 'f', 1));
        }
        if (item.instanceCount >= 30) {
            item.warnings.append(QString("High count: %1 instances").arg(item.instanceCount));
        }

        rep.entityItems.append(item);
        rep.items.append(item); // Alias for backwards compatibility
    }

    // Sort entity items descending by total RAM
    std::sort(rep.entityItems.begin(), rep.entityItems.end(), [](const EntityMemoryItem& a, const EntityMemoryItem& b) {
        return a.totalTypeRamBytes > b.totalTypeRamBytes;
    });
    std::sort(rep.items.begin(), rep.items.end(), [](const EntityMemoryItem& a, const EntityMemoryItem& b) {
        return a.totalTypeRamBytes > b.totalTypeRamBytes;
    });

    rep.totalEntityRamBytes = rep.totalEntityMeshRamBytes + rep.totalEntityTextureRamBytes +
                              rep.totalEntityNormalRamBytes + rep.totalEntitySpecularRamBytes +
                              rep.totalEntityAudioRamBytes + rep.totalEntityInstanceStructBytes;

    // =========================================================================
    // 3. UNIVERSE CSG, LIGHTMAPS & DIRECT3D ENGINE BASELINE
    // =========================================================================
    // Universe CSG BSP and Radiosity Lightmaps scale dynamically with placed segment volume
    float scale = std::max(1.0f, rep.totalPlacedSegmentBlocks / 40.0f);
    rep.universeCsgRamBytes = static_cast<qint64>(std::min(70.0f, 8.0f + scale * 1.8f) * 1024 * 1024);
    rep.lightmapsRamBytes = static_cast<qint64>(std::min(110.0f, 10.0f + scale * 2.5f) * 1024 * 1024);

    // Engine baseline (Weapons loadout, Skybox cubemap, Post-processing Bloom & Shadow Maps, D3D Device, Engine Runtime)
    rep.engineBaselineRamBytes = 235LL * 1024LL * 1024LL;

    // Direct3D 9 Managed Pool Duplicate estimate
    rep.d3dManagedDuplicateBytes = rep.totalSegmentTextureRamBytes + rep.totalEntityTextureRamBytes +
                                   rep.totalSegmentMeshRamBytes + rep.totalEntityMeshRamBytes;

    // Grand Total Estimated RAM
    rep.totalEstimatedRamBytes = rep.totalSegmentRamBytes + rep.totalEntityRamBytes +
                                 rep.universeCsgRamBytes + rep.lightmapsRamBytes + rep.engineBaselineRamBytes;

    rep.engineLimitPercent = (static_cast<float>(rep.totalEstimatedRamBytes) / FPSC_32BIT_LIMIT_BYTES) * 100.0f;

    if (rep.totalEstimatedRamBytes < 400LL * 1024LL * 1024LL) {
        rep.riskLevel = QStringLiteral("Safe (Optimal Memory)");
    } else if (rep.totalEstimatedRamBytes < 900LL * 1024LL * 1024LL) {
        rep.riskLevel = QStringLiteral("Moderate (Normal Level)");
    } else if (rep.totalEstimatedRamBytes < 1400LL * 1024LL * 1024LL) {
        rep.riskLevel = QStringLiteral("Warning (High RAM Usage)");
    } else {
        rep.riskLevel = QStringLiteral("Critical (High Risk of OOM Crash!)");
    }

    // =========================================================================
    // 4. ACTIONABLE OPTIMIZATION TIPS
    // =========================================================================
    int uncompressedTgaCount = 0;
    qint64 potentialDdsSavings = 0;
    for (const auto& itm : rep.entityItems) {
        if (itm.texturePath.toLower().endsWith(".tga") && itm.textureRamBytes > 2LL * 1024LL * 1024LL) {
            uncompressedTgaCount++;
            potentialDdsSavings += static_cast<qint64>(itm.textureRamBytes * 0.75);
        }
    }

    if (uncompressedTgaCount > 0) {
        rep.optimizationTips.append(QString("Convert %1 uncompressed TGA entity textures to DDS (DXT1/DXT5). Estimated RAM savings: %2 MB.")
            .arg(uncompressedTgaCount)
            .arg(potentialDdsSavings / (1024.0 * 1024.0), 0, 'f', 1));
    }

    if (rep.totalPlacedEntities > 250) {
        rep.optimizationTips.append(QString("High entity count (%1 placed). Use dynamic spawn zones or 'spawn at trigger' to conserve runtime object table memory.")
            .arg(rep.totalPlacedEntities));
    }

    if (rep.totalSegmentRamBytes > 200LL * 1024LL * 1024LL) {
        rep.optimizationTips.append(QString("Segment architecture consumes %1 MB RAM across %2 unique types. Re-use existing room segment packs to share texture memory.")
            .arg(rep.totalSegmentRamBytes / (1024.0 * 1024.0), 0, 'f', 1)
            .arg(rep.uniqueSegmentTypesCount));
    }

    if (rep.totalEntityMeshRamBytes > 50LL * 1024LL * 1024LL) {
        rep.optimizationTips.append(QString("Entity 3D Mesh data is heavy (%1 MB). Switch collision modes to primitive colliders (Box/Sphere) in FPE to reduce ODE physics tree RAM.")
            .arg(rep.totalEntityMeshRamBytes / (1024.0 * 1024.0), 0, 'f', 1));
    }

    return rep;
}
