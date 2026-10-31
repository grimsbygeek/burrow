#pragma once

#include <QString>

// Fixed colors with white text: readable in both light and dark themes.
namespace Style {

inline const QString connectButton = QStringLiteral(
    "QPushButton { background: #2e7d32; color: white; border: none; border-radius: 4px; padding: 6px 14px; font-weight: bold; }"
    "QPushButton:hover { background: #388e3c; }"
    "QPushButton:pressed { background: #1b5e20; }"
    "QPushButton:disabled { background: #7a8a7b; color: #e8e8e8; }");

inline const QString disconnectButton = QStringLiteral(
    "QPushButton { background: #c62828; color: white; border: none; border-radius: 4px; padding: 8px 20px; font-weight: bold; }"
    "QPushButton:hover { background: #d32f2f; }"
    "QPushButton:pressed { background: #8e0000; }"
    "QPushButton:disabled { background: #8f7a7a; color: #e8e8e8; }");

// Status dots: bright enough to read on both light and dark backgrounds.
inline const QString dotGreen = QStringLiteral("#2fb344");
inline const QString dotRed = QStringLiteral("#e03131");
inline const QString dotAmber = QStringLiteral("#f59f00");
inline const QString dotMuted = QStringLiteral("#8a8f98");

}
