#include "SegmentEditorDialog.h"
#include "Version.h"
#include "VisZoneManager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QMessageBox>
#include <QFileInfo>
#include <QMouseEvent>
#include <QFont>
#include <QPen>
#include <QBrush>
#include <QAbstractItemView>

// =========================================================================
// TileInteractiveWidget Implementation
// =========================================================================
TileInteractiveWidget::TileInteractiveWidget(QWidget* parent)
    : QWidget(parent)
{
    setMinimumSize(220, 220);
    setMaximumSize(280, 280);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    setMouseTracking(true);
}

void TileInteractiveWidget::setWalls(bool n, bool e, bool s, bool w) {
    m_wallN = n;
    m_wallE = e;
    m_wallS = s;
    m_wallW = w;
    update();
}

void TileInteractiveWidget::setSegmentLabel(const QString& label) {
    m_segmentLabel = label;
    update();
}

int TileInteractiveWidget::hitTestSide(const QPoint& pt) const {
    int w = width(), h = height();
    int margin = 32;

    // Check North (top)
    if (pt.y() < margin && pt.x() >= margin && pt.x() <= w - margin) return 0;
    // Check East (right)
    if (pt.x() > w - margin && pt.y() >= margin && pt.y() <= h - margin) return 1;
    // Check South (bottom)
    if (pt.y() > h - margin && pt.x() >= margin && pt.x() <= w - margin) return 2;
    // Check West (left)
    if (pt.x() < margin && pt.y() >= margin && pt.y() <= h - margin) return 3;

    // Corners check - closest side
    if (pt.y() < margin) return (pt.x() < w / 2) ? (pt.y() < pt.x() ? 0 : 3) : (pt.y() < w - pt.x() ? 0 : 1);
    if (pt.y() > h - margin) return (pt.x() < w / 2) ? (h - pt.y() < pt.x() ? 2 : 3) : (h - pt.y() < w - pt.x() ? 2 : 1);

    return -1;
}

void TileInteractiveWidget::mouseMoveEvent(QMouseEvent* event) {
    int side = hitTestSide(event->pos());
    if (side != m_hoveredSide) {
        m_hoveredSide = side;
        if (m_hoveredSide >= 0) {
            setCursor(Qt::PointingHandCursor);
        } else {
            setCursor(Qt::ArrowCursor);
        }
        update();
    }
}

void TileInteractiveWidget::leaveEvent(QEvent* /*event*/) {
    m_hoveredSide = -1;
    setCursor(Qt::ArrowCursor);
    update();
}

void TileInteractiveWidget::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        int side = hitTestSide(event->pos());
        if (side >= 0) {
            bool newState = false;
            if (side == 0) { m_wallN = !m_wallN; newState = m_wallN; }
            else if (side == 1) { m_wallE = !m_wallE; newState = m_wallE; }
            else if (side == 2) { m_wallS = !m_wallS; newState = m_wallS; }
            else if (side == 3) { m_wallW = !m_wallW; newState = m_wallW; }

            update();
            emit wallToggled(side, newState);
        }
    }
}

void TileInteractiveWidget::paintEvent(QPaintEvent* /*event*/) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    int w = width();
    int h = height();
    int inset = 24;

    // Background tile floor
    QRect floorRect(inset, inset, w - 2 * inset, h - 2 * inset);
    p.fillRect(rect(), QColor(22, 28, 36));
    p.fillRect(floorRect, QColor(35, 45, 56));
    p.setPen(QPen(QColor(55, 70, 85), 1));
    p.drawRect(floorRect);

    // Grid center label
    p.setPen(QColor(160, 185, 210));
    QFont font = p.font();
    font.setPointSize(9);
    font.setBold(true);
    p.setFont(font);
    p.drawText(floorRect, Qt::AlignCenter, m_segmentLabel.isEmpty() ? tr("Floor") : m_segmentLabel);

    // Wall Draw Helper
    auto drawWall = [&](int side, bool hasWall, const QRect& wallBox, const QString& name) {
        bool isHovered = (m_hoveredSide == side);

        if (hasWall) {
            QColor wallColor = isHovered ? QColor(80, 220, 255) : QColor(0, 180, 255);
            p.fillRect(wallBox, wallColor);
            p.setPen(QPen(isHovered ? Qt::white : QColor(0, 130, 200), 1));
            p.drawRect(wallBox);

            p.setPen(QColor(10, 25, 40));
            QFont wf = p.font();
            wf.setPointSize(8);
            wf.setBold(true);
            p.setFont(wf);
            p.drawText(wallBox, Qt::AlignCenter, name);
        } else {
            QColor openColor = isHovered ? QColor(70, 85, 100, 180) : QColor(45, 55, 68, 120);
            p.fillRect(wallBox, openColor);
            p.setPen(QPen(isHovered ? QColor(140, 160, 180) : QColor(75, 90, 105), 1, Qt::DashLine));
            p.drawRect(wallBox);

            p.setPen(isHovered ? QColor(200, 215, 230) : QColor(110, 130, 150));
            QFont wf = p.font();
            wf.setPointSize(7);
            wf.setBold(false);
            p.setFont(wf);
            p.drawText(wallBox, Qt::AlignCenter, tr("Open"));
        }
    };

    // North Wall (top)
    drawWall(0, m_wallN, QRect(inset, 4, w - 2 * inset, inset - 4), tr("North (Z+)"));
    // East Wall (right)
    drawWall(1, m_wallE, QRect(w - inset, inset, inset - 4, h - 2 * inset), tr("East (X+)"));
    // South Wall (bottom)
    drawWall(2, m_wallS, QRect(inset, h - inset, w - 2 * inset, inset - 4), tr("South (Z-)"));
    // West Wall (left)
    drawWall(3, m_wallW, QRect(4, inset, inset - 4, h - 2 * inset), tr("West (X-)"));
}

// =========================================================================
// ConflictDiagramWidget Implementation
// =========================================================================
ConflictDiagramWidget::ConflictDiagramWidget(QWidget* parent)
    : QWidget(parent)
{
    setMinimumSize(540, 210);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
}

void ConflictDiagramWidget::setConflictData(int layer, int x1, int y1, const QString& nameA, const QString& zoneA, bool nA, bool eA, bool sA, bool wA,
                                           int x2, int y2, const QString& nameB, const QString& zoneB, bool nB, bool eB, bool sB, bool wB,
                                           int clashingSideA)
{
    m_layer = layer;
    m_x1 = x1; m_y1 = y1;
    m_x2 = x2; m_y2 = y2;
    m_nameA = nameA; m_zoneA = zoneA;
    m_nameB = nameB; m_zoneB = zoneB;
    m_wallsA[0] = nA; m_wallsA[1] = eA; m_wallsA[2] = sA; m_wallsA[3] = wA;
    m_wallsB[0] = nB; m_wallsB[1] = eB; m_wallsB[2] = sB; m_wallsB[3] = wB;
    m_sideA = clashingSideA;
    update();
}

