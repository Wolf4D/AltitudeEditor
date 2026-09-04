#include "AssetManager.h"
#include <QFile>
#include <QDirIterator>
#include <QDebug>
#include <QImageReader>
#include <QMutexLocker>
#include <cstring>

#pragma pack(push, 1)
struct DDS_PIXELFORMAT {
    uint32_t dwSize;
    uint32_t dwFlags;
    uint32_t dwFourCC;
    uint32_t dwRGBBitCount;
    uint32_t dwRBitMask;
    uint32_t dwGBitMask;
    uint32_t dwBBitMask;
    uint32_t dwABitMask;
};

struct DDS_HEADER {
    uint32_t dwSize;
    uint32_t dwFlags;
    uint32_t dwHeight;
    uint32_t dwWidth;
    uint32_t dwPitchOrLinearSize;
    uint32_t dwDepth;
    uint32_t dwMipMapCount;
    uint32_t dwReserved1[11];
    DDS_PIXELFORMAT ddspf;
    uint32_t dwCaps;
    uint32_t dwCaps2;
    uint32_t dwCaps3;
    uint32_t dwCaps4;
    uint32_t dwReserved2;
};
#pragma pack(pop)

AssetManager& AssetManager::instance() {
    static AssetManager inst;
    return inst;
}

AssetManager::AssetManager() {
    m_engineRoot = QStringLiteral("C:/Program Files (x86)/The Game Creators/FPS Creator");
    if (!QDir(m_engineRoot).exists()) {
        m_engineRoot = QStringLiteral("C:\\Program Files (x86)\\The Game Creators\\FPS Creator");
    }
}

void AssetManager::setEngineRoot(const QString& path) {
    QMutexLocker locker(&m_mutex);
    m_engineRoot = QDir::cleanPath(path);
    clearCache();
}

QString AssetManager::engineRoot() const {
    QMutexLocker locker(&m_mutex);
    return m_engineRoot;
}

void AssetManager::clearCache() {
    QMutexLocker locker(&m_mutex);
    m_textureCache.clear();
    m_iconCache.clear();
    m_resolvedPathCache.clear();
    m_fileSizeCache.clear();
    m_textureMetricsCache.clear();
}

QString AssetManager::resolvePath(const QString& relPath) const {
    if (relPath.trimmed().isEmpty()) return QString();
    
    QString clean = relPath;
    clean.replace('\\', '/');
    while (clean.startsWith('/')) clean.remove(0, 1);

    QMutexLocker locker(&m_mutex);

    if (m_resolvedPathCache.contains(clean)) {
        return m_resolvedPathCache.value(clean);
    }

    if (QFileInfo::exists(clean)) {
        const_cast<AssetManager*>(this)->m_resolvedPathCache[clean] = clean;
        return clean;
    }

    int dotIdx = clean.lastIndexOf('.');
    QString baseWithoutExt = (dotIdx > 0) ? clean.left(dotIdx) : clean;

    QStringList candidateExts;
    if (dotIdx > 0) candidateExts << clean.mid(dotIdx);
    candidateExts << QStringLiteral(".dds") << QStringLiteral(".bmp") << QStringLiteral(".png")
                  << QStringLiteral(".tga") << QStringLiteral(".jpg")
                  << QStringLiteral(".DDS") << QStringLiteral(".BMP") << QStringLiteral(".PNG")
                  << QStringLiteral(".TGA") << QStringLiteral(".JPG");

    QStringList searchPrefixes = {
        m_engineRoot + "/Files/",
        m_engineRoot + "/Files/segments/",
        m_engineRoot + "/Files/entitybank/",
        m_engineRoot + "/Files/texturebank/",
        m_engineRoot + "/Files/meshbank/",
        m_engineRoot + "/Files/audiobank/",
        m_engineRoot + "/Files/gamecore/",
        m_engineRoot + "/"
    };

    for (const QString& prefix : searchPrefixes) {
        for (const QString& ext : candidateExts) {
            QString candidate = QDir::cleanPath(prefix + baseWithoutExt + ext);
            if (QFileInfo::exists(candidate)) {
                const_cast<AssetManager*>(this)->m_resolvedPathCache[clean] = candidate;
                return candidate;
            }
        }
    }

    // Try finding by basename if deep path failed
    QString baseName = QFileInfo(clean).fileName();
    int baseDotIdx = baseName.lastIndexOf('.');
    QString baseNameWithoutExt = (baseDotIdx > 0) ? baseName.left(baseDotIdx) : baseName;

    if (!baseName.isEmpty()) {
        for (const QString& prefix : searchPrefixes) {
            for (const QString& ext : candidateExts) {
                QString candidate = QDir::cleanPath(prefix + baseNameWithoutExt + ext);
                if (QFileInfo::exists(candidate)) {
                    const_cast<AssetManager*>(this)->m_resolvedPathCache[clean] = candidate;
                    return candidate;
                }
            }
        }
    }

    const_cast<AssetManager*>(this)->m_resolvedPathCache[clean] = QString();
    return QString();
}

