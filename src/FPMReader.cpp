#include "FPMReader.h"
#include "SegmentParser.h"
#include "EntityParser.h"
#include "miniz_deflate.h"
#include <QFile>
#include <QFileInfo>
#include <QDataStream>
#include <QDebug>
#include <cstring>

#pragma pack(push, 1)
struct ZipLocalHeader {
    uint32_t signature; // 0x04034b50
    uint16_t version;
    uint16_t flags;
    uint16_t method;
    uint16_t modTime;
    uint16_t modDate;
    uint32_t crc32;
    uint32_t compSize;
    uint32_t uncompSize;
    uint16_t nameLen;
    uint16_t extraLen;
};

struct ZipCentralDirHeader {
    uint32_t signature; // 0x02014b50
    uint16_t verMade;
    uint16_t verNeed;
    uint16_t flags;
    uint16_t method;
    uint16_t modTime;
    uint16_t modDate;
    uint32_t crc32;
    uint32_t compSize;
    uint32_t uncompSize;
    uint16_t nameLen;
    uint16_t extraLen;
    uint16_t commentLen;
    uint16_t diskStart;
    uint16_t intAttr;
    uint32_t extAttr;
    uint32_t localOffset;
};

struct ZipEOCD {
    uint32_t signature; // 0x06054b50
    uint16_t diskNum;
    uint16_t cdDisk;
    uint16_t numEntriesDisk;
    uint16_t numEntries;
    uint32_t cdSize;
    uint32_t cdOffset;
    uint16_t commentLen;
};
#pragma pack(pop)

bool FPMReader::extractZipEntries(
    const QString& zipPath,
    const QString& password,
    QMap<QString, QByteArray>& outEntries)
{
    QFile file(zipPath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Failed to open FPM file:" << zipPath;
        return false;
    }

    QByteArray fileData = file.readAll();
    file.close();

    const uint8_t* raw = reinterpret_cast<const uint8_t*>(fileData.constData());
    size_t fileSize = fileData.size();
    if (fileSize < sizeof(ZipEOCD)) return false;

    // Search for EOCD from end of file
    int eocdPos = -1;
    for (int i = static_cast<int>(fileSize) - sizeof(ZipEOCD); i >= 0 && i >= static_cast<int>(fileSize) - 65557; --i) {
        if (*reinterpret_cast<const uint32_t*>(raw + i) == 0x06054b50) {
            eocdPos = i;
            break;
        }
    }
    if (eocdPos < 0) return false;

    const ZipEOCD* eocd = reinterpret_cast<const ZipEOCD*>(raw + eocdPos);
    uint32_t cdOffset = eocd->cdOffset;
    uint16_t numEntries = eocd->numEntries;

    size_t curCD = cdOffset;
    for (int entryIdx = 0; entryIdx < numEntries; ++entryIdx) {
        if (curCD + sizeof(ZipCentralDirHeader) > fileSize) break;

        const ZipCentralDirHeader* cd = reinterpret_cast<const ZipCentralDirHeader*>(raw + curCD);
        if (cd->signature != 0x02014b50) break;

        QString name = QString::fromLatin1(reinterpret_cast<const char*>(raw + curCD + sizeof(ZipCentralDirHeader)), cd->nameLen);
        curCD += sizeof(ZipCentralDirHeader) + cd->nameLen + cd->extraLen + cd->commentLen;

        uint32_t locOffset = cd->localOffset;
        if (locOffset + sizeof(ZipLocalHeader) > fileSize) continue;

        const ZipLocalHeader* loc = reinterpret_cast<const ZipLocalHeader*>(raw + locOffset);
        if (loc->signature != 0x04034b50) continue;

        const uint8_t* compData = raw + locOffset + sizeof(ZipLocalHeader) + loc->nameLen + loc->extraLen;
        uint32_t compSize = loc->compSize;
        if (compSize == 0) compSize = cd->compSize;
        uint32_t uncompSize = loc->uncompSize;
        if (uncompSize == 0) uncompSize = cd->uncompSize;

        bool isEncrypted = (loc->flags & 1) != 0;

        std::vector<uint8_t> decryptedData;
        const uint8_t* payload = compData;
        size_t payloadSize = compSize;

        if (isEncrypted) {
            if (compSize < 12) continue;
            ZipUtils::ZipCryptoKey key;
            key.init(password.toStdString());

            decryptedData.resize(compSize);
            for (size_t k = 0; k < compSize; ++k) {
                decryptedData[k] = key.decrypt(compData[k]);
            }
            // First 12 bytes are encryption header
            payload = decryptedData.data() + 12;
            payloadSize = compSize - 12;
        }

        QByteArray entryBytes;
        if (loc->method == 0) { // Stored
            entryBytes = QByteArray(reinterpret_cast<const char*>(payload), payloadSize);
        } else if (loc->method == 8) { // Deflated
            std::vector<uint8_t> decomp;
            if (ZipUtils::DeflateDecompressor::decompress(payload, payloadSize, decomp, uncompSize)) {
                entryBytes = QByteArray(reinterpret_cast<const char*>(decomp.data()), decomp.size());
            } else {
                qWarning() << "Deflate decompression failed for entry:" << name;
            }
        }

        outEntries[name.toLower()] = entryBytes;
    }

    return !outEntries.isEmpty();
}