void ConflictDiagramWidget::paintEvent(QPaintEvent* /*event*/) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    int w = width();
    int h = height();

    p.fillRect(rect(), QColor(20, 26, 32));

    bool isHorizontal = (m_sideA == 1 || m_sideA == 3); // West/East clash
    int tileW = isHorizontal ? (w - 70) / 2 : (w - 80);
    int tileH = isHorizontal ? (h - 50) : (h - 70) / 2;

    QRect rA, rB, clashLine;

    if (isHorizontal) {
        if (m_sideA == 1) { // Cell A on left, Cell B on right
            rA = QRect(20, 30, tileW, tileH);
            rB = QRect(w - 20 - tileW, 30, tileW, tileH);
            clashLine = QRect(rA.right(), 30, rB.left() - rA.right(), tileH);
        } else {
            rB = QRect(20, 30, tileW, tileH);
            rA = QRect(w - 20 - tileW, 30, tileW, tileH);
            clashLine = QRect(rB.right(), 30, rA.left() - rB.right(), tileH);
        }
    } else {
        if (m_sideA == 2) { // Cell A on top, Cell B on bottom
            rA = QRect(40, 25, tileW, tileH);
            rB = QRect(40, h - 25 - tileH, tileW, tileH);
            clashLine = QRect(40, rA.bottom(), tileW, rB.top() - rA.bottom());
        } else {
            rB = QRect(40, 25, tileW, tileH);
            rA = QRect(40, h - 25 - tileH, tileW, tileH);
            clashLine = QRect(40, rB.bottom(), tileW, rA.top() - rB.bottom());
        }
    }

    // Draw Room A
    p.fillRect(rA, QColor(28, 42, 54));
    p.setPen(QPen(QColor(48, 80, 110), 2));
    p.drawRect(rA);

    p.setPen(QColor(100, 200, 255));
    QFont fontTitle = p.font();
    fontTitle.setPointSize(10);
    fontTitle.setBold(true);
    p.setFont(fontTitle);
    p.drawText(QRect(rA.x() + 8, rA.y() + 8, rA.width() - 16, 20), Qt::AlignLeft,
               tr("🏠 Room A: %1 (%2, %3)").arg(m_nameA).arg(m_x1).arg(m_y1));

    QFont fontSub = p.font();
    fontSub.setPointSize(9);
    fontSub.setBold(false);
    p.setFont(fontSub);
    p.setPen(QColor(160, 185, 210));
    p.drawText(QRect(rA.x() + 8, rA.y() + 30, rA.width() - 16, 20), Qt::AlignLeft,
               tr("Zone: %1").arg(m_zoneA.isEmpty() ? tr("None") : m_zoneA));

    // Draw Room B
    p.fillRect(rB, QColor(36, 46, 38));
    p.setPen(QPen(QColor(56, 95, 62), 2));
    p.drawRect(rB);

    p.setPen(QColor(114, 240, 123));
    p.setFont(fontTitle);
    p.drawText(QRect(rB.x() + 8, rB.y() + 8, rB.width() - 16, 20), Qt::AlignLeft,
               tr("🏠 Room B: %1 (%2, %3)").arg(m_nameB).arg(m_x2).arg(m_y2));

    p.setFont(fontSub);
    p.setPen(QColor(180, 215, 185));
    p.drawText(QRect(rB.x() + 8, rB.y() + 30, rB.width() - 16, 20), Qt::AlignLeft,
               tr("Zone: %1").arg(m_zoneB.isEmpty() ? tr("None") : m_zoneB));

    // Clashing Barrier Highlight
    p.fillRect(clashLine, QColor(220, 40, 40, 220));
    p.setPen(QPen(QColor(255, 80, 80), 3));
    p.drawRect(clashLine);

    // Clashing Indicator Label
    QFont fontClash = p.font();
    fontClash.setPointSize(8);
    fontClash.setBold(true);
    p.setFont(fontClash);
    p.setPen(Qt::white);
    p.drawText(clashLine, Qt::AlignCenter, tr("⚠️ CLASH"));
}

