#include "VisZoneDock.h"
#include "LanguageManager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QKeyEvent>
#include <QApplication>
#include <QScrollBar>
#include <QLayout>
#include <QStyle>
#include <QTimer>
#include <set>
#include <algorithm>
#include <functional>

// ============================================================================
// FlowLayout: Multi-row wrapping layout for chips and tags
// ============================================================================
class FlowLayout : public QLayout {
public:
    explicit FlowLayout(QWidget* parent = nullptr, int margin = 0, int hSpacing = -1, int vSpacing = -1)
        : QLayout(parent), m_hSpace(hSpacing), m_vSpace(vSpacing)
    {
        if (margin >= 0) setContentsMargins(margin, margin, margin, margin);
    }
    explicit FlowLayout(int margin, int hSpacing = -1, int vSpacing = -1)
        : m_hSpace(hSpacing), m_vSpace(vSpacing)
    {
        if (margin >= 0) setContentsMargins(margin, margin, margin, margin);
    }
    ~FlowLayout() override {
        QLayoutItem* item;
        while ((item = takeAt(0)))
            delete item;
    }

    void addItem(QLayoutItem* item) override {
        m_itemList.append(item);
    }

    int horizontalSpacing() const {
        if (m_hSpace >= 0) return m_hSpace;
        return smartSpacing(QStyle::PM_LayoutHorizontalSpacing);
    }

    int verticalSpacing() const {
        if (m_vSpace >= 0) return m_vSpace;
        return smartSpacing(QStyle::PM_LayoutVerticalSpacing);
    }

    int count() const override {
        return m_itemList.size();
    }

    QLayoutItem* itemAt(int index) const override {
        return m_itemList.value(index);
    }

    QLayoutItem* takeAt(int index) override {
        if (index >= 0 && index < m_itemList.size())
            return m_itemList.takeAt(index);
        return nullptr;
    }

    Qt::Orientations expandingDirections() const override {
        return { };
    }

    bool hasHeightForWidth() const override {
        return true;
    }

    int heightForWidth(int width) const override {
        if (width <= 0) width = 280;
        return doLayout(QRect(0, 0, width, 0), true);
    }

    void setGeometry(const QRect& rect) override {
        QLayout::setGeometry(rect);
        doLayout(rect, false);
    }

    QSize sizeHint() const override {
        return minimumSize();
    }

    QSize minimumSize() const override {
        QSize size;
        for (const QLayoutItem* item : m_itemList)
            size = size.expandedTo(item->minimumSize());
        const QMargins margins = contentsMargins();
        size += QSize(margins.left() + margins.right(), margins.top() + margins.bottom());
        return size;
    }

private:
    int doLayout(const QRect& rect, bool testOnly) const {
        int left, top, right, bottom;
        getContentsMargins(&left, &top, &right, &bottom);
        QRect effectiveRect = rect.adjusted(+left, +top, -right, -bottom);
        if (effectiveRect.width() <= 0) {
            effectiveRect.setWidth(qMax(200, rect.width()));
        }
        int x = effectiveRect.x();
        int y = effectiveRect.y();
        int lineHeight = 0;

        for (QLayoutItem* item : m_itemList) {
            int spaceX = horizontalSpacing();
            if (spaceX == -1)
                spaceX = 4;
            int spaceY = verticalSpacing();
            if (spaceY == -1)
                spaceY = 4;

            int itemW = item->sizeHint().width();
            int itemH = item->sizeHint().height();
            int nextX = x + itemW + spaceX;
            if (nextX - spaceX > effectiveRect.right() && lineHeight > 0) {
                x = effectiveRect.x();
                y = y + lineHeight + spaceY;
                nextX = x + itemW + spaceX;
                lineHeight = 0;
            }

            if (!testOnly)
                item->setGeometry(QRect(QPoint(x, y), QSize(itemW, itemH)));

            x = nextX;
            lineHeight = qMax(lineHeight, itemH);
        }
        return y + lineHeight - rect.y() + bottom;
    }

    int smartSpacing(QStyle::PixelMetric pm) const {
        QObject* parentObj = this->parent();
        if (!parentObj) {
            return -1;
        } else if (parentObj->isWidgetType()) {
            QWidget* pw = static_cast<QWidget*>(parentObj);
            return pw->style()->pixelMetric(pm, nullptr, pw);
        } else {
            return static_cast<QLayout*>(parentObj)->spacing();
        }
    }

    QList<QLayoutItem*> m_itemList;
    int m_hSpace = 4;
    int m_vSpace = 4;
};

// ============================================================================
// ZoneChipButton: Interactive zone chip button with distinct highlight state
// ============================================================================
class ZoneChipButton : public QPushButton {
public:
    ZoneChipButton(int zoneId, bool isBreach, QWidget* parent = nullptr)
        : QPushButton(parent), m_zoneId(zoneId), m_isBreach(isBreach)
    {
        setCursor(Qt::PointingHandCursor);
        setFixedHeight(20);
        updateChipStyle(false);
        bool isRu = (LanguageManager::instance().effectiveLanguage() == LanguageManager::Language::Russian);
        setToolTip(isRu
            ? QString::fromUtf8("ЛКМ: подсветить на карте | Двойной клик: перейти в Зону %1").arg(zoneId + 1)
            : QString("LMB: highlight on map | Double click: go to Zone %1").arg(zoneId + 1));
    }

    int zoneId() const { return m_zoneId; }

    void setHighlighted(bool hl) {
        if (m_isHighlighted != hl) {
            m_isHighlighted = hl;
            updateChipStyle(hl);
        }
    }

    bool isHighlighted() const { return m_isHighlighted; }

    void setCallbacks(std::function<void(int)> onSingleClick, std::function<void(int)> onDoubleClick) {
        m_onSingleClick = onSingleClick;
        m_onDoubleClick = onDoubleClick;
    }

protected:
    void mouseDoubleClickEvent(QMouseEvent* event) override {
        if (event->button() == Qt::LeftButton && m_onDoubleClick) {
            event->accept();
            int zid = m_zoneId;
            auto cb = m_onDoubleClick;
            QTimer::singleShot(0, [cb, zid]() {
                cb(zid);
            });
            return;
        }
        QPushButton::mouseDoubleClickEvent(event);
    }

private:
    void updateChipStyle(bool hl) {
        bool isRu = (LanguageManager::instance().effectiveLanguage() == LanguageManager::Language::Russian);
        QString zName = isRu ? QString::fromUtf8("Зона %1").arg(m_zoneId + 1) : QString("Zone %1").arg(m_zoneId + 1);
        if (hl) {
            setText(QStringLiteral("● ") + zName);
            if (m_isBreach) {
                setStyleSheet(QStringLiteral(
                    "QPushButton { background: #b91c1c; color: #ffffff; border: 2px solid #fecaca; border-radius: 3px; padding: 0px 5px; font-size: 10px; font-weight: bold; }"
                    "QPushButton:hover { background: #dc2626; border-color: #ffffff; }"
                ));
            } else {
                setStyleSheet(QStringLiteral(
                    "QPushButton { background: #0284c7; color: #ffffff; border: 2px solid #bae6fd; border-radius: 3px; padding: 0px 5px; font-size: 10px; font-weight: bold; }"
                    "QPushButton:hover { background: #0369a1; border-color: #ffffff; }"
                ));
            }
        } else {
            setText(zName);
            if (m_isBreach) {
                setStyleSheet(QStringLiteral(
                    "QPushButton { background: #7f1d1d; color: #fee2e2; border: 1px solid #ef4444; border-radius: 3px; padding: 1px 6px; font-size: 10px; font-weight: bold; }"
                    "QPushButton:hover { background: #991b1b; border-color: #f87171; color: #ffffff; }"
                    "QPushButton:pressed { background: #450a0a; }"
                ));
            } else {
                setStyleSheet(QStringLiteral(
                    "QPushButton { background: #075985; color: #e0f2fe; border: 1px solid #38bdf8; border-radius: 3px; padding: 1px 6px; font-size: 10px; font-weight: bold; }"
                    "QPushButton:hover { background: #0284c7; border-color: #7dd3fc; color: #ffffff; }"
                    "QPushButton:pressed { background: #0c4a6e; }"
                ));
            }
        }
    }

