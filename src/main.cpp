#include "MainWindow.h"
#include "AssetManager.h"
#include "FPMReader.h"
#include "MemoryAnalyzer.h"
#include "PortalLeakAnalyzer.h"
#include "Version.h"
#include "LanguageManager.h"
#include "SegmentEditorDialog.h"
#include "VisZoneManager.h"
#include <QApplication>
#include <QStyleFactory>
#include <QPalette>
#include <QDir>
#include <QDebug>
#include <QToolBar>
#include <QAction>
#include <QThread>
#include <QSettings>

static void printCliHelp() {
    fprintf(stdout,
        "Altitude Editor CLI - Level Analysis & Automation Tool\n"
        "Usage: AltitudeEditor-cli [options]\n\n"
        "General Options:\n"
        "  -h, --help                          Show this help message and exit\n"
        "  -v, --version                       Print version and build number\n\n"
        "Level Inspection & Analysis:\n"
        "  --analyze-memory <map.fpm>          Calculate exact level RAM footprint (Segments, Entities, CSG, Lights)\n"
        "  --check-leaks <map.fpm>             Analyze portals and CSG geometry for leaks and errors\n"
        "  --dump-floor <map.fpm> <floor>      Print dimensions and block stats for a specific floor\n\n"
        "Rendering & Export (Headless):\n"
        "  --export-png <map.fpm> <out.png> [floor] [--color-zones]\n"
        "                                      Render map floor to a high-resolution PNG image\n"
        "  --snapshot-window <map.fpm> <out.png> [entity_idx] [--size W H] [--color-zones] [--floor N]\n"
        "                                      Headless offscreen window snapshot for automated testing\n"
        "  --snapshot-memory <map.fpm> <out.png>\n"
        "                                      Headless offscreen memory analyzer dialog snapshot\n\n"
        "Examples:\n"
        "  AltitudeEditor-cli --version\n"
        "  AltitudeEditor-cli --analyze-memory \"Files/mapbank/my_level.fpm\"\n"
        "  AltitudeEditor-cli --check-leaks \"Files/mapbank/my_level.fpm\"\n"
        "  AltitudeEditor-cli --export-png \"Files/mapbank/my_level.fpm\" \"preview.png\" 0\n"
    );
    fflush(stdout);
}

int main(int argc, char* argv[]) {
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);

    QApplication app(argc, argv);
    app.setApplicationName(VersionInfo::AppName);
    app.setApplicationVersion(VersionInfo::Version);
    app.setOrganizationName(VersionInfo::Studio);

    QIcon appIcon(":/app.ico");
    appIcon.addFile(":/app.png");
    app.setWindowIcon(appIcon);

    // Initialize localization (loads system language on first launch or saved preference)
    LanguageManager::instance().init();

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
        "QTableWidget, QTreeWidget, QListWidget { background: #181b24; alternate-background-color: #1f2330; border: 1px solid #2e3545; gridline-color: #262b37; color: #d0d8e8; }"
        "QHeaderView::section { background: #222634; color: #9bb0d0; border: 1px solid #2e3545; padding: 4px; font-weight: bold; }"
        "QScrollBar:vertical { background: #181b24; width: 10px; margin: 0px; }"
        "QScrollBar::handle:vertical { background: #374151; min-height: 20px; border-radius: 4px; }"
        "QScrollBar::handle:vertical:hover { background: #4b5563; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; }"
        "QScrollBar:horizontal { background: #181b24; height: 10px; margin: 0px; }"
        "QScrollBar::handle:horizontal { background: #374151; min-width: 20px; border-radius: 4px; }"
        "QScrollBar::handle:horizontal:hover { background: #4b5563; }"
        "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { height: 0px; }"
        "QDockWidget { titlebar-close-icon: url(); titlebar-normal-icon: url(); font-weight: bold; }"
        "QDockWidget::title { background: #222634; border-bottom: 1px solid #2e3545; padding: 6px; text-align: left; color: #c4cede; }"
        "QStatusBar { background: #181b24; border-top: 1px solid #262c3a; color: #8898b0; }"
    );

    // Check for CLI options
    QStringList args = app.arguments();

    if (args.contains("--lang")) {
        int lIdx = args.indexOf("--lang");
        if (args.size() > lIdx + 1) {
            QString l = args.value(lIdx + 1).toLower();
            if (l == QStringLiteral("ru") || l == QStringLiteral("russian")) {
                LanguageManager::instance().setLanguage(LanguageManager::Language::Russian);
            } else if (l == QStringLiteral("en") || l == QStringLiteral("english")) {
                LanguageManager::instance().setLanguage(LanguageManager::Language::English);
            }
        }
    }