// =========================================================================
// SegmentEditorDialog Implementation
// =========================================================================
SegmentEditorDialog::SegmentEditorDialog(std::shared_ptr<FPSCMap> map, std::shared_ptr<VisZoneManager> visZoneMgr, QWidget* parent)
    : QDialog(parent), m_map(map), m_visZoneManager(visZoneMgr)
{
    QString mapName = m_map ? m_map->mapName : tr("No Map");
    setWindowTitle(tr("%1 — Segment Inspector & Conflict Resolver — %2").arg(VersionInfo::shortTitle(), mapName));
    resize(780, 640);

    NoWheelFilter* wheelFilter = new NoWheelFilter(this);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(8);

    m_tabs = new QTabWidget(this);
    m_tabs->setStyleSheet(QStringLiteral("QTabWidget::pane { border: 1px solid #364858; background: #182028; border-radius: 4px; }"
                                         "QTabBar::tab { background: #222d38; color: #a0b8cc; padding: 8px 20px; font-weight: bold; border-top-left-radius: 4px; border-top-right-radius: 4px; min-width: 140px; }"
                                         "QTabBar::tab:selected { background: #2c3e50; color: #ffffff; border-bottom: 2px solid #00b4d8; }"));

    // ==========================================
    // TAB 1: Conflict Resolver
    // ==========================================
    m_tabConflict = new QWidget(this);
    QVBoxLayout* confLayout = new QVBoxLayout(m_tabConflict);
    confLayout->setSpacing(10);

    m_lblConflictTitle = new QLabel(tr("<b>⚠️ Double-Wall Boundary Conflict Detected</b>"), m_tabConflict);
    m_lblConflictTitle->setStyleSheet(QStringLiteral("color: #ff6b6b; font-size: 14px;"));
    confLayout->addWidget(m_lblConflictTitle);

    m_lblConflictExplanation = new QLabel(tr("Two adjacent rooms place solid walls on the exact same shared border without a doorway.\n"
                                             "In the game engine, this creates severe texture flickering (Z-fighting) and PVS portal leaks.\n"
                                             "Choose how you want to resolve this boundary below:"), m_tabConflict);
    m_lblConflictExplanation->setStyleSheet(QStringLiteral("color: #cad8e6; font-size: 12px; line-height: 1.4;"));
    m_lblConflictExplanation->setWordWrap(true);
    confLayout->addWidget(m_lblConflictExplanation);

    m_conflictDiagram = new ConflictDiagramWidget(m_tabConflict);
    confLayout->addWidget(m_conflictDiagram);

    // 2 Action Buttons / Cards
    QVBoxLayout* actionsLayout = new QVBoxLayout();
    actionsLayout->setSpacing(8);

    m_btnRemoveWallA = new QPushButton(m_tabConflict);
    m_btnRemoveWallA->setStyleSheet(QStringLiteral("QPushButton { font-weight: bold; background-color: #243d26; color: #72f07b; border: 1px solid #3b6b3e; border-radius: 6px; padding: 10px 16px; text-align: left; font-size: 12px; }"
                                                   "QPushButton:hover { background-color: #2f5432; color: #94ff9c; }"));
    actionsLayout->addWidget(m_btnRemoveWallA);

    m_btnRemoveWallB = new QPushButton(m_tabConflict);
    m_btnRemoveWallB->setStyleSheet(QStringLiteral("QPushButton { font-weight: bold; background-color: #243d26; color: #72f07b; border: 1px solid #3b6b3e; border-radius: 6px; padding: 10px 16px; text-align: left; font-size: 12px; }"
                                                   "QPushButton:hover { background-color: #2f5432; color: #94ff9c; }"));
    actionsLayout->addWidget(m_btnRemoveWallB);
    confLayout->addLayout(actionsLayout);

    QHBoxLayout* confBottom = new QHBoxLayout();
    m_btnGoManual = new QPushButton(tr("🔧 Switch to Manual Tile Inspector"), m_tabConflict);
    m_btnGoManual->setStyleSheet(QStringLiteral("QPushButton { background-color: #2a3440; color: #cad8e6; border: 1px solid #3f4e5e; border-radius: 4px; padding: 6px 14px; }"
                                                "QPushButton:hover { background-color: #354353; }"));
    confBottom->addWidget(m_btnGoManual);
    confBottom->addStretch();
    confLayout->addLayout(confBottom);

    m_tabs->addTab(m_tabConflict, tr("⚡ Resolve Clash"));

    // ==========================================
    // TAB 2: Manual Tile Editor & Inspector
    // ==========================================
    m_tabManual = new QWidget(this);
    QVBoxLayout* manLayout = new QVBoxLayout(m_tabManual);
    manLayout->setSpacing(8);

    // Coordinate picker row
    QHBoxLayout* coordRow = new QHBoxLayout();
    m_lblFloor = new QLabel(tr("Floor:"), m_tabManual);
    coordRow->addWidget(m_lblFloor);
    m_spnLayer = new QSpinBox(m_tabManual);
    m_spnLayer->setRange(0, m_map ? m_map->header.layerMax : 20);
    m_spnLayer->installEventFilter(wheelFilter);
    coordRow->addWidget(m_spnLayer);

    m_lblX = new QLabel(tr("X:"), m_tabManual);
    coordRow->addWidget(m_lblX);
    m_spnX = new QSpinBox(m_tabManual);
    m_spnX->setRange(0, m_map ? m_map->header.maxX : 40);
    m_spnX->installEventFilter(wheelFilter);
    coordRow->addWidget(m_spnX);

    m_lblY = new QLabel(tr("Y:"), m_tabManual);
    coordRow->addWidget(m_lblY);
    m_spnY = new QSpinBox(m_tabManual);
    m_spnY->setRange(0, m_map ? m_map->header.maxY : 40);
    m_spnY->installEventFilter(wheelFilter);
    coordRow->addWidget(m_spnY);

    m_lblZoneInfo = new QLabel(m_tabManual);
    m_lblZoneInfo->setStyleSheet(QStringLiteral("color: #64b5f6; font-weight: bold; margin-left: 10px; font-size: 11px;"));
    coordRow->addWidget(m_lblZoneInfo);
    coordRow->addStretch();
    manLayout->addLayout(coordRow);

    // Segment selector with search filter
    QHBoxLayout* segRow = new QHBoxLayout();
    m_lblSegmentAsset = new QLabel(tr("Segment Asset:"), m_tabManual);
    segRow->addWidget(m_lblSegmentAsset);

    m_txtSegmentFilter = new QLineEdit(m_tabManual);
    m_txtSegmentFilter->setPlaceholderText(tr("🔍 Filter..."));
    m_txtSegmentFilter->setClearButtonEnabled(true);
    m_txtSegmentFilter->setMaximumWidth(150);
    m_txtSegmentFilter->setStyleSheet(QStringLiteral(
        "QLineEdit { background-color: #1a232c; color: #cad8e6; border: 1px solid #364858; border-radius: 4px; padding: 4px 8px; font-size: 11px; }"
        "QLineEdit:focus { border-color: #00b4d8; }"
    ));
    segRow->addWidget(m_txtSegmentFilter);

    m_cmbSegment = new QComboBox(m_tabManual);
    m_cmbSegment->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_cmbSegment->installEventFilter(wheelFilter);
    m_cmbSegment->setMaxVisibleItems(10);
    m_cmbSegment->view()->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_cmbSegment->setStyleSheet(QStringLiteral(
        "QComboBox { combobox-popup: 0; background-color: #1a232c; color: #cad8e6; border: 1px solid #364858; border-radius: 4px; padding: 5px 8px; }"
        "QComboBox QAbstractItemView { max-height: 240px; background-color: #182028; color: #cad8e6; selection-background-color: #2e537a; outline: none; border: 1px solid #364858; }"
        "QComboBox QAbstractItemView::item { min-height: 22px; padding: 2px 6px; }"
    ));
    segRow->addWidget(m_cmbSegment);
    manLayout->addLayout(segRow);

    m_lblSegmentPath = new QLabel(m_tabManual);
    m_lblSegmentPath->setStyleSheet(QStringLiteral("color: #8c9dae; font-size: 11px; margin-left: 90px;"));
    manLayout->addWidget(m_lblSegmentPath);

    // Center area: Visual Tile on Left, Presets & Checkboxes on Right
    QHBoxLayout* centerLayout = new QHBoxLayout();
    centerLayout->setSpacing(16);

    // Visual interactive tile widget
    QVBoxLayout* tileWidgetBox = new QVBoxLayout();
    m_lblTileHint = new QLabel(tr("<b>Interactive 4-Wall Topology:</b><br/><span style='color:#8c9dae;font-size:10px;'>Click on any wall to toggle it on/off</span>"), m_tabManual);
    m_lblTileHint->setTextFormat(Qt::RichText);
    tileWidgetBox->addWidget(m_lblTileHint);

    m_tileWidget = new TileInteractiveWidget(m_tabManual);
    tileWidgetBox->addWidget(m_tileWidget);
    tileWidgetBox->addStretch();
    centerLayout->addLayout(tileWidgetBox);

    // Right side: Checkboxes and Presets
    QVBoxLayout* rightBox = new QVBoxLayout();
    rightBox->setSpacing(8);

    m_lblCheckboxes = new QLabel(tr("Wall Toggles:"), m_tabManual);
    m_lblCheckboxes->setStyleSheet(QStringLiteral("font-weight: bold;"));
    rightBox->addWidget(m_lblCheckboxes);

    QGridLayout* checkGrid = new QGridLayout();
    m_chkWallN = new QCheckBox(tr("North Wall (Z+)"), m_tabManual);
    m_chkWallE = new QCheckBox(tr("East Wall (X+)"), m_tabManual);
    m_chkWallS = new QCheckBox(tr("South Wall (Z-)"), m_tabManual);
    m_chkWallW = new QCheckBox(tr("West Wall (X-)"), m_tabManual);

    checkGrid->addWidget(m_chkWallN, 0, 0);
    checkGrid->addWidget(m_chkWallE, 0, 1);
    checkGrid->addWidget(m_chkWallS, 1, 0);
    checkGrid->addWidget(m_chkWallW, 1, 1);
    rightBox->addLayout(checkGrid);

    rightBox->addSpacing(6);
    m_lblPresets = new QLabel(tr("Quick Presets:"), m_tabManual);
    rightBox->addWidget(m_lblPresets);
    m_cmbPreset = new QComboBox(m_tabManual);
    m_cmbPreset->installEventFilter(wheelFilter);
    m_cmbPreset->setMaxVisibleItems(10);
    m_cmbPreset->view()->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_cmbPreset->setStyleSheet(QStringLiteral(
        "QComboBox { combobox-popup: 0; background-color: #1a232c; color: #cad8e6; border: 1px solid #364858; border-radius: 4px; padding: 5px 8px; }"
        "QComboBox QAbstractItemView { max-height: 240px; background-color: #182028; color: #cad8e6; selection-background-color: #2e537a; outline: none; border: 1px solid #364858; }"
        "QComboBox QAbstractItemView::item { min-height: 22px; padding: 2px 6px; }"
    ));
    m_cmbPreset->addItem(tr("-- Choose Preset --"));
    m_cmbPreset->addItem(tr("🔲 All 4 Walls (Enclosed Room)"));
    m_cmbPreset->addItem(tr("🔲 3 Walls (U-Shape)"));
    m_cmbPreset->addItem(tr("🔲 Corner (2 Walls)"));
    m_cmbPreset->addItem(tr("🔲 Opposite (2 Walls)"));
    m_cmbPreset->addItem(tr("🔲 1 Wall Divider"));
    m_cmbPreset->addItem(tr("🔲 Open Floor (No Walls)"));
    m_cmbPreset->addItem(tr("🗑️ Clear Tile (Empty Void)"));
    rightBox->addWidget(m_cmbPreset);

    // Collapsible technical details
    m_grpTechnical = new QGroupBox(tr("Advanced Engine Parameters"), m_tabManual);
    QGridLayout* techGrid = new QGridLayout(m_grpTechnical);

    m_lblGround = new QLabel(tr("Ground Mode:"), m_grpTechnical);
    techGrid->addWidget(m_lblGround, 0, 0);
    m_cmbGround = new QComboBox(m_grpTechnical);
    m_cmbGround->installEventFilter(wheelFilter);
    m_cmbGround->setMaxVisibleItems(10);
    m_cmbGround->setStyleSheet(QStringLiteral("QComboBox { combobox-popup: 0; }"));
    m_cmbGround->addItem(tr("0: Room Floor"), 0);
    m_cmbGround->addItem(tr("1: Interior 2"), 1);
    m_cmbGround->addItem(tr("2: Roof/Ceiling Slab"), 2);
    m_cmbGround->addItem(tr("3: Exterior Ground"), 3);
    techGrid->addWidget(m_cmbGround, 0, 1);

    m_lblSymbol = new QLabel(tr("Symbol (Hole):"), m_grpTechnical);
    techGrid->addWidget(m_lblSymbol, 0, 2);
    m_spnSymbol = new QSpinBox(m_grpTechnical);
    m_spnSymbol->setRange(0, 63);
    m_spnSymbol->installEventFilter(wheelFilter);
    techGrid->addWidget(m_spnSymbol, 0, 3);

    m_lblTileType = new QLabel(tr("maptile:"), m_grpTechnical);
    techGrid->addWidget(m_lblTileType, 1, 0);
    m_spnTileType = new QSpinBox(m_grpTechnical);
    m_spnTileType->setRange(0, 15);
    m_spnTileType->installEventFilter(wheelFilter);
    techGrid->addWidget(m_spnTileType, 1, 1);

    m_lblRotation = new QLabel(tr("maprotate:"), m_grpTechnical);
    techGrid->addWidget(m_lblRotation, 1, 2);
    m_cmbRotation = new QComboBox(m_grpTechnical);
    m_cmbRotation->installEventFilter(wheelFilter);
    m_cmbRotation->setMaxVisibleItems(10);
    m_cmbRotation->setStyleSheet(QStringLiteral("QComboBox { combobox-popup: 0; }"));
    m_cmbRotation->addItem(tr("0°"), 0);
    m_cmbRotation->addItem(tr("90°"), 1);
    m_cmbRotation->addItem(tr("180°"), 2);
    m_cmbRotation->addItem(tr("270°"), 3);
    techGrid->addWidget(m_cmbRotation, 1, 3);

    m_lblOrientation = new QLabel(tr("maporient:"), m_grpTechnical);
    techGrid->addWidget(m_lblOrientation, 2, 0);
    m_cmbOrientation = new QComboBox(m_grpTechnical);
    m_cmbOrientation->installEventFilter(wheelFilter);
    m_cmbOrientation->setMaxVisibleItems(10);
    m_cmbOrientation->setStyleSheet(QStringLiteral("QComboBox { combobox-popup: 0; }"));
    m_cmbOrientation->addItem(tr("0°"), 0);
    m_cmbOrientation->addItem(tr("90°"), 1);
    m_cmbOrientation->addItem(tr("180°"), 2);
    m_cmbOrientation->addItem(tr("270°"), 3);
    techGrid->addWidget(m_cmbOrientation, 2, 1);

    rightBox->addWidget(m_grpTechnical);
    rightBox->addStretch();
    centerLayout->addLayout(rightBox);

    manLayout->addLayout(centerLayout);

    // Apply & Live Row
    QHBoxLayout* actionRow = new QHBoxLayout();
    m_chkLiveUpdate = new QCheckBox(tr("Live Auto-Apply"), m_tabManual);
    m_chkLiveUpdate->setChecked(true);
    actionRow->addWidget(m_chkLiveUpdate);
    actionRow->addStretch();

    m_btnApply = new QPushButton(tr("✔ Apply Changes"), m_tabManual);
    m_btnApply->setStyleSheet(QStringLiteral("QPushButton { font-weight: bold; background-color: #233e5c; color: #4dc4ff; border: 1px solid #365e8c; border-radius: 4px; padding: 7px 18px; }"
                                            "QPushButton:hover { background-color: #2e5178; color: #7ad5ff; }"));
    actionRow->addWidget(m_btnApply);
    manLayout->addLayout(actionRow);

    m_tabs->addTab(m_tabManual, tr("🧱 Tile Inspector"));
    mainLayout->addWidget(m_tabs);

    // Bottom close button
    QHBoxLayout* bottomBar = new QHBoxLayout();
    bottomBar->addStretch();
    m_btnClose = new QPushButton(tr("Close"), this);
    connect(m_btnClose, &QPushButton::clicked, this, &QDialog::accept);
    bottomBar->addWidget(m_btnClose);
    mainLayout->addLayout(bottomBar);

    // ==========================================
    // Connections
    // ==========================================
    connect(m_spnLayer, QOverload<int>::of(&QSpinBox::valueChanged), this, &SegmentEditorDialog::onCoordsChanged);
    connect(m_spnX, QOverload<int>::of(&QSpinBox::valueChanged), this, &SegmentEditorDialog::onCoordsChanged);
    connect(m_spnY, QOverload<int>::of(&QSpinBox::valueChanged), this, &SegmentEditorDialog::onCoordsChanged);

    connect(m_chkWallN, &QCheckBox::toggled, this, &SegmentEditorDialog::onWallCheckboxToggled);
    connect(m_chkWallE, &QCheckBox::toggled, this, &SegmentEditorDialog::onWallCheckboxToggled);
    connect(m_chkWallS, &QCheckBox::toggled, this, &SegmentEditorDialog::onWallCheckboxToggled);
    connect(m_chkWallW, &QCheckBox::toggled, this, &SegmentEditorDialog::onWallCheckboxToggled);

    connect(m_tileWidget, &TileInteractiveWidget::wallToggled, this, &SegmentEditorDialog::onTileWidgetWallToggled);
    connect(m_cmbPreset, QOverload<int>::of(&QComboBox::activated), this, &SegmentEditorDialog::onPresetTriggered);

    connect(m_spnTileType, QOverload<int>::of(&QSpinBox::valueChanged), this, &SegmentEditorDialog::onTechnicalParamChanged);
    connect(m_cmbRotation, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SegmentEditorDialog::onTechnicalParamChanged);
    connect(m_cmbOrientation, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SegmentEditorDialog::onTechnicalParamChanged);
    connect(m_cmbGround, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SegmentEditorDialog::onTechnicalParamChanged);
    connect(m_spnSymbol, QOverload<int>::of(&QSpinBox::valueChanged), this, &SegmentEditorDialog::onTechnicalParamChanged);

    connect(m_txtSegmentFilter, &QLineEdit::textChanged, this, &SegmentEditorDialog::onSegmentFilterChanged);
    connect(m_cmbSegment, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SegmentEditorDialog::onSegmentComboChanged);
    connect(m_btnApply, &QPushButton::clicked, this, &SegmentEditorDialog::onApplyCell);

    connect(m_btnRemoveWallA, &QPushButton::clicked, this, &SegmentEditorDialog::onResolveRemoveWallA);
    connect(m_btnRemoveWallB, &QPushButton::clicked, this, &SegmentEditorDialog::onResolveRemoveWallB);
    connect(m_btnGoManual, &QPushButton::clicked, this, &SegmentEditorDialog::onSwitchToManualEditor);

    setMap(m_map);
    retranslateUi();
}

