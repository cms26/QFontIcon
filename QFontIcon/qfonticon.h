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
 * @file qfonticon.h
 * @brief QFontIcon: QIcons drawn from an icon font such as Font Awesome.
 */

#ifndef QFONTICON_H
#define QFONTICON_H

#include <QApplication>
#include <QColor>
#include <QIcon>
#include <QIconEngine>
#include <QList>
#include <QObject>
#include <QPainter>
#include <QPalette>
#include <QString>
#include <QStringList>

class QFontIcon;
class QFontIconEngine;

/** @brief Shorthand for QFontIcon::icon(code). */
#define FIcon(code) QFontIcon::icon(code)

/**
 * @brief Draws one character of an icon font as an icon, at any size.
 *
 * Made by QFontIcon::icon(); a QIcon owns it. Sharp on high-DPI screens (it
 * draws at the screen's pixel ratio), in the palette's colours unless a base
 * colour is set, and greyed out for disabled widgets.
 */
class QFontIconEngine : public QIconEngine
{
public:
    /** @brief Creates an engine that draws nothing until a font and letter are set. */
    QFontIconEngine();

    /** @brief Destroys the engine. */
    ~QFontIconEngine();

    /**
     * @brief Draws the icon.
     * @param painter The painter to draw with.
     * @param rect    Where to draw; the character fills 80 % of its height.
     * @param mode    Normal, Disabled (greyed out), Active or Selected.
     * @param state   On or Off (not used).
     */
    virtual void paint(QPainter *painter, const QRect &rect, QIcon::Mode mode, QIcon::State state) Q_DECL_OVERRIDE;

    /**
     * @brief The icon as a pixmap, at a device pixel ratio of 1.
     * @param size  The size in pixels.
     * @param mode  The mode, as for paint().
     * @param state The state, as for paint().
     * @return The pixmap, transparent around the character.
     */
    virtual QPixmap pixmap(const QSize &size, QIcon::Mode mode, QIcon::State state) Q_DECL_OVERRIDE;

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    /**
     * @brief The icon as a pixmap for a screen's pixel ratio (Qt 6), so it
     *        stays sharp at 125 %, 150 %, 200 % ... scaling.
     * @param size  The size: in device-independent pixels from Qt 6.8, in
     *              device pixels (already multiplied by @p scale) before.
     * @param mode  The mode, as for paint().
     * @param state The state, as for paint().
     * @param scale The device pixel ratio.
     * @return A pixmap with all the device pixels, and that device pixel ratio.
     */
    QPixmap scaledPixmap(const QSize &size, QIcon::Mode mode, QIcon::State state, qreal scale) override;
#endif

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    /**
     * @brief Whether the engine would draw nothing (no font or no letter).
     * @return @c true if the icon is empty.
     */
    bool isNull() override;
#endif

    /**
     * @brief Identifies this engine type.
     * @return "QFontIconEngine".
     */
    QString key() const override;

    /**
     * @brief Sets the font family to draw with.
     * @param family A family added with QFontIcon::addFont().
     */
    void setFontFamily(const QString &family);

    /**
     * @brief Sets the font style within the family, e.g. "Solid" or "Regular".
     *
     * Needed where one family has several fonts, as Font Awesome 5 Free
     * (Solid and Regular). Empty: the family's default.
     * @param styleName The style name, as QFontDatabase::styles() lists it.
     */
    void setFontStyle(const QString &styleName);

    /**
     * @brief Sets the character to draw.
     *
     * Define the icon code using QChar, or implicitly using ushort, e.g. 0xf015.
     * @param letter The character.
     */
    void setLetter(const QChar &letter);

    /**
     * @brief Sets a fixed colour.
     *
     * Not advised: without one, the icon uses the palette's button text
     * colour and follows theme changes. Disabled icons are greyed out either way.
     * @param baseColor The colour; an invalid QColor uses the palette.
     */
    void setBaseColor(const QColor &baseColor);

    /**
     * @brief A copy of this engine (Qt copies icons with it).
     * @return The copy, with the same font, style, letter, colour and rotation.
     */
    virtual QIconEngine *clone() const override;

    /**
     * @brief Turns the character around the icon's centre.
     * @param angle The angle in degrees, clockwise.
     */
    void setRotation(double angle);

private:
    QString mFontFamily;   ///< The font family.
    QString mFontStyle;    ///< The style within the family ("Solid", ...); empty: default.
    QChar mLetter;         ///< The character drawn.
    QColor mBaseColor;     ///< Fixed colour; invalid: the palette's.
    double mAngle{0.0};    ///< Rotation in degrees, clockwise.
};