    int m_zoneId = -1;
    bool m_isBreach = false;
    bool m_isHighlighted = false;
    std::function<void(int)> m_onSingleClick;
    std::function<void(int)> m_onDoubleClick;
};

// ============================================================================
// PortalItemWidget: Custom row widget with wrapped clickable visible zones
// ============================================================================
class PortalItemWidget : public QFrame {
public:
    PortalItemWidget(int itemIndex, const HighlightedPortalInfo& p, QWidget* parent = nullptr)
        : QFrame(parent), m_itemIndex(itemIndex), m_isBreach(p.isBreach)
    {
        setObjectName("PortalItemWidget");
        setCursor(Qt::PointingHandCursor);
        setFocusPolicy(Qt::NoFocus);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

        QVBoxLayout* vLayout = new QVBoxLayout(this);
        vLayout->setContentsMargins(6, 5, 6, 5);
        vLayout->setSpacing(4);

        bool isRu = (LanguageManager::instance().effectiveLanguage() == LanguageManager::Language::Russian);

        // --- Row 1: Portal Title & Coordinates ---
        QString title;
        if (p.isBreach) {
            title = p.isHorizontal
                ? (isRu ? QString::fromUtf8("🚨 Пробоина перекрытия (Эт.%1: %2, %3) ➔ Зона %4").arg(p.layer).arg(p.x1).arg(p.y1).arg(p.toZone + 1)
                        : QString("🚨 Floor Breach (Floor %1: %2, %3) ➔ Zone %4").arg(p.layer).arg(p.x1).arg(p.y1).arg(p.toZone + 1))
                : (isRu ? QString::fromUtf8("🚨 Пробоина в стене (Эт.%1: %2, %3) ➔ Зона %4").arg(p.layer).arg(p.x1).arg(p.y1).arg(p.toZone + 1)
                        : QString("🚨 Wall Breach (Floor %1: %2, %3) ➔ Zone %4").arg(p.layer).arg(p.x1).arg(p.y1).arg(p.toZone + 1));
        } else if (p.isCrack) {
            title = isRu
                ? QString::fromUtf8("⚠️ Микрощель (Эт.%1: %2, %3) ➔ Зона %4").arg(p.layer).arg(p.x1).arg(p.y1).arg(p.toZone + 1)
                : QString("⚠️ Micro-crack (Floor %1: %2, %3) ➔ Zone %4").arg(p.layer).arg(p.x1).arg(p.y1).arg(p.toZone + 1);
        } else if (p.isExterior) {
            title = isRu
                ? QString::fromUtf8("🚪 Выход наружу (Эт.%1: %2, %3) ➔ Улица").arg(p.layer).arg(p.x1).arg(p.y1)
                : QString("🚪 Exit to Outdoors (Floor %1: %2, %3) ➔ Outdoors").arg(p.layer).arg(p.x1).arg(p.y1);
        } else if (p.isWindow) {
            title = p.isHorizontal
                ? (isRu ? QString::fromUtf8("🪟 Потолочное окно (Эт.%1: %2, %3) ➔ Зона %4").arg(p.layer).arg(p.x1).arg(p.y1).arg(p.toZone + 1)
                        : QString("🪟 Ceiling Window (Floor %1: %2, %3) ➔ Zone %4").arg(p.layer).arg(p.x1).arg(p.y1).arg(p.toZone + 1))
                : (isRu ? QString::fromUtf8("🪟 Окно (Эт.%1: %2, %3) ➔ Зона %4").arg(p.layer).arg(p.x1).arg(p.y1).arg(p.toZone + 1)
                        : QString("🪟 Window (Floor %1: %2, %3) ➔ Zone %4").arg(p.layer).arg(p.x1).arg(p.y1).arg(p.toZone + 1));
        } else if (p.isHorizontal) {
            title = isRu
                ? QString::fromUtf8("⬆ Проем перекрытия (Эт.%1: %2, %3) ➔ Зона %4").arg(p.layer).arg(p.x1).arg(p.y1).arg(p.toZone + 1)
                : QString("⬆ Floor Opening (Floor %1: %2, %3) ➔ Zone %4").arg(p.layer).arg(p.x1).arg(p.y1).arg(p.toZone + 1);
        } else {
            title = isRu
                ? QString::fromUtf8("🚪 Дверной проем (Эт.%1: %2, %3) ➔ Зона %4").arg(p.layer).arg(p.x1).arg(p.y1).arg(p.toZone + 1)
                : QString("🚪 Doorway (Floor %1: %2, %3) ➔ Zone %4").arg(p.layer).arg(p.x1).arg(p.y1).arg(p.toZone + 1);
        }

        m_lblTitle = new QLabel(title, this);
        m_lblTitle->setWordWrap(true);
        QFont f = m_lblTitle->font();
        f.setPointSize(9);
        f.setBold(true);
        m_lblTitle->setFont(f);
        m_lblTitle->setStyleSheet(p.isBreach ? QStringLiteral("color: #f87171;") :
                                  p.isCrack  ? QStringLiteral("color: #fde047;") :
                                  p.isExterior ? QStringLiteral("color: #86efac;") :
                                  p.isWindow ? QStringLiteral("color: #7dd3fc;") :
                                               QStringLiteral("color: #f1f5f9;"));
        vLayout->addWidget(m_lblTitle);

        // --- Row 2: Flow Layout for Visible Zones ---
        m_flowContainer = new QWidget(this);
        m_flowContainer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        m_flowLayout = new FlowLayout(m_flowContainer, 0, 4, 4);

        QLabel* lblPrefix = new QLabel(m_flowContainer);
        lblPrefix->setFixedHeight(20);
        lblPrefix->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
        if (p.isBreach) {
            lblPrefix->setText(isRu ? QString::fromUtf8("↳ ⚠️ УТЕЧКА:") : QStringLiteral("↳ ⚠️ LEAK:"));
            lblPrefix->setStyleSheet(QStringLiteral("color: #fca5a5; font-size: 10px; font-weight: bold;"));
        } else if (p.isExterior) {
            lblPrefix->setText(isRu ? QString::fromUtf8("↳ 🌲 Улица") : QStringLiteral("↳ 🌲 Outdoors"));
            lblPrefix->setStyleSheet(QStringLiteral("color: #86efac; font-size: 10px;"));
        } else {
            lblPrefix->setText(isRu ? QString::fromUtf8("↳ Видно:") : QStringLiteral("↳ Visible:"));
            lblPrefix->setStyleSheet(QStringLiteral("color: #94a3b8; font-size: 10px;"));
        }
        m_flowLayout->addWidget(lblPrefix);

        if (!p.isExterior) {
            if (p.visibleZoneIds.empty()) {
                QLabel* lblNone = new QLabel(isRu ? QString::fromUtf8("нет") : QStringLiteral("none"), m_flowContainer);
                lblNone->setFixedHeight(20);
                lblNone->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
                lblNone->setStyleSheet(QStringLiteral("color: #64748b; font-size: 10px; font-style: italic;"));
                m_flowLayout->addWidget(lblNone);
            } else {
                for (int vzId : p.visibleZoneIds) {
                    ZoneChipButton* chip = new ZoneChipButton(vzId, p.isBreach, m_flowContainer);
                    chip->setCallbacks(
                        [this, vzId](int) {
                            if (m_onHighlightZone) m_onHighlightZone(m_itemIndex, vzId);
                        },
                        [this](int targetZ) {
                            if (m_onJumpZone) m_onJumpZone(targetZ);
                        }
                    );
                    connect(chip, &QPushButton::clicked, this, [this, vzId]() {
                        if (m_onHighlightZone) m_onHighlightZone(m_itemIndex, vzId);
                    });
                    m_flowLayout->addWidget(chip);
                    m_chips.push_back(chip);
                }
            }
        }

        vLayout->addWidget(m_flowContainer);
        updateStyle();
    }