void SegmentEditorDialog::setMap(std::shared_ptr<FPSCMap> map) {
    m_map = map;
    if (!m_map) return;

    m_updatingUi = true;

    m_spnLayer->setRange(0, m_map->header.layerMax);
    m_spnX->setRange(0, m_map->header.maxX);
    m_spnY->setRange(0, m_map->header.maxY);

    populateSegmentCombo(m_txtSegmentFilter ? m_txtSegmentFilter->text() : QString());

    m_updatingUi = false;
    loadCellData();
}

void SegmentEditorDialog::populateSegmentCombo(const QString& filter) {
    if (!m_map) return;

    int curSegId = m_cmbSegment->currentData().toInt();

    m_cmbSegment->blockSignals(true);
    m_cmbSegment->clear();

    QString f = filter.trimmed().toLower();

    // Check if "(0) None / Empty Air" matches
    QString noneLabel = tr("(0) None / Empty Air");
    if (f.isEmpty() || noneLabel.toLower().contains(f) || QStringLiteral("0").contains(f)) {
        m_cmbSegment->addItem(noneLabel, 0);
    }

    for (int i = 0; i < m_map->segmentsBank.size(); ++i) {
        QString path = m_map->segmentsBank[i];
        QString name = QFileInfo(path).baseName();
        if (name.isEmpty()) name = path;
        QString itemText = QString("(%1) %2").arg(i + 1).arg(name);

        if (f.isEmpty() || itemText.toLower().contains(f) || path.toLower().contains(f)) {
            m_cmbSegment->addItem(itemText, i + 1);
        }
    }

    int idx = m_cmbSegment->findData(curSegId);
    if (idx >= 0) {
        m_cmbSegment->setCurrentIndex(idx);
    } else if (m_cmbSegment->count() > 0) {
        m_cmbSegment->setCurrentIndex(0);
    }
    m_cmbSegment->blockSignals(false);
}

