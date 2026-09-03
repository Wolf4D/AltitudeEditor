#include "MainWindow.h"
#include "AssetManager.h"
#include "FPMReader.h"
#include "MemoryAnalyzer.h"
#include <QApplication>
#include <QStyleFactory>
#include <QPalette>
#include <QDir>
#include <QDebug>

int main(int argc, char* argv[]) {
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);

    QApplication app(argc, argv);
    app.setApplicationName("FPSCreatorMapViewer");
    app.setOrganizationName("TheGameCreators");

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
        "QToolBar { background: #222634; border-bottom: 1px solid #333a4c; spacing: 6px; padding: 3px; }"
        "QToolButton { background: #2c3244; border: 1px solid #3d465c; border-radius: 4px; padding: 4px 8px; color: #d0d8e8; font-weight: bold; }"
        "QToolButton:hover { background: #3a4258; border-color: #5c6c8e; color: #ffffff; }"
        "QToolButton:pressed { background: #1f2330; }"
        "QToolButton:checked { background: #2980b9; border-color: #3498db; color: #ffffff; }"
        "QComboBox, QSpinBox, QLineEdit { background: #1e222d; border: 1px solid #3d465c; border-radius: 4px; padding: 4px; color: #ffffff; }"
        "QComboBox:hover, QSpinBox:hover, QLineEdit:hover { border-color: #5288db; }"
        "QTableWidget, QTreeWidget { background: #181b24; alternate-background-color: #1f2330; border: 1px solid #333a4c; gridline-color: #2b3140; color: #d0d8e8; }"
        "QHeaderView::section { background: #242938; color: #9bb0d0; border: 1px solid #333a4c; padding: 4px; font-weight: bold; }"
        "QDockWidget { titlebar-close-icon: url(); titlebar-normal-icon: url(); font-weight: bold; }"
        "QDockWidget::title { background: #242938; border: 1px solid #333a4c; padding: 6px; text-align: left; }"
        "QStatusBar { background: #1a1d26; border-top: 1px solid #2d3344; color: #90a0b8; }"
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
