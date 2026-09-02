#ifndef FPSCDATA_H
#define FPSCDATA_H

#include <QString>
#include <QVector>
#include <QColor>
#include <QMap>
#include <QPixmap>
#include <memory>
#include <cmath>

enum class EntityCategory {
    Unknown,
    Character,
    Weapon,
    Ammo,
    Door,
    Light,
    Zone,
    Item,
    Scenery,
    PlayerStart
};

inline QString entityCategoryToString(EntityCategory cat) {
    switch (cat) {
        case EntityCategory::Character:   return QStringLiteral("Character");
        case EntityCategory::Weapon:      return QStringLiteral("Weapon");
        case EntityCategory::Ammo:        return QStringLiteral("Ammo");
        case EntityCategory::Door:        return QStringLiteral("Door / Obstacle");
        case EntityCategory::Light:       return QStringLiteral("Light Source");
        case EntityCategory::Zone:        return QStringLiteral("Trigger / Zone");
        case EntityCategory::Item:        return QStringLiteral("Item / Pickup");
        case EntityCategory::Scenery:     return QStringLiteral("Scenery / Prop");
        case EntityCategory::PlayerStart: return QStringLiteral("Player Start");
        default:                          return QStringLiteral("Other");
    }
}

inline QColor entityCategoryColor(EntityCategory cat) {
    switch (cat) {
        case EntityCategory::PlayerStart: return QColor(0, 230, 64);
        case EntityCategory::Character:   return QColor(235, 77, 75);
        case EntityCategory::Weapon:      return QColor(240, 147, 43);
        case EntityCategory::Ammo:        return QColor(246, 229, 141);
        case EntityCategory::Light:       return QColor(249, 202, 36);
        case EntityCategory::Zone:        return QColor(104, 109, 224);
        case EntityCategory::Door:        return QColor(72, 52, 212);
        case EntityCategory::Item:        return QColor(26, 188, 156);
        case EntityCategory::Scenery:     return QColor(149, 175, 192);
        default:                          return QColor(120, 120, 120);
    }
}

struct SegmentPart {
    int partMode = 0;
    QString meshName;
    float offX = 0, offY = 0, offZ = 0;
    float rotX = 0, rotY = 0, rotZ = 0;
    QString texture;
    QString textured;
    int transparency = 0;
    int colMode = 0;
    
    bool isFloor = false;
    bool isWall = false;
    bool isCeiling = false;
    int wallSide = -1; // 0=North (Z+), 1=East (X+), 2=South (Z-), 3=West (X-)
};

struct FPSCSegment {
    int id = 0;
    QString relPath;
    QString name;
    
    QVector<SegmentPart> parts;
    QString floorTexture;
    QString roofTexture;
    QString wallTextures[4]; // 0=North, 1=East, 2=South, 3=West
    bool hasWall[4] = {false, false, false, false};
    
    int visWallB = -1, visWallR = -1, visWallF = -1, visWallL = -1;
    int visFloor = -1, visRoof = -1;
    int kindOf = 0;
    int groundMode = 0;
    bool isScenery = false;
    bool hasFloorOnThisLayer = true;
    bool hasRoofOnThisLayer = false;
};

struct FPSCEntityProfile {
    int bankId = 0;
    QString relPath;
    QString name;
    QString desc;
    EntityCategory category = EntityCategory::Unknown;
    
    QString modelPath;
    QString texturePath;
    QString altTexturePath;
    QString effectPath;
    QString iconBmpPath;
    
    int scale = 100;
    int health = 100;
    int speed = 100;
    bool isCharacter = false;
    bool isWeapon = false;
    bool isAmmo = false;
    bool isImmune = false;
    int collisionMode = 1;
    
    QString gunName;
    int ammoQty = 0;
    int quantity = 1;
    
    QString aiInit;
    QString aiMain;
    QString aiShoot;
    QString aiDestroy;
    QString useKey;
    QString ifUsed;
    
    QString soundSet;
    QString soundSet1;
    
    float lightRange = 0;
    QColor lightColor;
    
