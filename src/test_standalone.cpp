#include <QApplication>
#include <iostream>
#include "FPMReader.h"
#include "PortalLeakAnalyzer.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QString fpmPath = "C:/Program Files (x86)/The Game Creators/FPS Creator/Files/mapbank/leaks.fpm";
    auto map = FPMReader::loadMap(fpmPath, "mypassword");
    if (!map) return 1;

    PortalLeakAnalyzer analyzer(map);
    // default flags: checkCompiledUniverse=true, checkStaticMap=true
    auto warnings = analyzer.analyze();

    printf("Total warnings found in leaks.fpm: %zu\n", warnings.size());
    for (const auto& w : warnings) {
        printf("  [%s] %s at Layer %d (%d, %d): %s\n",
               w.severity == PortalLeakWarning::ERROR ? "ERROR" : "WARNING",
               qPrintable(w.type), w.layer, w.x, w.y, qPrintable(w.description));
    }
    return 0;
}