void SegmentEditorDialog::onSegmentFilterChanged(const QString& filter) {
    populateSegmentCombo(filter);
}

void SegmentEditorDialog::setVisZoneManager(std::shared_ptr<VisZoneManager> mgr) {
    m_visZoneManager = mgr;
    loadCellData();
}

void SegmentEditorDialog::inspectCell(int layer, int x, int y) {
    m_hasConflict = false;
    m_tabs->setTabVisible(0, false); // Hide conflict tab in single-tile mode
    m_tabs->setCurrentIndex(1);

    m_currentLayer = layer;
    m_currentX = x;
    m_currentY = y;

    m_updatingUi = true;
    m_spnLayer->setValue(layer);
    m_spnX->setValue(x);
    m_spnY->setValue(y);
    m_updatingUi = false;

    loadCellData();
}

void SegmentEditorDialog::inspectConflict(int layer, int x1, int y1, int x2, int y2) {
    m_hasConflict = true;
    m_conflictLayer = layer;
    m_conflictX1 = x1;
    m_conflictY1 = y1;
    m_conflictX2 = x2;
    m_conflictY2 = y2;

    m_tabs->setTabVisible(0, true);
    m_tabs->setCurrentIndex(0);

    updateConflictView();

    m_currentLayer = layer;
    m_currentX = x1;
    m_currentY = y1;
    m_updatingUi = true;
    m_spnLayer->setValue(layer);
    m_spnX->setValue(x1);
    m_spnY->setValue(y1);
    m_updatingUi = false;

    loadCellData();
}