    // Memory profiling metrics
    qint64 meshSizeBytes = 0;
    qint64 diffuseSizeBytes = 0;
    int texWidth = 0, texHeight = 0;
    qint64 otherTexturesSizeBytes = 0;
    qint64 audioSizeBytes = 0;
    qint64 estimatedRAMBytes = 0;
};

struct PlacedEntity {
    int index = 0;
    int mainType = 0;
    int bankIndex = 0;
    int staticFlag = 0;
    
    float x = 0;
    float y = 0;
    float z = 0;
    float rx = 0;
    float ry = 0;
    float rz = 0;
    
    QString instanceName;
    QString aiInit;
    QString aiMain;
    QString aiShoot;
    QString aiDestroy;
    
    int isObjective = 0;
    QString useKey;
    QString ifUsed;
    QString ifUsedNear;
    int uniqueElement = 0;
    
    QString texd;
    QString texaltd;
    QString effect;
    int transparency = 0;
    int editorFixed = 0;
    
    QString soundSet;
    QString soundSet1;
    
    int spawnMax = 0;
    int spawnDelay = 0;
    int spawnQty = 0;
    int hurtFall = 0;
    int castShadow = 1;
    int reduceTexture = 0;
    int speed = 100;
    
    QString hasWeapon;
    
    int lives = 1;
    int spawnSubMax = 0;
    int spawnSubDelay = 0;
    int spawnSubQty = 0;
    
    float scale = 100;
    float coneHeight = 0;
    float coneAngle = 0;
    int strength = 100;
    int isImmobile = 0;
    int canTakeWeapon = 1;
    int quantity = 1;
    int markerIndex = 0;
    
    float lightRange = 0;
    QColor lightColor;
    int lightIndex = 0;
    
    int trigX1 = 0, trigY1 = 0, trigZ1 = 0;
    int trigX2 = 0, trigY2 = 0, trigZ2 = 0;
    QString baseDecal;
    
    int health = 100;
    int physics = 0;
    int phyWeight = 0;
    int phyFriction = 0;
    int phyForceDamage = 0;
    int rotateThrow = 0;
    int explodable = 0;
    int explodeDamage = 0;
    int phyAlways = 0;
    
    QByteArray rawExtension102;
    QByteArray rawExtension103_218;
    
    int floorLayer = 0;
    std::shared_ptr<FPSCEntityProfile> profile;
};

struct AIWaypoint {
    float x = 0, y = 0, z = 0;
    QVector<int> connections;
};

struct MapHeader {
    int layerMax = 20;
    int maxX = 40;
    int maxY = 40;
    int tileSize = 100;
    int olayListMax = 100;
    int multiplayer = 0;
};

struct FPSCMap {
    QString filePath;
    QString mapName;
    MapHeader header;
    
    int eleVersion = 218;
    bool isEncrypted = true;
    QString password = QStringLiteral("mypassword");
    QMap<QString, QByteArray> rawEntries;
    bool isModified = false;
    
    QVector<QString> segmentsBank;
    QVector<QString> entitiesBank;
    
    QMap<int, std::shared_ptr<FPSCSegment>> segments;
    QMap<int, std::shared_ptr<FPSCEntityProfile>> entityProfiles;
    
    // 3D Grid: [Layer][Y][X] -> Segment ID
    // Layer 0..layerMax, Y 0..maxY, X 0..maxX
    QVector<QVector<QVector<int>>> gridBlocks;
    QVector<QVector<QVector<int>>> gridRotation; // 0, 1, 2, 3
    
    QVector<PlacedEntity> placedEntities;
    QVector<AIWaypoint> waypoints;
    
    int activeEditorLayer = 0;
    float cameraX = 0, cameraY = 0, cameraZoom = 1.0f;
    
    void clear() {
        filePath.clear();
        mapName.clear();
        header = MapHeader();
        eleVersion = 218;
        isEncrypted = true;
        password = QStringLiteral("mypassword");
        rawEntries.clear();
        isModified = false;
        segmentsBank.clear();
        entitiesBank.clear();
        segments.clear();
        entityProfiles.clear();
        gridBlocks.clear();
        gridRotation.clear();
        placedEntities.clear();
        waypoints.clear();
    }
};

#endif // FPSCDATA_H