    bool hasHeightForWidth() const override { return true; }

    int heightForWidth(int width) const override {
        int innerW = width - 12;
        if (innerW < 60) innerW = 260;
        int hTitle = m_lblTitle ? m_lblTitle->heightForWidth(innerW) : 18;
        if (hTitle <= 0) {
            hTitle = m_lblTitle ? m_lblTitle->sizeHint().height() : 18;
        }
        int hFlow = m_flowLayout ? m_flowLayout->heightForWidth(innerW) : 24;
        if (hFlow <= 0) hFlow = 24;
        return 5 + hTitle + 4 + hFlow + 5 + 4;
    }

    QSize sizeHint() const override {
        int w = width();
        if (w <= 60 && parentWidget()) w = parentWidget()->width();
        if (w <= 60) w = 300;
        return QSize(w, heightForWidth(w));
    }

    void setCallbacks(std::function<void(int)> onSelect,
                      std::function<void(int, int)> onHighlightZone,
                      std::function<void(int)> onJumpZone) {
        m_onSelect = onSelect;
        m_onHighlightZone = onHighlightZone;
        m_onJumpZone = onJumpZone;
    }

    void setSelected(bool sel) {
        if (m_isSelected != sel) {
            m_isSelected = sel;
            updateStyle();
        }
    }

    void setFocusedChip(int vzId) {
        for (auto* chip : m_chips) {
            chip->setHighlighted(chip->zoneId() == vzId);
        }
    }

protected:
    void mousePressEvent(QMouseEvent* event) override {
        QFrame::mousePressEvent(event);
        if (m_onSelect) {
            m_onSelect(m_itemIndex);
        }
    }

private:
    void updateStyle() {
        if (m_isSelected) {
            if (m_isBreach) {
                setStyleSheet(QStringLiteral(
                    "QFrame#PortalItemWidget { background-color: #450a0a; border: 2px solid #ef4444; border-radius: 5px; }"
                ));
            } else {
                setStyleSheet(QStringLiteral(
                    "QFrame#PortalItemWidget { background-color: #0c4a6e; border: 2px solid #38bdf8; border-radius: 5px; }"
                ));
            }
        } else {
            if (m_isBreach) {
                setStyleSheet(QStringLiteral(
                    "QFrame#PortalItemWidget { background-color: #2b1111; border: 1px solid #7f1d1d; border-radius: 5px; }"
                    "QFrame#PortalItemWidget:hover { background-color: #3b1515; border-color: #ef4444; }"
                ));
            } else {
                setStyleSheet(QStringLiteral(
                    "QFrame#PortalItemWidget { background-color: #1e293b; border: 1px solid #334155; border-radius: 5px; }"
                    "QFrame#PortalItemWidget:hover { background-color: #243247; border-color: #60a5fa; }"
                ));
            }
        }
    }

    int m_itemIndex = -1;
    bool m_isBreach = false;
    bool m_isSelected = false;
    QLabel* m_lblTitle = nullptr;
    QWidget* m_flowContainer = nullptr;
    FlowLayout* m_flowLayout = nullptr;
    std::vector<ZoneChipButton*> m_chips;
    std::function<void(int)> m_onSelect;
    std::function<void(int, int)> m_onHighlightZone;
    std::function<void(int)> m_onJumpZone;
};

// ============================================================================
// VisZoneDock
// ============================================================================
VisZoneDock::VisZoneDock(QWidget* parent)
    : QDockWidget(parent), m_analyzer(nullptr, nullptr)
{
    setObjectName("VisZoneDock");
    setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    setupUi();
}