void SegmentEditorDialog::updateConflictView() {
    if (!m_map || !m_hasConflict) return;

    int l = m_conflictLayer;
    int x1 = m_conflictX1, y1 = m_conflictY1;
    int x2 = m_conflictX2, y2 = m_conflictY2;

    int side1 = SegmentMath::getSideFacingNeighbor(x1, y1, x2, y2);
    auto getSideName = [this](int s) -> QString {
        switch (s) {
            case 0: return tr("North");
            case 1: return tr("East");
            case 2: return tr("South");
            case 3: return tr("West");
            default: return QStringLiteral("?");
        }
    };
    QString side1Name = (side1 >= 0) ? getSideName(side1) : QStringLiteral("?");
    int side2 = (side1 >= 0) ? (side1 + 2) % 4 : -1;
    QString side2Name = (side2 >= 0) ? getSideName(side2) : QStringLiteral("?");

    int seg1 = (l < m_map->gridBlocks.size() && y1 < m_map->gridBlocks[l].size() && x1 < m_map->gridBlocks[l][y1].size())
               ? m_map->gridBlocks[l][y1][x1] : 0;
    int seg2 = (l < m_map->gridBlocks.size() && y2 < m_map->gridBlocks[l].size() && x2 < m_map->gridBlocks[l][y2].size())
               ? m_map->gridBlocks[l][y2][x2] : 0;

    auto s1 = m_map->segments.value(seg1);
    auto s2 = m_map->segments.value(seg2);
    QString name1 = s1 ? s1->name : QString::number(seg1);
    QString name2 = s2 ? s2->name : QString::number(seg2);

    QString zone1Name = tr("Unzoned");
    QString zone2Name = tr("Unzoned");
    if (m_visZoneManager) {
        int z1 = m_visZoneManager->getZoneAt(l, x1, y1);
        int z2 = m_visZoneManager->getZoneAt(l, x2, y2);
        if (const VisZone* vz1 = m_visZoneManager->getZone(z1)) zone1Name = vz1->name;
        if (const VisZone* vz2 = m_visZoneManager->getZone(z2)) zone2Name = vz2->name;
    }

    int tile1 = m_map->gridTileType[l][y1][x1];
    int rot1 = m_map->gridRotation[l][y1][x1];
    bool n1, e1, s1b, w1;
    SegmentMath::tileTypeAndRotToWallBools(tile1, rot1, n1, e1, s1b, w1);

    int tile2 = m_map->gridTileType[l][y2][x2];
    int rot2 = m_map->gridRotation[l][y2][x2];
    bool n2, e2, s2b, w2;
    SegmentMath::tileTypeAndRotToWallBools(tile2, rot2, n2, e2, s2b, w2);

    m_conflictDiagram->setConflictData(l, x1, y1, name1, zone1Name, n1, e1, s1b, w1,
                                       x2, y2, name2, zone2Name, n2, e2, s2b, w2,
                                       side1);

    m_btnRemoveWallA->setText(tr("🟢 Keep Room A Wall — Remove %1 Wall from '%2' (%3, %4)\n   (Eliminates clashing wall from Room B, keeping Room A enclosed)")
                                  .arg(side2Name).arg(name2).arg(x2).arg(y2));

    m_btnRemoveWallB->setText(tr("🟢 Keep Room B Wall — Remove %1 Wall from '%2' (%3, %4)\n   (Eliminates clashing wall from Room A, keeping Room B enclosed)")
                                  .arg(side1Name).arg(name1).arg(x1).arg(y1));
}

void SegmentEditorDialog::loadCellData() {
    if (!m_map) return;
    int l = m_currentLayer;
    int x = m_currentX;
    int y = m_currentY;

    if (l < 0 || l >= m_map->gridBlocks.size() ||
        y < 0 || y >= m_map->gridBlocks[l].size() ||
        x < 0 || x >= m_map->gridBlocks[l][y].size()) {
        return;
    }

    m_updatingUi = true;

    // Zone label
    if (m_visZoneManager) {
        int zid = m_visZoneManager->getZoneAt(l, x, y);
        if (const VisZone* vz = m_visZoneManager->getZone(zid)) {
            m_lblZoneInfo->setText(tr("PVS: %1").arg(vz->name));
        } else {
            m_lblZoneInfo->setText(tr("PVS: Void / Exterior"));
        }
    } else {
        m_lblZoneInfo->setText(QString());
    }

    // Segment
    int segId = m_map->gridBlocks[l][y][x];
    int segIdx = m_cmbSegment->findData(segId);
    if (segIdx < 0 && m_txtSegmentFilter && !m_txtSegmentFilter->text().isEmpty()) {
        m_txtSegmentFilter->clear();
        segIdx = m_cmbSegment->findData(segId);
    }
    if (segIdx >= 0) {
        m_cmbSegment->setCurrentIndex(segIdx);
    } else {
        m_cmbSegment->setCurrentIndex(0);
    }

    QString segName = tr("Empty");
    if (segId > 0 && segId <= m_map->segmentsBank.size()) {
        m_lblSegmentPath->setText(m_map->segmentsBank[segId - 1]);
        if (m_map->segments.contains(segId)) segName = m_map->segments[segId]->name;
    } else {
        m_lblSegmentPath->setText(tr("No segment placed"));
    }
    m_tileWidget->setSegmentLabel(segName);

    // Ground & Symbol
    int ground = (l < m_map->gridGround.size() && y < m_map->gridGround[l].size() && x < m_map->gridGround[l][y].size())
                 ? m_map->gridGround[l][y][x] : 0;
    int sym = (l < m_map->gridSymbol.size() && y < m_map->gridSymbol[l].size() && x < m_map->gridSymbol[l][y].size())
              ? m_map->gridSymbol[l][y][x] : 0;

    int gIdx = m_cmbGround->findData(ground);
    m_cmbGround->setCurrentIndex(gIdx >= 0 ? gIdx : 0);
    m_spnSymbol->setValue(sym);

    // Maptile & Rotation
    int tile = (l < m_map->gridTileType.size() && y < m_map->gridTileType[l].size() && x < m_map->gridTileType[l][y].size())
               ? m_map->gridTileType[l][y][x] : 0;
    int rot = (l < m_map->gridRotation.size() && y < m_map->gridRotation[l].size() && x < m_map->gridRotation[l][y].size())
              ? m_map->gridRotation[l][y][x] : 0;
    int orient = (l < m_map->gridOrientation.size() && y < m_map->gridOrientation[l].size() && x < m_map->gridOrientation[l][y].size())
                 ? m_map->gridOrientation[l][y][x] : 0;

    m_spnTileType->setValue(tile);
    m_cmbRotation->setCurrentIndex(rot & 3);
    m_cmbOrientation->setCurrentIndex(orient & 3);

    // 4 Wall Booleans
    bool n, e, s, w;
    SegmentMath::tileTypeAndRotToWallBools(tile, rot, n, e, s, w);
    m_chkWallN->setChecked(n);
    m_chkWallE->setChecked(e);
    m_chkWallS->setChecked(s);
    m_chkWallW->setChecked(w);
    m_tileWidget->setWalls(n, e, s, w);

    m_updatingUi = false;
}

void SegmentEditorDialog::onCoordsChanged() {
    if (m_updatingUi) return;
    m_currentLayer = m_spnLayer->value();
    m_currentX = m_spnX->value();
    m_currentY = m_spnY->value();
    loadCellData();
}

