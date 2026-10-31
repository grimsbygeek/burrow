#pragma once

#include <QImage>
#include <QString>

namespace QrCode {

// Black-on-white QR code (with the required quiet zone), scaled up by a whole number of pixels per
// module so it stays sharp. Returns a null image if the text is too long for a QR code.
QImage render(const QString &text, int minSidePixels = 320);

}