qint64 AssetManager::getFileSizeBytes(const QString& relPath) const {
    if (relPath.trimmed().isEmpty()) return 0;

    QString clean = relPath;
    clean.replace('\\', '/');
    while (clean.startsWith('/')) clean.remove(0, 1);

    QMutexLocker locker(&m_mutex);

    if (m_fileSizeCache.contains(clean)) {
        return m_fileSizeCache.value(clean);
    }

    QString resolved = resolvePath(clean);
    if (resolved.isEmpty()) {
        const_cast<AssetManager*>(this)->m_fileSizeCache[clean] = 0;
        return 0;
    }

    qint64 sz = QFileInfo(resolved).size();
    const_cast<AssetManager*>(this)->m_fileSizeCache[clean] = sz;
    return sz;
}

bool AssetManager::getTextureMetrics(const QString& relPath, int& outWidth, int& outHeight, qint64& outRamBytes, qint64& outDiskBytes) {
    outWidth = 0;
    outHeight = 0;
    outRamBytes = 0;
    outDiskBytes = 0;

    if (relPath.trimmed().isEmpty()) return false;

    QString clean = relPath;
    clean.replace('\\', '/');
    while (clean.startsWith('/')) clean.remove(0, 1);

    QMutexLocker locker(&m_mutex);

    if (m_textureMetricsCache.contains(clean)) {
        const auto& tm = m_textureMetricsCache.value(clean);
        outWidth = tm.width;
        outHeight = tm.height;
        outRamBytes = tm.ramBytes;
        outDiskBytes = tm.diskBytes;
        return tm.valid;
    }

    QString resolved = resolvePath(clean);
    if (resolved.isEmpty()) {
        TextureMetrics tm;
        tm.valid = false;
        m_textureMetricsCache[clean] = tm;
        return false;
    }

    QFileInfo fi(resolved);
    outDiskBytes = fi.size();
    QString ext = fi.suffix().toLower();

    int w = 0;
    int h = 0;

    if (ext == "dds") {
        // Read DDS header (128 bytes) directly without full image decoding
        QFile f(resolved);
        if (f.open(QIODevice::ReadOnly)) {
            char hdrBuf[128];
            qint64 bytesRead = f.read(hdrBuf, 128);
            if (bytesRead >= 128 && hdrBuf[0] == 'D' && hdrBuf[1] == 'D' && hdrBuf[2] == 'S' && hdrBuf[3] == ' ') {
                const DDS_HEADER* hdr = reinterpret_cast<const DDS_HEADER*>(hdrBuf + 4);
                if (hdr->dwWidth > 0 && hdr->dwHeight > 0 && hdr->dwWidth <= 8192 && hdr->dwHeight <= 8192) {
                    w = static_cast<int>(hdr->dwWidth);
                    h = static_cast<int>(hdr->dwHeight);
                }
            }
        }
        if (w <= 0 || h <= 0) {
            QImage img = loadDDS(resolved);
            if (!img.isNull()) {
                w = img.width();
                h = img.height();
            }
        }
    } else if (ext == "tga") {
        // Read TGA header (18 bytes) directly without full image decoding
        QFile f(resolved);
        if (f.open(QIODevice::ReadOnly)) {
            unsigned char tgaHdr[18];
            if (f.read(reinterpret_cast<char*>(tgaHdr), 18) >= 18) {
                int tw = tgaHdr[12] | (tgaHdr[13] << 8);
                int th = tgaHdr[14] | (tgaHdr[15] << 8);
                if (tw > 0 && th > 0 && tw <= 8192 && th <= 8192) {
                    w = tw;
                    h = th;
                }
            }
        }
        if (w <= 0 || h <= 0) {
            QImage img = loadTGA(resolved);
            if (img.isNull()) img.load(resolved);
            if (!img.isNull()) {
                w = img.width();
                h = img.height();
            }
        }
    } else {
        // For BMP, PNG, JPG: QImageReader reads only header without full pixel buffer
        QImageReader reader(resolved);
        QSize sz = reader.size();
        if (sz.isValid() && sz.width() > 0 && sz.height() > 0) {
            w = sz.width();
            h = sz.height();
        } else {
            QImage img(resolved);
            if (!img.isNull()) {
                w = img.width();
                h = img.height();
            }
        }
    }

    if (w <= 0 || h <= 0) {
        outWidth = 256;
        outHeight = 256;
    } else {
        outWidth = w;
        outHeight = h;
    }

    // Direct3D 9 Managed Pool allocation in DarkBasic Pro (FPSC-Game.exe):
    // Regardless of whether source is DDS, TGA or BMP, DarkBasic Pro's D3D9 texture loader
    // allocates managed 32-bit ARGB8888 surfaces with full mipmap chains (1.333x),
    // keeping an active copy in 32-bit process System RAM + VRAM driver backing store (2x).
    qint64 rawPixels = static_cast<qint64>(outWidth) * outHeight * 4;
    qint64 withMips = static_cast<qint64>(rawPixels * 1.333333);
    outRamBytes = withMips * 2;

    TextureMetrics tm;
    tm.width = outWidth;
    tm.height = outHeight;
    tm.ramBytes = outRamBytes;
    tm.diskBytes = outDiskBytes;
    tm.valid = true;
    m_textureMetricsCache[clean] = tm;

    return true;
}

