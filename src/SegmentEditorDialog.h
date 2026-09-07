#pragma once

#include <QDialog>
#include <QSpinBox>
#include <QCheckBox>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QGroupBox>
#include <QTabWidget>
#include <QEvent>
#include <QWheelEvent>
#include <QPainter>
#include <memory>
#include "FPSCData.h"

class VisZoneManager;

// Event filter to completely block mouse wheel scrolling on spinboxes and combos
class NoWheelFilter : public QObject {
public:
    explicit NoWheelFilter(QObject* parent = nullptr) : QObject(parent) {}
protected:
    bool eventFilter(QObject* obj, QEvent* event) override {
        if (event->type() == QEvent::Wheel) {
            event->ignore();
            return true;
        }
        return QObject::eventFilter(obj, event);
    }
};

namespace SegmentMath {
    inline void tileTypeAndRotToWallBools(int tile, int rot, bool& n, bool& e, bool& s, bool& w) {
        static const bool baseWalls[16][4] = {
            {0, 0, 0, 0}, {1, 1, 1, 1}, {1, 0, 1, 1}, {1, 0, 0, 1},
            {1, 0, 1, 0}, {1, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0},
            {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0},
            {1, 0, 0, 0}, {1, 0, 0, 0}, {1, 0, 0, 0}, {1, 0, 0, 1}
        };
        auto isWall = [&](int side) -> bool {
            if (tile <= 0 || tile == 6) return false;
            int unrotated = (side - (rot & 3) + 4) % 4;
            if (tile >= 0 && tile < 16) return baseWalls[tile][unrotated];
            return false;
        };
        n = isWall(0);
        e = isWall(1);
        s = isWall(2);
        w = isWall(3);
    }

    inline void wallBoolsToTileTypeAndRot(bool n, bool e, bool s, bool w, int& outTile, int& outRot) {
        int mask = (n ? 1 : 0) | (e ? 2 : 0) | (s ? 4 : 0) | (w ? 8 : 0);
        switch (mask) {
            case 0:  outTile = 6; outRot = 0; break;
            case 1:  outTile = 5; outRot = 0; break; // N
            case 2:  outTile = 5; outRot = 1; break; // E
            case 4:  outTile = 5; outRot = 2; break; // S
            case 8:  outTile = 5; outRot = 3; break; // W
            case 9:  outTile = 3; outRot = 0; break; // N + W
            case 3:  outTile = 3; outRot = 1; break; // N + E
            case 6:  outTile = 3; outRot = 2; break; // E + S
            case 12: outTile = 3; outRot = 3; break; // S + W
            case 5:  outTile = 4; outRot = 0; break; // N + S
            case 10: outTile = 4; outRot = 1; break; // E + W
            case 13: outTile = 2; outRot = 0; break; // N + S + W (no E)
            case 11: outTile = 2; outRot = 1; break; // N + E + W (no S)
            case 7:  outTile = 2; outRot = 2; break; // N + E + S (no W)
            case 14: outTile = 2; outRot = 3; break; // E + S + W (no N)
            case 15: outTile = 1; outRot = 0; break; // All 4
            default: outTile = 6; outRot = 0; break;
        }
    }

    inline int getSideFacingNeighbor(int x1, int y1, int x2, int y2) {
        if (y2 == y1 - 1 && x2 == x1) return 0; // North
        if (x2 == x1 + 1 && y2 == y1) return 1; // East
        if (y2 == y1 + 1 && x2 == x1) return 2; // South
        if (x2 == x1 - 1 && y2 == y1) return 3; // West
        return -1;
    }
}

// -------------------------------------------------------------
// Interactive 2D Tile Widget (Click walls to toggle)
// -------------------------------------------------------------
class TileInteractiveWidget : public QWidget {
    Q_OBJECT
public:
    explicit TileInteractiveWidget(QWidget* parent = nullptr);

    void setWalls(bool n, bool e, bool s, bool w);
    void setSegmentLabel(const QString& label);

    bool wallN() const { return m_wallN; }
    bool wallE() const { return m_wallE; }
    bool wallS() const { return m_wallS; }
    bool wallW() const { return m_wallW; }

signals:
    void wallToggled(int side, bool active); // 0=N, 1=E, 2=S, 3=W

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    int hitTestSide(const QPoint& pt) const;

    bool m_wallN = false;
    bool m_wallE = false;
    bool m_wallS = false;
    bool m_wallW = false;
    QString m_segmentLabel;
    int m_hoveredSide = -1;
};

