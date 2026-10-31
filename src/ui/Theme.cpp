#include "ui/Theme.h"

#include <QApplication>
#include <QPalette>

namespace {

// QStyleHints::setColorScheme() is a no-op under some platform themes (verified with KDE's Breeze on
// Wayland), so light and dark are applied as explicit application palettes instead.
QPalette makePalette(bool dark)
{
    struct Colors {
        const char *window, *text, *base, *altBase, *button, *tipBase, *highlight, *link, *disabled;
    };
    const Colors c = dark ? Colors{"#2b2d31", "#eaeaea", "#1e1f22", "#26282c", "#383a40", "#383a40", "#3d7ec7", "#6cb2ff", "#8a8d93"}
                          : Colors{"#efefef", "#1b1b1b", "#ffffff", "#f6f6f6", "#e4e4e4", "#ffffdc", "#308cc6", "#0b5cad", "#8a8a8a"};

    QPalette p;
    p.setColor(QPalette::Window, c.window);
    p.setColor(QPalette::WindowText, c.text);
    p.setColor(QPalette::Base, c.base);
    p.setColor(QPalette::AlternateBase, c.altBase);
    p.setColor(QPalette::Text, c.text);
    p.setColor(QPalette::Button, c.button);
    p.setColor(QPalette::ButtonText, c.text);
    p.setColor(QPalette::ToolTipBase, c.tipBase);
    p.setColor(QPalette::ToolTipText, c.text);
    p.setColor(QPalette::PlaceholderText, c.disabled);
    p.setColor(QPalette::Highlight, c.highlight);
    p.setColor(QPalette::HighlightedText, Qt::white);
    p.setColor(QPalette::Link, c.link);
    for (const QPalette::ColorRole role : {QPalette::WindowText, QPalette::Text, QPalette::ButtonText})
        p.setColor(QPalette::Disabled, role, c.disabled);
    return p;
}

}

void Theme::apply(Preferences::Theme theme)
{
    switch (theme) {
    case Preferences::Theme::Light:
        QApplication::setPalette(makePalette(false));
        break;
    case Preferences::Theme::Dark:
        QApplication::setPalette(makePalette(true));
        break;
    case Preferences::Theme::System:
        QApplication::setPalette(QPalette()); // back to the desktop's own palette
        break;
    }
}
