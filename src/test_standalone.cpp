#include <QCoreApplication>
#include <iostream>
#include "UniverseDBUParser.h"

int main(int argc, char* argv[]) {
    UniverseDBUParser parser;
    if (parser.parse("C:/Program Files (x86)/The Game Creators/FPS Creator/Files/levelbank/testlevel/universe.dbu")) {
        const auto& portals = parser.allPortals();
        printf("=== DBU Portals (%d) ===\n", (int)portals.size());
        for (int i = 0; i < (int)portals.size(); ++i) {
            const auto& p = portals[i];
            printf("Portal [%d]: from (%u -> %u), X=(%.1f..%.1f), Y=(%.1f..%.1f), Z=(%.1f..%.1f), grid=(%d, %d), isExt=%d\n",
                   i, p.fromZone, p.targetZone, p.box.minX, p.box.maxX, p.box.minY, p.box.maxY, p.box.minZ, p.box.maxZ,
                   p.gridX(), p.gridY(), p.isExteriorHull);
        }
    } else {
        printf("Failed to parse testlevel universe.dbu\n");
    }

    return 0;
}
