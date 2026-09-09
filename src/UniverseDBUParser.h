#pragma once

#include <QString>
#include <QVector>
#include <QPointF>
#include <QRectF>
#include <vector>
#include <cstdint>
#include <cmath>

struct DBPUVector3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct DBPUBoundingBox {
    float minX = 0.0f, minY = 0.0f, minZ = 0.0f;
    float maxX = 0.0f, maxY = 0.0f, maxZ = 0.0f;
    float cenX = 0.0f, cenY = 0.0f, cenZ = 0.0f;

    bool contains(float x, float y, float z) const {
        return x >= minX && x <= maxX && y >= minY && y <= maxY && z >= minZ && z <= maxZ;
    }

    bool intersectsLayer(int layer) const {
        float lMin = layer * 100.0f;
        float lMax = (layer + 1) * 100.0f;
        return (maxY >= (lMin - 15.0f) && minY <= (lMax + 15.0f));
    }
};

struct DBUPortal {
    uint32_t fromZone = 0;
    uint32_t targetZone = 0;
    uint32_t numVertices = 0;
    uint32_t flags = 0;
    DBPUBoundingBox box;
    DBPUVector3 normal;
    std::vector<DBPUVector3> vertices;

    bool isLeak = false; // Flagged if pointing to outside void or unsealed boundary
    bool isExteriorHull = false; // Spans outside boundary into void

    float spanX() const { return box.maxX - box.minX; }
    float spanY() const { return box.maxY - box.minY; }
    float spanZ() const { return box.maxZ - box.minZ; }

    bool isHorizontal() const {
        return std::abs(normal.y) > 0.7f || spanY() < 1.0f;
    }

    float width() const {
        if (isHorizontal()) {
            return std::max(spanX(), spanZ());
        } else {
            return std::abs(normal.x) > std::abs(normal.z) ? spanZ() : spanX();
        }
    }

    float height() const {
        if (isHorizontal()) {
            return std::min(spanX(), spanZ());
        } else {
            return spanY();
        }
    }

    bool isSubSegment() const {
        float w = width();
        float h = height();
        return (w < 95.0f || h < 95.0f) && (w > 1.0f && h > 1.0f);
    }

    int minLayer() const { return static_cast<int>(box.minY / 100.0f); }
    int maxLayer() const { return static_cast<int>(box.maxY / 100.0f); }
    
    // Grid coordinate mapping (X: 0..40, Y: 0..40)
    int gridX() const { return static_cast<int>(box.cenX / 100.0f); }
    int gridY() const { 
        return static_cast<int>(std::abs(box.cenZ) / 100.0f);
    }
    int layer() const {
        if (isHorizontal()) {
            return static_cast<int>(std::round(box.cenY / 100.0f));
        } else {
            return static_cast<int>(std::floor(box.cenY / 100.0f));
        }
    }
};

struct DBUVisZone {
    uint32_t id = 0;
    DBPUBoundingBox box;
    std::vector<DBUPortal> portals;

    bool isLeaking = false;
};

class UniverseDBUParser {
public:
    UniverseDBUParser() = default;

    bool parse(const QString& filePath);

    uint32_t universeSizeX() const { return m_sizeX; }
    uint32_t universeSizeY() const { return m_sizeY; }
    uint32_t universeSizeZ() const { return m_sizeZ; }
    uint32_t numZones() const { return m_numZones; }

    const std::vector<DBUVisZone>& zones() const { return m_zones; }
    const std::vector<DBUPortal>& allPortals() const { return m_allPortals; }
    const std::vector<DBUPortal>& leakingPortals() const { return m_leakingPortals; }

private:
    uint32_t m_sizeX = 4000;
    uint32_t m_sizeY = 2100;
    uint32_t m_sizeZ = 4000;
    uint32_t m_numZones = 0;

    std::vector<DBUVisZone> m_zones;
    std::vector<DBUPortal> m_allPortals;
    std::vector<DBUPortal> m_leakingPortals;

    void identifyLeaks();
};
