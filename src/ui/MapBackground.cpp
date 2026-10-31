#include "ui/MapBackground.h"

#include <QFile>
#include <QPainter>

namespace {

// Bundled by tools/gen_worldmap.py: one ring per line, tenths of a degree.
QPainterPath loadLand()
{
    QPainterPath path;
    QFile f(":/worldmap.txt");
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return path;

    while (!f.atEnd()) {
        const QList<QByteArray> parts = f.readLine().simplified().split(' ');
        QPolygonF ring;
        for (qsizetype i = 0; i + 1 < parts.size(); i += 2)
            ring << QPointF(parts[i].toInt() / 10.0, -parts[i + 1].toInt() / 10.0); // north is up
        if (ring.size() >= 3)
            path.addPolygon(ring);
        path.closeSubpath();
    }
    return path;
}

}

MapBackground::MapBackground(QWidget *parent)
    : QWidget(parent)
    , m_land(loadLand())
{
}

void MapBackground::paintEvent(QPaintEvent *)
{
    if (m_land.isEmpty())
        return;

    const QRectF bounds = m_land.boundingRect();
    const qreal scale = qMin(width() / bounds.width(), height() / bounds.height());

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.translate(width() / 2.0, height() / 2.0);
    p.scale(scale, scale);
    p.translate(-bounds.center());

    QColor land = palette().color(QPalette::WindowText);
    land.setAlpha(26); // faint enough to keep the text on top readable in light and dark themes
    p.setPen(Qt::NoPen);
    p.setBrush(land);
    p.drawPath(m_land);
}
