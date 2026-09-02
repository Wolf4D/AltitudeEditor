#include "UniverseDBUParser.h"
#include <QFile>
#include <QDebug>
#include <cmath>

bool UniverseDBUParser::parse(const QString& filePath) {
    m_zones.clear();
    m_allPortals.clear();
    m_leakingPortals.clear();

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Failed to open universe.dbu:" << filePath;
        return false;
    }

    QByteArray data = file.readAll();
    file.close();

    if (data.size() < 16) return false;

    const uint8_t* raw = reinterpret_cast<const uint8_t*>(data.constData());
    int size = data.size();

    m_sizeX = *reinterpret_cast<const uint32_t*>(raw + 0);
    m_sizeY = *reinterpret_cast<const uint32_t*>(raw + 4);
    m_sizeZ = *reinterpret_cast<const uint32_t*>(raw + 8);
    m_numZones = *reinterpret_cast<const uint32_t*>(raw + 12);

    m_zones.resize(m_numZones);
    for (uint32_t i = 0; i < m_numZones; ++i) {
        m_zones[i].id = i;
    }

    // Robust scan for portal records in universe.dbu
    for (int i = 16; i < size - 120; ++i) {
        uint32_t targetZone = *reinterpret_cast<const uint32_t*>(raw + i);
        uint32_t numVerts = *reinterpret_cast<const uint32_t*>(raw + i + 4);
        uint32_t flags = *reinterpret_cast<const uint32_t*>(raw + i + 8);

        if (numVerts >= 3 && numVerts <= 8 && flags <= 5 && targetZone <= m_numZones + 5) {
            const float* f = reinterpret_cast<const float*>(raw + i + 12);
            bool validFloats = true;
            for (int k = 0; k < 12 + static_cast<int>(numVerts) * 3; ++k) {
                if (std::isnan(f[k]) || std::abs(f[k]) > 10000.0f) {
                    validFloats = false;
                    break;
                }
            }

            if (validFloats) {
                float minX = f[0], minY = f[1], minZ = f[2];
                float maxX = f[3], maxY = f[4], maxZ = f[5];
                float cenX = f[6], cenY = f[7], cenZ = f[8];
                float normX = f[9], normY = f[10], normZ = f[11];

                if (minX <= maxX && minY <= maxY && minZ <= maxZ && 
                    (maxX - minX + maxY - minY + maxZ - minZ) > 10.0f) {
                    
                    DBUPortal p;
                    p.targetZone = targetZone;
                    p.numVertices = numVerts;
                    p.flags = flags;
                    p.box = {minX, minY, minZ, maxX, maxY, maxZ, cenX, cenY, cenZ};
                    p.normal = {normX, normY, normZ};

                    for (uint32_t v = 0; v < numVerts; ++v) {
                        p.vertices.push_back({f[12 + v*3], f[12 + v*3 + 1], f[12 + v*3 + 2]});
                    }

                    // Assign to fromZone based on portal center containment
                    for (uint32_t z = 0; z < m_numZones; ++z) {
                        if (m_zones[z].box.contains(cenX, cenY, cenZ)) {
                            p.fromZone = z;
                            m_zones[z].portals.push_back(p);
                            break;
                        }
                    }

                    m_allPortals.push_back(p);
                    i += 12 + (12 + numVerts * 3) * 4 - 1;
                }
            }
        }
    }

    identifyLeaks();
    return true;
}

void UniverseDBUParser::identifyLeaks() {
    m_leakingPortals.clear();

    for (auto& portal : m_allPortals) {
        float lenX = portal.box.maxX - portal.box.minX;
        float lenZ = portal.box.maxZ - portal.box.minZ;

        if (lenX > 600.0f || lenZ > 600.0f || portal.box.minZ <= -3500.0f) {
            portal.isExteriorHull = true;
        }

        bool isLeak = false;

        // Leak Condition 1: Target zone does not exist
        if (portal.targetZone >= m_numZones) {
            isLeak = true;
        }

        // Leak Condition 2: Portal touches universe bounding limits with outward normal
        if ((portal.box.minX <= 10.0f && portal.normal.x < 0.0f) ||
            (portal.box.maxX >= (m_sizeX - 10.0f) && portal.normal.x > 0.0f) ||
            (portal.box.minZ <= (-static_cast<float>(m_sizeZ) + 10.0f) && portal.normal.z < 0.0f)) {
            isLeak = true;
        }

        if (isLeak) {
            portal.isLeak = true;
            m_leakingPortals.push_back(portal);
        }
    }
}
