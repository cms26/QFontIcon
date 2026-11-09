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
#include <cstdlib>
#include <QFile>
#include <QFont>
#include <QFontDatabase>
#include <QHash>
#include <QPainterPath>
#include <QRawFont>
#include <QSet>
#include <QTransform>

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

/**
 * @brief The compiled-in icons of the application (qfonticon_embed_icons()).
 *
 * Filled before main() by the generated code's static initializers, so it
 * holds no Qt objects that need the application.
 */
struct EmbeddedIcons
{
    QList<const QFontIconEmbeddedFont *> fonts;   ///< One per icon set ("solid", ...), first registered first.
    QHash<quint64, const QFontIconEmbeddedGlyph *> glyphs;   ///< By (font index << 32 | code).
    QHash<const QFontIconEmbeddedGlyph *, int> glyphFont;    ///< Each icon's index into fonts.
};

/**
 * @brief The registry of compiled-in icons.
 * @return It; created on first use.
 */
EmbeddedIcons &embeddedIcons()
{
    static EmbeddedIcons icons;
    return icons;
}

/**
 * @brief The lookup key of a compiled-in icon.
 * @param font The font's index in EmbeddedIcons::fonts.
 * @param code The character's code.
 * @return The key.
 */
quint64 glyphKey(int font, uint code)
{
    return (static_cast<quint64>(font) << 32) | code;
}

/**
 * @brief Reads SVG path data (as written by tools/generate_icons.py: M, L,
 *        H, V, Q, C, Z, absolute or relative) into a painter path.
 * @param data The path data.
 * @return The path, in the data's units.
 */
QPainterPath parseSvgPath(const char *data)
{
    QPainterPath path;
    path.setFillRule(Qt::WindingFill);   // fonts' outlines use non-zero winding
    const char *p = data;
    char command = 0;
    QPointF current;
    QPointF start;

    auto skipSeparators = [&p]() {
        while (*p == ' ' || *p == ',' || *p == '\t' || *p == '\n') {
            ++p;
        }
    };
    auto readNumber = [&p, &skipSeparators]() -> qreal {
        skipSeparators();
        char *end = nullptr;
        const double value = std::strtod(p, &end);
        p = end;
        return value;
    };
    auto atNumber = [&p, &skipSeparators]() {
        skipSeparators();
        return (*p >= '0' && *p <= '9') || *p == '-' || *p == '+' || *p == '.';
    };

    while (true) {
        skipSeparators();
        if (*p == 0) {
            break;
        }
        if (!atNumber()) {
            command = *p++;
        }
        const bool relative = (command >= 'a' && command <= 'z');
        const QPointF base = relative ? current : QPointF();
        switch (command) {
        case 'M':
        case 'm': {
            const qreal x = readNumber();
            const qreal y = readNumber();
            current = base + QPointF(x, y);
            start = current;
            path.moveTo(current);
            command = relative ? 'l' : 'L';   // more pairs: lines
            break;
        }
        case 'L':
        case 'l': {
            const qreal x = readNumber();
            const qreal y = readNumber();
            current = base + QPointF(x, y);
            path.lineTo(current);
            break;
        }
        case 'H':
        case 'h': {
            const qreal x = readNumber();
            current.setX(relative ? current.x() + x : x);
            path.lineTo(current);
            break;
        }
        case 'V':
        case 'v': {
            const qreal y = readNumber();
            current.setY(relative ? current.y() + y : y);
            path.lineTo(current);
            break;
        }
        case 'Q':
        case 'q': {
            const qreal cx = readNumber();
            const qreal cy = readNumber();
            const qreal x = readNumber();
            const qreal y = readNumber();
            path.quadTo(base + QPointF(cx, cy), base + QPointF(x, y));
            current = base + QPointF(x, y);
            break;
        }
        case 'C':
        case 'c': {
            const qreal c1x = readNumber();
            const qreal c1y = readNumber();
            const qreal c2x = readNumber();
            const qreal c2y = readNumber();
            const qreal x = readNumber();
            const qreal y = readNumber();
            path.cubicTo(base + QPointF(c1x, c1y), base + QPointF(c2x, c2y), base + QPointF(x, y));
            current = base + QPointF(x, y);
            break;
        }
        case 'Z':
        case 'z':
            path.closeSubpath();
            current = start;
            break;
        default:
            // Unknown command: stop rather than draw garbage
            qWarning("QFontIcon: unsupported path command '%c'", command);
            return path;
        }
    }
    return path;
}

/**
 * @brief A compiled-in icon's outline, read once and kept.
 * @param glyph The icon.
 * @return Its outline, in font units, y up.
 */
const QPainterPath &embeddedPath(const QFontIconEmbeddedGlyph *glyph)
{
    static QHash<const QFontIconEmbeddedGlyph *, QPainterPath> cache;
    auto it = cache.find(glyph);
    if (it == cache.end()) {
        it = cache.insert(glyph, parseSvgPath(glyph->path));
    }
    return it.value();
}

/**
 * @brief Finds a compiled-in icon.
 * @param code   The character.
 * @param family The font asked for: an icon set ("regular"), a family and
 *               style ("Font Awesome 5 Free Regular") or a family; empty: the first set.
 * @return The icon, or @c nullptr if it wasn't compiled in.
 */
