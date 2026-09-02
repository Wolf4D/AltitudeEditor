#include <QApplication>
#include <QKeyEvent>
#include <iostream>
#include <cassert>
#include "FPMReader.h"
#include "VisZoneManager.h"
#include "MapCanvas.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    auto map = FPMReader::loadMap("C:/Program Files (x86)/The Game Creators/FPS Creator/Files/mapbank/1.fpm", "mypassword");
    if (!map) return 1;

    auto visMgr = std::make_shared<VisZoneManager>();
    visMgr->buildFromMap(map);

    MapCanvas canvas;
    canvas.setMap(map);
    canvas.setVisZoneManager(visMgr);

    // 1. Isolate Zone 1
    canvas.setActiveVisZone(0);
    canvas.setVisZoneCulling(true, 0.0f);
    assert(canvas.activeVisZoneId() == 0);
    printf("1. Zone 0 activated and isolated successfully.\n");

    // 2. Press Escape key
    QKeyEvent escPress(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
    QCoreApplication::sendEvent(&canvas, &escPress);
    assert(canvas.activeVisZoneId() == -1);
    printf("2. Escape key reset active zone to -1 (Normal View) successfully.\n");

    // 3. Re-isolate and test direct reset
    canvas.setActiveVisZone(1);
    canvas.setVisZoneCulling(true, 0.15f);
    assert(canvas.activeVisZoneId() == 1);
    canvas.setActiveVisZone(-1);
    canvas.setVisZoneCulling(false, 0.0f);
    assert(canvas.activeVisZoneId() == -1);
    printf("3. Direct reset to Normal View succeeded.\n");

    printf("All regression tests passed successfully!\n");
    return 0;
}
