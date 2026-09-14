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
    QString formatStr;      // "DXT1", "DXT5", "DXT3", "RGBA32", "PNG", "TGA", etc.
    bool checked = true;
};

struct TextureOptimizationSettings {
    int maxSize = 1024;     // 0 = no limit, 2048, 1024, 512, 256
    bool generateMips = true;
    bool pureAlphaCheck = true;
    bool forcePot = true;
    bool createBackup = true;
    bool forceRecompress = false;
    QStringList selectedFilePaths;
};

class TextureOptimizationDialog : public QDialog {
    Q_OBJECT
public:
    explicit TextureOptimizationDialog(const QString& itemName, const QStringList& targetPaths, QWidget* parent = nullptr);
    ~TextureOptimizationDialog() override = default;

    TextureOptimizationSettings getSettings() const;

private slots:
    void onPresetChanged(int index);
    void onSettingChanged();
    void updateAdviceBanner();
    void onSelectAll(bool select);

private:
    void inspectTargets(const QStringList& paths);
    void setupUI();
    void loadSavedSettings();
    void saveSettings();

    QString m_itemName;
    QList<TextureTargetInfo> m_targets;

    QTableWidget* m_fileTable = nullptr;
    QComboBox* m_presetCombo = nullptr;
    QComboBox* m_maxSizeCombo = nullptr;
    QCheckBox* m_mipsCheck = nullptr;
    QCheckBox* m_pureAlphaCheck = nullptr;
    QCheckBox* m_forcePotCheck = nullptr;
    QCheckBox* m_backupCheck = nullptr;
    QCheckBox* m_forceCheck = nullptr;
    QLabel* m_adviceLabel = nullptr;
    QPushButton* m_okBtn = nullptr;
    QPushButton* m_cancelBtn = nullptr;

    bool m_updatingFromPreset = false;
};

#endif // TEXTUREOPTIMIZATIONDIALOG_H