void SegmentEditorDialog::onWallCheckboxToggled() {
    if (m_updatingUi) return;

    bool n = m_chkWallN->isChecked();
    bool e = m_chkWallE->isChecked();
    bool s = m_chkWallS->isChecked();
    bool w = m_chkWallW->isChecked();

    int newTile = 0, newRot = 0;
    SegmentMath::wallBoolsToTileTypeAndRot(n, e, s, w, newTile, newRot);

    m_updatingUi = true;
    m_tileWidget->setWalls(n, e, s, w);
    m_spnTileType->setValue(newTile);
    m_cmbRotation->setCurrentIndex(newRot & 3);
    m_updatingUi = false;

    if (m_chkLiveUpdate->isChecked()) {
        onApplyCell();
    }
}

void SegmentEditorDialog::onTileWidgetWallToggled(int /*side*/, bool /*active*/) {
    if (m_updatingUi) return;

    bool n = m_tileWidget->wallN();
    bool e = m_tileWidget->wallE();
    bool s = m_tileWidget->wallS();
    bool w = m_tileWidget->wallW();

    int newTile = 0, newRot = 0;
    SegmentMath::wallBoolsToTileTypeAndRot(n, e, s, w, newTile, newRot);

    m_updatingUi = true;
    m_chkWallN->setChecked(n);
    m_chkWallE->setChecked(e);
    m_chkWallS->setChecked(s);
    m_chkWallW->setChecked(w);
    m_spnTileType->setValue(newTile);
    m_cmbRotation->setCurrentIndex(newRot & 3);
    m_updatingUi = false;

    if (m_chkLiveUpdate->isChecked()) {
        onApplyCell();
    }
}

void SegmentEditorDialog::onPresetTriggered(int index) {
    if (index <= 0 || m_updatingUi) return;

    switch (index) {
        case 1: // All 4 walls
            m_chkWallN->setChecked(true);
            m_chkWallE->setChecked(true);
            m_chkWallS->setChecked(true);
            m_chkWallW->setChecked(true);
            break;
        case 2: // 3 walls (N, S, W)
            m_chkWallN->setChecked(true);
            m_chkWallE->setChecked(false);
            m_chkWallS->setChecked(true);
            m_chkWallW->setChecked(true);
            break;
        case 3: // Corner (N, W)
            m_chkWallN->setChecked(true);
            m_chkWallE->setChecked(false);
            m_chkWallS->setChecked(false);
            m_chkWallW->setChecked(true);
            break;
        case 4: // Opposite (N, S)
            m_chkWallN->setChecked(true);
            m_chkWallE->setChecked(false);
            m_chkWallS->setChecked(true);
            m_chkWallW->setChecked(false);
            break;
        case 5: // 1 wall (N)
            m_chkWallN->setChecked(true);
            m_chkWallE->setChecked(false);
            m_chkWallS->setChecked(false);
            m_chkWallW->setChecked(false);
            break;
        case 6: // Floor only
            m_chkWallN->setChecked(false);
            m_chkWallE->setChecked(false);
            m_chkWallS->setChecked(false);
            m_chkWallW->setChecked(false);
            break;
        case 7: // Empty void
            m_cmbSegment->setCurrentIndex(0);
            m_chkWallN->setChecked(false);
            m_chkWallE->setChecked(false);
            m_chkWallS->setChecked(false);
            m_chkWallW->setChecked(false);
            break;
    }

    m_cmbPreset->setCurrentIndex(0);
    if (m_chkLiveUpdate->isChecked()) {
        onApplyCell();
    }
}

void SegmentEditorDialog::onTechnicalParamChanged() {
    if (m_updatingUi) return;

    int tile = m_spnTileType->value();
    int rot = m_cmbRotation->currentIndex();

    bool n, e, s, w;
    SegmentMath::tileTypeAndRotToWallBools(tile, rot, n, e, s, w);

    m_updatingUi = true;
    m_chkWallN->setChecked(n);
    m_chkWallE->setChecked(e);
    m_chkWallS->setChecked(s);
    m_chkWallW->setChecked(w);
    m_tileWidget->setWalls(n, e, s, w);
    m_updatingUi = false;

    if (m_chkLiveUpdate->isChecked()) {
        onApplyCell();
    }
}

void SegmentEditorDialog::onSegmentComboChanged(int index) {
    if (m_updatingUi) return;
    int segId = m_cmbSegment->itemData(index).toInt();
    QString segName = tr("Empty");
    if (segId > 0 && segId <= m_map->segmentsBank.size()) {
        m_lblSegmentPath->setText(m_map->segmentsBank[segId - 1]);
        if (m_map->segments.contains(segId)) segName = m_map->segments[segId]->name;
    } else {
        m_lblSegmentPath->setText(tr("No segment placed"));
    }
    m_tileWidget->setSegmentLabel(segName);

    if (m_chkLiveUpdate->isChecked()) {
        onApplyCell();
    }
}

void SegmentEditorDialog::onApplyCell() {
    if (!m_map) return;
    int l = m_currentLayer;
    int x = m_currentX;
    int y = m_currentY;

    if (l < 0 || l >= m_map->gridBlocks.size() ||
        y < 0 || y >= m_map->gridBlocks[l].size() ||
        x < 0 || x >= m_map->gridBlocks[l][y].size()) {
        return;
    }

    int segId = m_cmbSegment->currentData().toInt();
    int tile = m_spnTileType->value();
    int rot = m_cmbRotation->currentIndex() & 3;
    int orient = m_cmbOrientation->currentIndex() & 3;
    int ground = m_cmbGround->currentData().toInt();
    int sym = m_spnSymbol->value();

    m_map->gridBlocks[l][y][x] = segId;
    m_map->gridTileType[l][y][x] = tile;
    m_map->gridRotation[l][y][x] = rot;
    m_map->gridOrientation[l][y][x] = orient;
    m_map->gridGround[l][y][x] = ground;
    m_map->gridSymbol[l][y][x] = sym;

    m_map->isModified = true;

    if (m_visZoneManager) {
        m_visZoneManager->buildFromMap(m_map);
    }

    emit segmentModified(l, x, y);
}

void SegmentEditorDialog::onResolveRemoveWallA() {
    if (!m_map || !m_hasConflict) return;
    int l = m_conflictLayer;
    int x1 = m_conflictX1, y1 = m_conflictY1;
    int x2 = m_conflictX2, y2 = m_conflictY2;

    int side1 = SegmentMath::getSideFacingNeighbor(x1, y1, x2, y2);
    int side2 = (side1 >= 0) ? (side1 + 2) % 4 : -1;
    if (side2 < 0) return;

    // Remove side2 from Room B
    int tile2 = m_map->gridTileType[l][y2][x2];
    int rot2 = m_map->gridRotation[l][y2][x2];
    bool n, e, s, w;
    SegmentMath::tileTypeAndRotToWallBools(tile2, rot2, n, e, s, w);

    if (side2 == 0) n = false;
    else if (side2 == 1) e = false;
    else if (side2 == 2) s = false;
    else if (side2 == 3) w = false;

    int newTile, newRot;
    SegmentMath::wallBoolsToTileTypeAndRot(n, e, s, w, newTile, newRot);
    m_map->gridTileType[l][y2][x2] = newTile;
    m_map->gridRotation[l][y2][x2] = newRot;
    m_map->isModified = true;

    if (m_visZoneManager) {
        m_visZoneManager->buildFromMap(m_map);
    }

    m_hasConflict = false;
    emit segmentModified(l, x2, y2);
    accept();
}

