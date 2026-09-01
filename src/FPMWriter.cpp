#include "FPMWriter.h"
#include "EntityParser.h"
#include "miniz_deflate.h"
#include <QFile>
#include <QFileInfo>
#include <QDataStream>
#include <QDebug>
#include <cstdlib>

#pragma pack(push, 1)
struct LocalFileHeader {
    uint32_t signature = 0x04034b50;
    uint16_t version = 20;
    uint16_t flags = 0;
    uint16_t method = 0; // Stored (no compression)
    uint16_t modTime = 0;
    uint16_t modDate = 0x5000;
    uint32_t crc32 = 0;
    uint32_t compSize = 0;
    uint32_t uncompSize = 0;
    uint16_t nameLen = 0;
    uint16_t extraLen = 0;
};

struct CentralDirHeader {
    uint32_t signature = 0x02014b50;
    uint16_t verMade = 20;
    uint16_t verNeed = 20;
    uint16_t flags = 0;
    uint16_t method = 0;
    uint16_t modTime = 0;
    uint16_t modDate = 0x5000;
    uint32_t crc32 = 0;
    uint32_t compSize = 0;
    uint32_t uncompSize = 0;
    uint16_t nameLen = 0;
    uint16_t extraLen = 0;
    uint16_t commentLen = 0;
    uint16_t diskStart = 0;
    uint16_t intAttr = 0;
    uint32_t extAttr = 0;
    uint32_t localOffset = 0;
};

struct EOCDRecord {
    uint32_t signature = 0x06054b50;
    uint16_t diskNum = 0;
    uint16_t cdDisk = 0;
    uint16_t numEntriesDisk = 0;
    uint16_t numEntries = 0;
    uint32_t cdSize = 0;
    uint32_t cdOffset = 0;
    uint16_t commentLen = 0;
};
#pragma pack(pop)

