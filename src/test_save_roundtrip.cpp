#include "FPMReader.h"
#include "FPMWriter.h"
#include <QCoreApplication>
#include <iostream>

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    std::cout << "Testing Light Color Persistence..." << std::endl;

    QString srcPath = "C:/Program Files (x86)/The Game Creators/FPS Creator/Files/mapbank/1.fpm";
    auto map = FPMReader::loadMap(srcPath, "mypassword");
    if (!map) {
        std::cerr << "FAIL: Could not load 1.fpm" << std::endl;
        return 1;
    }

    // Entity 2: Set to Cyan (0, 255, 255)
    map->placedEntities[2].lightColor = QColor(0, 255, 255);
    map->placedEntities[2].lightRange = 400.0f;

    // Entity 4: Set to Magenta (255, 0, 255)
    map->placedEntities[4].lightColor = QColor(255, 0, 255);
    map->placedEntities[4].lightRange = 450.0f;

    // Entity 5: Set to Custom Orange (255, 128, 0)
    map->placedEntities[5].lightColor = QColor(255, 128, 0);
    map->placedEntities[5].lightRange = 320.0f;

    QString savePath = "c:/FPSC Maped/test_lights.fpm";
    if (!FPMWriter::saveMap(map, savePath, "mypassword")) {
        std::cerr << "FAIL: Could not save test_lights.fpm" << std::endl;
        return 1;
    }

    auto loaded = FPMReader::loadMap(savePath, "mypassword");
    if (!loaded) {
        std::cerr << "FAIL: Could not reload test_lights.fpm" << std::endl;
        return 1;
    }

    const auto& e2 = loaded->placedEntities[2];
    const auto& e4 = loaded->placedEntities[4];
    const auto& e5 = loaded->placedEntities[5];

    std::cout << "E2: Color=RGB(" << e2.lightColor.red() << "," << e2.lightColor.green() << "," << e2.lightColor.blue()
              << ") Range=" << e2.lightRange << std::endl;
    std::cout << "E4: Color=RGB(" << e4.lightColor.red() << "," << e4.lightColor.green() << "," << e4.lightColor.blue()
              << ") Range=" << e4.lightRange << std::endl;
    std::cout << "E5: Color=RGB(" << e5.lightColor.red() << "," << e5.lightColor.green() << "," << e5.lightColor.blue()
              << ") Range=" << e5.lightRange << std::endl;

    if (e2.lightColor != QColor(0, 255, 255) || e2.lightRange != 400.0f) {
        std::cerr << "FAIL: E2 Cyan light mismatch!" << std::endl;
        return 1;
    }
    if (e4.lightColor != QColor(255, 0, 255) || e4.lightRange != 450.0f) {
        std::cerr << "FAIL: E4 Magenta light mismatch!" << std::endl;
        return 1;
    }
    if (e5.lightColor != QColor(255, 128, 0) || e5.lightRange != 320.0f) {
        std::cerr << "FAIL: E5 Orange light mismatch!" << std::endl;
        return 1;
    }

    std::cout << "SUCCESS: All modified light colors and ranges preserved 100%!" << std::endl;
    return 0;
}
