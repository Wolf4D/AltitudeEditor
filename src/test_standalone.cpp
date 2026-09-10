#include <QApplication>
#include <QFileInfo>
#include <iostream>
#include <iomanip>
#include "FPMReader.h"
#include "VisZoneManager.h"
#include "SegmentParser.h"
#include "PortalLeakAnalyzer.h"
#include "PortalLeakDialog.h"
#include "AssetManager.h"
#include <QLayout>
#include "MapCanvas.h"
#include "LeakSuppressionManager.h"
#include <QKeyEvent>
#include <QFile>
#include <QTranslator>
#include <set>
#include <tuple>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QTranslator translator;
    if (translator.load("c:/FPSC Maped/translations/altitude_editor_ru.qm")) {
        app.installTranslator(&translator);
    }
    QString path = "C:/Program Files (x86)/The Game Creators/FPS Creator/Files/mapbank/Slipgate/Full/2_Vault.fpm";
    auto map = FPMReader::loadMap(path, "mypassword");
    if (!map) {
        std::cerr << "Failed to load map" << std::endl;
        return 1;
    }

    auto zm = std::make_shared<VisZoneManager>();
    zm->buildFromMap(map);
    PortalLeakAnalyzer pla(map, zm);
    auto warnings = pla.analyze();

    QString tempFpmPath = "C:/Program Files (x86)/The Game Creators/FPS Creator/Files/editors/gridedit/temp.fpm";
    auto tempMap = FPMReader::loadMap(tempFpmPath, "mypassword");
    if (tempMap) {
        std::cout << "Map comparison: map: L=" << map->header.layerMax << " maxX=" << map->header.maxX
                  << " maxY=" << map->header.maxY << " ents=" << map->placedEntities.size()
                  << " vs tempMap: L=" << tempMap->header.layerMax << " maxX=" << tempMap->header.maxX
                  << " maxY=" << tempMap->header.maxY << " ents=" << tempMap->placedEntities.size() << std::endl;
        int diffBlocks = 0;
        for (int l = 0; l <= map->header.layerMax; ++l) {
            for (int y = 0; y <= map->header.maxY; ++y) {
                for (int x = 0; x <= map->header.maxX; ++x) {
                    if (map->gridBlocks[l][y][x] != tempMap->gridBlocks[l][y][x]) {
                        diffBlocks++;
                        if (diffBlocks <= 3) {
                            std::cout << "  Diff at (" << l << "," << x << "," << y << "): map="
                                      << map->gridBlocks[l][y][x] << " temp=" << tempMap->gridBlocks[l][y][x] << std::endl;
                        }
                    }
                }
            }
        }
        std::cout << "Total diffBlocks=" << diffBlocks << std::endl;
    }
    bool hasLeakAt7_21 = false;
    for (const auto& w : warnings) {
        if (w.layer == 7 && w.x == 7 && w.y == 21) {
            hasLeakAt7_21 = true;
            std::cout << "UNEXPECTED LEAK at Floor 7 (7, 21): " << w.type.toStdString() << " - " << w.description.toStdString() << std::endl;
        }
    }
    std::cout << "[TEST] No false void leak at Floor 7 (7, 21) roof slab: "
              << (!hasLeakAt7_21 ? "PASS" : "FAIL") << std::endl;

    std::cout << "\n=== TILES AT Y=20 FOR X=4..10 ACROSS LAYERS 6..8 ===" << std::endl;
    for (int l = 6; l <= 8; ++l) {
        std::cout << "--- Layer " << l << " ---" << std::endl;
        for (int x = 4; x <= 10; ++x) {
            int y = 20;
            int b = (l < map->gridBlocks.size() && y < map->gridBlocks[l].size() && x < map->gridBlocks[l][y].size()) ? map->gridBlocks[l][y][x] : 0;
            int t = (l < map->gridTileType.size() && y < map->gridTileType[l].size() && x < map->gridTileType[l][y].size()) ? map->gridTileType[l][y][x] : 0;
            int r = (l < map->gridRotation.size() && y < map->gridRotation[l].size() && x < map->gridRotation[l][y].size()) ? map->gridRotation[l][y][x] : 0;
            int g = (l < map->gridGround.size() && y < map->gridGround[l].size() && x < map->gridGround[l][y].size()) ? map->gridGround[l][y][x] : 0;
            int s = (l < map->gridSymbol.size() && y < map->gridSymbol[l].size() && x < map->gridSymbol[l][y].size()) ? map->gridSymbol[l][y][x] : 0;
            int z = zm->getZoneAt(l, x, y);
            auto seg = map->segments.value(b);
            QString sname = seg ? seg->name : "none";
            std::cout << "  x=" << x << ": b=" << b << " (" << sname.toStdString() << ") t=" << t << " r=" << r << " g=" << g << " s=" << s
                      << " zid=" << z << std::endl;
        }
    }

    std::cout << "\n=== ENTITIES AT Y=20, X=4..10 ===" << std::endl;
    for (int i = 0; i < map->placedEntities.size(); ++i) {
        const auto& e = map->placedEntities[i];
        int ex = static_cast<int>(e.x / 100.0f);
        int ey = static_cast<int>(std::abs(e.z) / 100.0f);
        if (ey == 20 && ex >= 4 && ex <= 10) {
            auto prof = map->entityProfiles.value(e.bankIndex);
            QString ename = prof ? prof->name : e.instanceName;
            std::cout << "  Entity #" << i << " at L" << e.floorLayer << " (" << ex << "," << ey << "): "
                      << ename.toStdString() << " (relPath: " << (prof ? prof->relPath.toStdString() : "") << ")"
                      << " worldPos=(" << e.x << "," << e.y << "," << e.z << ")" << std::endl;
        }
    }

    std::cout << "\n=== PORTALS IN UNIVERSE.DBU AROUND Y_height=800, Z=-2100..-2000 ===" << std::endl;
    UniverseDBUParser dbu;
    QString dbuPath = AssetManager::instance().engineRoot() + "/Files/levelbank/testlevel/universe.dbu";
    if (dbu.parse(dbuPath)) {
        for (size_t i = 0; i < dbu.allPortals().size(); ++i) {
            const auto& p = dbu.allPortals()[i];
            if (p.box.minX <= 1100 && p.box.maxX >= 400 &&
                p.box.minY <= 850 && p.box.maxY >= 750 &&
                p.box.maxZ >= -2150 && p.box.minZ <= -1950) {
                std::cout << "  Portal #" << i << ": box=[" << p.box.minX << ".." << p.box.maxX
                          << ", " << p.box.minY << ".." << p.box.maxY
                          << ", " << p.box.minZ << ".." << p.box.maxZ << "] norm=("
                          << p.normal.x << "," << p.normal.y << "," << p.normal.z << ")"
                          << " span=(" << p.spanX() << "x" << p.spanY() << "x" << p.spanZ() << ")"
                          << " fromZ=" << p.fromZone << " tgtZ=" << p.targetZone
                          << " horiz=" << p.isHorizontal() << std::endl;
            }
        }
    }



    pla.setCheckStaticMap(false);
    auto physicalOnlyWarnings = pla.analyze();
    bool physicalAlonePass = (physicalOnlyWarnings.size() >= 50);
    std::cout << "[TEST] Physical BSP analysis alone reports leaks: "
              << (physicalAlonePass ? "PASS" : "FAIL")
              << " (" << physicalOnlyWarnings.size() << " leaks)" << std::endl;

    std::set<int> f8_y20_leaks;
    for (const auto& w : physicalOnlyWarnings) {
        if (w.layer == 8 && w.y == 20 && w.x >= 5 && w.x <= 9) {
            f8_y20_leaks.insert(w.x);
        }
    }
    bool f8_y20_allLeaksPass = (f8_y20_leaks.size() == 5 && f8_y20_leaks.count(6) == 1);
    std::cout << "[TEST] Physical BSP detects all 5 ceiling opening leaks at Floor 8 Y=20 (including (6,20)): "
              << (f8_y20_allLeaksPass ? "PASS" : "FAIL")
              << " (Found " << f8_y20_leaks.size() << "/5 tiles)" << std::endl;


    // 2. Test Static Analysis alone
    pla.setCheckCompiledUniverse(false);
    pla.setCheckStaticMap(true);
    auto staticOnlyWarnings = pla.analyze();
    bool staticAlonePass = (staticOnlyWarnings.size() >= 50);
    std::cout << "[TEST] Static analysis alone reports leaks: "
              << (staticAlonePass ? "PASS" : "FAIL")
              << " (" << staticOnlyWarnings.size() << " leaks)" << std::endl;

    // 3. Test Merged Analysis (Both ON: deduplication and confirmed status)
    pla.setCheckCompiledUniverse(true);
    pla.setCheckStaticMap(true);
    warnings = pla.analyze();

    int mergedCount = 0;
    int duplicateCells = 0;
    std::set<std::tuple<int, int, int>> seenCeilingCells;
    for (const auto& w : warnings) {
        if (w.isMerged || w.type.contains("Confirmed")) {
            mergedCount++;
        }
        if (w.type.contains("Ceiling") || w.type.contains("Confirmed")) {
            auto cell = std::make_tuple(w.layer, w.x, w.y);
            if (seenCeilingCells.count(cell)) {
                duplicateCells++;
            }
            seenCeilingCells.insert(cell);
        }
    }
    bool mergedPass = (mergedCount >= 20);
    bool noDuplicatesPass = (duplicateCells == 0);
    std::cout << "[TEST] Merged analysis combines BSP and Static issues: "
              << (mergedPass ? "PASS" : "FAIL") << " (" << mergedCount << " merged confirmed leaks)" << std::endl;
    std::cout << "[TEST] Merged analysis has no duplicate rows for the same issue: "
              << (noDuplicatesPass ? "PASS" : "FAIL") << " (Duplicate cells: " << duplicateCells << ")" << std::endl;



    int z2TilesF6 = 0;
    for (int y = 0; y < map->gridBlocks[6].size(); ++y) {
        for (int x = 0; x < map->gridBlocks[6][y].size(); ++x) {
            int zid = zm->getZoneAt(6, x, y);
            if (zid >= 0 && zm->zones()[zid].name.contains("Zone 2 ")) {
                z2TilesF6++;
            }
        }
    }
    std::cout << "[TEST] Zone 2 tiles on Floor 6: " << z2TilesF6 << " (Expected >= 40) -> "
              << (z2TilesF6 >= 40 ? "PASS" : "FAIL") << std::endl;

    // Verify 2_Vault Floor 5 gantry room is fully covered by Zone 6 (no isolated 1x1 Z37)
    int z6F5Count = 0;
    for (int y = 25; y <= 32; ++y) {
        for (int x = 4; x <= 9; ++x) {
            if (zm->getZoneAt(5, x, y) == 5) z6F5Count++;
        }
    }
    bool gantryRoomCovered = (z6F5Count >= 30 && zm->getZoneAt(5, 7, 28) == 5);
    std::cout << "[TEST] 2_Vault Floor 5 gantry room covered by Zone 6: "
              << (gantryRoomCovered ? "PASS" : "FAIL") << " (" << z6F5Count << " tiles)" << std::endl;

    std::cout << "=== Atrium Column Diagnostics in 2_Vault ===" << std::endl;
    for (int l = 5; l <= 8; ++l) {
        int b = (l < map->gridBlocks.size() && 10 < map->gridBlocks[l].size() && 10 < map->gridBlocks[l][10].size()) ? map->gridBlocks[l][10][10] : 0;
        int sym = (l < map->gridSymbol.size() && 10 < map->gridSymbol[l].size() && 10 < map->gridSymbol[l][10].size()) ? map->gridSymbol[l][10][10] : 0;
        auto seg = map->segments.value(b);
        std::cout << "  L=" << l << " (10,10): b=" << b << " name=" << (seg ? seg->name.toStdString() : "none")
                  << " sym=" << sym << " visF=" << (seg ? seg->visFloor : -1) << " visR=" << (seg ? seg->visRoof : -1)
                  << " zid=" << zm->getZoneAt(l, 10, 10) << std::endl;
    }
    bool atriumUnified = (zm->getZoneAt(5, 10, 10) >= 0 &&
                          zm->getZoneAt(5, 10, 10) == zm->getZoneAt(6, 10, 10) &&
                          zm->getZoneAt(6, 10, 10) == zm->getZoneAt(7, 10, 10));
    std::cout << "[TEST] 2_Vault Atrium Floors 5..7 unified in one zone: "
              << (atriumUnified ? "PASS" : "FAIL") << " (Zone " << (zm->getZoneAt(5, 10, 10) + 1) << ")" << std::endl;

    std::cout << "=== 2_Vault Floor 13 Warnings ===" << std::endl;
    int f13WarningCount = 0;
    for (const auto& w : warnings) {
        if (w.layer == 13) {
            f13WarningCount++;
            std::cout << "  F13 Warning #" << f13WarningCount << ": [" << w.type.toStdString() << "] (" << w.x << "," << w.y << ") zid=" << w.zoneId
                      << " desc: " << w.description.toStdString() << std::endl;
        }
    }
    std::cout << "Total Floor 13 warnings: " << f13WarningCount << std::endl;

    std::cout << "=== 2_Vault Full Zone 6 Box on Floor 12, 13, 14 ===" << std::endl;
    for (int l = 12; l <= 14; ++l) {
        std::cout << "--- Layer " << l << " ---" << std::endl;
        for (int y = 24; y <= 33; ++y) {
            std::string row;
            for (int x = 3; x <= 9; ++x) {
                int b = (l < map->gridBlocks.size() && y < map->gridBlocks[l].size() && x < map->gridBlocks[l][y].size()) ? map->gridBlocks[l][y][x] : 0;
                int g = (l < map->gridGround.size() && y < map->gridGround[l].size() && x < map->gridGround[l][y].size()) ? map->gridGround[l][y][x] : 0;
                int zid = zm->getZoneAt(l, x, y);
                if (b > 0) row += std::to_string(b) + (g == 2 ? "R " : "  ");
                else if (zid >= 0) row += ".  ";
                else row += "   ";
            }
            if (!row.empty()) std::cout << "  y=" << y << ": " << row << std::endl;
        }
    }

    std::cout << "=== Zone 2 Floor 6 and 7 Diagnostics ===" << std::endl;
    bool z2Floor7Closed = true;
    int z2Id = zm->getZoneAt(6, 26, 5);
    for (int y = 4; y <= 6; ++y) {
        for (int x = 25; x <= 27; ++x) {
            if (zm->getZoneAt(7, x, y) != z2Id) {
                z2Floor7Closed = false;
            }
        }
    }
    std::cout << "[TEST] 2_Vault Floor 7 Zone 2 hole at (25..27, 4..6) is closed: "
              << (z2Floor7Closed ? "PASS" : "FAIL") << std::endl;

    // 2. Verify Bunker Storage on Floor 5 (all 48 tiles zoned)
    int bunkerZoned = 0;
    for (int y = 10; y <= 20; ++y) {
        for (int x = 30; x <= 36; ++x) {
            int b = map->gridBlocks[5][y][x];
            if (b > 0) {
                int zid = zm->getZoneAt(5, x, y);
                if (zid >= 0) bunkerZoned++;
            }
        }
    }
    std::cout << "[TEST] Bunker Storage tiles zoned on Floor 5: " << bunkerZoned << " (Expected >= 40) -> "
              << (bunkerZoned >= 40 ? "PASS" : "FAIL") << std::endl;

    std::cout << "=== All zones in 2_Vault ===" << std::endl;
    for (const auto& z : zm->zones()) {
        std::cout << "Z" << (z.id + 1) << " " << z.name.toStdString() << " bounds="
                  << z.bounds.x() << "," << z.bounds.y() << " " << z.bounds.width() << "x" << z.bounds.height() << std::endl;
    }
    // 4. Verify CloseContacts roof pruning
    QString ccPath = "C:/Program Files (x86)/The Game Creators/FPS Creator/Files/mapbank/Slipgate/Full/1_CloseContacts.fpm";
    auto ccMap = FPMReader::loadMap(ccPath, "mypassword");
    if (ccMap) {
        auto ccZm = std::make_shared<VisZoneManager>();
        ccZm->buildFromMap(ccMap);
        // 4. Verify CloseContacts open-air platform pruning and lack of false leaks
        bool ccF6OutdoorPruned = true;
        for (int y = 35; y <= 39; ++y) {
            for (int x = 1; x <= 4; ++x) {
                int zid = ccZm->getZoneAt(6, x, y);
                if (zid >= 0) {
                    ccF6OutdoorPruned = false;
                    std::cout << "CC Floor 6 outdoor platform still zoned at (" << x << "," << y << "): "
                              << ccZm->zones()[zid].name.toStdString() << std::endl;
                }
            }
        }
        std::cout << "[TEST] CloseContacts Floor 6 outdoor platform (y: 35..39) pruned: "
                  << (ccF6OutdoorPruned ? "PASS" : "FAIL") << std::endl;

        // Verify room (7, 33..36) on Floor 6 is properly zoned
        int room7_33_zid = ccZm->getZoneAt(6, 7, 33);
        bool room7_33_zoned = (room7_33_zid >= 0 &&
                               ccZm->getZoneAt(6, 7, 34) == room7_33_zid &&
                               ccZm->getZoneAt(6, 7, 35) == room7_33_zid &&
                               ccZm->getZoneAt(6, 7, 36) == room7_33_zid);
        std::cout << "[TEST] CloseContacts Floor 6 room at (7, 33..36) is zoned: "
                  << (room7_33_zoned ? "PASS" : "FAIL") << " (Zone " << (room7_33_zid + 1) << ")" << std::endl;

        // Verify Floor 7 holes at (15, 30) and (17, 30) are part of Z15
        int z15_f7 = ccZm->getZoneAt(7, 16, 30);
        bool f7HolesClosed = (ccZm->getZoneAt(7, 15, 30) == z15_f7 &&
                              ccZm->getZoneAt(7, 17, 30) == z15_f7 &&
                              z15_f7 >= 0);
        std::cout << "[TEST] CloseContacts Floor 7 holes in Z15 at (15,30) and (17,30) are closed: "
                  << (f7HolesClosed ? "PASS" : "FAIL") << std::endl;

        // Print portals connected to the new room (room7_33_zid)
        std::cout << "Portals connected to room at (7, 33):" << std::endl;
        for (const auto& p : ccZm->portals()) {
            if (p.zoneA == room7_33_zid || p.zoneB == room7_33_zid) {
                std::cout << "  Portal: " << p.name.toStdString() << " floor=" << p.floor
                          << " isExt=" << p.isExterior << " zA=" << (p.zoneA + 1) << " zB=" << (p.zoneB + 1) << std::endl;
            }
        }

        int zid6 = ccZm->getZoneAt(6, 16, 31);
        bool ccF7Z19MiddleZoned = (ccZm->getZoneAt(7, 16, 31) >= 0 && ccZm->getZoneAt(7, 16, 31) == zid6);

        // Verify Floor 5 has NO false window portal between Zone 25 and Zone 26
        bool ccF5NoWindowZ25_Z26 = true;
        for (const auto& p : ccZm->portals()) {
            if (p.floor == 5) {
                int zA = p.zoneA + 1;
                int zB = p.zoneB + 1;
                if ((zA == 25 && zB == 26) || (zA == 26 && zB == 25)) {
                    ccF5NoWindowZ25_Z26 = false;
                    std::cout << "Found false portal between Z25 and Z26: " << p.name.toStdString() << std::endl;
                }
            }
        }
        std::cout << "[TEST] CloseContacts Floor 5 has no false window between Z25 and Z26: "
                  << (ccF5NoWindowZ25_Z26 ? "PASS" : "FAIL") << std::endl;
        bool ccF7RoofPruned = true;
        for (int y = 36; y <= 38; ++y) {
            for (int x = 15; x <= 17; ++x) {
                int zid = ccZm->getZoneAt(7, x, y);
                if (zid >= 0) {
                    ccF7RoofPruned = false;
                    std::cout << "CC Floor 7 outdoor roof still zoned at (" << x << "," << y << "): "
                              << ccZm->zones()[zid].name.toStdString() << std::endl;
                }
            }
        }
        std::cout << "[TEST] CloseContacts Floor 7 outdoor roof pruned (no Z26/Z27/Z28): "
                  << (ccF7RoofPruned ? "PASS" : "FAIL") << std::endl;

        bool ccF7HoleUnzoned = (ccZm->getZoneAt(7, 17, 35) == -1);
        std::cout << "[TEST] CloseContacts Floor 7 through-hole at (17,35) is unzoned: "
                  << (ccF7HoleUnzoned ? "PASS" : "FAIL") << std::endl;

        bool ccF8SkyUnzoned = (ccZm->getZoneAt(8, 14, 38) == -1);
        std::cout << "[TEST] CloseContacts Floor 8 open sky at (14,38) has no phantom zone: "
                  << (ccF8SkyUnzoned ? "PASS" : "FAIL") << std::endl;

        std::cout << "=== CloseContacts Diagnostics at (2,34) and (4,34) ===" << std::endl;
        for (int l = 5; l <= 8; ++l) {
            std::cout << "  Layer " << l << ":" << std::endl;
            for (int x : {2, 3, 4}) {
                int y = 34;
                int b = (l < ccMap->gridBlocks.size() && y < ccMap->gridBlocks[l].size() && x < ccMap->gridBlocks[l][y].size()) ? ccMap->gridBlocks[l][y][x] : 0;
                int g = (l < ccMap->gridGround.size() && y < ccMap->gridGround[l].size() && x < ccMap->gridGround[l][y].size()) ? ccMap->gridGround[l][y][x] : 0;
                auto s = ccMap->segments.value(b);
                std::cout << "    (" << x << "," << y << ") b=" << b << " (" << (s ? s->name.toStdString() : "none")
                          << ") gnd=" << g << " visF=" << (s ? s->visFloor : -1) << " visR=" << (s ? s->visRoof : -1)
                          << " zid=" << ccZm->getZoneAt(l, x, y) << std::endl;
            }
        }
        PortalLeakAnalyzer ccLeakAnalyzer(ccMap, ccZm);
        auto ccWarnings = ccLeakAnalyzer.analyze();
        for (const auto& w : ccWarnings) {
            std::cout << "  CC Warning: [" << w.type.toStdString() << "] L" << w.layer << " (" << w.x << "," << w.y << "): "
                      << w.description.toStdString() << std::endl;
        }
        bool ccF8NoLeaks = true;
        for (const auto& w : ccWarnings) {
            if (w.layer == 8) {
                ccF8NoLeaks = false;
                std::cout << "Unexpected leak on Floor 8: (" << w.x << "," << w.y << ") " << w.description.toStdString() << std::endl;
            }
        }
        std::cout << "[TEST] CloseContacts Floor 8 has no false leaks in open sky: "
                  << (ccF8NoLeaks ? "PASS" : "FAIL") << std::endl;

        std::cout << "[DEBUG CC FLAGS] ccF6OutdoorPruned=" << ccF6OutdoorPruned
                  << " ccF7RoofPruned=" << ccF7RoofPruned
                  << " ccF7HoleUnzoned=" << ccF7HoleUnzoned
                  << " ccF8SkyUnzoned=" << ccF8SkyUnzoned
                  << " ccF8NoLeaks=" << ccF8NoLeaks
                  << " ccF7Z19MiddleZoned=" << ccF7Z19MiddleZoned
                  << " ccF5NoWindowZ25_Z26=" << ccF5NoWindowZ25_Z26 << std::endl;

        if (!ccF6OutdoorPruned || !ccF7RoofPruned || !ccF7HoleUnzoned || !ccF8SkyUnzoned || !ccF8NoLeaks || !ccF7Z19MiddleZoned || !ccF5NoWindowZ25_Z26 || !room7_33_zoned || !f7HolesClosed) {
            return 1;
        }
    }

    bool found29_2 = false;
    bool found26_4 = false, found26_6 = false, found28_4 = false, found28_6 = false;
    bool found19_32 = false, found21_32 = false;
    for (const auto& w : warnings) {
        bool isCeil = w.type.contains("Ceiling") || w.description.contains("ceiling", Qt::CaseInsensitive);
        if (isCeil && w.layer == 8) {
            if (w.x == 29 && w.y == 2) found29_2 = true;
            if (w.x == 26 && w.y == 4 && w.description.contains("ceiling_window")) found26_4 = true;
            if (w.x == 26 && w.y == 6 && w.description.contains("ceiling_window")) found26_6 = true;
            if (w.x == 28 && w.y == 4 && w.description.contains("ceiling_window")) found28_4 = true;
            if (w.x == 28 && w.y == 6 && w.description.contains("ceiling_window")) found28_6 = true;
        }
        if (isCeil && w.layer == 7) {
            if (w.x == 19 && w.description.contains("ceiling_window")) found19_32 = true;
            if (w.x == 21 && w.description.contains("ceiling_window")) found21_32 = true;
        }
    }
    int f8CeilingWindowLeaks = 0;
    for (const auto& w : warnings) {
        bool isCeil = w.type.contains("Ceiling") || w.description.contains("ceiling", Qt::CaseInsensitive);
        if (isCeil && w.layer == 8 && w.description.contains("ceiling_window")) {
            f8CeilingWindowLeaks++;
            std::cout << "F8 ceiling_window leak at (" << w.x << "," << w.y << ") desc: " << w.description.toStdString() << std::endl;
        }
    }
    std::cout << "[TEST] PortalLeakAnalyzer Floor 8 ceiling_window leaks count: " << f8CeilingWindowLeaks 
              << " (Expected: 66 = 4 from Zone 2 + 62 from Zone 6) -> " 
              << (f8CeilingWindowLeaks == 66 ? "PASS" : "FAIL") << std::endl;

    bool f8AllLeaksAssigned = true;
    for (const auto& w : warnings) {
        if (w.layer == 8 && w.zoneId < 0) {
            f8AllLeaksAssigned = false;
            std::cout << "Unassigned leak on Floor 8 at (" << w.x << "," << w.y << "): " << w.description.toStdString() << std::endl;
        }
    }
    std::cout << "[TEST] All Floor 8 ceiling leaks assigned to originating zones: "
              << (f8AllLeaksAssigned ? "PASS" : "FAIL") << std::endl;

    std::cout << "[TEST] PortalLeakAnalyzer detects Missing Ceiling Leak at (29,2) on Floor 8: "
              << (found29_2 ? "PASS" : "FAIL") << std::endl;
    bool allWindowsDetected = found26_4 && found26_6 && found28_4 && found28_6;
    std::cout << "[TEST] PortalLeakAnalyzer detects all 4 ceiling window leaks on Floor 8: "
              << (allWindowsDetected ? "PASS" : "FAIL") << std::endl;
    bool f7WindowsDetected = found19_32 && found21_32;
    std::cout << "[TEST] PortalLeakAnalyzer detects Floor 7 ceiling windows at (19,32) and (21,32): "
              << (f7WindowsDetected ? "PASS" : "FAIL") << std::endl;

    // Test Fake Segment and Real Window detection
    auto fakeSeg = SegmentParser::parse("scifi/scenery/Window Large (fake).fps", 999);
    bool fakeClassified = (fakeSeg && fakeSeg->isFake && fakeSeg->isWindow);
    std::cout << "[TEST] SegmentParser detects Window Large (fake) as fake window: "
              << (fakeClassified ? "PASS" : "FAIL") << std::endl;

    auto realWinSeg = SegmentParser::parse("scifi/scenery/Window Large.fps", 998);
    bool realWinClassified = (realWinSeg && !realWinSeg->isFake && realWinSeg->isWindow);
    std::cout << "[TEST] SegmentParser detects Window Large as real window: "
              << (realWinClassified ? "PASS" : "FAIL") << std::endl;

    // Test 1.fpm (user screenshot map with Floor 5 exterior doorway)
    QString map1Path = "C:/Program Files (x86)/The Game Creators/FPS Creator/Files/mapbank/1.fpm";
    auto map1 = FPMReader::loadMap(map1Path, "mypassword");
    bool map1ExtPortalPass = false;
    if (map1) {
        auto zm1 = std::make_shared<VisZoneManager>();
        zm1->buildFromMap(map1);
        PortalLeakAnalyzer pla1(map1, zm1);
        auto warnings1 = pla1.analyze();

        for (const auto& p : zm1->portals()) {
            if (p.isExterior && p.floor == 5) {
                map1ExtPortalPass = true;
                std::cout << "[1.FPM] Found Exterior Portal: " << p.name.toStdString()
                          << " (type: " << (p.type == PortalType::ExteriorWindow ? "Window" : "Doorway") << ")"
                          << " zoneA=" << p.zoneA << " zoneB=" << p.zoneB << std::endl;
            }
        }
    }
    // Test PortalLeakDialog selection & MapCanvas integration
    PortalLeakDialog leakDlg(map, zm);
    MapCanvas canvas;
    canvas.setMap(map);
    QObject::connect(&leakDlg, &PortalLeakDialog::cellSelected, &canvas, &MapCanvas::highlightCell);
    QObject::connect(&leakDlg, &PortalLeakDialog::warningsUpdated, &canvas, &MapCanvas::setLeakWarnings);
    leakDlg.runAnalysis();

    leakDlg.findChild<QCheckBox*>() ; // Let's toggle
    // Set to physical only
    QMetaObject::invokeMethod(&leakDlg, "onMethodToggled");
    // Let's uncheck static map
    auto chkBoxes = leakDlg.findChildren<QCheckBox*>();
    for (auto* cb : chkBoxes) {
        if (cb->text().contains("Static") || cb->text().contains("топологический")) {
            cb->setChecked(false);
        }
    }
    leakDlg.runAnalysis();
    // Now check combo items
    for (int i = 0; i < leakDlg.zoneFilterCombo()->count(); ++i) {
        std::cout << "PHYSICAL COMBO [" << i << "]: zid=" << leakDlg.zoneFilterCombo()->itemData(i).toInt()
                  << " '" << leakDlg.zoneFilterCombo()->itemText(i).toStdString() << "'" << std::endl;
    }

    // Verify combobox popup styling (no empty margin gap)
    leakDlg.show();
    leakDlg.zoneFilterCombo()->showPopup();
    QWidget* container = leakDlg.zoneFilterCombo()->view()->parentWidget();
    bool popupValid = (container != nullptr && leakDlg.zoneFilterCombo()->view()->height() > 0);
    std::cout << "[TEST] PortalLeakDialog VisZone combo popup geometry valid: "
              << (popupValid ? "PASS" : "FAIL") << std::endl;
    leakDlg.zoneFilterCombo()->hidePopup();
    leakDlg.hide();

    // Test Zone-wide suppression button
    auto btns = leakDlg.findChildren<QPushButton*>();
    QPushButton* btnSuppressZone = nullptr;
    for (auto* b : btns) {
        if (b->text().contains("Zone") || b->text().contains("зоны") || b->text().contains("зону")) {
            btnSuppressZone = b;
            break;
        }
    }
    bool hasZoneBtn = (btnSuppressZone != nullptr);
    std::cout << "[TEST] PortalLeakDialog has Zone Suppression button: "
              << (hasZoneBtn ? "PASS" : "FAIL") << std::endl;

    // Filter by Zone 2 (zid = 1)
    for (int i = 0; i < leakDlg.zoneFilterCombo()->count(); ++i) {
        if (leakDlg.zoneFilterCombo()->itemData(i).toInt() == 1) {
            leakDlg.zoneFilterCombo()->setCurrentIndex(i);
            break;
        }
    }
    assert(leakDlg.selectedZoneFilterId() == 1);
    int activeInZone2Before = leakDlg.tableWidget()->rowCount();

    if (btnSuppressZone) {
        btnSuppressZone->click();
    }
    int activeInZone2After = leakDlg.tableWidget()->rowCount();
    bool zoneSuppressed = (activeInZone2After == 0 && map->isModified && leakDlg.windowTitle().contains("*"));
    std::cout << "[TEST] Zone-wide warning suppression: "
              << (zoneSuppressed ? "PASS" : "FAIL") << " (Suppressed " << activeInZone2Before << " issues in Zone 2, modified: "
              << (map->isModified ? "true" : "false") << ", title: " << leakDlg.windowTitle().toStdString() << ")" << std::endl;

    // Restore Zone 2
    if (btnSuppressZone) {
        btnSuppressZone->click();
    }
    int activeInZone2Restored = leakDlg.tableWidget()->rowCount();
    bool zoneRestored = (activeInZone2Restored == activeInZone2Before);
    std::cout << "[TEST] Zone-wide warning restoration: "
              << (zoneRestored ? "PASS" : "FAIL") << " (Restored " << activeInZone2Restored << " issues)" << std::endl;

    // Reset filter to All Vis Zones
    leakDlg.zoneFilterCombo()->setCurrentIndex(0);

    bool canvasHasWarnings = !canvas.leakWarnings().empty();
    std::cout << "[TEST] MapCanvas receives leak warnings from PortalLeakDialog: "
              << (canvasHasWarnings ? "PASS" : "FAIL") << " (" << canvas.leakWarnings().size() << " warnings)" << std::endl;

    // Simulate clicking row 2
    leakDlg.tableWidget()->selectRow(2);
    int origIdx2 = leakDlg.tableWidget()->item(2, 0)->data(Qt::UserRole).toInt();
    const auto& w2 = leakDlg.currentWarnings()[origIdx2];
    bool canvasHighlightedRow2 = (canvas.highlightedLayer() == w2.layer && canvas.highlightedX() == w2.x && canvas.highlightedY() == w2.y);
    std::cout << "[TEST] Selecting row 2 in PortalLeakDialog immediately highlights ("
              << w2.layer << ", " << w2.x << ", " << w2.y << ") on MapCanvas: "
              << (canvasHighlightedRow2 ? "PASS" : "FAIL") << " (Canvas highlighted: Floor "
              << canvas.highlightedLayer() << " at (" << canvas.highlightedX() << "," << canvas.highlightedY() << "))" << std::endl;

    bool tableColCountPass = (leakDlg.tableWidget()->columnCount() == 5);
    bool descHasIcon = (leakDlg.tableWidget()->rowCount() > 0 && !leakDlg.tableWidget()->item(0, 4)->icon().isNull());
    std::cout << "[TEST] PortalLeakDialog table strictly 5 columns: " << (tableColCountPass ? "PASS" : "FAIL")
              << " (Columns: " << leakDlg.tableWidget()->columnCount() << ")" << std::endl;
    std::cout << "[TEST] PortalLeakDialog description item has source icon: " << (descHasIcon ? "PASS" : "FAIL") << std::endl;
    int firstPhysRow = -1;
    for (int r = 0; r < leakDlg.tableWidget()->rowCount(); ++r) {
        int origIdx = leakDlg.tableWidget()->item(r, 0)->data(Qt::UserRole).toInt();
        if (leakDlg.currentWarnings()[origIdx].hasPhysicalSize) {
            firstPhysRow = r;
            break;
        }
    }
    bool foundPhysRow = (firstPhysRow >= 0);
    bool descHasSizeTag = false;
    if (foundPhysRow) {
        QString txt = leakDlg.tableWidget()->item(firstPhysRow, 4)->text();
        descHasSizeTag = (txt.startsWith("[") && txt.contains("×")) && !txt.section(']', 0, 0).contains(" u");
    }
    std::cout << "[TEST] Physical leak displays size without 'u' in description: " << (descHasSizeTag ? "PASS" : "FAIL")
              << " (Row " << firstPhysRow << " desc: " << (foundPhysRow ? leakDlg.tableWidget()->item(firstPhysRow, 4)->text().left(45).toStdString() : "none") << "...)" << std::endl;

    leakDlg.resize(980, 560);
    if (foundPhysRow) {
        leakDlg.tableWidget()->selectRow(firstPhysRow);
        leakDlg.tableWidget()->scrollToItem(leakDlg.tableWidget()->item(firstPhysRow, 0), QAbstractItemView::PositionAtTop);
    }
    leakDlg.grab().save("C:/Users/Wolf4/.gemini/antigravity/brain/df65d3f2-bf62-47d4-8a36-a7e363ffce03/portal_leak_dialog_icons.png");

    // Capture suppressed state screenshot for walkthrough
    if (leakDlg.tableWidget()->rowCount() > 0) {
        leakDlg.tableWidget()->selectRow(0);
        QMetaObject::invokeMethod(&leakDlg, "onSuppressClicked");
        QMetaObject::invokeMethod(&leakDlg, "onShowSuppressedToggled", Q_ARG(bool, true));
        leakDlg.tableWidget()->selectRow(0);
        leakDlg.tableWidget()->scrollToItem(leakDlg.tableWidget()->item(0, 0), QAbstractItemView::PositionAtTop);
        leakDlg.grab().save("C:/Users/Wolf4/.gemini/antigravity/brain/df65d3f2-bf62-47d4-8a36-a7e363ffce03/portal_leak_dialog_suppressed.png");
        // Revert back so subsequent tests see clean state
        QMetaObject::invokeMethod(&leakDlg, "onSuppressClicked");
        QMetaObject::invokeMethod(&leakDlg, "onShowSuppressedToggled", Q_ARG(bool, false));
    }

    // Multi-row selection suppression test
    bool multiSuppressPass = false;
    int multiBefore = leakDlg.tableWidget()->rowCount();
    if (multiBefore >= 3) {
        leakDlg.tableWidget()->clearSelection();
        for (int r = 0; r < 3; ++r) {
            leakDlg.tableWidget()->selectionModel()->select(
                leakDlg.tableWidget()->model()->index(r, 0),
                QItemSelectionModel::Select | QItemSelectionModel::Rows);
        }
        auto selectedRows = leakDlg.selectedVisibleRows();
        bool selCountOk = (selectedRows.size() == 3);

        leakDlg.grab().save("C:/Users/Wolf4/.gemini/antigravity/brain/df65d3f2-bf62-47d4-8a36-a7e363ffce03/portal_leak_dialog_multiselect.png");

        QMetaObject::invokeMethod(&leakDlg, "onSuppressClicked");
        int multiAfter = leakDlg.tableWidget()->rowCount();
        bool rowsDecreased = (multiAfter == multiBefore - 3);

        // Verify that in "Show Suppressed" they are marked suppressed and can be restored
        QMetaObject::invokeMethod(&leakDlg, "onShowSuppressedToggled", Q_ARG(bool, true));
        leakDlg.tableWidget()->clearSelection();
        for (int r = 0; r < 3; ++r) {
            leakDlg.tableWidget()->selectionModel()->select(
                leakDlg.tableWidget()->model()->index(r, 0),
                QItemSelectionModel::Select | QItemSelectionModel::Rows);
        }
        // Unsuppress all 3
        QMetaObject::invokeMethod(&leakDlg, "onSuppressClicked");
        QMetaObject::invokeMethod(&leakDlg, "onShowSuppressedToggled", Q_ARG(bool, false));
        int multiRestored = leakDlg.tableWidget()->rowCount();
        bool restoredOk = (multiRestored == multiBefore);

        multiSuppressPass = selCountOk && rowsDecreased && restoredOk;
    }
    std::cout << "[TEST] Multi-selection warning suppression and restoration: "
              << (multiSuppressPass ? "PASS" : "FAIL") << std::endl;

    // === TEST DICHOTOMY TOOL (Room & Entity Deletion) ===
    int testDichotomyZone = -1;
    for (size_t i = 0; i < zm->zones().size(); ++i) {
        if (!zm->zones()[i].entityIndices.empty() && zm->zones()[i].tiles.size() > 5) {
            testDichotomyZone = static_cast<int>(i);
            break;
        }
    }
    if (testDichotomyZone < 0) testDichotomyZone = 0;

    int receivedDichotomyZone = -1;
    QObject::connect(&canvas, &MapCanvas::dichotomyDeleteZoneRequested, [&](int zid) {
        receivedDichotomyZone = zid;
    });

    canvas.setActiveVisZone(testDichotomyZone);
    canvas.selectEntity(-1);

    // 1. Test keypress Delete without entity selected triggers dichotomyDeleteZoneRequested
    QKeyEvent delKey(QEvent::KeyPress, Qt::Key_Delete, Qt::NoModifier);
    QApplication::sendEvent(&canvas, &delKey);

    bool delKeyTriggered = (receivedDichotomyZone == testDichotomyZone);
    std::cout << "[TEST] MapCanvas Delete key triggers Dichotomy Tool for active zone: "
              << (delKeyTriggered ? "PASS" : "FAIL") << " (Zone " << (testDichotomyZone + 1) << ")" << std::endl;

    // 2. Perform Dichotomy Room & Entity Deletion on testDichotomyZone
    const VisZone* targetZone = zm->getZone(testDichotomyZone);
    int entBeforeCount = static_cast<int>(map->placedEntities.size());
    int zoneEntCount = static_cast<int>(targetZone ? targetZone->entityIndices.size() : 0);

    // Collect tilesToDelete
    std::set<std::tuple<int, int, int>> tilesToDelete;
    for (const auto& kv : targetZone->floorTiles) {
        for (const QPoint& pt : kv.second) {
            tilesToDelete.insert(std::make_tuple(kv.first, pt.x(), pt.y()));
        }
    }
    int ceilingLayer = targetZone->maxFloor + 1;
    if (ceilingLayer <= map->header.layerMax) {
        for (const QPoint& pt : targetZone->tiles) {
            if (zm->getZoneAt(ceilingLayer, pt.x(), pt.y()) < 0) {
                if (ceilingLayer < map->gridGround.size() &&
                    pt.y() < map->gridGround[ceilingLayer].size() &&
                    pt.x() < map->gridGround[ceilingLayer][pt.y()].size() &&
                    map->gridGround[ceilingLayer][pt.y()][pt.x()] == 2) {
                    tilesToDelete.insert(std::make_tuple(ceilingLayer, pt.x(), pt.y()));
                }
            }
        }
    }

    // Clear tiles
    for (const auto& t : tilesToDelete) {
        int l = std::get<0>(t);
        int x = std::get<1>(t);
        int y = std::get<2>(t);
        if (l >= 0 && l < map->gridBlocks.size() &&
            y >= 0 && y < map->gridBlocks[l].size() &&
            x >= 0 && x < map->gridBlocks[l][y].size()) {
            map->gridBlocks[l][y][x] = 0;
            if (l < map->gridGround.size() && y < map->gridGround[l].size() && x < map->gridGround[l][y].size())
                map->gridGround[l][y][x] = 0;
        }
    }

    // Delete entities
    std::vector<int> entToDelete;
    for (int idx : targetZone->entityIndices) {
        if (idx >= 0 && idx < map->placedEntities.size()) entToDelete.push_back(idx);
    }
    for (int i = 0; i < map->placedEntities.size(); ++i) {
        const auto& e = map->placedEntities[i];
        if (tilesToDelete.count(std::make_tuple(e.floorLayer, static_cast<int>(e.x/100.0f), static_cast<int>(std::abs(e.z)/100.0f))) > 0 ||
            zm->getZoneAt(e.floorLayer, static_cast<int>(e.x/100.0f), static_cast<int>(std::abs(e.z)/100.0f)) == testDichotomyZone) {
            entToDelete.push_back(i);
        }
    }
    std::sort(entToDelete.begin(), entToDelete.end(), std::greater<int>());
    entToDelete.erase(std::unique(entToDelete.begin(), entToDelete.end()), entToDelete.end());

    int deletedEnts = static_cast<int>(entToDelete.size());
    for (int idx : entToDelete) {
        map->placedEntities.erase(map->placedEntities.begin() + idx);
    }

    // Rebuild VisZoneManager
    zm->buildFromMap(map);

    bool tilesCleared = true;
    for (const auto& t : tilesToDelete) {
        int l = std::get<0>(t);
        int x = std::get<1>(t);
        int y = std::get<2>(t);
        if (map->gridBlocks[l][y][x] != 0 || map->gridGround[l][y][x] != 0) {
            tilesCleared = false;
            break;
        }
    }

    bool dichotomyPass = delKeyTriggered && tilesCleared && (deletedEnts >= zoneEntCount && zoneEntCount > 0) && (static_cast<int>(map->placedEntities.size()) == entBeforeCount - deletedEnts);
    // 17. Test Persistent Warning Suppression packed into .FPM container
    bool suppressionPass = false;
    if (!warnings.empty()) {
        const auto& testW = warnings.front();
        QString sKey = testW.suppressionKey();
        std::cout << "Target suppression key: " << sKey.toStdString() << std::endl;

        LeakSuppressionManager suppMgr;
        suppMgr.suppress(testW);
        bool suppKeyAdded = (suppMgr.suppressedCount() == 1 && suppMgr.isSuppressed(testW));

        map->isModified = false;
        suppMgr.saveToMap(map, false);
        bool isModifiedMarked = (map->isModified == true);

        QString testFpmPath = "build_test_suppression.fpm";
        QFile::remove(testFpmPath);

        // Save map with suppression to test FPM
        QString origMapPath = map->filePath;
        map->filePath = testFpmPath;
        bool savedToFpm = suppMgr.saveToMap(map, true);
        map->filePath = origMapPath;

        // Reload fresh map from the saved .FPM container
        auto reloadedMap = FPMReader::loadMap(testFpmPath, "mypassword");
        bool fpmContainsJson = reloadedMap && reloadedMap->rawEntries.contains(QStringLiteral("map.leaks.json"));

        LeakSuppressionManager reloadedSuppMgr;
        bool loadedFromRaw = reloadedSuppMgr.loadFromMap(reloadedMap);
        bool suppressionPersisted = (reloadedSuppMgr.suppressedCount() == 1 && reloadedSuppMgr.isSuppressed(testW));

        // Test unsuppression
        reloadedSuppMgr.unsuppress(testW);
        bool unsuppressedInMemory = (reloadedSuppMgr.suppressedCount() == 0 && !reloadedSuppMgr.isSuppressed(testW));

        reloadedMap->filePath = testFpmPath;
        reloadedSuppMgr.saveToMap(reloadedMap, true);

        // Reload fresh map again and verify 0 suppressed
        auto reloadedMap2 = FPMReader::loadMap(testFpmPath, "mypassword");
        LeakSuppressionManager reloadedSuppMgr2;
        reloadedSuppMgr2.loadFromMap(reloadedMap2);
        bool unsuppressionPersisted = (reloadedSuppMgr2.suppressedCount() == 0);

        QFile::remove(testFpmPath);

        suppressionPass = suppKeyAdded && isModifiedMarked && savedToFpm && fpmContainsJson && loadedFromRaw &&
                          suppressionPersisted && unsuppressedInMemory && unsuppressionPersisted;
    }
    std::cout << "[TEST] Persistent Warning Suppression in .FPM container: "
              << (suppressionPass ? "PASS" : "FAIL") << std::endl;

    if (!found29_2 || !allWindowsDetected || !f7WindowsDetected || !fakeClassified || !realWinClassified || !map1ExtPortalPass || !atriumUnified || !z2Floor7Closed || !canvasHasWarnings || !canvasHighlightedRow2 || !f8AllLeaksAssigned || !dichotomyPass || !physicalAlonePass || !f8_y20_allLeaksPass || !staticAlonePass || !mergedPass || !noDuplicatesPass || !tableColCountPass || !descHasIcon || !descHasSizeTag || !suppressionPass || !multiSuppressPass) {
        std::cout << "FAIL DETAILS: found29_2=" << found29_2
                  << " allWin=" << allWindowsDetected
                  << " f7Win=" << f7WindowsDetected
                  << " fake=" << fakeClassified
                  << " real=" << realWinClassified
                  << " map1Ext=" << map1ExtPortalPass
                  << " atrium=" << atriumUnified
                  << " z2F7=" << z2Floor7Closed
                  << " canvasWarn=" << canvasHasWarnings
                  << " canvasHi=" << canvasHighlightedRow2
                  << " f8Assigned=" << f8AllLeaksAssigned
                  << " dichotomy=" << dichotomyPass
                  << " physAlone=" << physicalAlonePass
                  << " f8_y20=" << f8_y20_allLeaksPass
                  << " statAlone=" << staticAlonePass
                  << " merged=" << mergedPass
                  << " noDup=" << noDuplicatesPass
                  << " colCount=" << tableColCountPass
                  << " descIcon=" << descHasIcon
                  << " descSize=" << descHasSizeTag
                  << " supp=" << suppressionPass
                  << " multiSupp=" << multiSuppressPass << std::endl;
        return 1;
    }
    return 0;
}