void VisZoneDock::setupUi() {
    setWindowTitle(tr("Visibility Zones & Portals (PVS)"));
    setMinimumWidth(330);

    setStyleSheet(QStringLiteral(
        "QDockWidget#VisZoneDock { color: #e2e8f0; font-family: 'Segoe UI', sans-serif; }"
        "QDockWidget#VisZoneDock::title { background: #1e293b; color: #93c5fd; font-weight: bold; border-bottom: 1px solid #334155; padding: 6px; text-align: left; }"
        "QWidget#VisZoneCentral { background-color: #0f172a; }"
        "QGroupBox { font-weight: bold; font-size: 11px; color: #93c5fd; border: 1px solid #334155; border-radius: 4px; margin-top: 6px; padding: 4px; background-color: #1e293b; }"
        "QGroupBox::title { subcontrol-origin: margin; subcontrol-position: top left; left: 8px; padding: 0 4px; color: #93c5fd; background-color: #1e293b; }"
        "QComboBox, QSpinBox, QLineEdit { background: #0f172a; border: 1px solid #475569; border-radius: 4px; padding: 2px 6px; height: 24px; color: #f1f5f9; font-size: 11px; }"
        "QComboBox:hover { border-color: #3b82f6; }"
        "QComboBox QAbstractItemView { background: #0f172a; color: #f1f5f9; selection-background-color: #2563eb; border: 1px solid #475569; }"
        "QPushButton { background: #1e293b; border: 1px solid #475569; border-radius: 4px; padding: 2px 8px; height: 24px; color: #f1f5f9; font-weight: bold; font-size: 11px; }"
        "QPushButton:hover { background: #334155; border-color: #60a5fa; color: #ffffff; }"
        "QPushButton:pressed { background: #0f172a; }"
        "QPushButton:disabled { background: #0f172a; color: #475569; border-color: #1e293b; }"
        "QListWidget { background-color: #0b1120; border: 1px solid #334155; border-radius: 4px; color: #f1f5f9; font-size: 11px; outline: none; }"
        "QListWidget::item { padding: 1px; border-radius: 4px; margin-bottom: 3px; border: none; }"
        "QListWidget::item:selected { background: transparent; }"
        "QScrollBar:vertical { background: #0b1120; width: 8px; margin: 0; }"
        "QScrollBar::handle:vertical { background: #334155; min-height: 20px; border-radius: 4px; }"
        "QScrollBar::handle:vertical:hover { background: #475569; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"
        "QScrollBar:horizontal { height: 0px; width: 0px; }"
    ));

    QWidget* central = new QWidget(this);
    central->setObjectName("VisZoneCentral");
    QVBoxLayout* mainLayout = new QVBoxLayout(central);
    mainLayout->setContentsMargins(6, 6, 6, 6);
    mainLayout->setSpacing(6);

    // ==========================================
    // TOP HEADER: Shared Zone Navigation Bar
    // ==========================================
    QHBoxLayout* navRow = new QHBoxLayout();
    navRow->setSpacing(4);

    m_btnPrev = new QPushButton(QStringLiteral("◀"), central);
    m_btnPrev->setObjectName("btnPrevZone");
    m_btnPrev->setToolTip(tr("Предыдущая зона"));
    m_btnPrev->setFixedWidth(28);
    navRow->addWidget(m_btnPrev);

    m_zoneCombo = new QComboBox(central);
    m_zoneCombo->setObjectName("comboZones");
    m_zoneCombo->setMaxVisibleItems(14);
    m_zoneCombo->view()->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_zoneCombo->setSizeAdjustPolicy(QComboBox::AdjustToContentsOnFirstShow);
    navRow->addWidget(m_zoneCombo, 1);

    m_btnNext = new QPushButton(QStringLiteral("▶"), central);
    m_btnNext->setObjectName("btnNextZone");
    m_btnNext->setToolTip(tr("Следующая зона"));
    m_btnNext->setFixedWidth(28);
    navRow->addWidget(m_btnNext);

    m_btnFocus = new QPushButton(QStringLiteral("🔍"), central);
    m_btnFocus->setObjectName("btnFocusZone");
    m_btnFocus->setToolTip(tr("Центрировать камеру на зоне"));
    m_btnFocus->setFixedWidth(28);
    navRow->addWidget(m_btnFocus);

    m_btnResetZone = new QPushButton(QStringLiteral("✕"), central);
    m_btnResetZone->setObjectName("btnResetZone");
    m_btnResetZone->setToolTip(tr("Сбросить выбор зоны / Показать все"));
    m_btnResetZone->setFixedWidth(28);
    m_btnResetZone->setStyleSheet(QStringLiteral("QPushButton { color: #f87171; font-weight: bold; } QPushButton:hover { background: #451a1a; border-color: #ef4444; color: #ffffff; }"));
    navRow->addWidget(m_btnResetZone);

    mainLayout->addLayout(navRow);

    // Second Row: Floor Filter, Refresh, Show All
    QHBoxLayout* subRow = new QHBoxLayout();
    subRow->setSpacing(6);

    m_chkCurrentFloorOnly = new QCheckBox(tr("Только текущий этаж"), central);
    m_chkCurrentFloorOnly->setChecked(false);
    subRow->addWidget(m_chkCurrentFloorOnly, 1);

    m_btnRefresh = new QPushButton(QStringLiteral("🔄"), central);
    m_btnRefresh->setToolTip(tr("Пересчитать зоны и PVS-граф"));
    m_btnRefresh->setFixedWidth(28);
    subRow->addWidget(m_btnRefresh);

    m_btnReset = new QPushButton(tr("✕ Сброс зоны"), central);
    m_btnReset->setToolTip(tr("Сбросить выбор зоны / Показать все зоны"));
    m_btnReset->setFixedHeight(24);
    subRow->addWidget(m_btnReset);

    mainLayout->addLayout(subRow);

    // ==========================================
    // PORTALS & VISIBILITY (Direct placement, no tabs)
    // ==========================================
    // Status / KPI banner
    m_lblKpi = new QLabel(central);
    m_lblKpi->setWordWrap(true);
    m_lblKpi->setStyleSheet(QStringLiteral("background: #1e293b; color: #94a3b8; border: 1px solid #334155; border-radius: 4px; padding: 5px 8px; font-weight: bold; font-size: 11px;"));
    mainLayout->addWidget(m_lblKpi);

    // List of Portals and their Visibility
    m_listPortals = new QListWidget(central);
    m_listPortals->setObjectName("listPortals");
    m_listPortals->setSelectionMode(QAbstractItemView::SingleSelection);
    m_listPortals->setWordWrap(true);
    m_listPortals->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_listPortals->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    mainLayout->addWidget(m_listPortals, 1);

    // Action buttons below portal list
    QHBoxLayout* portalActionRow = new QHBoxLayout();
    portalActionRow->setSpacing(4);

    m_btnShowOnMap = new QPushButton(tr("🔍 Показать на карте"), central);
    m_btnShowOnMap->setEnabled(false);
    m_btnShowOnMap->setStyleSheet(QStringLiteral("QPushButton { background-color: #1e3a8a; color: #bfdbfe; border-color: #3b82f6; } QPushButton:hover { background-color: #2563eb; color: #ffffff; }"));
    portalActionRow->addWidget(m_btnShowOnMap);

    m_btnEditWall = new QPushButton(tr("🧱 Редактировать стену..."), central);
    m_btnEditWall->setEnabled(false);
    m_btnEditWall->setStyleSheet(QStringLiteral("QPushButton { background-color: #78350f; color: #fde68a; border-color: #f59e0b; } QPushButton:hover { background-color: #92400e; color: #ffffff; }"));
    portalActionRow->addWidget(m_btnEditWall);

    mainLayout->addLayout(portalActionRow);

    // Visibility Culling Options
    m_grpIsolation = new QGroupBox(tr("Изоляция и цвета"), central);
    QVBoxLayout* isoLayout = new QVBoxLayout(m_grpIsolation);
    isoLayout->setContentsMargins(6, 4, 6, 4);
    isoLayout->setSpacing(3);

    QHBoxLayout* dimLayout = new QHBoxLayout();
    dimLayout->setSpacing(6);
    m_chkIsolate = new QCheckBox(tr("Затемнять невидимые"), m_grpIsolation);
    m_chkIsolate->setChecked(true);
    dimLayout->addWidget(m_chkIsolate);

    m_lblDim = new QLabel(tr("Яркость: 50%"), m_grpIsolation);
    dimLayout->addWidget(m_lblDim);

    m_sliderDim = new QSlider(Qt::Horizontal, m_grpIsolation);
    m_sliderDim->setRange(0, 100);
    m_sliderDim->setValue(50);
    m_sliderDim->setToolTip(tr("Яркость неактивных зон (0% = скрыть, 50% = полупрозрачно, 100% = полная яркость)"));
    dimLayout->addWidget(m_sliderDim, 1);
    isoLayout->addLayout(dimLayout);

    QHBoxLayout* colorRow = new QHBoxLayout();
    colorRow->setSpacing(6);
    m_chkColorAll = new QCheckBox(tr("🎨 Раскрасить все"), m_grpIsolation);
    m_btnRecolor = new QPushButton(tr("🎲 Палитра"), m_grpIsolation);
    m_btnRecolor->setFixedWidth(80);
    colorRow->addWidget(m_chkColorAll, 1);
    colorRow->addWidget(m_btnRecolor, 0);
    isoLayout->addLayout(colorRow);

    mainLayout->addWidget(m_grpIsolation);
    setWidget(central);

    // ==========================================
    // SIGNAL CONNECTIONS
    // ==========================================
    connect(m_btnPrev, &QPushButton::clicked, this, &VisZoneDock::onPrevZone);
    connect(m_btnNext, &QPushButton::clicked, this, &VisZoneDock::onNextZone);
    connect(m_btnFocus, &QPushButton::clicked, this, &VisZoneDock::onFocusZoneClicked);
    connect(m_btnResetZone, &QPushButton::clicked, this, &VisZoneDock::resetToNormalView);
    connect(m_btnRefresh, &QPushButton::clicked, this, &VisZoneDock::refreshGraph);
    connect(m_btnReset, &QPushButton::clicked, this, &VisZoneDock::resetToNormalView);
    connect(m_chkCurrentFloorOnly, &QCheckBox::toggled, this, &VisZoneDock::onFloorFilterToggled);

    connect(m_zoneCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &VisZoneDock::onZoneComboChanged);

    // Portals Connections
    connect(m_listPortals, &QListWidget::itemClicked, this, &VisZoneDock::onPortalClicked);
    connect(m_listPortals, &QListWidget::itemDoubleClicked, this, &VisZoneDock::onPortalDoubleClicked);
    connect(m_btnShowOnMap, &QPushButton::clicked, this, &VisZoneDock::onShowOnMapClicked);
    connect(m_btnEditWall, &QPushButton::clicked, this, &VisZoneDock::onEditWallClicked);

    connect(m_chkIsolate, &QCheckBox::toggled, this, &VisZoneDock::onIsolationOptionChanged);
    connect(m_sliderDim, &QSlider::valueChanged, this, &VisZoneDock::onIsolationOptionChanged);
    connect(m_chkColorAll, &QCheckBox::toggled, this, [this](bool checked) {
        emit colorAllZonesToggled(checked);
    });
    connect(m_btnRecolor, &QPushButton::clicked, this, [this]() {
        if (m_mgr) {
            static int s_hueShift = 0;
            s_hueShift = (s_hueShift + 67) % 360;
            m_mgr->recolorAllZones(s_hueShift);
            if (!m_chkColorAll->isChecked()) {
                m_chkColorAll->setChecked(true);
            } else {
                emit colorAllZonesToggled(true);
            }
        }
    });

    retranslateUi();
}