static QImage makeTransparentChromaKey(const QImage& src) {
    if (src.isNull()) return src;
    QImage img = src.convertToFormat(QImage::Format_ARGB32);
    int w = img.width();
    int h = img.height();
    if (w < 2 || h < 2) return img;

    // Sample corner pixels to detect background color
    QRgb c00 = img.pixel(0, 0);
    QRgb c01 = img.pixel(0, h - 1);
    QRgb c10 = img.pixel(w - 1, 0);
    QRgb c11 = img.pixel(w - 1, h - 1);

    // If already has non-opaque alpha, keep original alpha channel
    bool hasTransparentAlpha = false;
    for (int y = 0; y < qMin(h, 8); ++y) {
        for (int x = 0; x < qMin(w, 8); ++x) {
            if (qAlpha(img.pixel(x, y)) < 245) {
                hasTransparentAlpha = true;
                break;
            }
        }
        if (hasTransparentAlpha) break;
    }
    if (hasTransparentAlpha) return img;

    int r0 = qRed(c00), g0 = qGreen(c00), b0 = qBlue(c00);
    // Check if corners are near uniform (white, black, or custom key color)
    bool isCornerUniform = (
        qAbs(qRed(c01) - r0) < 18 && qAbs(qGreen(c01) - g0) < 18 && qAbs(qBlue(c01) - b0) < 18 &&
        qAbs(qRed(c10) - r0) < 18 && qAbs(qGreen(c10) - g0) < 18 && qAbs(qBlue(c10) - b0) < 18
    );

    // If corner color is near pure white (>235), pure black (<18), pure magenta, or uniform:
    bool isChromaKeyCandidate = isCornerUniform || 
        (r0 > 235 && g0 > 235 && b0 > 235) || 
        (r0 < 18 && g0 < 18 && b0 < 18) || 
        (r0 > 235 && g0 < 20 && b0 > 235);

    if (!isChromaKeyCandidate) return img;

    // Apply smooth chroma key
    for (int y = 0; y < h; ++y) {
        QRgb* scan = reinterpret_cast<QRgb*>(img.scanLine(y));
        for (int x = 0; x < w; ++x) {
            QRgb pixel = scan[x];
            int pr = qRed(pixel);
            int pg = qGreen(pixel);
            int pb = qBlue(pixel);

            int diff = qMax(qAbs(pr - r0), qMax(qAbs(pg - g0), qAbs(pb - b0)));
            if (diff < 12) {
                scan[x] = qRgba(pr, pg, pb, 0);
            } else if (diff < 28) {
                int a = static_cast<int>(((diff - 12) / 16.0f) * 255.0f);
                scan[x] = qRgba(pr, pg, pb, a);
            }
        }
    }

    return img;
}

