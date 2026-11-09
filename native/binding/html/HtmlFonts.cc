//========================================================================
//
// This file comes from pdftohtml project
// http://pdftohtml.sourceforge.net
//
// Copyright from:
// Gueorgui Ovtcharov
// Rainer Dorsch <http://www.ra.informatik.uni-stuttgart.de/~rainer/>
// Mikhail Kruk <meshko@cs.brandeis.edu>
//
//========================================================================

//========================================================================
//
// Modified under the Poppler project - http://poppler.freedesktop.org
//
// All changes made under the Poppler project to this file are licensed
// under GPL version 2 or later
//
// Copyright (C) 2007, 2010, 2012, 2018, 2020 Albert Astals Cid <aacid@kde.org>
// Copyright (C) 2008 Boris Toloknov <tlknv@yandex.ru>
// Copyright (C) 2008 Tomas Are Haavet <tomasare@gmail.com>
// Copyright (C) 2010 OSSD CDAC Mumbai by Leena Chourey (leenac@cdacmumbai.in) and Onkar Potdar (onkar@cdacmumbai.in)
// Copyright (C) 2011 Joshua Richardson <jric@chegg.com>
// Copyright (C) 2011 Stephen Reichling <sreichling@chegg.com>
// Copyright (C) 2012 Igor Slepchin <igor.slepchin@gmail.com>
// Copyright (C) 2012 Luis Parravicini <lparravi@gmail.com>
// Copyright (C) 2013 Julien Nabet <serval2412@yahoo.fr>
// Copyright (C) 2017 Jason Crain <jason@inspiresomeone.us>
// Copyright (C) 2018 Klarälvdalens Datakonsult AB, a KDAB Group company, <info@kdab.com>. Work sponsored by the LiMux project of the city of Munich
// Copyright (C) 2018 Steven Boswell <ulatekh@yahoo.com>
// Copyright (C) 2018 Adam Reichold <adam.reichold@t-online.de>
// Copyright (C) 2019 Oliver Sander <oliver.sander@tu-dresden.de>
// Copyright (C) 2020 Eddie Kohler <ekohler@gmail.com>
//
// To see a description of the changes please see the Changelog file that
// came with your tarball or type make ChangeLog if you are building from git
//
//========================================================================

#include <cstdio>

#include <goo/GooString.h>
#include <poppler/GfxFont.h>
#include <poppler/GlobalParams.h>
#include <poppler/UnicodeMap.h>

#include "../ReadPDFOutputs.hpp"
#include "HtmlFonts.h"
#include "HtmlUtils.h"

namespace {

const char *const defaultFamilyName = "Times";

const char *const styleSuffixes[] = {
    "-Regular", "-Bold", "-BoldOblique", "-BoldItalic", "-Oblique", "-Italic", "-Roman",
};

void removeStyleSuffix(std::string &familyName)
{
    for (const char *const styleSuffix : styleSuffixes) {
        auto pos = familyName.rfind(styleSuffix);
        if (pos != std::string::npos) {
            familyName.resize(pos);
            return;
        }
    }
}

}

HtmlFontColor::HtmlFontColor(GfxRGB rgb, double opacity_)
{
    r = static_cast<int>(rgb.r / 65535.0 * 255.0);
    g = static_cast<int>(rgb.g / 65535.0 * 255.0);
    b = static_cast<int>(rgb.b / 65535.0 * 255.0);
    opacity = static_cast<int>(opacity_ * 255.999);
    if (!(Ok(r) && Ok(b) && Ok(g) && Ok(opacity))) {
        if (!globalParams->getErrQuiet())
            fprintf(stderr, "Error : Bad color (%d,%d,%d,%d) reset to (0,0,0,255)\n", r, g, b, opacity);
        r = 0;
        g = 0;
        b = 0;
        opacity = 255;
    }
}

