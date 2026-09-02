#include <QApplication>
#include <iostream>
#include "FPMReader.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    auto map = FPMReader::loadMap("C:/Program Files (x86)/The Game Creators/FPS Creator/Files/mapbank/1.fpm", "mypassword");
    if (!map) return 1;

    for (int l = 5; l <= 7; ++l) {
        int floorCount = 0;
        int ceilingCount = 0;
        int noFloorCount = 0;
        for (int y = 0; y < map->gridBlocks[l].size(); ++y) {
            for (int x = 0; x < map->gridBlocks[l][y].size(); ++x) {
                int s = map->gridBlocks[l][y][x];
                if (s > 0) {
                    const auto& seg = map->segments[s];
                    int sym = map->gridSymbol[l][y][x];
                    int ground = map->gridGround[l][y][x];
                    bool isCeilingSeg = (ground == 2) || (seg->groundMode == 2 && seg->hasRoofOnThisLayer) ||
                                        (seg->visFloor == -1 && seg->visRoof >= 0);
                    if (isCeilingSeg) {
                        ceilingCount++;
                    } else if (seg->hasFloorOnThisLayer && seg->visFloor >= 0 && sym != 1) {
                        floorCount++;
                    } else {
                        noFloorCount++;
                    }
                }
            }
        }
        printf("1.fpm layer %d: %d floors, %d ceilings, %d wall-only (no floor)\n",
               l, floorCount, ceilingCount, noFloorCount);
    }
    return 0;
}
