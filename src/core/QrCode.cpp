#include "core/QrCode.h"

#include <qrencode.h>

#include <memory>

namespace {

struct QrDeleter {
    void operator()(QRcode *q) const { QRcode_free(q); }
};

}

QImage QrCode::render(const QString &text, int minSidePixels)
{
    const QByteArray data = text.toUtf8();
    if (data.isEmpty())
        return {};

    // Medium error correction scans better off a screen; fall back to low only when the text needs the room.
    std::unique_ptr<QRcode, QrDeleter> code(QRcode_encodeData(int(data.size()), reinterpret_cast<const unsigned char *>(data.constData()), 0, QR_ECLEVEL_M));
    if (!code)
        code.reset(QRcode_encodeData(int(data.size()), reinterpret_cast<const unsigned char *>(data.constData()), 0, QR_ECLEVEL_L));
    if (!code)
        return {};

    constexpr int quiet = 4; // modules of white around the code, per the QR spec
    const int modules = code->width + 2 * quiet;
    const int scale = qMax(1, (minSidePixels + modules - 1) / modules);

    QImage image(modules * scale, modules * scale, QImage::Format_RGB32);
    image.fill(Qt::white);
    for (int y = 0; y < code->width; ++y) {
        for (int x = 0; x < code->width; ++x) {
            if (!(code->data[y * code->width + x] & 1))
                continue;
            for (int dy = 0; dy < scale; ++dy) {
                QRgb *row = reinterpret_cast<QRgb *>(image.scanLine((y + quiet) * scale + dy));
                for (int dx = 0; dx < scale; ++dx)
                    row[(x + quiet) * scale + dx] = qRgb(0, 0, 0);
            }
        }
    }
    return image;
}
