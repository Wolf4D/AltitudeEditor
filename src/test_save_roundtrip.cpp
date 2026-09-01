#include "FPMReader.h"
#include "FPMWriter.h"
#include <QCoreApplication>
#include <iostream>

bool testMapRoundtrip(const QString& mapName) {
    QString srcPath = "C:/Program Files (x86)/The Game Creators/FPS Creator/Files/mapbank/" + mapName;
    auto map = FPMReader::loadMap(srcPath, "mypassword");
    if (!map) {
        std::cerr << "FAIL: Could not load " << mapName.toStdString() << std::endl;
        return false;
    }
    int origCount = map->placedEntities.size();

    // Modify first entity
    map->placedEntities[0].instanceName = "Custom Test Name";
    map->placedEntities[0].x += 20.0f;
    map->placedEntities[0].health = 999;

    QString savePath = "c:/FPSC Maped/test_roundtrip_" + mapName;
    if (!FPMWriter::saveMap(map, savePath, "mypassword")) {
        std::cerr << "FAIL: Could not save " << savePath.toStdString() << std::endl;
        return false;
    }

    auto reloaded = FPMReader::loadMap(savePath, "mypassword");
    if (!reloaded || reloaded->placedEntities.size() != origCount) {
        std::cerr << "FAIL: Entity count mismatch on reload for " << mapName.toStdString() << std::endl;
        return false;
    }

    const auto& e0 = reloaded->placedEntities[0];
    if (e0.instanceName != "Custom Test Name" || e0.health != 999) {
        std::cerr << "FAIL: Modified property mismatch in " << mapName.toStdString() << std::endl;
        return false;
    }

    std::cout << "SUCCESS: " << mapName.toStdString() << " (" << origCount << " entities) saved and reloaded perfectly!" << std::endl;
    return true;
}

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    std::cout << "Running comprehensive roundtrip tests..." << std::endl;

    bool ok1 = testMapRoundtrip("1.fpm");
    bool ok2 = testMapRoundtrip("BadMondayFixing.fpm");
    bool ok3 = testMapRoundtrip("123.fpm");
    bool ok4 = testMapRoundtrip("EMINEM.fpm");

    if (ok1 && ok2 && ok3 && ok4) {
        std::cout << "ALL MAP ROUNDTRIP TESTS PASSED (100%)!" << std::endl;
        return 0;
    }
    return 1;
}
