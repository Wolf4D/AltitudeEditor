#include <QApplication>
#include <iostream>
#include "FPMReader.h"
#include "VisZoneManager.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    // 1.fpm
    {
        QString fpmPath = "C:/Program Files (x86)/The Game Creators/FPS Creator/Files/mapbank/1.fpm";
        auto map = FPMReader::loadMap(fpmPath, "mypassword");
        VisZoneManager vm;
        vm.buildFromMap(map);
        printf("1.fpm total zones: %zu\n", vm.zones().size());
        for (size_t i = 0; i < vm.zones().size(); ++i) {
            const auto& z = vm.zones()[i];
            printf("  Zone %zu: minFloor=%d maxFloor=%d name='%s'\n",
                   i + 1, z.minFloor, z.maxFloor, qPrintable(z.name));
        }
    }

    // CloseContacts.fpm
    {
        QString fpmPath = "C:/Program Files (x86)/The Game Creators/FPS Creator/Files/mapbank/Slipgate/Full/1_CloseContacts.fpm";
        auto map = FPMReader::loadMap(fpmPath, "mypassword");
        VisZoneManager vm;
        vm.buildFromMap(map);
        printf("CloseContacts total zones: %zu\n", vm.zones().size());
        for (size_t i = 0; i < vm.zones().size(); ++i) {
            const auto& z = vm.zones()[i];
            if (z.hasFloor(8)) {
                printf("  Zone on floor 8: Zone %zu (id %d) minFloor=%d maxFloor=%d name='%s'\n",
                       i + 1, z.id, z.minFloor, z.maxFloor, qPrintable(z.name));
            }
        }
    }

    return 0;
}
