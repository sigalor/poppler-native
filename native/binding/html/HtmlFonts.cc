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
// Copyright (C) 2007, 2010, 2012, 2018 Albert Astals Cid <aacid@kde.org>
// Copyright (C) 2008 Boris Toloknov <tlknv@yandex.ru>
// Copyright (C) 2008 Tomas Are Haavet <tomasare@gmail.com>
// Copyright (C) 2010 OSSD CDAC Mumbai by Leena Chourey (leenac@cdacmumbai.in) and Onkar Potdar (onkar@cdacmumbai.in)
// Copyright (C) 2011 Joshua Richardson <jric@chegg.com>
// Copyright (C) 2011 Stephen Reichling <sreichling@chegg.com>
// Copyright (C) 2012 Igor Slepchin <igor.slepchin@gmail.com>
// Copyright (C) 2012 Luis Parravicini <lparravi@gmail.com>
// Copyright (C) 2013 Julien Nabet <serval2412@yahoo.fr>
// Copyright (C) 2017 Jason Crain <jason@inspiresomeone.us>
// Copyright (C) 2018 Klarälvdalens Datakonsult AB, a KDAB Group company, <info@kdab.com>. Work sponsored by the LiMux
// project of the city of Munich Copyright (C) 2018 Steven Boswell <ulatekh@yahoo.com> Copyright (C) 2018 Adam Reichold
// <adam.reichold@t-online.de> Copyright (C) 2019 Oliver Sander <oliver.sander@tu-dresden.de>
//
// To see a description of the changes please see the Changelog file that
// came with your tarball or type make ChangeLog if you are building from git
//
//========================================================================

#include <cstdio>

#include <poppler/GfxFont.h>
#include <poppler/GlobalParams.h>
#include <poppler/UnicodeMap.h>

#include "../ReadPDFOutputs.hpp"
#include "HtmlFonts.h"
#include "HtmlUtils.h"

namespace {

const char* const defaultFamilyName = "Times";

const char* const styleSuffixes[] = {
    "-Regular", "-Bold", "-BoldOblique", "-BoldItalic", "-Oblique", "-Italic", "-Roman",
};

void removeStyleSuffix(std::string& familyName) {
  for (const char* const styleSuffix : styleSuffixes) {
    auto pos = familyName.rfind(styleSuffix);
    if (pos != std::string::npos) {
      familyName.resize(pos);
      return;
    }
  }
}

}  // namespace

HtmlFontColor::HtmlFontColor(GfxRGB rgb) {
  r = static_cast<int>(rgb.r / 65535.0 * 255.0);
  g = static_cast<int>(rgb.g / 65535.0 * 255.0);
  b = static_cast<int>(rgb.b / 65535.0 * 255.0);
  if (!(Ok(r) && Ok(b) && Ok(g))) {
    if (!globalParams->getErrQuiet()) fprintf(stderr, "Error : Bad color (%d,%d,%d) reset to (0,0,0)\n", r, g, b);
    r = 0;
    g = 0;
    b = 0;
  }
}

GooString* HtmlFontColor::convtoX(unsigned int xcol) const {
  GooString* xret = new GooString();
  char tmp;
  unsigned int k;
  k = (xcol / 16);
  if (k < 10)
    tmp = (char)('0' + k);
  else
    tmp = (char)('a' + k - 10);
  xret->append(tmp);
  k = (xcol % 16);
  if (k < 10)
    tmp = (char)('0' + k);
  else
    tmp = (char)('a' + k - 10);
  xret->append(tmp);
  return xret;
}

GooString* HtmlFontColor::toString() const {
  GooString* tmp = new GooString("#");
  GooString* tmpr = convtoX(r);
  GooString* tmpg = convtoX(g);
  GooString* tmpb = convtoX(b);
  tmp->append(tmpr);
  tmp->append(tmpg);
  tmp->append(tmpb);
  delete tmpr;
  delete tmpg;
  delete tmpb;
  return tmp;
}