void VisZoneDock::setVisZoneManager(std::shared_ptr<VisZoneManager> mgr) {
    m_mgr = mgr;
    m_analyzer = PortalLeakAnalyzer(m_map, m_mgr);
    populateZoneCombo();
    refreshGraph();
}

void VisZoneDock::setMap(std::shared_ptr<FPSCMap> map) {
    m_map = map;
    m_analyzer = PortalLeakAnalyzer(m_map, m_mgr);
    populateZoneCombo();
    refreshGraph();
}

void VisZoneDock::onFloorChanged(int floor) {
    m_currentFloor = floor;
    if (m_chkCurrentFloorOnly->isChecked()) {
        populateZoneCombo();
    }
}

void VisZoneDock::onFloorFilterToggled(bool /*checked*/) {
    populateZoneCombo();
}

void VisZoneDock::populateZoneCombo() {
    m_updatingCombo = true;
    m_zoneCombo->blockSignals(true);
    m_zoneCombo->clear();

    bool isRu = (LanguageManager::instance().effectiveLanguage() == LanguageManager::Language::Russian);
    m_zoneCombo->addItem(isRu ? QString::fromUtf8("Все зоны (Обычный вид)") : QStringLiteral("All Zones (Normal View)"), -1);

    if (!m_mgr) {
        m_zoneCombo->blockSignals(false);
        m_updatingCombo = false;
        return;
    }

    bool floorOnly = m_chkCurrentFloorOnly->isChecked();
    const auto& zones = m_mgr->zones();

    int selectedIdx = 0;
    for (size_t i = 0; i < zones.size(); ++i) {
        const auto& z = zones[i];
        if (floorOnly && !z.hasFloor(m_currentFloor)) continue;

        QString floorStr = (z.minFloor == z.maxFloor)
                           ? (isRu ? QString::fromUtf8("Этаж %1").arg(z.floor) : QString("Floor %1").arg(z.floor))
                           : (isRu ? QString::fromUtf8("Этажи %1..%2").arg(z.minFloor).arg(z.maxFloor) : QString("Floors %1..%2").arg(z.minFloor).arg(z.maxFloor));
        QString label = isRu
            ? QString::fromUtf8("Зона %1 (%2: %3 ячеек)").arg(z.id + 1).arg(floorStr).arg(z.tiles.size())
            : QString("Zone %1 (%2: %3 tiles)").arg(z.id + 1).arg(floorStr).arg(z.tiles.size());

        m_zoneCombo->addItem(label, z.id);
        if (z.id == m_activeZoneId) {
            selectedIdx = m_zoneCombo->count() - 1;
        }
    }

    m_zoneCombo->setCurrentIndex(selectedIdx);
    m_zoneCombo->blockSignals(false);
    m_updatingCombo = false;
}

void VisZoneDock::onZoneComboChanged(int index) {
    if (m_updatingCombo || index < 0) return;

    int zoneId = m_zoneCombo->itemData(index).toInt();
    m_activeZoneId = zoneId;

    emit portalHighlightCleared();

    if (zoneId >= 0 && m_chkIsolate && !m_chkIsolate->isChecked()) {
        m_chkIsolate->blockSignals(true);
        m_chkIsolate->setChecked(true);
        m_chkIsolate->blockSignals(false);
    }

    updateActiveZoneDetails();

    emit zoneSelected(zoneId);
    onIsolationOptionChanged();
}

void VisZoneDock::onPrevZone() {
    int cur = m_zoneCombo->currentIndex();
    if (cur > 1) {
        m_zoneCombo->setCurrentIndex(cur - 1);
    } else if (m_zoneCombo->count() > 1) {
        m_zoneCombo->setCurrentIndex(m_zoneCombo->count() - 1);
    }
}

void VisZoneDock::onNextZone() {
    int cur = m_zoneCombo->currentIndex();
    if (cur < m_zoneCombo->count() - 1) {
        m_zoneCombo->setCurrentIndex(cur + 1);
    } else if (m_zoneCombo->count() > 1) {
        m_zoneCombo->setCurrentIndex(1); // Skip index 0 (All zones)
    }
}

void VisZoneDock::onFocusZoneClicked() {
    if (!m_mgr || m_activeZoneId < 0) return;
    const VisZone* z = m_mgr->getZone(m_activeZoneId);
    if (z && !z->tiles.empty()) {
        emit cellSelected(z->minFloor, z->tiles.front().x(), z->tiles.front().y());
    }
}

void VisZoneDock::onExternalZoneSelected(int zoneId) {
    if (zoneId >= 0) {
        show();
        raise();
    }
    m_activeZoneId = zoneId;
    emit portalHighlightCleared();

    if (zoneId >= 0 && m_chkIsolate && !m_chkIsolate->isChecked()) {
        m_chkIsolate->blockSignals(true);
        m_chkIsolate->setChecked(true);
        m_chkIsolate->blockSignals(false);
    } else if (zoneId < 0 && m_chkIsolate && m_chkIsolate->isChecked()) {
        m_chkIsolate->blockSignals(true);
        m_chkIsolate->setChecked(false);
        m_chkIsolate->blockSignals(false);
    }

    for (int i = 0; i < m_zoneCombo->count(); ++i) {
        if (m_zoneCombo->itemData(i).toInt() == zoneId) {
            m_updatingCombo = true;
            m_zoneCombo->setCurrentIndex(i);
            m_updatingCombo = false;
            updateActiveZoneDetails();
            emit zoneSelected(zoneId);
            onIsolationOptionChanged();
            return;
        }
    }

    if (m_chkCurrentFloorOnly->isChecked()) {
        m_chkCurrentFloorOnly->setChecked(false);
        for (int i = 0; i < m_zoneCombo->count(); ++i) {
            if (m_zoneCombo->itemData(i).toInt() == zoneId) {
                m_updatingCombo = true;
                m_zoneCombo->setCurrentIndex(i);
                m_updatingCombo = false;
                updateActiveZoneDetails();
                emit zoneSelected(zoneId);
                onIsolationOptionChanged();
                return;
            }
        }
    }
}