QPixmap AssetManager::loadTexture(const QString& relPath) {
    if (relPath.trimmed().isEmpty()) return QPixmap();

    QString clean = relPath;
    clean.replace('\\', '/');
    while (clean.startsWith('/')) clean.remove(0, 1);

    QMutexLocker locker(&m_mutex);

    if (m_textureCache.contains(clean)) {
        return m_textureCache.value(clean);
    }

    QString resolved = resolvePath(clean);
    if (resolved.isEmpty()) {
        m_textureCache[clean] = QPixmap();
        return QPixmap();
    }

    QFileInfo fi(resolved);
    QString ext = fi.suffix().toLower();
    QImage img;

    if (ext == "dds") {
        img = loadDDS(resolved);
    } else if (ext == "tga") {
        img = loadTGA(resolved);
        if (img.isNull()) img.load(resolved);
    } else {
        img.load(resolved);
    }

    if (img.isNull()) {
        m_textureCache[clean] = QPixmap();
        return QPixmap();
    }

    QPixmap px = QPixmap::fromImage(img);
    m_textureCache[clean] = px;
    return px;
}

QPixmap AssetManager::loadIcon(const QString& relPath) {
    if (relPath.trimmed().isEmpty()) return QPixmap();

    QString clean = relPath;
    clean.replace('\\', '/');
    while (clean.startsWith('/')) clean.remove(0, 1);

    QMutexLocker locker(&m_mutex);

    if (m_iconCache.contains(clean)) {
        return m_iconCache.value(clean);
    }

    QString resolved = resolvePath(clean);
    if (resolved.isEmpty()) {
        m_iconCache[clean] = QPixmap();
        return QPixmap();
    }

    QFileInfo fi(resolved);
    QString ext = fi.suffix().toLower();
    QImage img;

    if (ext == "dds") {
        img = loadDDS(resolved);
    } else if (ext == "tga") {
        img = loadTGA(resolved);
        if (img.isNull()) img.load(resolved);
    } else {
        img.load(resolved);
    }

    if (img.isNull()) {
        m_iconCache[clean] = QPixmap();
        return QPixmap();
    }

    // Apply transparent chroma keying for BMP / opaque icons
    QImage transparentImg = makeTransparentChromaKey(img);

    QPixmap px = QPixmap::fromImage(transparentImg);
    if (!px.isNull()) {
        QPixmap iconPx = px.scaled(48, 48, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        m_iconCache[clean] = iconPx;
        return iconPx;
    }

    m_iconCache[clean] = QPixmap();
    return QPixmap();
}

// -------------------------------------------------------------
// Direct DDS Decoder (DXT1, DXT3, DXT5, BGRA8, RGBA8)
// -------------------------------------------------------------

#define FOURCC_DXT1 0x31545844
#define FOURCC_DXT3 0x33545844
#define FOURCC_DXT5 0x35545844

static void decodeDXT1Block(const uint8_t* block, uint32_t* out, int width, int bx, int by, int imgW, int imgH) {
    uint16_t c0 = block[0] | (block[1] << 8);
    uint16_t c1 = block[2] | (block[3] << 8);

    uint8_t r0 = ((c0 >> 11) & 0x1F) * 255 / 31;
    uint8_t g0 = ((c0 >> 5) & 0x3F) * 255 / 63;
    uint8_t b0 = (c0 & 0x1F) * 255 / 31;

    uint8_t r1 = ((c1 >> 11) & 0x1F) * 255 / 31;
    uint8_t g1 = ((c1 >> 5) & 0x3F) * 255 / 63;
    uint8_t b1 = (c1 & 0x1F) * 255 / 31;

    uint32_t colors[4];
    colors[0] = 0xFF000000 | (r0 << 16) | (g0 << 8) | b0;
    colors[1] = 0xFF000000 | (r1 << 16) | (g1 << 8) | b1;

    if (c0 > c1) {
        colors[2] = 0xFF000000 | (((2 * r0 + r1) / 3) << 16) | (((2 * g0 + g1) / 3) << 8) | ((2 * b0 + b1) / 3);
        colors[3] = 0xFF000000 | (((r0 + 2 * r1) / 3) << 16) | (((g0 + 2 * g1) / 3) << 8) | ((b0 + 2 * b1) / 3);
    } else {
        colors[2] = 0xFF000000 | (((r0 + r1) / 2) << 16) | (((g0 + g1) / 2) << 8) | ((b0 + b1) / 2);
        colors[3] = 0x00000000; // Transparent
    }

    uint32_t code = block[4] | (block[5] << 8) | (block[6] << 16) | (block[7] << 24);

    for (int y = 0; y < 4; ++y) {
        for (int x = 0; x < 4; ++x) {
            int px = bx + x;
            int py = by + y;
            if (px < imgW && py < imgH) {
                int bit = 2 * (y * 4 + x);
                int idx = (code >> bit) & 0x3;
                out[py * width + px] = colors[idx];
            }
        }
    }
}

static void decodeDXT5Block(const uint8_t* block, uint32_t* out, int width, int bx, int by, int imgW, int imgH) {
    uint8_t a0 = block[0];
    uint8_t a1 = block[1];
    uint8_t alphas[8];
    alphas[0] = a0;
    alphas[1] = a1;
    if (a0 > a1) {
        for (int i = 1; i <= 6; ++i) {
            alphas[i + 1] = ((7 - i) * a0 + i * a1) / 7;
        }
    } else {
        for (int i = 1; i <= 4; ++i) {
            alphas[i + 1] = ((5 - i) * a0 + i * a1) / 5;
        }
        alphas[6] = 0;
        alphas[7] = 255;
    }

    uint64_t a_bits = 0;
    for (int i = 0; i < 6; ++i) {
        a_bits |= (static_cast<uint64_t>(block[2 + i]) << (i * 8));
    }

    const uint8_t* col_block = block + 8;
    uint16_t c0 = col_block[0] | (col_block[1] << 8);
    uint16_t c1 = col_block[2] | (col_block[3] << 8);

    uint8_t r0 = ((c0 >> 11) & 0x1F) * 255 / 31;
    uint8_t g0 = ((c0 >> 5) & 0x3F) * 255 / 63;
    uint8_t b0 = (c0 & 0x1F) * 255 / 31;

    uint8_t r1 = ((c1 >> 11) & 0x1F) * 255 / 31;
    uint8_t g1 = ((c1 >> 5) & 0x3F) * 255 / 63;
    uint8_t b1 = (c1 & 0x1F) * 255 / 31;

    uint32_t rgb[4];
    rgb[0] = (r0 << 16) | (g0 << 8) | b0;
    rgb[1] = (r1 << 16) | (g1 << 8) | b1;
    rgb[2] = (((2 * r0 + r1) / 3) << 16) | (((2 * g0 + g1) / 3) << 8) | ((2 * b0 + b1) / 3);
    rgb[3] = (((r0 + 2 * r1) / 3) << 16) | (((g0 + 2 * g1) / 3) << 8) | ((b0 + 2 * b1) / 3);

    uint32_t code = col_block[4] | (col_block[5] << 8) | (col_block[6] << 16) | (col_block[7] << 24);

    for (int y = 0; y < 4; ++y) {
        for (int x = 0; x < 4; ++x) {
            int px = bx + x;
            int py = by + y;
            if (px < imgW && py < imgH) {
                int pix_idx = y * 4 + x;
                int c_idx = (code >> (2 * pix_idx)) & 0x3;
                int a_idx = (a_bits >> (3 * pix_idx)) & 0x7;
                uint32_t alpha = alphas[a_idx];
                out[py * width + px] = (alpha << 24) | rgb[c_idx];
            }
        }
    }
}

QImage AssetManager::decodeDDSMemory(const uint8_t* data, size_t size) {
    if (size < 128) return QImage();
    if (data[0] != 'D' || data[1] != 'D' || data[2] != 'S' || data[3] != ' ') return QImage();

    const DDS_HEADER* hdr = reinterpret_cast<const DDS_HEADER*>(data + 4);
    int width = hdr->dwWidth;
    int height = hdr->dwHeight;
    if (width <= 0 || height <= 0 || width > 8192 || height > 8192) return QImage();

    QImage img(width, height, QImage::Format_ARGB32);
    uint32_t* pixels = reinterpret_cast<uint32_t*>(img.bits());
    const uint8_t* src = data + 128;
    size_t src_avail = size - 128;

    uint32_t fourCC = hdr->ddspf.dwFourCC;

    if (fourCC == FOURCC_DXT1) {
        int bw = (width + 3) / 4;
        int bh = (height + 3) / 4;
        if (src_avail < static_cast<size_t>(bw * bh * 8)) return QImage();
        for (int by = 0; by < bh; ++by) {
            for (int bx = 0; bx < bw; ++bx) {
                decodeDXT1Block(src, pixels, width, bx * 4, by * 4, width, height);
                src += 8;
            }
        }
        return img;
    } else if (fourCC == FOURCC_DXT5 || fourCC == FOURCC_DXT3) {
        int bw = (width + 3) / 4;
        int bh = (height + 3) / 4;
        if (src_avail < static_cast<size_t>(bw * bh * 16)) return QImage();
        for (int by = 0; by < bh; ++by) {
            for (int bx = 0; bx < bw; ++bx) {
                decodeDXT5Block(src, pixels, width, bx * 4, by * 4, width, height);
                src += 16;
            }
        }
        return img;
    } else if (hdr->ddspf.dwRGBBitCount == 32) {
        // Uncompressed BGRA / RGBA
        if (src_avail < static_cast<size_t>(width * height * 4)) return QImage();
        memcpy(pixels, src, width * height * 4);
        return img;
    }

    return QImage();
}

QImage AssetManager::loadDDS(const QString& fullPath) {
    QFile f(fullPath);
    if (!f.open(QIODevice::ReadOnly)) return QImage();
    QByteArray bytes = f.readAll();
    return decodeDDSMemory(reinterpret_cast<const uint8_t*>(bytes.constData()), bytes.size());
}

// -------------------------------------------------------------
// Direct TGA Decoder (Uncompressed & RLE 24/32-bit)
// -------------------------------------------------------------
QImage AssetManager::loadTGA(const QString& fullPath) {
    QFile f(fullPath);
    if (!f.open(QIODevice::ReadOnly)) return QImage();
    QByteArray data = f.readAll();
    if (data.size() < 18) return QImage();

    const uint8_t* ptr = reinterpret_cast<const uint8_t*>(data.constData());
    uint8_t idLen = ptr[0];
    uint8_t imgType = ptr[2];
    int width = ptr[12] | (ptr[13] << 8);
    int height = ptr[14] | (ptr[15] << 8);
    uint8_t bpp = ptr[16];
    uint8_t desc = ptr[17];

    if (width <= 0 || height <= 0 || width > 8192 || height > 8192) return QImage();
    if (bpp != 24 && bpp != 32) return QImage();

    bool flipV = !(desc & 0x20); // Standard TGA is bottom-up

    QImage img(width, height, QImage::Format_ARGB32);
    const uint8_t* src = ptr + 18 + idLen;
    size_t remaining = data.size() - (18 + idLen);

    if (imgType == 2) { // Uncompressed truecolor
        int bytesPerPixel = bpp / 8;
        if (remaining < static_cast<size_t>(width * height * bytesPerPixel)) return QImage();
        for (int y = 0; y < height; ++y) {
            int dstY = flipV ? (height - 1 - y) : y;
            uint32_t* line = reinterpret_cast<uint32_t*>(img.scanLine(dstY));
            for (int x = 0; x < width; ++x) {
                uint8_t b = *src++;
                uint8_t g = *src++;
                uint8_t r = *src++;
                uint8_t a = (bpp == 32) ? *src++ : 255;
                line[x] = (a << 24) | (r << 16) | (g << 8) | b;
            }
        }
        return img;
    } else if (imgType == 10) { // RLE truecolor
        int bytesPerPixel = bpp / 8;
        int totalPixels = width * height;
        QVector<uint32_t> buf(totalPixels, 0);
        int p = 0;
        while (p < totalPixels && remaining >= static_cast<size_t>(1 + bytesPerPixel)) {
            uint8_t header = *src++;
            remaining--;
            int count = (header & 0x7F) + 1;
            if (header & 0x80) { // RLE packet
                if (remaining < static_cast<size_t>(bytesPerPixel)) break;
                uint8_t b = *src++;
                uint8_t g = *src++;
                uint8_t r = *src++;
                uint8_t a = (bpp == 32) ? *src++ : 255;
                remaining -= bytesPerPixel;
                uint32_t color = (a << 24) | (r << 16) | (g << 8) | b;
                for (int i = 0; i < count && p < totalPixels; ++i) buf[p++] = color;
            } else { // Raw packet
                for (int i = 0; i < count && p < totalPixels; ++i) {
                    if (remaining < static_cast<size_t>(bytesPerPixel)) break;
                    uint8_t b = *src++;
                    uint8_t g = *src++;
                    uint8_t r = *src++;
                    uint8_t a = (bpp == 32) ? *src++ : 255;
                    remaining -= bytesPerPixel;
                    buf[p++] = (a << 24) | (r << 16) | (g << 8) | b;
                }
            }
        }
        for (int y = 0; y < height; ++y) {
            int dstY = flipV ? (height - 1 - y) : y;
            uint32_t* line = reinterpret_cast<uint32_t*>(img.scanLine(dstY));
            for (int x = 0; x < width; ++x) {
                line[x] = buf[y * width + x];
            }
        }
        return img;
    }

    return QImage();
}
