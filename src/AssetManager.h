#ifndef ASSETMANAGER_H
#define ASSETMANAGER_H

#include <QString>
#include <QPixmap>
#include <QImage>
#include <QMap>
#include <QFileInfo>
#include <QDir>
#include <QMutex>
#include <cstdint>

struct TextureMetrics {
    int width = 0;
    int height = 0;
    qint64 ramBytes = 0;
    qint64 diskBytes = 0;
    bool valid = false;
};

class AssetManager {
public:
    static AssetManager& instance();

    void setEngineRoot(const QString& path);
    QString engineRoot() const;

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

    mutable QMutex m_mutex{QMutex::Recursive};
    QString m_engineRoot;
    QMap<QString, QPixmap> m_textureCache;
    QMap<QString, QPixmap> m_iconCache;
    QMap<QString, QString> m_resolvedPathCache;
    QMap<QString, qint64> m_fileSizeCache;
    QMap<QString, TextureMetrics> m_textureMetricsCache;
};

#endif // ASSETMANAGER_H
