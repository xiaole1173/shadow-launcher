// 诊断：Qt 6.8.3 的 QImage 能否解码 webp？QImageReader 支持哪些格式？
#include <QCoreApplication>
#include <QImage>
#include <QImageReader>
#include <QFile>
#include <cstdio>
#include <webp/decode.h>

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);

    // 1. 支持的格式
    QStringList fmts;
    for (const QByteArray& f : QImageReader::supportedImageFormats())
        fmts << QString::fromLatin1(f);
    std::printf("supportedImageFormats: %s\n", fmts.join(",").toUtf8().constData());

    // 2. 直接解码用户磁盘上的 webp 内容文件
    const QString p = QString::fromLocal8Bit(argv[1]);
    QImage img(p);
    std::printf("load(%s): %s, %dx%d\n", p.toUtf8().constData(),
                img.isNull() ? "FAIL" : "OK", img.width(), img.height());

    // 3. loadFromData 测试
    QFile f(p);
    if (f.open(QIODevice::ReadOnly)) {
        const QByteArray data = f.readAll();
        QImage img2;
        img2.loadFromData(data);
        std::printf("loadFromData: %s, %dx%d\n", img2.isNull() ? "FAIL" : "OK",
                    img2.width(), img2.height());
        if (!img2.isNull()) {
            const QString out = p + ".converted.png";
            bool ok = img2.save(out, "PNG");
            std::printf("save PNG: %s -> %s\n", ok ? "OK" : "FAIL", out.toUtf8().constData());
        }
    }

    // 4. QImageReader::canRead（引擎清理逻辑用）
    QImageReader reader(p);
    std::printf("QImageReader::canRead: %s\n", reader.canRead() ? "true" : "false");

    // 5. libwebp 解码（引擎 webp 支持验证）
    QFile f2(p);
    if (f2.open(QIODevice::ReadOnly)) {
        const QByteArray data = f2.readAll();
        int w = 0, h = 0;
        uint8_t* rgba = WebPDecodeRGBA(
            reinterpret_cast<const uint8_t*>(data.constData()), data.size(), &w, &h);
        if (rgba && w > 0 && h > 0) {
            QImage wi(rgba, w, h, QImage::Format_RGBA8888,
                      [](void* ptr) { WebPFree(ptr); }, rgba);
            const QString out = p + ".webp2png.png";
            bool ok = wi.save(out, "PNG");
            std::printf("libwebp decode: OK %dx%d, save PNG: %s\n", w, h, ok ? "OK" : "FAIL");
        } else {
            WebPFree(rgba);
            std::printf("libwebp decode: FAIL (not webp or corrupt)\n");
        }
    }
    return 0;
}