void VisZoneDock::onIsolationOptionChanged() {
    bool isolate = m_chkIsolate ? m_chkIsolate->isChecked() : true;
    if (m_lblDim) m_lblDim->setEnabled(isolate);
    if (m_sliderDim) m_sliderDim->setEnabled(isolate);

    bool isRu = (LanguageManager::instance().effectiveLanguage() == LanguageManager::Language::Russian);
    int val = m_sliderDim ? m_sliderDim->value() : 50;
    if (m_lblDim) m_lblDim->setText(isRu ? QString::fromUtf8("Яркость: %1%").arg(val) : QString("Brightness: %1%").arg(val));

    float dimOpacity = isolate ? (val / 100.0f) : 1.0f;
    emit isolationChanged(isolate, dimOpacity);
}

void VisZoneDock::refreshGraph() {
    if (!m_map || !m_mgr) return;
    if (m_refreshingGraph) return;

    m_refreshingGraph = true;
    QApplication::setOverrideCursor(Qt::WaitCursor);
    m_pvsGraph = m_analyzer.buildPvsGraph();
    QApplication::restoreOverrideCursor();
    m_refreshingGraph = false;

    updateActiveZoneDetails();
}

void VisZoneDock::updateActiveZoneDetails() {
    m_currentPortals.clear();
    m_listPortals->clear();
    m_btnShowOnMap->setEnabled(false);
    m_btnEditWall->setEnabled(false);
    emit portalHighlightCleared();
    emit tracePathSelected({});

    bool isRu = (LanguageManager::instance().effectiveLanguage() == LanguageManager::Language::Russian);
    if (m_activeZoneId < 0 || !m_mgr) {
        m_lblKpi->setText(isRu
            ? QString::fromUtf8("Режим показа всех зон. Выберите зону выше для анализа порталов и PVS-видимости.")
            : QStringLiteral("Showing all zones. Select a zone above to inspect portals and PVS visibility."));
        m_lblKpi->setStyleSheet(QStringLiteral("background: #1e293b; color: #94a3b8; border: 1px solid #334155; border-radius: 4px; padding: 5px 8px; font-weight: bold; font-size: 11px;"));
        return;
    }

    const VisZone* zone = m_mgr->getZone(m_activeZoneId);
    if (!zone) {
        m_lblKpi->setText(isRu
            ? QString::fromUtf8("Выбранная зона не найдена.")
            : QStringLiteral("Selected zone not found."));
        return;
    }

    // 1. Collect portals from PVS Graph if available
    if (m_pvsGraph.contains(m_activeZoneId)) {
        const auto& pvsInfo = m_pvsGraph[m_activeZoneId];
        for (const auto& conn : pvsInfo.directConnections) {
            HighlightedPortalInfo hInfo;
            hInfo.layer = conn.layer;
            hInfo.x1 = conn.x1;
            hInfo.y1 = conn.y1;
            hInfo.x2 = conn.x2;
            hInfo.y2 = conn.y2;
            hInfo.fromZone = conn.fromZone;
            hInfo.toZone = conn.toZone;
            hInfo.isHorizontal = conn.isHorizontal;
            hInfo.isBreach = conn.isBreach;
            hInfo.isCrack = conn.isCrack;
            hInfo.isWindow = conn.isWindow || (conn.description.contains(QStringLiteral("Window"), Qt::CaseInsensitive) ||
                              conn.description.contains(QStringLiteral("Окно"), Qt::CaseInsensitive));
            hInfo.width = conn.width;
            hInfo.height = conn.height;
            hInfo.description = conn.description;
            hInfo.pathsToOtherZones = conn.pathsToOtherZones;

            // Target zone and any LOS-visible zones through this portal
            std::set<int> visSet;
            if (conn.toZone >= 0) {
                visSet.insert(conn.toZone);
            }
            for (int vz : conn.visibleZones) {
                if (vz >= 0) visSet.insert(vz);
            }

            for (auto it = conn.pathsToOtherZones.begin(); it != conn.pathsToOtherZones.end(); ++it) {
                if (it.key() >= 0) visSet.insert(it.key());
            }

            // Find all other zones reachable through this portal hop in pathsToOtherZones
            for (auto it = pvsInfo.pathsToOtherZones.begin(); it != pvsInfo.pathsToOtherZones.end(); ++it) {
                int targetZ = it.key();
                const auto& path = it.value();
                if (!path.empty()) {
                    const auto& firstHop = path.front();
                    if (firstHop.toZone == conn.toZone &&
                        firstHop.layer == conn.layer &&
                        firstHop.x1 == conn.x1 &&
                        firstHop.y1 == conn.y1 &&
                        firstHop.isHorizontal == conn.isHorizontal) {
                        visSet.insert(targetZ);
                    }
                }
            }

            hInfo.visibleZoneIds.assign(visSet.begin(), visSet.end());
            m_currentPortals.push_back(hInfo);
        }
    }

    // 2. Include exterior portals from VisZoneManager
    for (const auto& mp : m_mgr->portals()) {
        if (mp.isExterior && (mp.zoneA == m_activeZoneId || mp.zoneB == m_activeZoneId)) {
            HighlightedPortalInfo extInfo;
            extInfo.layer = mp.floor;
            extInfo.x1 = mp.tileA.x();
            extInfo.y1 = mp.tileA.y();
            extInfo.x2 = mp.tileB.x();
            extInfo.y2 = mp.tileB.y();
            extInfo.fromZone = m_activeZoneId;
            extInfo.toZone = -1;
            extInfo.isExterior = true;
            extInfo.isWindow = (mp.type == PortalType::ExteriorWindow);
            extInfo.description = extInfo.isWindow ? tr("Exterior window to outdoors") : tr("Exterior door to outdoors");
            m_currentPortals.push_back(extInfo);
        }
    }

    // 3. Ensure all topological portals from VisZoneManager are included
    for (int pIdx : zone->portalIndices) {
        const MapPortal* mp = m_mgr->getPortal(pIdx);
        if (!mp || mp->isExterior) continue;
        int targetZ = (mp->zoneA == m_activeZoneId) ? mp->zoneB : mp->zoneA;
        if (targetZ < 0) continue;

        bool alreadyPresent = false;
        for (const auto& existing : m_currentPortals) {
            if (existing.toZone == targetZ && existing.layer == mp->floor &&
                ((existing.x1 == mp->tileA.x() && existing.y1 == mp->tileA.y()) ||
                 (existing.x1 == mp->tileB.x() && existing.y1 == mp->tileB.y()))) {
                alreadyPresent = true;
                break;
            }
        }
        if (!alreadyPresent) {
            HighlightedPortalInfo fInfo;
            fInfo.layer = mp->floor;
            fInfo.x1 = mp->tileA.x();
            fInfo.y1 = mp->tileA.y();
            fInfo.x2 = mp->tileB.x();
            fInfo.y2 = mp->tileB.y();
            fInfo.fromZone = m_activeZoneId;
            fInfo.toZone = targetZ;
            fInfo.isExterior = false;
            fInfo.isWindow = (mp->type == PortalType::InterZoneWindow || mp->type == PortalType::ExteriorWindow);
            fInfo.visibleZoneIds.push_back(targetZ);
            fInfo.description = fInfo.isWindow 
                ? tr("Window passage at Floor %1 (%2, %3)").arg(fInfo.layer).arg(fInfo.x1).arg(fInfo.y1)
                : tr("Doorway at Floor %1 (%2, %3)").arg(fInfo.layer).arg(fInfo.x1).arg(fInfo.y1);
            m_currentPortals.push_back(fInfo);
        }
    }

    // Deduplicate portals
    std::vector<HighlightedPortalInfo> uniquePortals;
    for (const auto& p : m_currentPortals) {
        bool found = false;
        for (auto& u : uniquePortals) {
            if (u.layer == p.layer && u.x1 == p.x1 && u.y1 == p.y1 && u.toZone == p.toZone && u.isHorizontal == p.isHorizontal) {
                for (int vz : p.visibleZoneIds) {
                    if (std::find(u.visibleZoneIds.begin(), u.visibleZoneIds.end(), vz) == u.visibleZoneIds.end()) {
                        u.visibleZoneIds.push_back(vz);
                    }
                }
                for (auto it = p.pathsToOtherZones.begin(); it != p.pathsToOtherZones.end(); ++it) {
                    if (!u.pathsToOtherZones.contains(it.key()) || it.value().size() < u.pathsToOtherZones[it.key()].size()) {
                        u.pathsToOtherZones[it.key()] = it.value();
                    }
                }
                found = true;
                break;
            }
        }
        if (!found) {
            uniquePortals.push_back(p);
        }
    }
    m_currentPortals = std::move(uniquePortals);

    // Sort: Leaks first, then Cracks, then Doors/Windows, then Exterior
    std::stable_sort(m_currentPortals.begin(), m_currentPortals.end(), [](const HighlightedPortalInfo& a, const HighlightedPortalInfo& b) {
        int prioA = a.isBreach ? 0 : (a.isCrack ? 1 : (a.isExterior ? 3 : 2));
        int prioB = b.isBreach ? 0 : (b.isCrack ? 1 : (b.isExterior ? 3 : 2));
        return prioA < prioB;
    });

    // Populate Portal List Widget with custom clickable rows
    int leakCount = 0;
    int listW = m_listPortals->viewport()->width();
    if (listW <= 60) listW = m_listPortals->width() - 10;
    if (listW <= 60) listW = 320;

    for (size_t i = 0; i < m_currentPortals.size(); ++i) {
        const auto& p = m_currentPortals[i];
        if (p.isBreach) ++leakCount;

        QListWidgetItem* item = new QListWidgetItem(m_listPortals);
        item->setData(Qt::UserRole, static_cast<int>(i));

        PortalItemWidget* w = new PortalItemWidget(static_cast<int>(i), p, m_listPortals);
        w->setCallbacks(
            [this](int idx) {
                m_listPortals->setCurrentRow(idx);
                onPortalClicked(m_listPortals->item(idx));
            },
            [this](int portalIdx, int vzId) {
                onVisibleZoneChipClicked(portalIdx, vzId);
            },
            [this](int targetZoneId) {
                onExternalZoneSelected(targetZoneId);
            }
        );
        item->setSizeHint(QSize(listW, w->heightForWidth(listW)));
        m_listPortals->setItemWidget(item, w);
    }

    if (m_currentPortals.empty()) {
        QListWidgetItem* item = new QListWidgetItem(
            isRu ? QString::fromUtf8("В этой зоне нет обнаруженных порталов.") : QStringLiteral("No portals detected in this zone."),
            m_listPortals);
        item->setData(Qt::UserRole, -1);
        item->setForeground(QColor(148, 163, 184));
    }

    // Update KPI label
    if (leakCount > 0) {
        m_lblKpi->setText(isRu
            ? QString::fromUtf8("🚪 Порталов в зоне: %1  •  🚨 Утечек видимости: %2").arg(m_currentPortals.size()).arg(leakCount)
            : QString("🚪 Portals in zone: %1  •  🚨 Visibility leaks: %2").arg(m_currentPortals.size()).arg(leakCount));
        m_lblKpi->setStyleSheet(QStringLiteral("background: #2f1717; color: #f87171; border: 1px solid #ef4444; border-radius: 4px; padding: 5px 8px; font-weight: bold; font-size: 11px;"));
    } else {
        m_lblKpi->setText(isRu
            ? QString::fromUtf8("🚪 Порталов в зоне: %1  •  ✓ Утечек нет").arg(m_currentPortals.size())
            : QString("🚪 Portals in zone: %1  •  ✓ No leaks").arg(m_currentPortals.size()));
        m_lblKpi->setStyleSheet(QStringLiteral("background: #162a1c; color: #4ade80; border: 1px solid #22c55e; border-radius: 4px; padding: 5px 8px; font-weight: bold; font-size: 11px;"));
    }
}