HtmlFont::HtmlFont(GfxFont* font, int _size, GfxRGB rgb) {
  color = HtmlFontColor(rgb);

  lineSize = -1;

  size = (_size - 1);
  italic = false;
  bold = false;
  rotOrSkewed = false;

  if (font->isBold() || font->getWeight() >= GfxFont::W700) bold = true;
  if (font->isItalic()) italic = true;

  if (const GooString* fontname = font->getName()) {
    FontName = new GooString(fontname);

    GooString fontnameLower(fontname);
    fontnameLower.lowerCase();

    if (!bold && strstr(fontnameLower.c_str(), "bold")) {
      bold = true;
    }

    if (!italic && (strstr(fontnameLower.c_str(), "italic") || strstr(fontnameLower.c_str(), "oblique"))) {
      italic = true;
    }

    familyName = fontname->c_str();
    removeStyleSuffix(familyName);
  } else {
    FontName = new GooString(defaultFamilyName);
    familyName = defaultFamilyName;
  }

  rotSkewMat[0] = rotSkewMat[1] = rotSkewMat[2] = rotSkewMat[3] = 0;
}

HtmlFont::HtmlFont(const HtmlFont& x) {
  size = x.size;
  lineSize = x.lineSize;
  italic = x.italic;
  bold = x.bold;
  familyName = x.familyName;
  color = x.color;
  FontName = new GooString(x.FontName);
  rotOrSkewed = x.rotOrSkewed;
  memcpy(rotSkewMat, x.rotSkewMat, sizeof(rotSkewMat));
}

HtmlFont::~HtmlFont() { delete FontName; }

HtmlFont& HtmlFont::operator=(const HtmlFont& x) {
  if (this == &x) return *this;
  size = x.size;
  lineSize = x.lineSize;
  italic = x.italic;
  bold = x.bold;
  familyName = x.familyName;
  color = x.color;
  delete FontName;
  FontName = new GooString(x.FontName);
  return *this;
}

/*
  This function is used to compare font uniquely for insertion into
  the list of all encountered fonts
*/
bool HtmlFont::isEqual(const HtmlFont& x) const {
  return (size == x.size) && (lineSize == x.lineSize) && (FontName->cmp(x.FontName) == 0) && (bold == x.bold) &&
         (italic == x.italic) && (color.isEqual(x.getColor())) && isRotOrSkewed() == x.isRotOrSkewed() &&
         (!isRotOrSkewed() || rot_matrices_equal(getRotMat(), x.getRotMat()));
}

/*
  This one is used to decide whether two pieces of text can be joined together
  and therefore we don't care about bold/italics properties
*/
bool HtmlFont::isEqualIgnoreBold(const HtmlFont& x) const {
  return ((size == x.size) && (familyName == x.familyName) && (color.isEqual(x.getColor())));
}

GooString* HtmlFont::getFontName() { return new GooString(familyName); }

GooString* HtmlFont::getFullName() { return new GooString(FontName); }

// this method if plain wrong todo
std::string HtmlFont::codepointsEncode(const Unicode* u, int uLen) {
  std::string ret;
  UnicodeMap* uMap;
  char buf[8];
  int n;

  // get the output encoding
  if (!(uMap = globalParams->getTextEncoding())) {
    return ret;
  }

  for (int i = 0; i < uLen; ++i) {
    if ((n = uMap->mapUnicode(u[i], buf, sizeof(buf))) > 0) {
      ret += std::string(buf, buf + n);
    }
  }

  uMap->decRefCnt();
  return ret;
}

HtmlFontAccu::HtmlFontAccu() { accu = new std::vector<HtmlFont>(); }

HtmlFontAccu::~HtmlFontAccu() {
  if (accu) delete accu;
}

int HtmlFontAccu::AddFont(const HtmlFont& font) {
  std::vector<HtmlFont>::iterator i;
  for (i = accu->begin(); i != accu->end(); ++i) {
    if (font.isEqual(*i)) {
      return (int)(i - (accu->begin()));
    }
  }

  accu->push_back(font);
  return (accu->size() - 1);
}

ReadPDFOutputs::Font HtmlFontAccu::serialize(int i) {
  HtmlFont font = (*accu)[i];
  GooString* colorStr = font.color.toString();

  ReadPDFOutputs::Font ret;
  ret.id = i;
  ret.size = font.size;
  ret.family = font.familyName;
  ret.color = colorStr->c_str();
  ret.bold = font.bold;
  ret.italic = font.italic;

  delete colorStr;
  return ret;
}
