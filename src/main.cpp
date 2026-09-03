#include "MainWindow.h"
#include "AssetManager.h"
#include "FPMReader.h"
#include "MemoryAnalyzer.h"
#include "Version.h"
#include <QApplication>
#include <QStyleFactory>
#include <QPalette>
#include <QDir>
#include <QDebug>

int main(int argc, char* argv[]) {
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);

    QApplication app(argc, argv);
    app.setApplicationName(VersionInfo::AppName);
    app.setApplicationVersion(VersionInfo::Version);
    app.setOrganizationName(VersionInfo::Studio);

    // Apply Deep Dark Fusion Theme
    app.setStyle(QStyleFactory::create("Fusion"));

    QPalette darkPalette;
    darkPalette.setColor(QPalette::Window, QColor(30, 34, 45));
    darkPalette.setColor(QPalette::WindowText, QColor(220, 225, 235));
    darkPalette.setColor(QPalette::Base, QColor(20, 23, 30));
    darkPalette.setColor(QPalette::AlternateBase, QColor(27, 31, 40));
    darkPalette.setColor(QPalette::ToolTipBase, QColor(25, 30, 40));
    darkPalette.setColor(QPalette::ToolTipText, Qt::white);
    darkPalette.setColor(QPalette::Text, QColor(225, 230, 240));
    darkPalette.setColor(QPalette::Button, QColor(38, 43, 56));
    darkPalette.setColor(QPalette::ButtonText, QColor(230, 235, 245));
    darkPalette.setColor(QPalette::BrightText, Qt::red);
    darkPalette.setColor(QPalette::Link, QColor(41, 128, 185));
    darkPalette.setColor(QPalette::Highlight, QColor(52, 152, 219));
    darkPalette.setColor(QPalette::HighlightedText, Qt::white);

    app.setPalette(darkPalette);

    app.setStyleSheet(
        "QToolTip { color: #ffffff; background-color: #1e222d; border: 1px solid #4a5568; padding: 4px; border-radius: 3px; }"
        "QToolBar { background: #1e222d; border-bottom: 1px solid #2e3545; spacing: 5px; padding: 3px 6px; }"
        "QToolBar::separator { background: #353d4f; width: 1px; margin: 4px 6px; }"
        "QToolButton { background: #262c3a; border: 1px solid #384256; border-radius: 4px; padding: 0px 8px; height: 26px; min-height: 26px; max-height: 26px; color: #c4cede; font-size: 12px; }"
        "QToolButton:hover { background: #303748; border-color: #4a5670; color: #ffffff; }"
        "QToolButton:pressed { background: #1c202a; }"
        "QToolButton:checked { background: #2b3d59; border: 1px solid #4f6e9e; color: #ffffff; font-weight: bold; }"
        "QToolButton:checked:hover { background: #354768; border-color: #6185bd; }"
        "QComboBox, QSpinBox, QLineEdit { background: #181b24; border: 1px solid #384256; border-radius: 4px; padding: 0px 8px; height: 26px; min-height: 26px; max-height: 26px; color: #ffffff; font-size: 12px; }"
        "QComboBox:hover, QSpinBox:hover, QLineEdit:hover { border-color: #4a75b5; }"
        "QPushButton { background: #262c3a; border: 1px solid #384256; border-radius: 4px; padding: 0px 10px; height: 26px; min-height: 26px; max-height: 26px; color: #c4cede; font-size: 12px; }"
        "QPushButton:hover { background: #303748; border-color: #4a5670; color: #ffffff; }"
        "QPushButton:pressed { background: #1c202a; }"
        "QTableWidget, QTreeWidget { background: #181b24; alternate-background-color: #1f2330; border: 1px solid #2e3545; gridline-color: #262b37; color: #d0d8e8; }"
        "QHeaderView::section { background: #222634; color: #9bb0d0; border: 1px solid #2e3545; padding: 4px; font-weight: bold; }"
        "QDockWidget { titlebar-close-icon: url(); titlebar-normal-icon: url(); font-weight: bold; }"
        "QDockWidget::title { background: #222634; border-bottom: 1px solid #2e3545; padding: 6px; text-align: left; color: #c4cede; }"
        "QStatusBar { background: #181b24; border-top: 1px solid #262c3a; color: #8898b0; }"
    );

    // Check for CLI memory analysis mode
    QStringList args = app.arguments();
    if (args.contains("--analyze-memory") && args.size() >= 3) {
        int idx = args.indexOf("--analyze-memory");
        QString mapPath = args.value(idx + 1);
        auto map = FPMReader::loadMap(mapPath, "mypassword");
        if (!map) {
            fprintf(stderr, "Failed to load map: %s\n", qPrintable(mapPath));
            std::exit(1);
        }
        auto rep = MemoryAnalyzer::analyze(map);
        fprintf(stdout, "=== MEMORY ANALYSIS: %s ===\n", qPrintable(map->mapName));
        fprintf(stdout, "Total Estimated Level RAM: %.2f MB\n", rep.totalEstimatedRamBytes / (1024.0 * 1024.0));
        fprintf(stdout, "  - Segment Architecture: %.2f MB (%d types, %d blocks)\n",
                rep.totalSegmentRamBytes / (1024.0 * 1024.0), rep.uniqueSegmentTypesCount, rep.totalPlacedSegmentBlocks);
        fprintf(stdout, "  - Placed Entities: %.2f MB (%d types, %d placed)\n",
                rep.totalEntityRamBytes / (1024.0 * 1024.0), rep.uniqueEntityTypesCount, rep.totalPlacedEntities);
        fprintf(stdout, "  - Universe & Lightmaps: %.2f MB\n", (rep.universeCsgRamBytes + rep.lightmapsRamBytes) / (1024.0 * 1024.0));
        fprintf(stdout, "  - Engine Baseline: %.2f MB\n", rep.engineBaselineRamBytes / (1024.0 * 1024.0));
        fprintf(stdout, "Engine 32-bit Limit: %.1f%% (Status: %s)\n", rep.engineLimitPercent, qPrintable(rep.riskLevel));
        fflush(stdout);
        std::exit(0);
    }

    // Check for CLI export / test mode
    if (args.contains("--export-png")) {
        int idx = args.indexOf("--export-png");
        if (args.size() >= idx + 3) {
            QString mapPath = args.value(idx + 1);
            QString outPath = args.value(idx + 2);
            int floor = (args.size() > idx + 3) ? args.value(idx + 3).toInt() : 0;

            fprintf(stdout, "Loading map: %s\n", qPrintable(mapPath));
            fflush(stdout);

            auto map = FPMReader::loadMap(mapPath, "mypassword");
            if (!map) {
                fprintf(stderr, "Failed to load map: %s\n", qPrintable(mapPath));
                std::exit(1);
            }

            MapCanvas canvas;
            canvas.resize(1400, 1000);
            canvas.setMap(map);
            canvas.setFloor(floor);
            if (args.contains("--color-zones")) {
                canvas.setColorAllVisZones(true);
            }
            canvas.zoomFit();

            QPixmap pix(canvas.size());
            pix.fill(QColor(22, 25, 34));
            canvas.render(&pix);
            bool ok = pix.save(outPath);
            fprintf(stdout, "Saved snapshot (%dx%d) to: %s (Result: %d)\n", pix.width(), pix.height(), qPrintable(outPath), ok ? 1 : 0);
            fflush(stdout);
            std::exit(ok ? 0 : 1);
        }
    }

    if (args.contains("--snapshot-window")) {
        int idx = args.indexOf("--snapshot-window");
        if (args.size() >= idx + 3) {
            QString mapPath = args.value(idx + 1);
            QString outPath = args.value(idx + 2);
            int entIdx = (args.size() > idx + 3) ? args.value(idx + 3).toInt() : 0;

            MainWindow window;
            window.resize(1400, 900);
            window.loadMapFile(mapPath);
            window.show();
            app.processEvents();

            if (args.contains("--color-zones")) {
                if (auto* dock = window.findChild<VisZoneDock*>()) {
                    dock->show();
                    dock->setColorAllZones(true);
                }
            }

            // Select entity to show in inspector
            // Focus on entity
            QMetaObject::invokeMethod(&window, "onFocusEntityRequested", Q_ARG(int, entIdx));
            app.processEvents();

            QPixmap pix(window.size());
            window.render(&pix);
            bool ok = pix.save(outPath);
            fprintf(stdout, "Saved window snapshot to: %s (Result: %d)\n", qPrintable(outPath), ok ? 1 : 0);
            fflush(stdout);
            std::exit(ok ? 0 : 1);
        }
    }

    if (args.contains("--snapshot-memory")) {
        int idx = args.indexOf("--snapshot-memory");
        if (args.size() >= idx + 3) {
            QString mapPath = args.value(idx + 1);
            QString outPath = args.value(idx + 2);

            auto map = FPMReader::loadMap(mapPath, "mypassword");
            if (map) {
                MemoryAnalyzerDialog dlg(map);
                dlg.resize(1060, 720);
                dlg.show();
                app.processEvents();

                QPixmap pix(dlg.size());
                dlg.render(&pix);
                bool ok = pix.save(outPath);
                fprintf(stdout, "Saved memory snapshot to: %s (Result: %d)\n", qPrintable(outPath), ok ? 1 : 0);
                fflush(stdout);
                std::exit(ok ? 0 : 1);
            }
        }
    }
    if (args.contains("--dump-floor")) {
        int idx = args.indexOf("--dump-floor");
        QString mapPath = args.value(idx + 1);
        int floor = args.value(idx + 2).toInt();
        auto map = FPMReader::loadMap(mapPath, "mypassword");
        if (map && floor < map->gridBlocks.size()) {
            int rows = map->gridBlocks[floor].size();
            int cols = rows > 0 ? map->gridBlocks[floor][0].size() : 0;
            fprintf(stdout, "Map: %s Floor %d: %dx%d\n", qPrintable(map->mapName), floor, cols, rows);
        }
        fflush(stdout);
        std::exit(0);
    }

    MainWindow window;
    if (args.size() > 1 && !args[1].startsWith('-')) {
        window.loadMapFile(args[1]);
    } else {
        QString defaultMap = AssetManager::instance().engineRoot() + "/Files/mapbank/1.fpm";
        if (QFileInfo::exists(defaultMap)) {
            window.loadMapFile(defaultMap);
        }
    }
    window.show();

    return app.exec();
}