void VisZoneDock::onPortalClicked(QListWidgetItem* item) {
    if (!item) return;
    int idx = item->data(Qt::UserRole).toInt();

    for (int r = 0; r < m_listPortals->count(); ++r) {
        QListWidgetItem* it = m_listPortals->item(r);
        auto* w = dynamic_cast<PortalItemWidget*>(m_listPortals->itemWidget(it));
        if (w) {
            bool isCurrent = (r == m_listPortals->row(item));
            w->setSelected(isCurrent);
            if (!isCurrent) {
                w->setFocusedChip(-1);
            }
        }
    }

    if (idx < 0 || idx >= static_cast<int>(m_currentPortals.size())) {
        m_btnShowOnMap->setEnabled(false);
        m_btnEditWall->setEnabled(false);
        emit portalHighlightCleared();
        emit tracePathSelected({});
        return;
    }

    auto& portal = m_currentPortals[idx];
    portal.focusedVisibleZone = -1;

    auto* curW = dynamic_cast<PortalItemWidget*>(m_listPortals->itemWidget(item));
    if (curW) {
        curW->setFocusedChip(-1);
    }

    emit tracePathSelected({});
    emit portalHighlighted(portal);
    emit cellSelected(portal.layer, portal.x1, portal.y1);

    m_btnShowOnMap->setEnabled(true);
    m_btnEditWall->setEnabled(portal.layer >= 0 && portal.x1 >= 0 && portal.y1 >= 0);
}

void VisZoneDock::onPortalDoubleClicked(QListWidgetItem* item) {
    onPortalClicked(item);
    if (!item) return;
    int idx = item->data(Qt::UserRole).toInt();
    if (idx >= 0 && idx < static_cast<int>(m_currentPortals.size())) {
        const auto& portal = m_currentPortals[idx];
        if (portal.isBreach) {
            emit editSegmentRequested(portal.layer, portal.x1, portal.y1);
        }
    }
}

void VisZoneDock::onVisibleZoneChipClicked(int portalIdx, int vzId) {
    if (portalIdx < 0 || portalIdx >= static_cast<int>(m_currentPortals.size())) return;

    auto& portal = m_currentPortals[portalIdx];
    int targetFocusedZone = (portal.focusedVisibleZone == vzId) ? -1 : vzId;
    portal.focusedVisibleZone = targetFocusedZone;

    m_listPortals->setCurrentRow(portalIdx);
    for (int r = 0; r < m_listPortals->count(); ++r) {
        QListWidgetItem* it = m_listPortals->item(r);
        auto* w = dynamic_cast<PortalItemWidget*>(m_listPortals->itemWidget(it));
        if (w) {
            bool isCurrent = (r == portalIdx);
            w->setSelected(isCurrent);
            if (isCurrent) {
                w->setFocusedChip(targetFocusedZone);
            } else {
                w->setFocusedChip(-1);
            }
        }
    }

    emit portalHighlighted(portal);
    updateItemSizeHints();

    if (targetFocusedZone >= 0) {
        emit cellSelected(portal.layer, portal.x1, portal.y1);

        if (portal.pathsToOtherZones.contains(targetFocusedZone)) {
            emit tracePathSelected(portal.pathsToOtherZones[targetFocusedZone]);
        } else if (targetFocusedZone == portal.toZone) {
            ZoneConnection directConn;
            directConn.fromZone = portal.fromZone;
            directConn.toZone = portal.toZone;
            directConn.layer = portal.layer;
            directConn.x1 = portal.x1; directConn.y1 = portal.y1;
            directConn.x2 = portal.x2; directConn.y2 = portal.y2;
            directConn.isHorizontal = portal.isHorizontal;
            directConn.isBreach = portal.isBreach;
            directConn.isCrack = portal.isCrack;
            directConn.isWindow = portal.isWindow;
            directConn.description = portal.description;
            emit tracePathSelected({ directConn });
        } else if (m_pvsGraph.contains(m_activeZoneId) &&
                   m_pvsGraph[m_activeZoneId].pathsToOtherZones.contains(targetFocusedZone)) {
            emit tracePathSelected(m_pvsGraph[m_activeZoneId].pathsToOtherZones[targetFocusedZone]);
        } else {
            emit tracePathSelected({});
        }
    } else {
        emit tracePathSelected({});
    }

    m_btnShowOnMap->setEnabled(true);
    m_btnEditWall->setEnabled(portal.layer >= 0 && portal.x1 >= 0 && portal.y1 >= 0);
}