const QFontIconEmbeddedGlyph *findEmbedded(uint code, const QString &family)
{
    const EmbeddedIcons &icons = embeddedIcons();
    if (icons.fonts.isEmpty()) {
        return nullptr;
    }
    // The set asked for, then the others in order
    int wanted = 0;
    if (!family.isEmpty()) {
        for (int i = 0; i < icons.fonts.size(); ++i) {
            const QFontIconEmbeddedFont *font = icons.fonts.at(i);
            const QString full = QString::fromUtf8(font->family) + QLatin1Char(' ') + QString::fromUtf8(font->fontStyle);
            if (family.compare(QString::fromUtf8(font->style), Qt::CaseInsensitive) == 0
                || family.compare(full, Qt::CaseInsensitive) == 0) {
                wanted = i;
                break;
            }
            if (family.compare(QString::fromUtf8(font->family), Qt::CaseInsensitive) == 0 && wanted == 0) {
                wanted = i;
            }
        }
    }
    if (const QFontIconEmbeddedGlyph *glyph = icons.glyphs.value(glyphKey(wanted, code))) {
        return glyph;
    }
    for (int i = 0; i < icons.fonts.size(); ++i) {
        if (const QFontIconEmbeddedGlyph *glyph = icons.glyphs.value(glyphKey(i, code))) {
            return glyph;
        }
    }
    return nullptr;
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
    QFontIconEngine *engine = nullptr;

    // Compiled in (qfonticon_embed_icons()): no font needed
    if (const QFontIconEmbeddedGlyph *glyph = findEmbedded(code.unicode(), family)) {
        const EmbeddedIcons &icons = embeddedIcons();
        engine = new QFontIconEngine;
        engine->setEmbeddedGlyph(glyph, icons.fonts.at(icons.glyphFont.value(glyph)));
        engine->setFontFamily(QString::fromUtf8(icons.fonts.at(icons.glyphFont.value(glyph))->family));
    } else if (!instance()->families().isEmpty()) {
        // From a font added with addFont()
        const Font font = instance()->fontFor(code, family);
        engine = new QFontIconEngine;
        engine->setFontFamily(font.family);
        engine->setFontStyle(font.styleName);
    } else {
        if (embeddedIcons().fonts.isEmpty()) {
            qWarning() << Q_FUNC_INFO << "No font family installed";
        } else {
            // Once per code: say which entry the icon list needs
            static QSet<uint> reported;
            if (!reported.contains(code.unicode())) {
                reported.insert(code.unicode());
                qWarning("QFontIcon: icon %04x is not compiled in: add \"%04x\" to the qfonticon_embed_icons() list",
                         code.unicode(), code.unicode());
            }
        }
        return QIcon();
    }

    engine->setLetter(code);
    engine->setBaseColor(baseColor);
    engine->setRotation(angle);
    return QIcon(engine);
}

QIcon QFontIcon::icon(uint code, const QColor &baseColor, double angle, const QString &family)
{
    return icon(QChar(static_cast<char16_t>(code)), baseColor, angle, family);
}

bool QFontIcon::addEmbeddedIcons(const QFontIconEmbeddedFont *fonts, int fontCount,
                                 const QFontIconEmbeddedGlyph *glyphs, int glyphCount)
{
    EmbeddedIcons &icons = embeddedIcons();
    // This file's fonts -> the registry's (icon sets with the same name are one set)
    QList<int> index;
    for (int i = 0; i < fontCount; ++i) {
        int found = -1;
        for (int j = 0; j < icons.fonts.size(); ++j) {
            if (qstrcmp(icons.fonts.at(j)->style, fonts[i].style) == 0) {
                found = j;
                break;
            }
        }
        if (found < 0) {
            icons.fonts.append(&fonts[i]);
            found = icons.fonts.size() - 1;
        }
        index.append(found);
    }
    for (int i = 0; i < glyphCount; ++i) {
        const QFontIconEmbeddedGlyph *glyph = &glyphs[i];
        if (glyph->font < 0 || glyph->font >= fontCount) {
            continue;
        }
        const int font = index.at(glyph->font);
        icons.glyphs.insert(glyphKey(font, glyph->code), glyph);
        icons.glyphFont.insert(glyph, font);
    }
    return true;
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
    if (mGlyph && mGlyphFont) {
        paintEmbedded(painter, rect, penColor);
    } else {
        painter->drawText(rect, Qt::AlignCenter | Qt::AlignVCenter, mLetter);
    }
    painter->restore();
}

void QFontIconEngine::paintEmbedded(QPainter *painter, const QRect &rect, const QColor &color) const
{
    // Placed as drawText() places the font's character: the same size (80 %
    // of the height), centred on its advance width and on the font's line
    // (ascent + descent)
    const qreal pixelSize = qMax(1, qRound(rect.height() * 0.8));
    const qreal scale = pixelSize / mGlyphFont->unitsPerEm;
    const qreal width = mGlyph->advance * scale;
    const qreal lineHeight = (mGlyphFont->ascent - mGlyphFont->descent) * scale;
    const qreal x = rect.x() + (rect.width() - width) / 2.0;
    const qreal baseline = rect.y() + (rect.height() - lineHeight) / 2.0 + mGlyphFont->ascent * scale;

    // Font units, y up -> pixels, y down
    const QTransform toRect(scale, 0, 0, -scale, x, baseline);
    painter->setPen(Qt::NoPen);
    painter->setBrush(color);
    painter->drawPath(toRect.map(embeddedPath(mGlyph)));
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
    if (mGlyph) {
        return false;
    }
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

void QFontIconEngine::setEmbeddedGlyph(const QFontIconEmbeddedGlyph *glyph, const QFontIconEmbeddedFont *font)
{
    mGlyph = glyph;
    mGlyphFont = font;
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
    engine->setEmbeddedGlyph(mGlyph, mGlyphFont);
    return engine;
}