#ifdef ALTITUDE_CLI_TOOL
    if (args.size() <= 1 || args.contains("--help") || args.contains("-h")) {
        printCliHelp();
        std::exit(0);
    }
#endif

    if (args.contains("--version") || args.contains("-v")) {
        fprintf(stdout, "%s v%s\n", qPrintable(VersionInfo::AppName), qPrintable(VersionInfo::Version));
        fflush(stdout);
        std::exit(0);
    }

    if (args.contains("--help") || args.contains("-h")) {
        printCliHelp();
        std::exit(0);
    }

    if (args.contains("--check-leaks")) {
        int idx = args.indexOf("--check-leaks");
        if (args.size() >= idx + 2) {
            QString mapPath = args.value(idx + 1);
            auto map = FPMReader::loadMap(mapPath, "mypassword");
            if (!map) {
                fprintf(stderr, "Error: Failed to load map: %s\n", qPrintable(mapPath));
                std::exit(1);
            }
            PortalLeakAnalyzer analyzer(map);
            auto warnings = analyzer.analyze();
            fprintf(stdout, "=== PORTAL & CSG LEAK ANALYSIS: %s ===\n", qPrintable(map->mapName));
            fprintf(stdout, "Total issues detected: %zu\n", warnings.size());
            int errCount = 0, warnCount = 0;
            for (const auto& w : warnings) {
                if (w.severity == PortalLeakWarning::ERROR) errCount++;
                else warnCount++;
                fprintf(stdout, "  [%s] %s at Floor %d (Grid: %d, %d): %s\n",
                        w.severity == PortalLeakWarning::ERROR ? "ERROR" : "WARN",
                        qPrintable(w.type), w.layer, w.x, w.y, qPrintable(w.description));
            }
            fprintf(stdout, "Summary: %d Errors, %d Warnings\n", errCount, warnCount);
            fflush(stdout);
            std::exit(errCount > 0 ? 2 : 0);
        } else {
            fprintf(stderr, "Error: Missing map path for --check-leaks. Usage: --check-leaks <map.fpm>\n");
            std::exit(1);
        }
    }

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
            if (args.contains("--show-leaks")) {
                PortalLeakAnalyzer analyzer(map, canvas.visZoneManager());
                canvas.setLeakWarnings(analyzer.analyze());
            }
            if (args.contains("--highlight")) {
                int hIdx = args.indexOf("--highlight");
                if (args.size() > hIdx + 3) {
                    canvas.highlightCell(args.value(hIdx + 1).toInt(), args.value(hIdx + 2).toInt(), args.value(hIdx + 3).toInt());
                }
            } else {
                canvas.zoomFit();
            }

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

            int w = 1400, h = 900;
            if (args.contains("--size")) {
                int sIdx = args.indexOf("--size");
                if (args.size() > sIdx + 2) {
                    w = args.value(sIdx + 1).toInt();
                    h = args.value(sIdx + 2).toInt();
                }
            }
            MainWindow window;
            window.resize(w, h);
            window.loadMapFile(mapPath);
            window.show();
            app.processEvents();


            if (args.contains("--color-zones")) {
                if (auto* dock = window.findChild<VisZoneDock*>()) {
                    dock->show();
                    dock->setColorAllZones(true);
                }
                if (auto* canvas = window.findChild<MapCanvas*>()) {
                    canvas->setColorAllVisZones(true);
                }
            }

            if (args.contains("--zone")) {
                int zIdx = args.indexOf("--zone");
                if (args.size() > zIdx + 1) {
                    int zid = args.value(zIdx + 1).toInt();
                    if (auto* dock = window.findChild<VisZoneDock*>()) {
                        dock->show();
                        dock->onExternalZoneSelected(zid);
                    }
                }
            }

            if (args.contains("--floor")) {
                int fIdx = args.indexOf("--floor");
                if (args.size() > fIdx + 1) {
                    int fl = args.value(fIdx + 1).toInt();
                    if (auto* canvas = window.findChild<MapCanvas*>()) {
                        canvas->setFloor(fl);
                        canvas->zoomFit();
                    }
                }
            } else if (entIdx >= 0) {
                // Select entity to show in inspector
                // Focus on entity
                QMetaObject::invokeMethod(&window, "onFocusEntityRequested", Q_ARG(int, entIdx));
            } else {
                if (auto* canvas = window.findChild<MapCanvas*>()) {
                    canvas->zoomFit();
                }
            }
            if (args.contains("--wait-ms")) {
                int wIdx = args.indexOf("--wait-ms");
                if (args.size() > wIdx + 1) {
                    int ms = args.value(wIdx + 1).toInt();
                    QElapsedTimer timer;
                    timer.start();
                    while (timer.elapsed() < ms) {
                        app.processEvents();
                        QThread::msleep(20);
                    }
                }
            }
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

    if (args.contains("--snapshot-segment")) {
        int idx = args.indexOf("--snapshot-segment");
        if (args.size() >= idx + 3) {
            QString mapPath = args.value(idx + 1);
            QString outPath = args.value(idx + 2);

            auto map = FPMReader::loadMap(mapPath, "mypassword");
            if (map) {
                auto zm = std::make_shared<VisZoneManager>();
                zm->buildFromMap(map);

                SegmentEditorDialog dlg(map, zm);
                dlg.resize(780, 640);
                if (args.size() >= idx + 8) {
                    int fl = args.value(idx + 3).toInt();
                    int x1 = args.value(idx + 4).toInt();
                    int y1 = args.value(idx + 5).toInt();
                    int x2 = args.value(idx + 6).toInt();
                    int y2 = args.value(idx + 7).toInt();
                    dlg.inspectConflict(fl, x1, y1, x2, y2);
                } else if (args.size() >= idx + 6) {
                    int fl = args.value(idx + 3).toInt();
                    int x = args.value(idx + 4).toInt();
                    int y = args.value(idx + 5).toInt();
                    dlg.inspectCell(fl, x, y);
                }
                dlg.show();
                app.processEvents();

                QPixmap pix(dlg.size());
                dlg.render(&pix);
                bool ok = pix.save(outPath);
                fprintf(stdout, "Saved segment snapshot to: %s (Result: %d)\n", qPrintable(outPath), ok ? 1 : 0);
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

#ifdef ALTITUDE_CLI_TOOL
    fprintf(stderr, "Error: Unknown command or missing parameters. Run 'AltitudeEditor-cli --help' for usage.\n");
    std::exit(1);
#endif

    MainWindow window;
    window.show();

    if (args.size() > 1 && !args[1].startsWith('-')) {
        window.loadMapFile(args[1]);
    } else {
        // Automatically open the last launched map from recent files if available
        QSettings settings(QStringLiteral("TGC"), QStringLiteral("FPSCMapViewer"));
        QStringList recent = settings.value(QStringLiteral("recentMaps")).toStringList();
        for (const QString& f : recent) {
            if (!f.isEmpty() && QFileInfo::exists(f)) {
                window.loadMapFile(f);
                break;
            }
        }
    }

    return app.exec();
}