/**
 * @brief Makes QIcons from the characters of icon fonts such as Font Awesome.
 *
 * Load the fonts once, e.g. in main():
 * @code
 * QFontIcon::addFont(":/fa-solid-900.ttf");     // Font Awesome 5 Free Solid: the default
 * QFontIcon::addFont(":/fa-regular-400.ttf");   // Font Awesome 5 Free Regular
 * QFontIcon::addFont(":/fa-brands-400.ttf");    // Font Awesome 5 Brands
 * @endcode
 * then make icons from the characters' codes:
 * @code
 * QAction *action = new QAction(QFontIcon::icon(0xf015), tr("Home"));
 * button->setIcon(QFontIcon::icon(0xf08d, QColor(), 90.0));                        // turned 90 degrees
 * button->setIcon(QFontIcon::icon(0xf15b, QColor(), 0.0, "Font Awesome 5 Free Regular"));   // outline style
 * @endcode
 *
 * Which font: the @p family argument of icon(), else the first font added.
 * If that font doesn't have the character, the other added fonts are tried
 * in the order they were added, so Brands icons work without naming the font.
 */
class QFontIcon : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Loads an icon font. By default, the first one added is used.
     * @param filename The font file, e.g. ":/fa-solid-900.ttf" from a resource.
     * @return @c true if the font was loaded.
     */
    static bool addFont(const QString &filename);

    /**
     * @brief The one QFontIcon object, which keeps the added fonts.
     * @return The instance; created on first call.
     */
    static QFontIcon *instance();

    /**
     * @brief Makes an icon from a character's code.
     * @param code      The character, e.g. 0xf015 for Font Awesome's "home".
     * @param baseColor A fixed colour; invalid (default): the palette's, which
     *                  follows theme changes.
     * @param angle     Rotation in degrees, clockwise.
     * @param family    The font: a family ("Font Awesome 5 Free") or a family
     *                  and style ("Font Awesome 5 Free Regular"); empty: the
     *                  first font added.
     * @return The icon; a null icon if no font was added.
     */
    static QIcon icon(const QChar &code, const QColor &baseColor = QColor(), double angle = 0.0,
                      const QString &family = QString());

    /**
     * @brief Makes an icon from a character's code given as a number.
     *
     * Same as icon(const QChar &, ...). Needed with Qt 6, where a plain
     * number such as 0xf015 can't be passed as a QChar (the conversion is ambiguous).
     * @param code      The character's code, e.g. 0xf015.
     * @param baseColor A fixed colour; invalid (default): the palette's.
     * @param angle     Rotation in degrees, clockwise.
     * @param family    The font, as for icon(const QChar &, ...).
     * @return The icon.
     */
    static QIcon icon(uint code, const QColor &baseColor = QColor(), double angle = 0.0,
                      const QString &family = QString());

    /**
     * @brief The family names of the added fonts, in the order they were added.
     *
     * A family appears once per font added, e.g. "Font Awesome 5 Free" twice
     * for Solid and Regular.
     * @return The families.
     */
    const QStringList &families() const;

protected:
    /**
     * @brief Remembers an added font.
     * @param family The font's family name.
     */
    void addFamily(const QString &family);

private:
    /** @brief One added font: its family and its style within the family. */
    struct Font
    {
        QString family;      ///< E.g. "Font Awesome 5 Free".
        QString styleName;   ///< E.g. "Solid"; empty if unknown.
    };

    explicit QFontIcon(QObject *parent = 0);
    ~QFontIcon();

    /**
     * @brief Remembers an added font with its style.
     * @param family    The font's family name.
     * @param styleName Its style name.
     */
    void registerFont(const QString &family, const QString &styleName);

    /**
     * @brief The font to draw a character with.
     * @param code   The character.
     * @param family The family (or "family style") asked for; empty: the first added.
     * @return The font; its family is empty if no font was added.
     */
    Font fontFor(const QChar &code, const QString &family) const;

    /**
     * @brief Whether a font has a glyph for a character.
     * @param font The font.
     * @param code The character.
     * @return @c true if it has one.
     */
    static bool hasGlyph(const Font &font, const QChar &code);

    static QFontIcon *mInstance;   ///< The instance, see instance().
    QStringList mfamilies;         ///< Family names of the added fonts, see families().
    QList<Font> mFonts;            ///< The added fonts with their styles.
};

#endif // QFONTICON_H