// -------------------------------------------------------------
// Visual 2D Conflict Diagram Widget (Draws Room A & Room B colliding)
// -------------------------------------------------------------
class ConflictDiagramWidget : public QWidget {
    Q_OBJECT
public:
    explicit ConflictDiagramWidget(QWidget* parent = nullptr);

    void setConflictData(int layer, int x1, int y1, const QString& nameA, const QString& zoneA, bool nA, bool eA, bool sA, bool wA,
                         int x2, int y2, const QString& nameB, const QString& zoneB, bool nB, bool eB, bool sB, bool wB,
                         int clashingSideA);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    int m_layer = 0;
    int m_x1 = 0, m_y1 = 0;
    int m_x2 = 0, m_y2 = 0;
    QString m_nameA, m_zoneA;
    QString m_nameB, m_zoneB;
    bool m_wallsA[4] = {false};
    bool m_wallsB[4] = {false};
    int m_sideA = -1;
};

// -------------------------------------------------------------
// SegmentEditorDialog Main Dialog
// -------------------------------------------------------------
class SegmentEditorDialog : public QDialog {
    Q_OBJECT
public:
    explicit SegmentEditorDialog(std::shared_ptr<FPSCMap> map, std::shared_ptr<VisZoneManager> visZoneMgr = nullptr, QWidget* parent = nullptr);

    void setMap(std::shared_ptr<FPSCMap> map);
    void setVisZoneManager(std::shared_ptr<VisZoneManager> mgr);

    void inspectCell(int layer, int x, int y);
    void inspectConflict(int layer, int x1, int y1, int x2, int y2);

signals:
    void segmentModified(int layer, int x, int y);

protected:
    void changeEvent(QEvent* event) override;

private slots:
    void onCoordsChanged();
    void onWallCheckboxToggled();
    void onTileWidgetWallToggled(int side, bool active);
    void onPresetTriggered(int index);
    void onTechnicalParamChanged();
    void onApplyCell();
    void onSegmentComboChanged(int index);

    // Conflict action slots
    void onResolveRemoveWallA();
    void onResolveRemoveWallB();
    void onResolveOpenBoth();
    void onSwitchToManualEditor();

private:
    void retranslateUi();
    void loadCellData();
    void syncUiFromWallBools();
    void updateConflictView();

    std::shared_ptr<FPSCMap> m_map;
    std::shared_ptr<VisZoneManager> m_visZoneManager;

    int m_currentLayer = 0;
    int m_currentX = 0;
    int m_currentY = 0;

    // Conflict state
    bool m_hasConflict = false;
    int m_conflictLayer = 0;
    int m_conflictX1 = -1, m_conflictY1 = -1;
    int m_conflictX2 = -1, m_conflictY2 = -1;

    // Tab Widget
    QTabWidget* m_tabs = nullptr;
    QWidget* m_tabConflict = nullptr;
    QWidget* m_tabManual = nullptr;

    // Conflict Tab UI
    ConflictDiagramWidget* m_conflictDiagram = nullptr;
    QLabel* m_lblConflictTitle = nullptr;
    QLabel* m_lblConflictExplanation = nullptr;
    QPushButton* m_btnRemoveWallA = nullptr;
    QPushButton* m_btnRemoveWallB = nullptr;
    QPushButton* m_btnOpenBoth = nullptr;
    QPushButton* m_btnGoManual = nullptr;

    // Manual Tab UI
    QSpinBox* m_spnLayer = nullptr;
    QSpinBox* m_spnX = nullptr;
    QSpinBox* m_spnY = nullptr;
    QLabel* m_lblZoneInfo = nullptr;

    TileInteractiveWidget* m_tileWidget = nullptr;
    QCheckBox* m_chkWallN = nullptr;
    QCheckBox* m_chkWallE = nullptr;
    QCheckBox* m_chkWallS = nullptr;
    QCheckBox* m_chkWallW = nullptr;

    QComboBox* m_cmbPreset = nullptr;
    QComboBox* m_cmbSegment = nullptr;
    QLabel* m_lblSegmentPath = nullptr;

    // Technical collapsible group
    QGroupBox* m_grpTechnical = nullptr;
    QComboBox* m_cmbGround = nullptr;
    QSpinBox* m_spnSymbol = nullptr;
    QSpinBox* m_spnTileType = nullptr;
    QComboBox* m_cmbRotation = nullptr;
    QComboBox* m_cmbOrientation = nullptr;

    QCheckBox* m_chkLiveUpdate = nullptr;
    QPushButton* m_btnApply = nullptr;

    bool m_updatingUi = false;
};
