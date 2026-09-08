#include "SegmentParser.h"
#include "AssetManager.h"
#include <QFile>
#include <QTextStream>
#include <QFileInfo>
#include <QDebug>

std::shared_ptr<FPSCSegment> SegmentParser::parse(const QString& relPath, int segId) {
    auto seg = std::make_shared<FPSCSegment>();
    seg->id = segId;
    seg->relPath = relPath;
    seg->name = QFileInfo(relPath).completeBaseName();

    QString fullPath = AssetManager::instance().resolvePath(relPath);
    if (fullPath.isEmpty()) {
        // Create default placeholder segment
        seg->isScenery = false;
        return seg;
    }

    QFile file(fullPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return seg;
    }

    QTextStream in(&file);
    QMap<QString, QString> kv;
    int maxPartIdx = -1;

    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty() || line.startsWith(';') || line.startsWith("//")) continue;

        int eqPos = line.indexOf('=');
        if (eqPos < 0) continue;

        QString key = line.left(eqPos).trimmed().toLower();
        QString val = line.mid(eqPos + 1).trimmed();

        kv[key] = val;

        if (key.startsWith("meshname") || key.startsWith("texture") || key.startsWith("partmode")) {
            for (int i = 0; i < key.length(); ++i) {
                if (key[i].isDigit()) {
                    int pIdx = key.mid(i).toInt();
                    if (pIdx > maxPartIdx) maxPartIdx = pIdx;
                    break;
                }
            }
        }
    }

    if (kv.contains("partmax")) {
        int pm = kv["partmax"].toInt();
        if (pm > maxPartIdx) maxPartIdx = pm;
    }

    if (kv.contains("kindof")) seg->kindOf = kv["kindof"].toInt();
    if (kv.contains("groundmode")) seg->groundMode = kv["groundmode"].toInt();
    if (kv.contains("mode")) seg->mode = kv["mode"].toInt();
    if (kv.contains("visoverlay")) seg->visOverlay = kv["visoverlay"].toInt();
    if (kv.contains("viswallb")) seg->visWallB = kv["viswallb"].toInt();
    if (kv.contains("viswallr")) seg->visWallR = kv["viswallr"].toInt();
    if (kv.contains("viswallf")) seg->visWallF = kv["viswallf"].toInt();
    if (kv.contains("viswalll")) seg->visWallL = kv["viswalll"].toInt();
    if (kv.contains("visfloor")) seg->visFloor = kv["visfloor"].toInt();
    if (kv.contains("visroof")) seg->visRoof = kv["visroof"].toInt();

    // Parse individual parts
    for (int i = 0; i <= maxPartIdx; ++i) {
        QString sIdx = QString::number(i);
        SegmentPart part;
        part.partMode = kv.value("partmode" + sIdx, "0").toInt();
        part.meshName = kv.value("meshname" + sIdx);
        part.offX = kv.value("offx" + sIdx, "0").toFloat();
        part.offY = kv.value("offy" + sIdx, "0").toFloat();
        part.offZ = kv.value("offz" + sIdx, "0").toFloat();
        part.rotX = kv.value("rotx" + sIdx, "0").toFloat();
        part.rotY = kv.value("roty" + sIdx, "0").toFloat();
        part.rotZ = kv.value("rotz" + sIdx, "0").toFloat();
        part.texture = kv.value("texture" + sIdx);
        part.textured = kv.value("textured" + sIdx);
        if (part.texture.isEmpty() && !part.textured.isEmpty()) part.texture = part.textured;
        part.transparency = kv.value("transparency" + sIdx, "0").toInt();
        part.colMode = kv.value("colmode" + sIdx, "0").toInt();

        QString meshLower = part.meshName.toLower();

        // Detect CSG punch mesh
        if (part.partMode == 1 || meshLower.contains("punch")) {
            seg->hasPunch = true;
        }

        // Categorize part
        if (meshLower.contains("ceiling") || meshLower.contains("roof") || (part.offY >= 25.0f && !meshLower.contains("floor"))) {
            part.isCeiling = true;
            if (seg->roofTexture.isEmpty() && !part.texture.isEmpty()) {
                seg->roofTexture = part.texture;
            }
        } else if (meshLower.contains("floor") || (part.offY <= -25.0f && !meshLower.contains("wall"))) {
            part.isFloor = true;
            if (seg->floorTexture.isEmpty() && !part.texture.isEmpty()) {
                seg->floorTexture = part.texture;
            }
        } else if (meshLower.contains("wall") || std::abs(part.offX) >= 25.0f || std::abs(part.offZ) >= 25.0f || meshLower.contains("door")) {
            part.isWall = true;

            // Determine wall facing orientation
            // 0=North (Z+), 1=East (X+), 2=South (Z-), 3=West (X-)
            int side = -1;
            int rotYInt = (static_cast<int>(std::round(part.rotY)) % 360 + 360) % 360;

            if (part.offX <= -25.0f || rotYInt == 270) {
                side = 3; // West
            } else if (part.offX >= 25.0f || rotYInt == 90) {
                side = 1; // East
            } else if (part.offZ >= 25.0f || rotYInt == 0) {
                side = 0; // North
            } else if (part.offZ <= -25.0f || rotYInt == 180) {
                side = 2; // South
            }

            if (side >= 0) {
                part.wallSide = side;
                seg->hasWall[side] = true;
                if (seg->wallTextures[side].isEmpty()) {
                    seg->wallTextures[side] = part.texture;
                }
            }
        }

        seg->parts.append(part);
    }

    // Default wall textures propagation
    QString primaryWallTex;
    for (int s = 0; s < 4; ++s) {
        if (!seg->wallTextures[s].isEmpty()) {
            primaryWallTex = seg->wallTextures[s];
            break;
        }
    }
    if (primaryWallTex.isEmpty()) {
        for (const auto& p : seg->parts) {
            if (!p.texture.isEmpty() && !p.isFloor && !p.isCeiling) {
                primaryWallTex = p.texture;
                break;
            }
        }
    }
    for (int s = 0; s < 4; ++s) {
        if (seg->hasWall[s] && seg->wallTextures[s].isEmpty()) {
            seg->wallTextures[s] = primaryWallTex;
        }
    }

    // Segment visibility state directly from .fps specification:
    // visfloor defines whether this segment possesses a floor mesh limb (visfloor >= 0).
    // visroof defines whether this segment possesses a roof/ceiling mesh limb (visroof >= 0).
    seg->hasFloorOnThisLayer = (seg->visFloor >= 0);
    seg->hasRoofOnThisLayer = (seg->visRoof >= 0);

    // Check if fake window or fake door (has "fake" in name/relPath, or has a blank mesh backing)
    seg->isFake = seg->name.contains("fake", Qt::CaseInsensitive) ||
                  seg->relPath.contains("fake", Qt::CaseInsensitive);

    for (const auto& p : seg->parts) {
        if (p.meshName.contains("blank", Qt::CaseInsensitive)) {
            seg->isFake = true;
            break;
        }
    }

    seg->isWindow = seg->name.contains("window", Qt::CaseInsensitive) ||
                    seg->relPath.contains("window", Qt::CaseInsensitive);
    if (!seg->isWindow) {
        for (const auto& p : seg->parts) {
            if (p.meshName.contains("window", Qt::CaseInsensitive) ||
                p.meshName.contains("win_", Qt::CaseInsensitive) ||
                p.meshName.contains("glass", Qt::CaseInsensitive) ||
                p.meshName.contains("fullview", Qt::CaseInsensitive)) {
                seg->isWindow = true;
                break;
            }
        }
    }

    // Structural type classification from .fps specification:
    // A. Door / Window: has CSG punch cutout or door naming (real opening only, not fake!)
    bool isDoor = !seg->isFake && (seg->hasPunch ||
                  seg->name.contains("door", Qt::CaseInsensitive) ||
                  seg->name.contains("gate", Qt::CaseInsensitive));

    // B. Enclosed Room / Ceiling Cap / Shaft: standard room block with floor/roof/wall limbs (visoverlay == 0)
    bool isRoomOrCeiling = (seg->visOverlay == 0 && (seg->visRoof >= 0 || (seg->visFloor >= 0 && seg->visWallB >= 0))) ||
                           seg->name.contains("(top)", Qt::CaseInsensitive) ||
                           seg->name.contains("(base)", Qt::CaseInsensitive) ||
                           seg->name.contains("(roof)", Qt::CaseInsensitive) ||
                           seg->name.endsWith(" top", Qt::CaseInsensitive) ||
                           seg->name.endsWith(" roof", Qt::CaseInsensitive);

    bool isGantry = false;
    bool isStairs = false;

    if (!isDoor && !isRoomOrCeiling) {
        // Physical geometry check: stairs flights have parts with stepping vertical heights (offY)
        float minY = 9999.0f, maxY = -9999.0f;
        for (const auto& p : seg->parts) {
            minY = qMin(minY, p.offY);
            maxY = qMax(maxY, p.offY);
        }
        bool hasSteppedGeometry = (seg->parts.size() >= 4 && (maxY - minY) >= 40.0f);

        QString lowerName = seg->name.trimmed().toLower();
        if (hasSteppedGeometry || lowerName.contains("stairs") || lowerName.contains("staircase") ||
            lowerName.contains("step") || (lowerName.contains("stair") && !lowerName.contains("stairwell"))) {
            isStairs = true;
        } else if (lowerName.contains("gantry") || lowerName.contains("platform") ||
                   lowerName.contains("catwalk") || lowerName.contains("walkway")) {
            isGantry = true;
        }
    }

    if (isGantry || isStairs) {
        if (isStairs) {
            seg->isStairs = true;
        } else {
            seg->isPlatformOrGantry = true;
        }
        seg->hasFloorOnThisLayer = true;

        // Ensure floor texture is populated from the first available part
        if (seg->floorTexture.isEmpty() && !seg->parts.isEmpty()) {
            for (const auto& p : seg->parts) {
                if (!p.texture.isEmpty()) {
                    seg->floorTexture = p.texture;
                    break;
                }
            }
        }
    }

    // Floor slab (groundmode == 2, e.g. ground.fps or techfloor1.fps) with single mesh
    if (seg->parts.size() == 1 && seg->groundMode == 2) {
        seg->hasFloorOnThisLayer = true;
        if (seg->floorTexture.isEmpty() && !seg->parts[0].texture.isEmpty()) {
            seg->floorTexture = seg->parts[0].texture;
        }
    }

    // Pure scenery/prop object: single mesh without any wall or floor visibility indices
    if (seg->parts.size() == 1 && seg->visFloor == -1 && seg->visRoof == -1 &&
        seg->visWallB == -1 && seg->visWallR == -1 && seg->visWallF == -1 && seg->visWallL == -1 &&
        !seg->isPlatformOrGantry && !seg->isStairs) {
        seg->isScenery = true;
        if (seg->floorTexture.isEmpty() && !primaryWallTex.isEmpty()) {
            seg->floorTexture = primaryWallTex;
        }
    }

    return seg;
}
