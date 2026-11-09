/*
MIT License

Copyright (c) 2017 Sacha Schutz

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

/**
 * @file qfonticon.cpp
 * @brief Implementation of QFontIcon and QFontIconEngine.
 */

#include "qfonticon.h"

#include <QDebug>
#include <QFile>
#include <QFont>
#include <QFontDatabase>
#include <QRawFont>

QFontIcon *QFontIcon::mInstance = Q_NULLPTR;

namespace {
/**
 * @brief A font of a family and style.
 * @param family    The family.
 * @param styleName The style within it; empty: the family's default.
 * @return The font.
 */
QFont makeFont(const QString &family, const QString &styleName)
{
    QFont font(family);
    if (!styleName.isEmpty()) {
        // Picks e.g. Font Awesome 5 Free Solid (weight 900) rather than
        // Regular (400), which share the family name
        font.setStyleName(styleName);
    }
    return font;
}
} // namespace

//=======================================================================================================

bool QFontIcon::addFont(const QString &filename)
{
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "QFontIcon: cannot open font file" << filename;
        return false;
    }
    const QByteArray data = file.readAll();

    const int id = QFontDatabase::addApplicationFontFromData(data);
    const QStringList loaded = (id == -1) ? QStringList() : QFontDatabase::applicationFontFamilies(id);
    if (loaded.isEmpty()) {
        qWarning() << "QFontIcon: cannot load font" << filename;
        return false;
    }

    // The font's style ("Solid", "Regular", ...): one family can have
    // several fonts, as Font Awesome 5 Free
    const QString styleName = QRawFont(data, 16).styleName();
    instance()->registerFont(loaded.first(), styleName);
    return true;
}

QFontIcon *QFontIcon::instance()
{
    if (!mInstance) {
        mInstance = new QFontIcon;
    }
    return mInstance;
}

QIcon QFontIcon::icon(const QChar &code, const QColor &baseColor, double angle, const QString &family)
{
    if (instance()->families().isEmpty()) {
        qWarning() << Q_FUNC_INFO << "No font family installed";
        return QIcon();
    }

    const Font font = instance()->fontFor(code, family);

    QFontIconEngine *engine = new QFontIconEngine;
    engine->setFontFamily(font.family);
    engine->setFontStyle(font.styleName);
    engine->setLetter(code);
    engine->setBaseColor(baseColor);
    engine->setRotation(angle);
    return QIcon(engine);
}

QIcon QFontIcon::icon(uint code, const QColor &baseColor, double angle, const QString &family)
{
    return icon(QChar(static_cast<char16_t>(code)), baseColor, angle, family);
}

const QStringList &QFontIcon::families() const
{
    return mfamilies;
}

void QFontIcon::addFamily(const QString &family)
{
    registerFont(family, QString());
}

void QFontIcon::registerFont(const QString &family, const QString &styleName)
{
    mfamilies.append(family);
    mFonts.append(Font{family, styleName});
}

QFontIcon::Font QFontIcon::fontFor(const QChar &code, const QString &family) const
{
    // The font asked for: by family, or by "family style"; else the first added
    Font wanted = mFonts.first();
    if (!family.isEmpty()) {
        bool found = false;
        for (const Font &font : mFonts) {
            const QString fullName = font.styleName.isEmpty() ? font.family
                                                              : font.family + QLatin1Char(' ') + font.styleName;
            if (family.compare(fullName, Qt::CaseInsensitive) == 0) {
                wanted = font;
                found = true;
                break;
            }
        }
        if (!found) {
            for (const Font &font : mFonts) {
                if (family.compare(font.family, Qt::CaseInsensitive) == 0) {
                    wanted = font;
                    found = true;
                    break;
                }
            }
        }
        if (!found) {
            // Not an added font (e.g. a system font): use it as it is
            return Font{family, QString()};
        }
    }

    if (hasGlyph(wanted, code)) {
        return wanted;
    }
    // Not in that font: the first other added font that has it (e.g. a
    // Brands icon, or a Solid-only icon asked for in Regular)
    for (const Font &font : mFonts) {
        if (hasGlyph(font, code)) {
            return font;
        }
    }
    return wanted;
}

