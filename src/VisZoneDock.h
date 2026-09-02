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

public slots:
    void onFloorChanged(int floor);
    void onExternalZoneSelected(int zoneId);
    void resetToNormalView();

protected:
    void closeEvent(QCloseEvent* event) override;

signals:
    void zoneSelected(int zoneId);
    void isolationChanged(bool isolate, float dimOpacity);
    void entitySelected(int entityIndex);

private slots:
    void onZoneComboChanged(int index);
    void onPrevZone();
    void onNextZone();
    void onIsolationOptionChanged();
    void onPortalDoubleClicked(QListWidgetItem* item);
    void onEntityClicked(QListWidgetItem* item);
    void onFloorFilterToggled(bool checked);

private:
    void populateZoneCombo();
    void updateActiveZoneDetails();

    std::shared_ptr<FPSCMap> m_map;
    std::shared_ptr<VisZoneManager> m_mgr;
    int m_currentFloor = 0;
    int m_activeZoneId = -1;

    QCheckBox* m_chkCurrentFloorOnly = nullptr;
    QComboBox* m_zoneCombo = nullptr;
    QPushButton* m_btnPrev = nullptr;
    QPushButton* m_btnNext = nullptr;

    QCheckBox* m_chkIsolate = nullptr;
    QRadioButton* m_radioHide = nullptr;
    QRadioButton* m_radioDim = nullptr;
    QSlider* m_sliderDim = nullptr;

    QLabel* m_lblStats = nullptr;
    QListWidget* m_listPortals = nullptr;
    QListWidget* m_listEntities = nullptr;

    bool m_updatingCombo = false;
};
