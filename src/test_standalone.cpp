#include "FPMReader.h"
#include "AssetManager.h"
#include "MemoryAnalyzer.h"
#include "MapCanvas.h"
#include <QApplication>
#include <QPainter>
#include <iostream>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    std::cout << "Starting standalone test..." << std::endl;

    QString mapPath = "C:/Program Files (x86)/The Game Creators/FPS Creator/Files/mapbank/1.fpm";
    QString outPng = "C:/Users/Wolf4/.gemini/antigravity/brain/df65d3f2-bf62-47d4-8a36-a7e363ffce03/screenshot_map1.png";
    int floor = 0;

    if (argc > 1) mapPath = argv[1];
    if (argc > 2) outPng = argv[2];
    if (argc > 3) floor = QString(argv[3]).toInt();

    std::cout << "Loading map: " << mapPath.toStdString() << std::endl;
    auto map = FPMReader::loadMap(mapPath, "mypassword");

    if (!map) {
        std::cerr << "Failed to load map!" << std::endl;
        return 1;
    }

    std::cout << "Map Loaded Successfully!" << std::endl;
    std::cout << "  Name: " << map->mapName.toStdString() << std::endl;
    std::cout << "  Header: layers=" << map->header.layerMax << ", max_x=" << map->header.maxX << ", max_y=" << map->header.maxY << std::endl;
    std::cout << "  Segments count: " << map->segmentsBank.size() << std::endl;
    std::cout << "  Entity types: " << map->entitiesBank.size() << std::endl;
    std::cout << "  Placed entities: " << map->placedEntities.size() << std::endl;

    std::cout << "Running Memory Analysis..." << std::endl;
    auto rep = MemoryAnalyzer::analyze(map);
    std::cout << "Memory Report:" << std::endl;
    std::cout << "  Total RAM: " << (rep.totalEstimatedRamBytes / (1024.0 * 1024.0)) << " MB" << std::endl;
    std::cout << "  Meshes: " << (rep.totalMeshRamBytes / (1024.0 * 1024.0)) << " MB" << std::endl;
    std::cout << "  Textures: " << (rep.totalTextureRamBytes / (1024.0 * 1024.0)) << " MB" << std::endl;
    std::cout << "  Audio: " << (rep.totalAudioRamBytes / (1024.0 * 1024.0)) << " MB" << std::endl;
    std::cout << "  Engine Limit: " << rep.engineLimitPercent << "% (" << rep.riskLevel.toStdString() << ")" << std::endl;

    std::cout << "Rendering MapCanvas step by step..." << std::endl;
    MapCanvas canvas;
    canvas.resize(1280, 800);
    canvas.setMap(map);
    canvas.setFloor(floor);

    float customZoom = 0.0f;
    if (argc > 4) customZoom = QString(argv[4]).toFloat();

    if (customZoom > 0.01f) {
        canvas.zoomReset();
        // Zoom in to inspect Doom-style wall strips & entity details
        for (int z = 0; z < static_cast<int>(customZoom); ++z) {
            canvas.zoomIn();
        }
        // Center on the first placed entity on active floor or first segment
        bool centered = false;
        for (int i = 0; i < map->placedEntities.size(); ++i) {
            if (map->placedEntities[i].floorLayer == floor) {
                canvas.focusOnEntity(i);
                centered = true;
                break;
            }
        }
        if (!centered) {
            canvas.setFloor(floor);
            canvas.zoomFit();
        }
    } else {
        canvas.zoomFit();
    }

    QImage img(1280, 800, QImage::Format_ARGB32_Premultiplied);
    img.fill(QColor(22, 25, 34));
    QPainter painter(&img);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    std::cout << "Calling renderMap..." << std::endl;
    canvas.renderMap(painter);
    painter.end();
    std::cout << "Finished renderMap, saving image..." << std::endl;

    bool ok = img.save(outPng);
    std::cout << "Saved screenshot to " << outPng.toStdString() << ": " << (ok ? "SUCCESS" : "FAILED") << std::endl;

    std::cout << "Standalone test complete!" << std::endl;
    std::exit(ok ? 0 : 1);
}

