#ifndef TEXTUREOPTIMIZATIONDIALOG_H
#define TEXTUREOPTIMIZATIONDIALOG_H

#include <QDialog>
#include <QString>
#include <QStringList>
#include <QList>
#include <cstdint>

class QTableWidget;
class QComboBox;
class QCheckBox;
class QLabel;
class QPushButton;

struct TextureTargetInfo {
    QString filePath;
    QString semantic;       // "Diffuse", "Normal Map", "Specular", "Illumination", etc.
    int currentWidth = 0;
    int currentHeight = 0;
    int mipCount = 0;
    qint64 currentSizeBytes = 0;
    qint64 currentVramBytes = 0;
    QString formatStr;      // "DXT1", "DXT5", "DXT3", "RGBA32", "PNG", "TGA", etc.
    bool hasAlpha = false;
    bool checked = true;
    int targetWidth = 0;
    int targetHeight = 0;
};

struct TextureTargetTask {
    QString filePath;
    int targetMaxSize = 0; // Maximum dimension to clamp this texture to (0 = original)
};

struct TextureOptimizationSettings {
    bool generateMips = true;
    bool pureAlphaCheck = true;
    bool forcePot = true;
    bool createBackup = true;
    bool forceRecompress = false;
    QList<TextureTargetTask> tasks;
};

class TextureOptimizationDialog : public QDialog {
    Q_OBJECT
public:
    explicit TextureOptimizationDialog(const QString& itemName, const QStringList& targetPaths, QWidget* parent = nullptr);
    ~TextureOptimizationDialog() override = default;

    TextureOptimizationSettings getSettings() const;

private slots:
    void onGlobalScaleChanged(int index);
    void onRowResolutionChanged(int row, int comboIndex);
    void onSettingChanged();
    void onSelectAll(bool select);

private:
    void inspectTargets(const QStringList& paths);
    void setupUI();
    void updateCalculations();
    void loadSavedSettings();
    void saveSettings();

    QWidget* createStatCard(const QString& title, QLabel*& outValLabel, const QString& accentColor);

    QString m_itemName;
    QList<TextureTargetInfo> m_targets;

    // Stat Dashboard cards
    QLabel* m_cardBeforeVal = nullptr;
    QLabel* m_cardAfterVal = nullptr;
    QLabel* m_cardDeltaVal = nullptr;

    QTableWidget* m_fileTable = nullptr;
    QComboBox* m_globalScaleCombo = nullptr;
    QCheckBox* m_mipsCheck = nullptr;
    QCheckBox* m_pureAlphaCheck = nullptr;
    QCheckBox* m_forcePotCheck = nullptr;
    QCheckBox* m_backupCheck = nullptr;
    QCheckBox* m_forceCheck = nullptr;
    QLabel* m_adviceLabel = nullptr;
    QPushButton* m_okBtn = nullptr;
    QPushButton* m_cancelBtn = nullptr;

    bool m_updatingFromGlobal = false;
};

#endif // TEXTUREOPTIMIZATIONDIALOG_H
