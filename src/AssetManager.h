#ifndef ASSETMANAGER_H
#define ASSETMANAGER_H

#include <QString>
#include <QPixmap>
#include <QImage>
#include <QMap>
#include <QFileInfo>
#include <QDir>
#include <cstdint>

class AssetManager {
public:
    static AssetManager& instance();

    void setEngineRoot(const QString& path);
    QString engineRoot() const { return m_engineRoot; }

    QString resolvePath(const QString& relPath) const;
    
    QPixmap loadTexture(const QString& relPath);
    QPixmap loadIcon(const QString& relPath);
    
    // Texture metrics for memory profiling
    bool getTextureMetrics(const QString& relPath, int& outWidth, int& outHeight, qint64& outRamBytes, qint64& outDiskBytes);
    qint64 getFileSizeBytes(const QString& relPath) const;

    // Direct DDS / TGA Image decoding
    static QImage loadDDS(const QString& fullPath);
    static QImage loadTGA(const QString& fullPath);
    static QImage decodeDDSMemory(const uint8_t* data, size_t size);

    void clearCache();

private:
    AssetManager();
    ~AssetManager() = default;

    QString m_engineRoot;
    QMap<QString, QPixmap> m_textureCache;
    QMap<QString, QPixmap> m_iconCache;
    QMap<QString, QString> m_resolvedPathCache;
};

#endif // ASSETMANAGER_H
