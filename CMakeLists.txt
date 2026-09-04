cmake_minimum_required(VERSION 3.16)
project(FPSCMapViewer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt5 REQUIRED COMPONENTS Core Gui Widgets)

set(SOURCES
    src/main.cpp
    src/MainWindow.cpp
    src/MapCanvas.cpp
    src/FPMReader.cpp
    src/FPMWriter.cpp
    src/SegmentParser.cpp
    src/EntityParser.cpp
    src/AssetManager.cpp
    src/MemoryAnalyzer.cpp
    src/MemoryAnalyzerDialog.cpp
    src/EntitySearchDock.cpp
    src/EntityInspector.cpp
    src/PortalLeakAnalyzer.cpp
    src/PortalLeakDialog.cpp
    src/UniverseDBUParser.cpp
    src/VisZoneManager.cpp
    src/VisZoneDock.cpp
    src/LanguageManager.cpp
    src/resources.qrc
)

set(MANIFEST_OBJ "${CMAKE_CURRENT_SOURCE_DIR}/src/app_res.o")

set(HEADERS
    src/Version.h
    src/MainWindow.h
    src/MapCanvas.h
    src/FPMReader.h
    src/FPMWriter.h
    src/FPSCData.h
    src/SegmentParser.h
    src/EntityParser.h
    src/AssetManager.h
    src/MemoryAnalyzer.h
    src/MemoryAnalyzerDialog.h
    src/EntitySearchDock.h
    src/EntityInspector.h
    src/PortalLeakAnalyzer.h
    src/PortalLeakDialog.h
    src/UniverseDBUParser.h
    src/VisZoneManager.h
    src/VisZoneDock.h
    src/LanguageManager.h
    src/miniz_deflate.h
)

add_executable(FPSCMapViewer WIN32 ${SOURCES} ${HEADERS} ${MANIFEST_OBJ})
target_link_libraries(FPSCMapViewer PRIVATE Qt5::Core Qt5::Gui Qt5::Widgets)

add_executable(FPSCMapViewerTool ${SOURCES} ${HEADERS} ${MANIFEST_OBJ})
target_link_libraries(FPSCMapViewerTool PRIVATE Qt5::Core Qt5::Gui Qt5::Widgets)

add_executable(test_standalone
    src/test_standalone.cpp
    src/MapCanvas.cpp
    src/FPMReader.cpp
    src/FPMWriter.cpp
    src/SegmentParser.cpp
    src/EntityParser.cpp
    src/AssetManager.cpp
    src/MemoryAnalyzer.cpp
    src/PortalLeakAnalyzer.cpp
    src/PortalLeakDialog.cpp
    src/UniverseDBUParser.cpp
    src/VisZoneManager.cpp
)
target_link_libraries(test_standalone PRIVATE Qt5::Core Qt5::Gui Qt5::Widgets)





