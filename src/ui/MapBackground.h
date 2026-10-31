#pragma once

#include <QPainterPath>
#include <QWidget>

// A container that paints a faint world map behind its children. Purely decorative.
class MapBackground : public QWidget {
    Q_OBJECT
public:
    explicit MapBackground(QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QPainterPath m_land; // in degrees: x = longitude, y = -latitude
};
