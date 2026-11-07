#pragma once
#include <QIcon>
#include <QPixmap>
#include <QPainter>
#include <QFont>
inline QIcon layoutBadgeIcon(const QString &code, bool active) {
    QPixmap image(64,64); image.fill(Qt::transparent);
    QPainter painter(&image); painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen); painter.setBrush(QColor(active ? "#16834e" : "#ba3434"));
    painter.drawRoundedRect(QRectF(1,1,62,62),10,10);
    QFont font("DejaVu Sans"); font.setBold(true); font.setPixelSize(29);
    painter.setFont(font); painter.setPen(Qt::white);
    painter.drawText(QRect(0,0,64,64),Qt::AlignCenter,code);
    return QIcon(image);
}