bool FPMWriter::saveMap(
    const std::shared_ptr<FPSCMap>& map,
    const QString& targetPath,
    const QString& password)
{
    if (!map || targetPath.isEmpty()) return false;

    // 1. Serialize updated entity data into map.ele
    QByteArray eleData = EntityParser::serializeMapEle(map);
    map->rawEntries[QStringLiteral("map.ele")] = eleData;

    // 2. Build ZIP archive buffer
    QByteArray zipBuffer;
    struct EntryMeta {
        QString name;
        uint32_t crc32;
        uint32_t compSize;
        uint32_t uncompSize;
        uint16_t flags;
        uint32_t localOffset;
    };
    QVector<EntryMeta> metaList;

    bool useEncryption = !password.isEmpty();

    // Standard DarkBasic Pro file ordering
    QStringList orderedNames = {
        QStringLiteral("header.dat"),
        QStringLiteral("map.ele"),
        QStringLiteral("map.ent"),
        QStringLiteral("map.seg"),
        QStringLiteral("map.way"),
        QStringLiteral("map.fpmb"),
        QStringLiteral("map.fpmo"),
        QStringLiteral("map.fpml"),
        QStringLiteral("cfg.cfg")
    };

    // Add any remaining keys that weren't in standard list
    for (auto it = map->rawEntries.constBegin(); it != map->rawEntries.constEnd(); ++it) {
        if (!orderedNames.contains(it.key())) {
            orderedNames.append(it.key());
        }
    }

    uint16_t modTime = 0x5000;
    uint16_t modDate = 0x5000;

    for (const QString& name : orderedNames) {
        if (!map->rawEntries.contains(name)) continue;

        QByteArray uncompData = map->rawEntries[name];
        QByteArray nameBytes = name.toLatin1();

        uint32_t crc = ZipUtils::calcCRC32(
            reinterpret_cast<const uint8_t*>(uncompData.constData()),
            uncompData.size()
        );

        // Compress with Deflate (RFC 1951)
        QByteArray defBytes;
        if (uncompData.isEmpty()) {
            defBytes = QByteArray();
        } else {
            QByteArray z = qCompress(uncompData, 6);
            if (z.size() >= 10) {
                defBytes = z.mid(6, z.size() - 10);
            } else {
                defBytes = uncompData;
            }
        }

        uint16_t flags = useEncryption ? 0x000B : 0x0002;
        uint16_t method = 8; // Deflate

        EntryMeta meta;
        meta.name = name;
        meta.crc32 = crc;
        meta.uncompSize = static_cast<uint32_t>(uncompData.size());
        meta.localOffset = static_cast<uint32_t>(zipBuffer.size());
        meta.flags = flags;

        QByteArray payload;
        if (useEncryption) {
            ZipUtils::ZipCryptoKey key;
            key.init(password.toStdString());

            // 12-byte encryption header
            uint8_t encHdr[12];
            for (int i = 0; i < 11; ++i) {
                encHdr[i] = static_cast<uint8_t>(std::rand() & 0xFF);
            }
            // Byte 11 is MSB of CRC32
            encHdr[11] = static_cast<uint8_t>((crc >> 24) & 0xFF);

            payload.resize(12 + defBytes.size());
            for (int i = 0; i < 12; ++i) {
                payload[i] = static_cast<char>(key.encrypt(encHdr[i]));
            }

            const uint8_t* pDef = reinterpret_cast<const uint8_t*>(defBytes.constData());
            for (int i = 0; i < defBytes.size(); ++i) {
                payload[12 + i] = static_cast<char>(key.encrypt(pDef[i]));
            }

            meta.compSize = static_cast<uint32_t>(payload.size());
        } else {
            payload = defBytes;
            meta.compSize = static_cast<uint32_t>(payload.size());
        }

        LocalFileHeader lfh;
        lfh.flags = meta.flags;
        lfh.method = method;
        lfh.modTime = modTime;
        lfh.modDate = modDate;
        lfh.crc32 = meta.crc32;
        lfh.compSize = meta.compSize;
        lfh.uncompSize = meta.uncompSize;
        lfh.nameLen = static_cast<uint16_t>(nameBytes.size());
        lfh.extraLen = 0;

        zipBuffer.append(reinterpret_cast<const char*>(&lfh), sizeof(LocalFileHeader));
        zipBuffer.append(nameBytes);
        zipBuffer.append(payload);

        // Data descriptor (PK\x07\x08)
        uint32_t ddSig = 0x08074b50;
        zipBuffer.append(reinterpret_cast<const char*>(&ddSig), 4);
        zipBuffer.append(reinterpret_cast<const char*>(&meta.crc32), 4);
        zipBuffer.append(reinterpret_cast<const char*>(&meta.compSize), 4);
        zipBuffer.append(reinterpret_cast<const char*>(&meta.uncompSize), 4);

        metaList.append(meta);
    }

    // 3. Write Central Directory
    uint32_t cdStart = static_cast<uint32_t>(zipBuffer.size());
    for (const EntryMeta& meta : metaList) {
        QByteArray nameBytes = meta.name.toLatin1();

        CentralDirHeader cdh;
        cdh.flags = meta.flags;
        cdh.method = 8; // Deflate
        cdh.modTime = modTime;
        cdh.modDate = modDate;
        cdh.crc32 = meta.crc32;
        cdh.compSize = meta.compSize;
        cdh.uncompSize = meta.uncompSize;
        cdh.nameLen = static_cast<uint16_t>(nameBytes.size());
        cdh.extraLen = 0;
        cdh.commentLen = 0;
        cdh.localOffset = meta.localOffset;

        zipBuffer.append(reinterpret_cast<const char*>(&cdh), sizeof(CentralDirHeader));
        zipBuffer.append(nameBytes);
    }
    uint32_t cdEnd = static_cast<uint32_t>(zipBuffer.size());
    uint32_t cdSize = cdEnd - cdStart;

    // 4. Write End of Central Directory Record
    EOCDRecord eocd;
    eocd.numEntriesDisk = static_cast<uint16_t>(metaList.size());
    eocd.numEntries = static_cast<uint16_t>(metaList.size());
    eocd.cdSize = cdSize;
    eocd.cdOffset = cdStart;
    eocd.commentLen = 0;

    zipBuffer.append(reinterpret_cast<const char*>(&eocd), sizeof(EOCDRecord));

    // 5. Write to target file
    QFile outFile(targetPath);
    if (!outFile.open(QIODevice::WriteOnly)) {
        qWarning() << "FPMWriter: Failed to open file for writing:" << targetPath;
        return false;
    }

    qint64 written = outFile.write(zipBuffer);
    outFile.close();

    if (written != zipBuffer.size()) {
        qWarning() << "FPMWriter: Failed to write complete buffer to" << targetPath;
        return false;
    }

    map->filePath = targetPath;
    map->isModified = false;
    qDebug() << "FPMWriter: Successfully saved map to" << targetPath << "size:" << written << "bytes";
    return true;
}
