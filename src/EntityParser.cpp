#include "EntityParser.h"
#include "AssetManager.h"
#include <QFile>
#include <QTextStream>
#include <QFileInfo>
#include <QDir>
#include <QDataStream>
#include <QDebug>
#include <cstring>

static QString readCRLFString(const char* data, int size, int& offset) {
    if (offset >= size) return QString();
    int start = offset;
    while (offset < size) {
        if (data[offset] == '\r' && offset + 1 < size && data[offset + 1] == '\n') {
            QString s = QString::fromLatin1(data + start, offset - start);
            offset += 2;
            return s;
        }
        offset++;
    }
    QString s = QString::fromLatin1(data + start, size - start);
    return s;
}

std::shared_ptr<FPSCEntityProfile> EntityParser::parseProfile(const QString& relPath, int bankId) {
    auto prof = std::make_shared<FPSCEntityProfile>();
    prof->bankId = bankId;
    prof->relPath = relPath;
    prof->name = QFileInfo(relPath).completeBaseName();

    QString fullPath = AssetManager::instance().resolvePath(relPath);
    if (fullPath.isEmpty()) {
        return prof;
    }

    QFile file(fullPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return prof;
    }

    QTextStream in(&file);
    QMap<QString, QString> kv;

    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty() || line.startsWith(';') || line.startsWith("//")) continue;

        int eqPos = line.indexOf('=');
        if (eqPos < 0) continue;

        QString key = line.left(eqPos).trimmed().toLower();
        QString val = line.mid(eqPos + 1).trimmed();
        kv[key] = val;
    }

    if (kv.contains("desc")) prof->desc = kv["desc"];
    if (kv.contains("model")) prof->modelPath = kv["model"];
    if (kv.contains("textured")) prof->texturePath = kv["textured"];
    if (kv.contains("texalt")) prof->altTexturePath = kv["texalt"];
    if (kv.contains("effect")) prof->effectPath = kv["effect"];

    if (kv.contains("scale")) prof->scale = kv["scale"].toInt();
    if (kv.contains("health")) prof->health = kv["health"].toInt();
    if (kv.contains("speed")) prof->speed = kv["speed"].toInt();
    if (kv.contains("ischaracter")) prof->isCharacter = (kv["ischaracter"].toInt() != 0);
    if (kv.contains("isweapon")) prof->isWeapon = (kv["isweapon"].toInt() != 0);
    if (kv.contains("isammo")) prof->isAmmo = (kv["isammo"].toInt() != 0);
    if (kv.contains("isimmune")) prof->isImmune = (kv["isimmune"].toInt() != 0);
    if (kv.contains("collisionmode")) prof->collisionMode = kv["collisionmode"].toInt();

    if (kv.contains("gunname")) prof->gunName = kv["gunname"];
    if (kv.contains("ammo")) prof->ammoQty = kv["ammo"].toInt();
    if (kv.contains("quantity")) prof->quantity = kv["quantity"].toInt();

    if (kv.contains("ai_init")) prof->aiInit = kv["ai_init"];
    if (kv.contains("ai_main")) prof->aiMain = kv["ai_main"];
    if (kv.contains("ai_shoot")) prof->aiShoot = kv["ai_shoot"];
    if (kv.contains("ai_destroy")) prof->aiDestroy = kv["ai_destroy"];
    if (kv.contains("usekey")) prof->useKey = kv["usekey"];
    if (kv.contains("ifused")) prof->ifUsed = kv["ifused"];

    if (kv.contains("soundset")) prof->soundSet = kv["soundset"];
    if (kv.contains("soundset1")) prof->soundSet1 = kv["soundset1"];

    if (kv.contains("lightrange")) prof->lightRange = kv["lightrange"].toFloat();
    int lr = kv.value("lightred", "255").toInt();
    int lg = kv.value("lightgreen", "255").toInt();
    int lb = kv.value("lightblue", "255").toInt();
    prof->lightColor = QColor(qBound(0, lr, 255), qBound(0, lg, 255), qBound(0, lb, 255));

    // Determine Entity Category
    QString pLower = relPath.toLower();
    QString nLower = prof->name.toLower();

    if (nLower.contains("player start") || pLower.contains("player start")) {
        prof->category = EntityCategory::PlayerStart;
    } else if (prof->isCharacter || pLower.contains("characters") || pLower.contains("zombie") || pLower.contains("monster") || pLower.contains("ai (") || pLower.contains("ai.")) {
        prof->category = EntityCategory::Character;
    } else if (prof->isWeapon || pLower.contains("weapons") || nLower.contains("gun") || nLower.contains("rifle") || nLower.contains("shotgun") || nLower.contains("pistol")) {
        prof->category = EntityCategory::Weapon;
    } else if (prof->isAmmo || pLower.contains("ammo") || nLower.contains("ammo")) {
        prof->category = EntityCategory::Ammo;
    } else if (prof->lightRange > 0 || nLower.contains("light") || pLower.contains("light")) {
        prof->category = EntityCategory::Light;
    } else if (nLower.contains("zone") || pLower.contains("zone") || pLower.contains("triggers") || nLower.contains("trigger") || nLower.contains("checkpoint")) {
        prof->category = EntityCategory::Zone;
    } else if (nLower.contains("door") || pLower.contains("doors")) {
        prof->category = EntityCategory::Door;
    } else if (nLower.contains("key") || nLower.contains("health") || nLower.contains("item") || nLower.contains("pickup")) {
        prof->category = EntityCategory::Item;
    } else {
        prof->category = EntityCategory::Scenery;
    }

    // Locate BMP icon / thumbnail
    QFileInfo fi(fullPath);
    QString dir = fi.absolutePath();
    QString base = fi.completeBaseName();

    QString candidateBmp = dir + "/" + base + ".bmp";
    if (QFileInfo::exists(candidateBmp)) {
        prof->iconBmpPath = candidateBmp;
    } else if (QFileInfo::exists(dir + "/icon.bmp")) {
        prof->iconBmpPath = dir + "/icon.bmp";
    } else if (QFileInfo::exists(dir + "/thumb.bmp")) {
        prof->iconBmpPath = dir + "/thumb.bmp";
    } else if (!prof->texturePath.isEmpty()) {
        prof->iconBmpPath = prof->texturePath;
    }

    // Measure memory footprint metrics
    if (!prof->modelPath.isEmpty()) {
        prof->meshSizeBytes = AssetManager::instance().getFileSizeBytes(prof->modelPath);
    }
    if (!prof->texturePath.isEmpty()) {
        qint64 ramBytes = 0, diskBytes = 0;
        int w = 0, h = 0;
        AssetManager::instance().getTextureMetrics(prof->texturePath, w, h, ramBytes, diskBytes);
        prof->diffuseSizeBytes = ramBytes;
        prof->texWidth = w;
        prof->texHeight = h;
    }
    if (!prof->altTexturePath.isEmpty()) {
        prof->otherTexturesSizeBytes += AssetManager::instance().getFileSizeBytes(prof->altTexturePath);
    }
    if (!prof->soundSet.isEmpty()) {
        prof->audioSizeBytes += AssetManager::instance().getFileSizeBytes(prof->soundSet);
    }
    if (!prof->soundSet1.isEmpty()) {
        prof->audioSizeBytes += AssetManager::instance().getFileSizeBytes(prof->soundSet1);
    }

    // Compute base RAM estimate: Mesh + Texture RAM + Audio + ~2KB struct
    prof->estimatedRAMBytes = prof->meshSizeBytes + prof->diffuseSizeBytes + prof->otherTexturesSizeBytes + prof->audioSizeBytes + 2048;

    return prof;
}