void SegmentEditorDialog::onResolveRemoveWallB() {
    if (!m_map || !m_hasConflict) return;
    int l = m_conflictLayer;
    int x1 = m_conflictX1, y1 = m_conflictY1;
    int x2 = m_conflictX2, y2 = m_conflictY2;

    int side1 = SegmentMath::getSideFacingNeighbor(x1, y1, x2, y2);
    if (side1 < 0) return;

    // Remove side1 from Room A
    int tile1 = m_map->gridTileType[l][y1][x1];
    int rot1 = m_map->gridRotation[l][y1][x1];
    bool n, e, s, w;
    SegmentMath::tileTypeAndRotToWallBools(tile1, rot1, n, e, s, w);

    if (side1 == 0) n = false;
    else if (side1 == 1) e = false;
    else if (side1 == 2) s = false;
    else if (side1 == 3) w = false;

    int newTile, newRot;
    SegmentMath::wallBoolsToTileTypeAndRot(n, e, s, w, newTile, newRot);
    m_map->gridTileType[l][y1][x1] = newTile;
    m_map->gridRotation[l][y1][x1] = newRot;
    m_map->isModified = true;

    if (m_visZoneManager) {
        m_visZoneManager->buildFromMap(m_map);
    }

    m_hasConflict = false;
    emit segmentModified(l, x1, y1);
    accept();
}

void SegmentEditorDialog::onSwitchToManualEditor() {
    m_tabs->setCurrentIndex(1);
}

void SegmentEditorDialog::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
    }
    QDialog::changeEvent(event);
}

void SegmentEditorDialog::retranslateUi() {
    QString mapName = m_map ? m_map->mapName : tr("No Map");
    setWindowTitle(tr("%1 — Segment Inspector & Conflict Resolver — %2").arg(VersionInfo::shortTitle(), mapName));

    if (m_tabs) {
        m_tabs->setTabText(0, tr("⚡ Resolve Clash"));
        m_tabs->setTabText(1, tr("🧱 Tile Inspector"));
    }

    if (m_lblConflictTitle) {
        m_lblConflictTitle->setText(tr("<b>⚠️ Double-Wall Boundary Conflict Detected</b>"));
    }
    if (m_lblConflictExplanation) {
        m_lblConflictExplanation->setText(tr("Two adjacent rooms place solid walls on the exact same shared border without a doorway.\n"
                                             "In the game engine, this creates severe texture flickering (Z-fighting) and PVS portal leaks.\n"
                                             "Choose how you want to resolve this boundary below:"));
    }
    if (m_btnGoManual) {
        m_btnGoManual->setText(tr("🔧 Switch to Manual Tile Inspector"));
    }

    if (m_lblFloor) m_lblFloor->setText(tr("Floor:"));
    if (m_lblX) m_lblX->setText(tr("X:"));
    if (m_lblY) m_lblY->setText(tr("Y:"));
    if (m_lblSegmentAsset) m_lblSegmentAsset->setText(tr("Segment Asset:"));
    if (m_txtSegmentFilter) m_txtSegmentFilter->setPlaceholderText(tr("🔍 Filter..."));
    populateSegmentCombo(m_txtSegmentFilter ? m_txtSegmentFilter->text() : QString());

    if (m_lblTileHint) {
        m_lblTileHint->setText(tr("<b>Interactive 4-Wall Topology:</b><br/><span style='color:#8c9dae;font-size:10px;'>Click on any wall to toggle it on/off</span>"));
    }
    if (m_lblCheckboxes) m_lblCheckboxes->setText(tr("Wall Toggles:"));
    if (m_chkWallN) m_chkWallN->setText(tr("North Wall (Z+)"));
    if (m_chkWallE) m_chkWallE->setText(tr("East Wall (X+)"));
    if (m_chkWallS) m_chkWallS->setText(tr("South Wall (Z-)"));
    if (m_chkWallW) m_chkWallW->setText(tr("West Wall (X-)"));

    if (m_lblPresets) m_lblPresets->setText(tr("Quick Presets:"));
    if (m_cmbPreset) {
        int curIdx = m_cmbPreset->currentIndex();
        m_cmbPreset->blockSignals(true);
        m_cmbPreset->clear();
        m_cmbPreset->addItem(tr("-- Choose Preset --"));
        m_cmbPreset->addItem(tr("🔲 All 4 Walls (Enclosed Room)"));
        m_cmbPreset->addItem(tr("🔲 3 Walls (U-Shape)"));
        m_cmbPreset->addItem(tr("🔲 Corner (2 Walls)"));
        m_cmbPreset->addItem(tr("🔲 Opposite (2 Walls)"));
        m_cmbPreset->addItem(tr("🔲 1 Wall Divider"));
        m_cmbPreset->addItem(tr("🔲 Open Floor (No Walls)"));
        m_cmbPreset->addItem(tr("🗑️ Clear Tile (Empty Void)"));
        m_cmbPreset->setCurrentIndex(curIdx >= 0 && curIdx < m_cmbPreset->count() ? curIdx : 0);
        m_cmbPreset->blockSignals(false);
    }

    if (m_grpTechnical) m_grpTechnical->setTitle(tr("Advanced Engine Parameters"));
    if (m_lblGround) m_lblGround->setText(tr("Ground Mode:"));
    if (m_cmbGround) {
        int curG = m_cmbGround->currentData().toInt();
        m_cmbGround->blockSignals(true);
        m_cmbGround->clear();
        m_cmbGround->addItem(tr("0: Room Floor"), 0);
        m_cmbGround->addItem(tr("1: Interior 2"), 1);
        m_cmbGround->addItem(tr("2: Roof/Ceiling Slab"), 2);
        m_cmbGround->addItem(tr("3: Exterior Ground"), 3);
        int idx = m_cmbGround->findData(curG);
        m_cmbGround->setCurrentIndex(idx >= 0 ? idx : 0);
        m_cmbGround->blockSignals(false);
    }

    if (m_lblSymbol) m_lblSymbol->setText(tr("Symbol (Hole):"));
    if (m_lblTileType) m_lblTileType->setText(tr("maptile:"));
    if (m_lblRotation) m_lblRotation->setText(tr("maprotate:"));
    if (m_lblOrientation) m_lblOrientation->setText(tr("maporient:"));

    if (m_chkLiveUpdate) m_chkLiveUpdate->setText(tr("Live Auto-Apply"));
    if (m_btnApply) m_btnApply->setText(tr("✔ Apply Changes"));
    if (m_btnClose) m_btnClose->setText(tr("Close"));

    loadCellData();
    if (m_hasConflict) {
        updateConflictView();
    }
}
