#include <QApplication>
#include <QPainter>
#include <iostream>
#include "FPMReader.h"
#include "MapCanvas.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    auto map = FPMReader::loadMap("C:/Program Files (x86)/The Game Creators/FPS Creator/Files/mapbank/leaks.fpm", "mypassword");
    if (!map) return 1;

    MapCanvas canvas;
    canvas.resize(1000, 700);
    canvas.setMap(map);
    canvas.setFloor(6);
    canvas.setShowGhostLayer(true);

    QPixmap px(1000, 700);
    px.fill(QColor(20, 24, 30));
    QPainter p(&px);
    canvas.render(&p);
    p.end();

    px.save("C:/Users/Wolf4/.gemini/antigravity/brain/df65d3f2-bf62-47d4-8a36-a7e363ffce03/test_floor6_with_ghost_lower.png");
    printf("Rendered test_floor6_with_ghost_lower.png successfully!\n");
    return 0;
}