QVector<PlacedEntity> EntityParser::parseMapEle(
    const QByteArray& eleData,
    const QVector<QString>& entBank,
    const QMap<int, std::shared_ptr<FPSCEntityProfile>>& profiles)
{
    QVector<PlacedEntity> result;
    if (eleData.size() < 8) return result;

    const char* raw = eleData.constData();
    int size = eleData.size();
    int offset = 0;

    int version = *reinterpret_cast<const int32_t*>(raw + offset); offset += 4;
    int count = 0;

    if (version < 100) {
        count = version;
        version = 100;
    } else {
        count = *reinterpret_cast<const int32_t*>(raw + offset); offset += 4;
    }

    for (int e = 0; e < count; ++e) {
        if (offset >= size) break;

        PlacedEntity ent;
        ent.index = e;

        if (version >= 101) {
            if (offset + 36 > size) break;
            int32_t mType = 0, bIndex = 0, sFlag = 0;
            float ex = 0, ey = 0, ez = 0, erx = 0, ery = 0, erz = 0;
            memcpy(&mType, raw + offset, 4);
            memcpy(&bIndex, raw + offset + 4, 4);
            memcpy(&sFlag, raw + offset + 8, 4);
            memcpy(&ex, raw + offset + 12, 4);
            memcpy(&ey, raw + offset + 16, 4);
            memcpy(&ez, raw + offset + 20, 4);
            memcpy(&erx, raw + offset + 24, 4);
            memcpy(&ery, raw + offset + 28, 4);
            memcpy(&erz, raw + offset + 32, 4);

            ent.mainType = mType;
            ent.bankIndex = bIndex;
            ent.staticFlag = sFlag;
            ent.x = ex;
            ent.y = ey;
            ent.z = ez;
            ent.rx = erx;
            ent.ry = ery;
            ent.rz = erz;
            offset += 36;

            ent.instanceName = readCRLFString(raw, size, offset);
            ent.aiInit = readCRLFString(raw, size, offset);
            ent.aiMain = readCRLFString(raw, size, offset);
            ent.aiDestroy = readCRLFString(raw, size, offset);

            if (offset + 4 > size) break;
            offset += 4; // isobjective

            ent.useKey = readCRLFString(raw, size, offset);
            ent.ifUsed = readCRLFString(raw, size, offset);
            ent.ifUsedNear = readCRLFString(raw, size, offset);

            if (offset + 4 > size) break;
            offset += 4; // uniqueelement

            ent.texd = readCRLFString(raw, size, offset);
            ent.texaltd = readCRLFString(raw, size, offset);
            ent.effect = readCRLFString(raw, size, offset);

            if (offset + 8 > size) break;
            offset += 8; // transparency, editorfixed

            readCRLFString(raw, size, offset); // soundset
            readCRLFString(raw, size, offset); // soundset1

            if (offset + 28 > size) break;
            const int32_t* v7 = reinterpret_cast<const int32_t*>(raw + offset);
            ent.speed = v7[6];
            offset += 28;

            ent.aiShoot = readCRLFString(raw, size, offset);
            readCRLFString(raw, size, offset); // hasweapon

            if (offset + 80 > size) break;
            const int32_t* ivals = reinterpret_cast<const int32_t*>(raw + offset);
            const float* fvals = reinterpret_cast<const float*>(raw + offset);

            ent.lives = ivals[0];
            ent.scale = fvals[4];
            ent.health = ivals[7];
            uint32_t col = static_cast<uint32_t>(ivals[12]);
            ent.lightColor = QColor((col >> 16) & 0xFF, (col >> 8) & 0xFF, col & 0xFF);
            ent.lightRange = static_cast<float>(ivals[13]);
            ent.trigX1 = ivals[14];
            ent.trigY1 = ivals[15];
            ent.trigZ1 = ivals[16];
            ent.trigX2 = ivals[17];
            ent.trigY2 = ivals[18];
            ent.trigZ2 = ivals[19];
            offset += 80;

            readCRLFString(raw, size, offset); // basedecal
        }

        if (version >= 102) { offset += 80; }
        if (version >= 103) {
            if (offset + 36 <= size) {
                const int32_t* pv = reinterpret_cast<const int32_t*>(raw + offset);
                ent.physics = pv[0];
                ent.phyWeight = pv[1];
                ent.explodable = pv[5];
            }
            offset += 36;
        }
        if (version >= 104) { offset += 4; }
        if (version >= 105) { offset += 24; }
        if (version >= 106) { offset += 8; }
        if (version >= 107) {
            if (offset + 4 <= size) {
                ent.lightIndex = *reinterpret_cast<const int32_t*>(raw + offset);
            }
            offset += 4;
        }
        if (version >= 199) { offset += 68; }
        if (version >= 200) { offset += 24; }
        if (version >= 217) { offset += 68; }
        if (version >= 218) { offset += 4; }

        if (profiles.contains(ent.bankIndex)) {
            ent.profile = profiles.value(ent.bankIndex);
        }

        // Calculate floor layer (Y height in world units, 1 floor = 100 units)
        // In FPSC world coords, floor 0 is Y=0..100 (midpoint 50), floor 5 is Y=500..600 (midpoint 550)
        ent.floorLayer = qBound(0, static_cast<int>(std::floor((ent.y + 25.0f) / 100.0f)), 20);

        // If light range / color set on instance
        if (ent.lightRange <= 0 && ent.profile && ent.profile->lightRange > 0) {
            ent.lightRange = ent.profile->lightRange;
            ent.lightColor = ent.profile->lightColor;
        }

        result.append(ent);
    }

    return result;
}
