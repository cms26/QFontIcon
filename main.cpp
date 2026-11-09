/**
 * @file main.cpp
 * @brief QFontIcon example: Font Awesome 5 icons on buttons, in every style.
 */

#include <QApplication>
#include <QtWidgets>

#include "qfonticon.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    // Font Awesome 5: the first font added is the default (Solid)
    QFontIcon::addFont(":/fonts/fa-solid-900.ttf");
    QFontIcon::addFont(":/fonts/fa-regular-400.ttf");
    QFontIcon::addFont(":/fonts/fa-brands-400.ttf");

    QWidget window;
    window.setWindowTitle("QFontIcon: Font Awesome 5");
    auto *layout = new QGridLayout(&window);

    struct Example
    {
        ushort code;
        const char *text;
        double angle;
        const char *family;
    };
    const Example examples[] = {
        {0xf015, "home (solid)", 0.0, ""},
        {0xf15b, "file (solid)", 0.0, ""},
        {0xf15b, "file (regular)", 0.0, "Font Awesome 5 Free Regular"},
        {0xf08d, "thumbtack", 0.0, ""},
        {0xf08d, "thumbtack, 90 degrees", 90.0, ""},
        {0xf09b, "github (brands, found by itself)", 0.0, ""},
        {0xf1f8, "trash, fixed colour", 0.0, ""},
        {0xf2d0, "window maximize", 0.0, ""},
    };

    int row = 0;
    for (const Example &example : examples) {
        const QColor color = (example.code == 0xf1f8) ? QColor(220, 60, 60) : QColor();
        auto *button = new QPushButton(QFontIcon::icon(QChar(example.code), color, example.angle, example.family),
                                       QString::fromLatin1(example.text));
        button->setIconSize(QSize(24, 24));
        auto *disabled = new QPushButton(button->icon(), QStringLiteral("disabled"));
        disabled->setIconSize(QSize(24, 24));
        disabled->setEnabled(false);
        layout->addWidget(button, row, 0);
        layout->addWidget(disabled, row, 1);
        ++row;
    }

    window.show();
    return a.exec();
}