std::string HtmlFontColor::convtoX(unsigned int xcol)
{
    std::string xret;
    char tmp;
    unsigned int k;
    k = (xcol / 16);
    if (k < 10)
        tmp = (char)('0' + k);
    else
        tmp = (char)('a' + k - 10);
    xret.push_back(tmp);
    k = (xcol % 16);
    if (k < 10)
        tmp = (char)('0' + k);
    else
        tmp = (char)('a' + k - 10);
    xret.push_back(tmp);
    return xret;
}

std::string HtmlFontColor::toString() const
{
    std::string color(1, '#');
    color += convtoX(r);
    color += convtoX(g);
    color += convtoX(b);
    return color;
}

HtmlFont::HtmlFont(const GfxFont &font, int _size, GfxRGB rgb, double opacity)
{
    color = HtmlFontColor(rgb, opacity);

    lineSize = -1;

    size = _size;
    italic = false;
    bold = false;
    rotOrSkewed = false;

    if (font.isBold() || font.getWeight() >= GfxFont::W700)
        bold = true;
    if (font.isItalic())
        italic = true;

    if (const std::optional<std::string> &fontname = font.getName()) {
        FontName = *fontname;

        std::string fontnameLower = *fontname;
        GooString::lowerCase(fontnameLower);

        if (!bold && fontnameLower.find("bold") != std::string::npos) {
            bold = true;
        }

        if (!italic && (fontnameLower.find("italic") != std::string::npos || fontnameLower.find("oblique") != std::string::npos)) {
            italic = true;
        }

        fullName = *fontname;
        familyName = *fontname;
        removeStyleSuffix(familyName);
    } else {
        FontName = defaultFamilyName;
        familyName = defaultFamilyName;
        fullName = defaultFamilyName;
    }

    rotSkewMat = { 0, 0, 0, 0 };

    hasToUnicodeCMap = font.hasToUnicodeCMap();
}

/*
  This function is used to compare font uniquely for insertion into
  the list of all encountered fonts
*/
bool HtmlFont::isEqual(const HtmlFont &x) const
{
    return (size == x.size) && (lineSize == x.lineSize) && (FontName == x.FontName) && (bold == x.bold) && (italic == x.italic) && (color.isEqual(x.getColor())) && isRotOrSkewed() == x.isRotOrSkewed()
            && (!isRotOrSkewed() || rot_matrices_equal(getRotMat(), x.getRotMat()));
}

/*
  This one is used to decide whether two pieces of text can be joined together
  and therefore we don't care about bold/italics properties
*/
bool HtmlFont::isEqualIgnoreBold(const HtmlFont &x) const
{
    return ((size == x.size) && (familyName == x.familyName) && (color.isEqual(x.getColor())));
}

std::string HtmlFont::codepointsEncode(const Unicode *u, int uLen)
{
    std::string tmp;
    const UnicodeMap *uMap = globalParams->getTextEncoding();
    char buf[8];
    int n;

    // get the output encoding
    if (!uMap) {
        return tmp;
    }

    for (int i = 0; i < uLen; ++i) {
        if ((n = uMap->mapUnicode(u[i], buf, sizeof(buf))) > 0) {
            tmp += std::string(buf, buf + n);
        }
    }

    return tmp;
}

int HtmlFontAccu::AddFont(const HtmlFont &font)
{
    std::vector<HtmlFont>::iterator i;
    for (i = accu.begin(); i != accu.end(); ++i) {
        if (font.isEqual(*i)) {
            return (int)(i - (accu.begin()));
        }
    }

    accu.push_back(font);
    return (accu.size() - 1);
}

ReadPDFOutputs::Font HtmlFontAccu::serialize(int i) const
{
    const HtmlFont &font = accu[i];

    ReadPDFOutputs::Font ret;
    ret.id = i;
    ret.size = font.size;
    ret.family = font.familyName;
    ret.fullName = font.fullName;
    ret.color = font.color.toString();
    ret.bold = font.bold;
    ret.italic = font.italic;
    ret.hasToUnicodeCMap = font.hasToUnicodeCMap;
    return ret;
}