bool QFontIcon::hasGlyph(const Font &font, const QChar &code)
{
    // QRawFont: this font file only, without Qt's fallback to other fonts
    const QRawFont raw = QRawFont::fromFont(makeFont(font.family, font.styleName));
    return raw.isValid() && raw.familyName() == font.family && raw.supportsCharacter(code);
}

QFontIcon::QFontIcon(QObject *parent)
    : QObject(parent)
{
}

QFontIcon::~QFontIcon()
{
}

//=======================================================================================================

QFontIconEngine::QFontIconEngine()
    : QIconEngine()
{
}

QFontIconEngine::~QFontIconEngine()
{
}

void QFontIconEngine::paint(QPainter *painter, const QRect &rect, QIcon::Mode mode, QIcon::State state)
{
    Q_UNUSED(state);
    QFont font = makeFont(mFontFamily, mFontStyle);
    const int drawSize = qRound(rect.height() * 0.8);
    font.setPixelSize(qMax(1, drawSize));

    // The palette's colours, read now, so the icon follows theme changes
    const QPalette palette = QApplication::palette("QWidget");
    QColor penColor;
    if (!mBaseColor.isValid()) {
        penColor = palette.color(QPalette::Normal, QPalette::ButtonText);
    } else {
        penColor = mBaseColor;
    }

    if (mode == QIcon::Disabled) {
        penColor = palette.color(QPalette::Disabled, QPalette::ButtonText);
    }

    if (mode == QIcon::Selected) {
        penColor = palette.color(QPalette::Active, QPalette::ButtonText);
    }

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setRenderHint(QPainter::TextAntialiasing);
    painter->setPen(QPen(penColor));
    painter->setFont(font);
    if (!qFuzzyIsNull(mAngle)) {
        // Around the centre of the icon
        const QPointF center = QRectF(rect).center();
        painter->translate(center);
        painter->rotate(mAngle);
        painter->translate(-center);
    }
    painter->drawText(rect, Qt::AlignCenter | Qt::AlignVCenter, mLetter);
    painter->restore();
}

QPixmap QFontIconEngine::pixmap(const QSize &size, QIcon::Mode mode, QIcon::State state)
{
    QPixmap pix(size);
    pix.fill(Qt::transparent);

    QPainter painter(&pix);
    paint(&painter, QRect(QPoint(0, 0), size), mode, state);
    return pix;
}

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
QPixmap QFontIconEngine::scaledPixmap(const QSize &size, QIcon::Mode mode, QIcon::State state, qreal scale)
{
    // Drawn with all the screen's pixels and marked with the scale, so Qt
    // shows it at the logical size, sharp on a high-DPI screen.
    // Qt 6.8 and newer pass the size in device-independent pixels; older
    // Qt 6 passes it already multiplied by the scale.
    const qreal ratio = scale > 0 ? scale : 1.0;
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
    const QSize logicalSize = size;
#else
    const QSize logicalSize = (QSizeF(size) / ratio).toSize();
#endif
    QPixmap pix((QSizeF(logicalSize) * ratio).toSize());
    pix.setDevicePixelRatio(ratio);
    pix.fill(Qt::transparent);

    QPainter painter(&pix);
    paint(&painter, QRect(QPoint(0, 0), logicalSize), mode, state);
    return pix;
}
#endif

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
bool QFontIconEngine::isNull()
{
    return mFontFamily.isEmpty() || mLetter.isNull();
}
#endif

QString QFontIconEngine::key() const
{
    return QStringLiteral("QFontIconEngine");
}

void QFontIconEngine::setFontFamily(const QString &family)
{
    mFontFamily = family;
}

void QFontIconEngine::setFontStyle(const QString &styleName)
{
    mFontStyle = styleName;
}

void QFontIconEngine::setLetter(const QChar &letter)
{
    mLetter = letter;
}

void QFontIconEngine::setBaseColor(const QColor &baseColor)
{
    mBaseColor = baseColor;
}

void QFontIconEngine::setRotation(double angle)
{
    mAngle = angle;
}

QIconEngine *QFontIconEngine::clone() const
{
    // Everything, the letter included (a copied icon was empty without it)
    QFontIconEngine *engine = new QFontIconEngine;
    engine->setFontFamily(mFontFamily);
    engine->setFontStyle(mFontStyle);
    engine->setLetter(mLetter);
    engine->setBaseColor(mBaseColor);
    engine->setRotation(mAngle);
    return engine;
}