std::shared_ptr<FPSCMap> FPMReader::loadMap(const QString& fpmPath, const QString& password, ProgressCallback progressCallback) {
    if (progressCallback) progressCallback(5, QStringLiteral("Opening map archive..."));

    auto map = std::make_shared<FPSCMap>();
    map->filePath = fpmPath;
    map->mapName = QFileInfo(fpmPath).completeBaseName();

    QMap<QString, QByteArray> entries;
    if (!extractZipEntries(fpmPath, password, entries)) {
        qWarning() << "Failed to extract FPM entries from:" << fpmPath;
        return nullptr;
    }

    if (progressCallback) progressCallback(15, QStringLiteral("Reading map header..."));

    // 1. Parse header.dat
    if (entries.contains("header.dat")) {
        const QByteArray& hData = entries["header.dat"];
        if (hData.size() >= 16) {
            const int32_t* h = reinterpret_cast<const int32_t*>(hData.constData());
            map->header.layerMax = h[0];
            map->header.maxX = h[1];
            map->header.maxY = h[2];
            map->header.tileSize = h[3];
            if (hData.size() >= 20) map->header.olayListMax = h[4];
            if (hData.size() >= 24) map->header.multiplayer = h[5];
        }
    } else {
        map->header.layerMax = 20;
        map->header.maxX = 40;
        map->header.maxY = 40;
        map->header.tileSize = 100;
    }

    int layers = map->header.layerMax + 1;
    int rows = map->header.maxY + 1;
    int cols = map->header.maxX + 1;

    // Allocate 3D grid
    map->gridBlocks.resize(layers);
    map->gridRotation.resize(layers);
    map->gridOrientation.resize(layers);
    map->gridTileType.resize(layers);
    map->gridGround.resize(layers);
    map->gridSymbol.resize(layers);
    map->gridOverlays.resize(layers);
    map->gridOverlayRotation.resize(layers);
    map->gridTileOverlays.resize(layers);
    for (int l = 0; l < layers; ++l) {
        map->gridBlocks[l].resize(rows);
        map->gridRotation[l].resize(rows);
        map->gridOrientation[l].resize(rows);
        map->gridTileType[l].resize(rows);
        map->gridGround[l].resize(rows);
        map->gridSymbol[l].resize(rows);
        map->gridOverlays[l].resize(rows);
        map->gridOverlayRotation[l].resize(rows);
        map->gridTileOverlays[l].resize(rows);
        for (int y = 0; y < rows; ++y) {
            map->gridBlocks[l][y].fill(0, cols);
            map->gridRotation[l][y].fill(0, cols);
            map->gridOrientation[l][y].fill(0, cols);
            map->gridTileType[l][y].fill(0, cols);
            map->gridGround[l][y].fill(0, cols);
            map->gridSymbol[l][y].fill(0, cols);
            map->gridOverlays[l][y].fill(0, cols);
            map->gridOverlayRotation[l][y].fill(0, cols);
            map->gridTileOverlays[l][y].resize(cols);
        }
    }

    // 2. Parse map.seg
    if (entries.contains("map.seg")) {
        const QByteArray& sData = entries["map.seg"];
        QString segText;
        if (sData.size() >= 4) {
            segText = QString::fromLatin1(sData.constData() + 4, sData.size() - 4);
        } else {
            segText = QString::fromLatin1(sData);
        }
        QStringList lines = segText.split(QRegExp("[\r\n]+"), Qt::SkipEmptyParts);
        for (int i = 0; i < lines.size(); ++i) {
            QString sPath = lines[i].trimmed();
            if (!sPath.isEmpty()) {
                map->segmentsBank.append(sPath);
                int segId = i + 1; // 1-based index
                map->segments[segId] = SegmentParser::parse(sPath, segId);
            }
            if (progressCallback && lines.size() > 0) {
                progressCallback(15 + (30 * (i + 1)) / lines.size(),
                                 QString("Loading segments (%1/%2)...").arg(i + 1).arg(lines.size()));
            }
        }
    }

    // 3. Parse map.ent
    if (entries.contains("map.ent")) {
        const QByteArray& eData = entries["map.ent"];
        QString entText;
        if (eData.size() >= 4) {
            entText = QString::fromLatin1(eData.constData() + 4, eData.size() - 4);
        } else {
            entText = QString::fromLatin1(eData);
        }
        QStringList lines = entText.split(QRegExp("[\r\n]+"), Qt::SkipEmptyParts);
        for (int i = 0; i < lines.size(); ++i) {
            QString ePath = lines[i].trimmed();
            if (!ePath.isEmpty()) {
                map->entitiesBank.append(ePath);
                int bankId = i + 1; // 1-based index
                map->entityProfiles[bankId] = EntityParser::parseProfile(ePath, bankId);
            }
            if (progressCallback && lines.size() > 0) {
                progressCallback(45 + (30 * (i + 1)) / lines.size(),
                                 QString("Loading entities (%1/%2)...").arg(i + 1).arg(lines.size()));
            }
        }
    }

    // 4. Parse map.fpmb, map.fpmo & map.fpml (3D Segment Grid & Overlays)
    if (progressCallback) progressCallback(75, QStringLiteral("Constructing map grid and overlays..."));

    if (entries.contains("map.fpmb")) {
        const QByteArray& bData = entries["map.fpmb"];
        const QByteArray& oData = entries.contains("map.fpmo") ? entries["map.fpmo"] : QByteArray();
        const QByteArray& lData = entries.contains("map.fpml") ? entries["map.fpml"] : (entries.contains("map.fpol") ? entries["map.fpol"] : QByteArray());

        // Decode olaylist from map.fpml
        QVector<uint32_t> olayPrimaryMapId;
        if (lData.size() >= 8) {
            int totalOlayElements = (lData.size() - 8) / 8;
            const int32_t* lStream = reinterpret_cast<const int32_t*>(lData.constData() + 8);
            olayPrimaryMapId.resize(totalOlayElements);
            for (int i = 0; i < totalOlayElements; ++i) {
                olayPrimaryMapId[i] = static_cast<uint32_t>(lStream[i * 2 + 1]);
            }
        }

        if (bData.size() >= 8) {
            const int32_t* bHdr = reinterpret_cast<const int32_t*>(bData.constData());
            int totalCells = bHdr[1];

            const int32_t* bStream = reinterpret_cast<const int32_t*>(bData.constData() + 8);
            const int32_t* oStream = (oData.size() >= 8) ? reinterpret_cast<const int32_t*>(oData.constData() + 8) : nullptr;

            int availCells = (bData.size() - 8) / 8;
            int cellsToRead = qMin(totalCells, availCells);

            int totalOlayElements = (lData.size() >= 8) ? (lData.size() - 8) / 8 : 0;
            // In Dark Basic Pro: dim olaylist(olaylistmax, 50) as DWORD -> 51 entries per olayindex!
            int olayStride = (totalOlayElements > 0) ? (totalOlayElements / 51) : 1;
            const int32_t* lStream = (lData.size() >= 8) ? reinterpret_cast<const int32_t*>(lData.constData() + 8) : nullptr;

            for (int i = 0; i < cellsToRead; ++i) {
                int layer = i % layers;
                int rem = i / layers;
                int x = rem % cols;
                int y = rem / cols;

                // 1. Read Base Segment Block
                int32_t mapid = bStream[i * 2 + 1];
                if (mapid != 0) {
                    int segId = (mapid >> 20) & 0xFFF;
                    int rotVal = (mapid >> 12) & 0x3;
                    int orientVal = (mapid >> 10) & 0x3;
                    int groundVal = (mapid >> 14) & 0x3;
                    int symbolVal = (mapid >> 4) & 0x3F;
                    int tileVal = mapid & 0xF;
                    if (layer < layers && y < rows && x < cols && segId > 0) {
                        map->gridBlocks[layer][y][x] = segId;
                        map->gridRotation[layer][y][x] = rotVal & 3;
                        map->gridOrientation[layer][y][x] = orientVal & 3;
                        map->gridGround[layer][y][x] = groundVal;
                        map->gridSymbol[layer][y][x] = symbolVal;
                        map->gridTileType[layer][y][x] = tileVal;
                    }
                }

                // 2. Read Overlay List (matches DarkBasic olaylist(olayindex, 0..50))
                if (oStream && i * 2 + 1 < (oData.size() - 8) / 4) {
                    int32_t oVal = oStream[i * 2 + 1];
                    if (oVal > 0 && lStream && olayStride > 0) {
                        for (int ti = 0; ti <= 50; ++ti) {
                            int elemIdx = oVal + ti * olayStride;
                            if (elemIdx >= totalOlayElements) break;
                            uint32_t val = static_cast<uint32_t>(lStream[elemIdx * 2 + 1]);
                            if (val == 0) break;
                            int sId = (val >> 20) & 0xFFF;
                            int rot = (val >> 12) & 0x3;
                            int orient = (val >> 10) & 0x3;
                            int tile = val & 0xF;
                            if (map->segments.contains(sId)) {
                                PlacedOverlay po;
                                po.segmentId = sId;
                                po.rotate = rot;
                                po.orient = orient;
                                po.tile = tile;
                                if (layer < layers && y < rows && x < cols) {
                                    map->gridTileOverlays[layer][y][x].append(po);
                                }
                            }
                        }

                        if (layer < layers && y < rows && x < cols && !map->gridTileOverlays[layer][y][x].isEmpty()) {
                            // Primary overlay for compatibility: prefer punch or gantry/stairs if available
                            int primIdx = 0;
                            for (int idx = 0; idx < map->gridTileOverlays[layer][y][x].size(); ++idx) {
                                auto seg = map->segments.value(map->gridTileOverlays[layer][y][x][idx].segmentId);
                                if (seg && (seg->hasPunch || seg->isPlatformOrGantry || seg->isStairs)) {
                                    primIdx = idx;
                                    break;
                                }
                            }
                            const auto& prim = map->gridTileOverlays[layer][y][x][primIdx];
                            map->gridOverlays[layer][y][x] = prim.segmentId;
                            map->gridOverlayRotation[layer][y][x] = prim.orient;
                        }
                    }
                }
            }
        }
    }

    map->filePath = fpmPath;
    map->password = password;
    map->rawEntries = entries;
    map->isModified = false;

    // 5. Parse map.ele (Placed Entities)
    if (entries.contains("map.ele")) {
        const QByteArray& eleData = entries["map.ele"];
        if (eleData.size() >= 4) {
            map->eleVersion = *reinterpret_cast<const int32_t*>(eleData.constData());
        }
        map->placedEntities = EntityParser::parseMapEle(
            eleData,
            map->entitiesBank,
            map->entityProfiles
        );
    }

    // 6. Parse map.way (AI Waypoints)
    if (entries.contains("map.way")) {
        const QByteArray& wData = entries["map.way"];
        if (wData.size() >= 8) {
            const int32_t* wHdr = reinterpret_cast<const int32_t*>(wData.constData());
            int wpCount = wHdr[0];
            int pathCount = wHdr[1];

            if (wpCount > 0 && wData.size() >= 8 + wpCount * 12) {
                const float* pStream = reinterpret_cast<const float*>(wData.constData() + 8);
                for (int i = 0; i < wpCount; ++i) {
                    AIWaypoint wp;
                    wp.x = pStream[i * 3 + 0];
                    wp.y = pStream[i * 3 + 1];
                    wp.z = pStream[i * 3 + 2];
                    map->waypoints.append(wp);
                }
            }
        }
    }

    // 7. Parse cfg.cfg
    if (entries.contains("cfg.cfg")) {
        const QByteArray& cData = entries["cfg.cfg"];
        if (cData.size() >= 16) {
            const float* fCam = reinterpret_cast<const float*>(cData.constData());
            const int32_t* iCam = reinterpret_cast<const int32_t*>(cData.constData());
            map->cameraX = fCam[0];
            map->cameraY = fCam[1];
            map->cameraZoom = fCam[2];
            map->activeEditorLayer = qBound(0, iCam[3], map->header.layerMax);
        }
    }

    if (progressCallback) progressCallback(100, QStringLiteral("Done"));

    return map;
}
