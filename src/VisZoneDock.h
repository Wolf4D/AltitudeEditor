#pragma once

#include <QDockWidget>
#include <QComboBox>
#include <QPushButton>
#include <QCheckBox>
#include <QRadioButton>
#include <QSlider>
#include <QLabel>
#include <QListWidget>
#include <memory>
#include "FPSCData.h"
#include "VisZoneManager.h"

class VisZoneDock : public QDockWidget {
    Q_OBJECT
public:
    explicit VisZoneDock(QWidget* parent = nullptr);

    void setVisZoneManager(std::shared_ptr<VisZoneManager> mgr);
    void setMap(std::shared_ptr<FPSCMap> map);

    int activeZoneId() const { return m_activeZoneId; }

public slots:
    void onFloorChanged(int floor);
    void onExternalZoneSelected(int zoneId);
    void resetToNormalView();
    void setColorAllZones(bool enabled);
    bool isColorAllZones() const;

protected:
    void closeEvent(QCloseEvent* event) override;
    void changeEvent(QEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

signals:
    void zoneSelected(int zoneId);
    void isolationChanged(bool isolate, float dimOpacity);
    void entitySelected(int entityIndex);
    void colorAllZonesToggled(bool enabled);
    void dichotomyDeleteZoneRequested(int zoneId);

private slots:
    void onZoneComboChanged(int index);
    void onPrevZone();
    void onNextZone();
    void onIsolationOptionChanged();
    void onPortalDoubleClicked(QListWidgetItem* item);
    void onEntityClicked(QListWidgetItem* item);
    void onFloorFilterToggled(bool checked);

private:
    void retranslateUi();
    void populateZoneCombo();
    void updateActiveZoneDetails();

    std::shared_ptr<FPSCMap> m_map;
    std::shared_ptr<VisZoneManager> m_mgr;
    int m_currentFloor = 0;
    int m_activeZoneId = -1;

    class QGroupBox* m_grpSelection = nullptr;
    class QGroupBox* m_grpIsolation = nullptr;
    class QGroupBox* m_grpDetails = nullptr;

    QCheckBox* m_chkCurrentFloorOnly = nullptr;
    QCheckBox* m_chkColorAll = nullptr;
    QPushButton* m_btnRecolor = nullptr;
    QComboBox* m_zoneCombo = nullptr;
    QPushButton* m_btnPrev = nullptr;
    QPushButton* m_btnNext = nullptr;
    QPushButton* m_btnReset = nullptr;
    QPushButton* m_btnDichotomyDelete = nullptr;

    QCheckBox* m_chkIsolate = nullptr;
    QRadioButton* m_radioHide = nullptr;
    QRadioButton* m_radioDim = nullptr;
    QLabel* m_lblDim = nullptr;
    QSlider* m_sliderDim = nullptr;

    QLabel* m_lblStats = nullptr;
    QLabel* m_lblPortalsHeader = nullptr;
    QLabel* m_lblEntitiesHeader = nullptr;
    QListWidget* m_listPortals = nullptr;
    QListWidget* m_listEntities = nullptr;

    bool m_updatingCombo = false;
};