void VisZoneDock::onShowOnMapClicked() {
    if (!m_listPortals || !m_listPortals->currentItem()) return;
    int idx = m_listPortals->currentItem()->data(Qt::UserRole).toInt();
    if (idx >= 0 && idx < static_cast<int>(m_currentPortals.size())) {
        const auto& portal = m_currentPortals[idx];
        emit portalHighlighted(portal);
        emit cellSelected(portal.layer, portal.x1, portal.y1);
    }
}

void VisZoneDock::onEditWallClicked() {
    if (!m_listPortals || !m_listPortals->currentItem()) return;
    int idx = m_listPortals->currentItem()->data(Qt::UserRole).toInt();
    if (idx >= 0 && idx < static_cast<int>(m_currentPortals.size())) {
        const auto& portal = m_currentPortals[idx];
        emit editSegmentRequested(portal.layer, portal.x1, portal.y1);
    }
}

void VisZoneDock::resizeEvent(QResizeEvent* event) {
    QDockWidget::resizeEvent(event);
    updateItemSizeHints();
}

void VisZoneDock::updateItemSizeHints() {
    if (!m_listPortals) return;
    int listWidth = m_listPortals->viewport()->width();
    if (listWidth <= 60) listWidth = m_listPortals->width() - 10;
    if (listWidth <= 60) return;

    for (int r = 0; r < m_listPortals->count(); ++r) {
        QListWidgetItem* item = m_listPortals->item(r);
        auto* w = dynamic_cast<PortalItemWidget*>(m_listPortals->itemWidget(item));
        if (w) {
            int h = w->heightForWidth(listWidth);
            item->setSizeHint(QSize(listWidth, h));
        }
    }
}

void VisZoneDock::resetToNormalView() {
    m_activeZoneId = -1;
    m_zoneCombo->setCurrentIndex(0);
    emit zoneSelected(-1);
    emit portalHighlightCleared();
    updateActiveZoneDetails();
    if (m_chkIsolate) {
        m_chkIsolate->setChecked(false);
    }
}

void VisZoneDock::setColorAllZones(bool enabled) {
    if (m_chkColorAll && m_chkColorAll->isChecked() != enabled) {
        m_chkColorAll->setChecked(enabled);
    }
}

bool VisZoneDock::isColorAllZones() const {
    return m_chkColorAll && m_chkColorAll->isChecked();
}

void VisZoneDock::closeEvent(QCloseEvent* event) {
    emit portalHighlightCleared();
    QDockWidget::closeEvent(event);
}

void VisZoneDock::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
    }
    QDockWidget::changeEvent(event);
}

void VisZoneDock::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Escape) {
        resetToNormalView();
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Delete && m_activeZoneId >= 0) {
        emit dichotomyDeleteZoneRequested(m_activeZoneId);
        event->accept();
        return;
    }
    QDockWidget::keyPressEvent(event);
}

void VisZoneDock::retranslateUi() {
    bool isRu = (LanguageManager::instance().effectiveLanguage() == LanguageManager::Language::Russian);
    setWindowTitle(isRu ? QString::fromUtf8("Зоны видимости и порталы (PVS)") : QStringLiteral("Visibility Zones & Portals (PVS)"));
    if (m_btnPrev) m_btnPrev->setToolTip(isRu ? QString::fromUtf8("Предыдущая зона") : QStringLiteral("Previous zone"));
    if (m_btnNext) m_btnNext->setToolTip(isRu ? QString::fromUtf8("Следующая зона") : QStringLiteral("Next zone"));
    if (m_btnFocus) m_btnFocus->setToolTip(isRu ? QString::fromUtf8("Центрировать камеру на зоне") : QStringLiteral("Center camera on zone"));
    if (m_btnResetZone) m_btnResetZone->setToolTip(isRu ? QString::fromUtf8("Сбросить выбор зоны / Показать все") : QStringLiteral("Reset zone selection / Show all"));
    if (m_btnRefresh) m_btnRefresh->setToolTip(isRu ? QString::fromUtf8("Пересчитать зоны и PVS-граф") : QStringLiteral("Recompute zones & PVS graph"));
    if (m_btnReset) {
        m_btnReset->setText(isRu ? QString::fromUtf8("✕ Сброс зоны") : QStringLiteral("✕ Reset Zone"));
        m_btnReset->setToolTip(isRu ? QString::fromUtf8("Сбросить выбор зоны / Показать все зоны") : QStringLiteral("Reset zone selection / Show all zones"));
    }
    if (m_chkCurrentFloorOnly) m_chkCurrentFloorOnly->setText(isRu ? QString::fromUtf8("Только текущий этаж") : QStringLiteral("Current floor only"));

    if (m_btnShowOnMap) m_btnShowOnMap->setText(isRu ? QString::fromUtf8("🔍 Показать на карте") : QStringLiteral("🔍 Show on map"));
    if (m_btnEditWall) m_btnEditWall->setText(isRu ? QString::fromUtf8("🧱 Редактировать стену...") : QStringLiteral("🧱 Edit Wall..."));

    if (m_grpIsolation) m_grpIsolation->setTitle(isRu ? QString::fromUtf8("Изоляция и цвета") : QStringLiteral("Isolation && Colors"));
    if (m_chkIsolate) m_chkIsolate->setText(isRu ? QString::fromUtf8("Затемнять невидимые") : QStringLiteral("Dim invisible"));
    if (m_lblDim && m_sliderDim) m_lblDim->setText(isRu ? QString::fromUtf8("Яркость: %1%").arg(m_sliderDim->value()) : QString("Brightness: %1%").arg(m_sliderDim->value()));
    if (m_sliderDim) m_sliderDim->setToolTip(isRu ? QString::fromUtf8("Яркость неактивных зон (0% = скрыть, 50% = полупрозрачно, 100% = полная яркость)") : QStringLiteral("Brightness of inactive zones (0% = hide, 50% = translucent, 100% = full brightness)"));
    if (m_chkColorAll) m_chkColorAll->setText(isRu ? QString::fromUtf8("🎨 Раскрасить все") : QStringLiteral("🎨 Color All"));
    if (m_btnRecolor) m_btnRecolor->setText(isRu ? QString::fromUtf8("🎲 Палитра") : QStringLiteral("🎲 Palette"));

    populateZoneCombo();
    updateActiveZoneDetails();
}
