//========================================================================
//
// HtmlOutputDev.cc
//
// Copyright 1997-2002 Glyph & Cog, LLC
//
// Changed 1999-2000 by G.Ovtcharov
//
// Changed 2002 by Mikhail Kruk
//
//========================================================================

//========================================================================
//
// Modified under the Poppler project - http://poppler.freedesktop.org
//
// All changes made under the Poppler project to this file are licensed
// under GPL version 2 or later
//
// Copyright (C) 2005-2013, 2016-2020 Albert Astals Cid <aacid@kde.org>
// Copyright (C) 2008 Kjartan Maraas <kmaraas@gnome.org>
// Copyright (C) 2008 Boris Toloknov <tlknv@yandex.ru>
// Copyright (C) 2008 Haruyuki Kawabe <Haruyuki.Kawabe@unisys.co.jp>
// Copyright (C) 2008 Tomas Are Haavet <tomasare@gmail.com>
// Copyright (C) 2009 Warren Toomey <wkt@tuhs.org>
// Copyright (C) 2009, 2011 Carlos Garcia Campos <carlosgc@gnome.org>
// Copyright (C) 2009 Reece Dunn <msclrhd@gmail.com>
// Copyright (C) 2010, 2012, 2013 Adrian Johnson <ajohnson@redneon.com>
// Copyright (C) 2010 Hib Eris <hib@hiberis.nl>
// Copyright (C) 2010 OSSD CDAC Mumbai by Leena Chourey (leenac@cdacmumbai.in) and Onkar Potdar (onkar@cdacmumbai.in)
// Copyright (C) 2011 Joshua Richardson <jric@chegg.com>
// Copyright (C) 2011 Stephen Reichling <sreichling@chegg.com>
// Copyright (C) 2011, 2012 Igor Slepchin <igor.slepchin@gmail.com>
// Copyright (C) 2012 Ihar Filipau <thephilips@gmail.com>
// Copyright (C) 2012 Gerald Schmidt <solahcin@gmail.com>
// Copyright (C) 2012 Pino Toscano <pino@kde.org>
// Copyright (C) 2013 Thomas Freitag <Thomas.Freitag@alfa.de>
// Copyright (C) 2013 Julien Nabet <serval2412@yahoo.fr>
// Copyright (C) 2013 Johannes Brandstätter <jbrandstaetter@gmail.com>
// Copyright (C) 2014 Fabio D'Urso <fabiodurso@hotmail.it>
// Copyright (C) 2016 Vincent Le Garrec <legarrec.vincent@gmail.com>
// Copyright (C) 2017 Caolán McNamara <caolanm@redhat.com>
// Copyright (C) 2018 Klarälvdalens Datakonsult AB, a KDAB Group company, <info@kdab.com>. Work sponsored by the LiMux project of the city of Munich
// Copyright (C) 2018 Thibaut Brard <thibaut.brard@gmail.com>
// Copyright (C) 2018-2020 Adam Reichold <adam.reichold@t-online.de>
// Copyright (C) 2019, 2020 Oliver Sander <oliver.sander@tu-dresden.de>
// Copyright (C) 2020 Eddie Kohler <ekohler@gmail.com>
//
// To see a description of the changes please see the Changelog file that
// came with your tarball or type make ChangeLog if you are building from git
//
//========================================================================

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <utility>

#include <config.h>
#include <goo/GooString.h>
#include <goo/PNGWriter.h>
#include <goo/gbase64.h>
#include <goo/gbasename.h>
#include <goo/gmem.h>
#include <poppler/Annot.h>
#include <poppler/Error.h>
#include <poppler/GfxFont.h>
#include <poppler/GfxState.h>
#include <poppler/GlobalParams.h>
#include <poppler/Link.h>
#include <poppler/Outline.h>
#include <poppler/PDFDoc.h>
#include <poppler/Page.h>
#include <poppler/Stream.h>
#include <poppler/UnicodeMap.h>

#include "HtmlFonts.h"
#include "HtmlOutputDev.h"
#include "HtmlUtils.h"
#include "InMemoryFile.h"

#define DEBUG __FILE__ << ": " << __LINE__ << ": DEBUG: "

class HtmlImage
{
public:
    HtmlImage(std::string &&_fName, GfxState *state) : fName(std::move(_fName))
    {
        state->transform(0, 0, &xMin, &yMax);
        state->transform(1, 1, &xMax, &yMin);
    }
    HtmlImage(const HtmlImage &) = delete;
    HtmlImage &operator=(const HtmlImage &) = delete;

    double xMin, xMax; // image x coordinates
    double yMin, yMax; // image y coordinates
    std::string fName; // image file name
};

// returns true if x is closer to y than x is to z
static inline bool IS_CLOSER(float x, float y, float z)
{
    return std::fabs((x) - (y)) < std::fabs((x) - (z));
}

static bool debug = false;

template<size_t N>
static std::string print_matrix(const std::array<double, N> &mat)
{
    std::string ret = "[";
    for (size_t i = 0; i < N; i++) {
        ret += GooString::format("{0:g}", mat[i]);
        ret += i + 1 < N ? " " : "]";
    }
    return ret;
}

static std::string print_uni_str(const Unicode *u, const unsigned uLen)
{
    if (!uLen)
        return "";
    std::string ret(1, static_cast<char>(*u < 0x7F ? *u & 0xFF : '?'));
    for (unsigned i = 1; i < uLen; i++) {
        if (u[i] < 0x7F) {
            ret.push_back(static_cast<char>(u[i]) & 0xFF);
        }
    }
    return ret;
}

//------------------------------------------------------------------------
// HtmlString
//------------------------------------------------------------------------

HtmlString::HtmlString(GfxState *state, double fontSize, HtmlFontAccu *_fonts) : fonts(_fonts)
{
    double x, y;

    // Poppler 26.x keeps the text position apart from the path position (curX/curY in 21.05)
    state->transform(state->getCurTextX(), state->getCurTextY(), &x, &y);
    if (const std::shared_ptr<GfxFont> &font = state->getFont()) {
        double ascent = font->getAscent();
        double descent = font->getDescent();
        if (ascent > 1.05) {
            // printf( "ascent=%.15g is too high, descent=%.15g\n", ascent, descent );
            ascent = 1.05;
        }
        if (descent < -0.4) {
            // printf( "descent %.15g is too low, ascent=%.15g\n", descent, ascent );
            descent = -0.4;
        }
        yMin = y - ascent * fontSize;
        yMax = y - descent * fontSize;
        GfxRGB rgb;
        state->getFillRGB(&rgb);
        HtmlFont hfont = HtmlFont(*font, static_cast<int>(fontSize), rgb, state->getFillOpacity());
        if (isMatRotOrSkew(state->getTextMat())) {
            const std::array<double, 6> &textMat = state->getTextMat();
            std::array<double, 4> normalizedMatrix = { textMat[0], textMat[1], textMat[2], textMat[3] };
            // browser rotates the opposite way
            // so flip the sign of the angle -> sin() components change sign
            if (debug)
                std::cerr << DEBUG << "before transform: " << print_matrix(normalizedMatrix) << std::endl;
            normalizedMatrix[1] *= -1;
            normalizedMatrix[2] *= -1;
            if (debug)
                std::cerr << DEBUG << "after reflecting angle: " << print_matrix(normalizedMatrix) << std::endl;
            normalizeRotMat(normalizedMatrix);
            if (debug)
                std::cerr << DEBUG << "after norm: " << print_matrix(normalizedMatrix) << std::endl;
            hfont.setRotMat(normalizedMatrix);
        }
        fontpos = fonts->AddFont(hfont);
    } else {
        // this means that the PDF file draws text without a current font,
        // which should never happen
        yMin = y - 0.95 * fontSize;
        yMax = y + 0.35 * fontSize;
        fontpos = 0;
    }
    if (yMin == yMax) {
        // this is a sanity check for a case that shouldn't happen -- but
        // if it does happen, we want to avoid dividing by zero later
        yMin = y;
        yMax = y + 1;
    }
    col = 0;
    text = nullptr;
    xRight = nullptr;
    link = nullptr;
    len = size = 0;
    yxNext = nullptr;
    xyNext = nullptr;
    dir = textDirUnknown;
}

HtmlString::~HtmlString()
{
    gfree(text);
    gfree(xRight);
}

void HtmlString::addChar(GfxState *state, double x, double y, double dx, double dy, Unicode u)
{
    if (dir == textDirUnknown) {
        // dir = UnicodeMap::getDirection(u);
        dir = textDirLeftRight;
    }

    if (len == size) {
        size += 16;
        text = (Unicode *)grealloc(text, size * sizeof(Unicode));
        xRight = (double *)grealloc(xRight, size * sizeof(double));
    }
    text[len] = u;
    if (len == 0) {
        xMin = x;
    }
    xMax = xRight[len] = x + dx;
    // printf("added char: %f %f xright = %f\n", x, dx, x+dx);
    ++len;
}

void HtmlString::endString()
{
    if (dir == textDirRightLeft && len > 1) {
        // printf("will reverse!\n");
        for (int i = 0; i < len / 2; i++) {
            Unicode ch = text[i];
            text[i] = text[len - i - 1];
            text[len - i - 1] = ch;
        }
    }
}

//------------------------------------------------------------------------
// HtmlPage
//------------------------------------------------------------------------

HtmlPage::HtmlPage(bool newMergeLines, double newWordBreakThreshold) : mergeLines(newMergeLines), wordBreakThreshold(newWordBreakThreshold)
{
    curStr = nullptr;
    yxStrings = nullptr;
    xyStrings = nullptr;
    yxCur1 = yxCur2 = nullptr;
    fonts = std::make_unique<HtmlFontAccu>();
    links = std::make_unique<HtmlLinks>();
    pageWidth = 0;
    pageHeight = 0;
    fontsPageMarker = 0;
    firstPage = -1;
}

HtmlPage::~HtmlPage()
{
    clear();
}

void HtmlPage::updateFont(GfxState *state)
{
    const char *name;
    int code;
    double w;

    // adjust the font size
    fontSize = state->getTransformedFontSize();
    const GfxFont *font = state->getFont().get();
    if (font && font->getType() == fontType3) {
        // This is a hack which makes it possible to deal with some Type 3
        // fonts.  The problem is that it's impossible to know what the
        // base coordinate system used in the font is without actually
        // rendering the font.  This code tries to guess by looking at the
        // width of the character 'm' (which breaks if the font is a
        // subset that doesn't contain 'm').
        for (code = 0; code < 256; ++code) {
            if ((name = static_cast<const Gfx8BitFont *>(font)->getCharName(code)) && name[0] == 'm' && name[1] == '\0') {
                break;
            }
        }
        if (code < 256) {
            w = static_cast<const Gfx8BitFont *>(font)->getWidth(static_cast<CID>(code));
            if (w != 0) {
                // 600 is a generic average 'm' width -- yes, this is a hack
                fontSize *= w / 0.6;
            }
        }
        const std::array<double, 6> &fm = font->getFontMatrix();
        if (fm[0] != 0) {
            fontSize *= fabs(fm[3] / fm[0]);
        }
    }
}

void HtmlPage::beginString(GfxState *state, const std::string & /*s*/)
{
    curStr = new HtmlString(state, fontSize, fonts.get());
}

void HtmlPage::conv()
{
    for (HtmlString *tmp = yxStrings; tmp; tmp = tmp->yxNext) {
        tmp->htext = HtmlFont::codepointsEncode(tmp->text, tmp->len);

        int linkIndex = 0;
        if (links->inLink(tmp->xMin, tmp->yMin, tmp->xMax, tmp->yMax, linkIndex)) {
            tmp->link = links->getLink(linkIndex);
        }
    }
}

void HtmlPage::addChar(GfxState *state, double x, double y, double dx, double dy, double ox, double oy, const Unicode *u, int uLen)
{
    double x1, y1, w1, h1, dx2, dy2;
    int n, i;
    state->transform(x, y, &x1, &y1);
    n = curStr->len;

    // check that new character is in the same direction as current string
    // and is not too far away from it before adding
    // if ((UnicodeMap::getDirection(u[0]) != curStr->dir) ||
    // XXX
    if (debug) {
        const std::array<double, 6> &text_mat = state->getTextMat();
        // rotation is (cos q, sin q, -sin q, cos q, 0, 0)
        // sin q is zero iff there is no rotation, or 180 deg. rotation;
        // for 180 rotation, cos q will be negative
        if (text_mat[0] < 0 || !is_within(text_mat[1], .1, 0)) {
            std::cerr << DEBUG << "rotation matrix for \"" << print_uni_str(u, uLen) << '"' << std::endl;
            std::cerr << "text " << print_matrix(state->getTextMat());
        }
    }
    if (n > 0 && // don't start a new string, unless there is already a string
                 // TODO: the following line assumes that text is flowing left to
                 // right, which will not necessarily be the case, e.g. if rotated;
                 // It assesses whether or not two characters are close enough to
                 // be part of the same string
        fabs(x1 - curStr->xRight[n - 1]) > wordBreakThreshold * (curStr->yMax - curStr->yMin) &&
        // rotation is (cos q, sin q, -sin q, cos q, 0, 0)
        // sin q is zero iff there is no rotation, or 180 deg. rotation;
        // for 180 rotation, cos q will be negative
        !rot_matrices_equal(curStr->getFont().getRotMat(), state->getTextMat())) {
        endString();
        beginString(state, std::string());
    }
    state->textTransformDelta(state->getCharSpace() * state->getHorizScaling(), 0, &dx2, &dy2);
    dx -= dx2;
    dy -= dy2;
    state->transformDelta(dx, dy, &w1, &h1);
    if (uLen != 0) {
        w1 /= uLen;
        h1 /= uLen;
    }
    for (i = 0; i < uLen; ++i) {
        curStr->addChar(state, x1 + i * w1, y1 + i * h1, w1, h1, u[i]);
    }
}

void HtmlPage::endString()
{
    HtmlString *p1, *p2;

    // throw away zero-length strings -- they don't have valid xMin/xMax
    // values, and they're useless anyway
    if (curStr->len == 0) {
        delete curStr;
        curStr = nullptr;
        return;
    }

    curStr->endString();

    // insert string in y-major list
    p1 = yxCur1;
    p2 = nullptr;
    yxCur1 = curStr;
    if (p1)
        p1->yxNext = curStr;
    else
        yxStrings = curStr;
    curStr->yxNext = p2;
    curStr = nullptr;
}

// Strings are lines of text;
// This function aims to combine strings into lines and paragraphs if mergeLines
// It may also strip out duplicate strings (if they are on top of each other); sometimes they are to create a font effect
void HtmlPage::coalesce()
{
    HtmlString *str1, *str2;
    const HtmlFont *hfont1, *hfont2;
    double space, horSpace, vertSpace, vertOverlap;
    bool addSpace, addLineBreak;
    int n, i;
    double curX, curY;

    str1 = yxStrings;
    if (!str1) return;

    hfont1 = getFont(str1);
    curX = str1->xMin;
    curY = str1->yMin;

    while (str1 && (str2 = str1->yxNext)) {
        hfont2 = getFont(str2);
        space = str1->yMax - str1->yMin; // the height of the font's bounding box
        horSpace = str2->xMin - str1->xMax;
        // if strings line up on left-hand side AND they are on subsequent lines, we need a line break
        addLineBreak = mergeLines && (fabs(str1->xMin - str2->xMin) < 0.4) && IS_CLOSER(str2->yMax, str1->yMax + space, str1->yMax);
        vertSpace = str2->yMin - str1->yMax;

        // printf("coalesce %d %d %f? ", str1->dir, str2->dir, d);

        if (str2->yMin >= str1->yMin && str2->yMin <= str1->yMax) {
            vertOverlap = str1->yMax - str2->yMin;
        } else if (str2->yMax >= str1->yMin && str2->yMax <= str1->yMax) {
            vertOverlap = str2->yMax - str1->yMin;
        } else {
            vertOverlap = 0;
        }

        const std::string *linkdest1 = str1->link ? &str1->link->getDest() : nullptr;
        const std::string *linkdest2 = str2->link ? &str2->link->getDest() : nullptr;
        bool linkdestsEqual = linkdest1 == linkdest2 || (linkdest1 != nullptr && linkdest2 != nullptr && *linkdest1 == *linkdest2);

        // Combine strings if:
        //  They appear to be the same font && going in the same direction or contain the same link (if
        //  any) AND at least one of the following:
        //  1.  They appear to be part of the same line of text
        //  2.  They appear to be subsequent lines of a paragraph
        //  We assume (1) or (2) above, respectively, based on:
        //  (1)  strings overlap vertically AND
        //       horizontal space between end of str1 and start of str2 is consistent with a single space or less;
        //       because of rawOrder, the strings have to overlap vertically by at least 50%
        //  (2)  Strings flow down the page, but the space between them is not too great, and they are lined up on the left
        if ((((vertOverlap > 0.5 * space) && (horSpace > -0.5 * space && horSpace < space)) ||
            (vertSpace >= 0 && vertSpace < 0.5 * space && addLineBreak)) &&
            ((hfont1->isEqual(*hfont2))) &&  // fonts must be the same, in other modes fonts do not metter
            str1->dir == str2->dir &&        // text direction the same
            linkdestsEqual                   // links the same
        ) {
            //      printf("yes\n");
            n = str1->len + str2->len;
            if ((addSpace = horSpace > wordBreakThreshold * space)) {
                ++n;
            }
            if (addLineBreak) {
                ++n;
            }

            str1->size = (n + 15) & ~15;
            str1->text = (Unicode *)grealloc(str1->text, str1->size * sizeof(Unicode));
            str1->xRight = (double *)grealloc(str1->xRight, str1->size * sizeof(double));
            if (addSpace) {
                str1->text[str1->len] = 0x20;
                str1->htext += " ";
                str1->xRight[str1->len] = str2->xMin;
                ++str1->len;
            }
            if (addLineBreak) {
                str1->text[str1->len] = '\n';
                str1->htext += "<br/>";
                str1->xRight[str1->len] = str2->xMin;
                ++str1->len;
                str1->yMin = str2->yMin;
                str1->yMax = str2->yMax;
                str1->xMax = str2->xMax;
                int fontLineSize = hfont1->getLineSize();
                int curLineSize = (int)(vertSpace + space);
                if (curLineSize != fontLineSize) {
                    HtmlFont newfnt(*hfont1);
                    newfnt.setLineSize(curLineSize);
                    str1->fontpos = fonts->AddFont(newfnt);
                    hfont1 = getFont(str1);
                    // we have to reget hfont2 because it's location could have
                    // changed on resize
                    hfont2 = getFont(str2);
                }
            }
            for (i = 0; i < str2->len; ++i) {
                str1->text[str1->len] = str2->text[i];
                str1->xRight[str1->len] = str2->xRight[i];
                ++str1->len;
            }

            str1->htext += str2->htext;
            // str1 now contains href for link of str2 (if it is defined)
            // str1->link = str2->link;
            hfont1 = hfont2;
            if (str2->xMax > str1->xMax) {
                str1->xMax = str2->xMax;
            }
            if (str2->yMax > str1->yMax) {
                str1->yMax = str2->yMax;
            }
            str1->yxNext = str2->yxNext;
            delete str2;
        } else {
            str1->xMin = curX;
            str1->yMin = curY;
            str1 = str2;
            curX = str1->xMin;
            curY = str1->yMin;
            hfont1 = hfont2;
        }
    }
    str1->xMin = curX;
    str1->yMin = curY;
}

void HtmlPage::serialize(int pageNumber, ReadPDFOutputs::Page &dest) {
  dest.number = pageNumber;
  dest.width = pageWidth;
  dest.height = pageHeight;

  for (int i = fontsPageMarker; i < fonts->size(); i++) {
    dest.fonts.push_back(fonts->serialize(i));
  }

  for (const std::unique_ptr<HtmlImage> &img : imgList) {
    dest.images.push_back(ReadPDFOutputs::Image());
    ReadPDFOutputs::Image &imgSer = dest.images.back();
    imgSer.top = img->yMin;
    imgSer.left = img->xMin;
    imgSer.width = img->xMax - img->xMin;
    imgSer.height = img->yMax - img->yMin;
    imgSer.src = img->fName;
  }
  imgList.clear();

  for (HtmlString *tmp = yxStrings; tmp; tmp = tmp->yxNext) {
    dest.strings.push_back(ReadPDFOutputs::String());
    ReadPDFOutputs::String &strSer = dest.strings.back();

    strSer.top = tmp->yMin;
    strSer.left = tmp->xMin;
    strSer.width = tmp->xMax - tmp->xMin;
    strSer.height = tmp->yMax - tmp->yMin;
    strSer.font = tmp->fontpos;
    strSer.text = tmp->htext;
    if (tmp->link) {
      strSer.link = tmp->link->getDest();
    }
  }
}

void HtmlPage::clear()
{
    HtmlString *p1, *p2;

    if (curStr) {
        delete curStr;
        curStr = nullptr;
    }
    for (p1 = yxStrings; p1; p1 = p2) {
        p2 = p1->yxNext;
        delete p1;
    }
    yxStrings = nullptr;
    xyStrings = nullptr;
    yxCur1 = yxCur2 = nullptr;

    fontsPageMarker = fonts->size();

    links = std::make_unique<HtmlLinks>();
}

void HtmlPage::addImage(std::string &&fname, GfxState *state)
{
    imgList.push_back(std::make_unique<HtmlImage>(std::move(fname), state));
}

//------------------------------------------------------------------------
// HtmlOutputDev
//------------------------------------------------------------------------

HtmlOutputDev::HtmlOutputDev(Catalog *catalogA, const char *fileName, int numPages, bool newIgnoreImages,
                             bool newShowHiddenCharacters, bool mergeLines, double wordBreakThreshold,
                             std::vector<ReadPDFOutputs::Page> &allPagesDest, TextRepair::TextRepair *textRepairA)
    : ignoreImages(newIgnoreImages), showHiddenCharacters(newShowHiddenCharacters), textRepair(textRepairA), allPages(allPagesDest)
{
    catalog = catalogA;
    docPage = nullptr;
    pageNum = 0;
    dumpJPEG = true;
    pages = std::make_unique<HtmlPage>(mergeLines, wordBreakThreshold);

    maxPageWidth = 0;
    maxPageHeight = 0;

    Docname = fileName;
}

HtmlOutputDev::~HtmlOutputDev() = default;

void HtmlOutputDev::startPage(int pageNumA, GfxState *state, XRef * /*xref*/)
{
    pageNum = pageNumA;
    pages->clear();
    pages->pageWidth = static_cast<int>(state->getPageWidth());
    pages->pageHeight = static_cast<int>(state->getPageHeight());
}

void HtmlOutputDev::endPage()
{
    std::unique_ptr<Links> linksList = docPage->getLinks();
    for (const std::shared_ptr<AnnotLink> &link : linksList->getLinks()) {
        doProcessLink(link.get());
    }

    pages->conv();
    pages->coalesce();
    allPages.push_back(ReadPDFOutputs::Page());
    pages->serialize(pageNum, allPages.back());
    currPageIndex++;

    // I don't yet know what to do in the case when there are pages of different
    // sizes and we want complex output: running ghostscript many times
    // seems very inefficient. So for now I'll just use last page's size
    maxPageWidth = pages->pageWidth;
    maxPageHeight = pages->pageHeight;
}

void HtmlOutputDev::updateFont(GfxState *state)
{
    pages->updateFont(state);
}

void HtmlOutputDev::beginString(GfxState *state, const std::string &s)
{
    pages->beginString(state, s);
}

void HtmlOutputDev::endString(GfxState * /*state*/)
{
    pages->endString();
}

void HtmlOutputDev::drawChar(GfxState *state, double x, double y, double dx, double dy, double originX, double originY, CharCode code, int /*nBytes*/, const Unicode *u, int uLen)
{
    if (!showHiddenCharacters && (state->getRender() & 3) == 3) {
        return;
    }
    if (textRepair && textRepair->repair(state, code, u, uLen, repairedText)) {
        pages->addChar(state, x, y, dx, dy, originX, originY, repairedText.data(), static_cast<int>(repairedText.size()));
        return;
    }
    pages->addChar(state, x, y, dx, dy, originX, originY, u, uLen);
}

void HtmlOutputDev::drawJpegImage(GfxState *state, Stream *str)
{
    InMemoryFile ims;
    int c;

    // open the image file
    FILE *f1 = ims.open("wb");
    if (!f1) {
        error(errIO, -1, "Couldn't open in-memory image file");
        return;
    }

    // initialize stream
    str = str->getNextStream();
    if (!str->rewind()) {
        fclose(f1);
        error(errIO, -1, "Couldn't rewind stream");
        return;
    }

    // copy the stream
    while ((c = str->getChar()) != EOF)
        fputc(c, f1);

    fclose(f1);

    pages->addImage(std::string("data:image/jpeg;base64,") + gbase64Encode(ims.getBuffer()), state);
}

void HtmlOutputDev::drawPngImage(GfxState *state, Stream *str, int width, int height, GfxImageColorMap *colorMap, bool isMask)
{
#if ENABLE_LIBPNG
    InMemoryFile ims;

    if (!colorMap && !isMask) {
        error(errInternal, -1, "Can't have color image without a color map");
        return;
    }

    // open the image file
    FILE *f1 = ims.open("wb");
    if (!f1) {
        error(errIO, -1, "Couldn't open in-memory image file");
        return;
    }

    PNGWriter *writer = new PNGWriter(isMask ? PNGWriter::MONOCHROME : PNGWriter::RGB);
    // TODO can we calculate the resolution of the image?
    if (!writer->init(f1, width, height, 72, 72)) {
        error(errInternal, -1, "Can't init PNG for in-memory image");
        delete writer;
        fclose(f1);
        return;
    }

    if (!isMask) {
        unsigned char *p;
        GfxRGB rgb;

        // Initialize the image stream
        ImageStream *imgStr = new ImageStream(str, width, colorMap->getNumPixelComps(), colorMap->getBits());
        if (!imgStr->rewind()) {
            error(errInternal, -1, "Can't rewind image stream");
            delete writer;
            delete imgStr;
            fclose(f1);
            return;
        }
        unsigned char *row = (unsigned char *)gmalloc(3 * width); // 3 bytes/pixel: RGB
        unsigned char **row_pointer = &row;

        // For each line...
        for (int y = 0; y < height; y++) {

            // Convert into a PNG row
            p = imgStr->getLine();
            if (!p) {
                error(errIO, -1, "Failed to read PNG. In-memory file will be incorrect");
                gfree(row);
                delete writer;
                delete imgStr;
                fclose(f1);
                return;
            }
            for (int x = 0; x < width; x++) {
                colorMap->getRGB(p, &rgb);
                // Write the RGB pixels into the row
                row[3 * x] = colToByte(rgb.r);
                row[3 * x + 1] = colToByte(rgb.g);
                row[3 * x + 2] = colToByte(rgb.b);
                p += colorMap->getNumPixelComps();
            }

            if (!writer->writeRow(row_pointer)) {
                error(errIO, -1, "Failed to write into in-memory PNG file");
                gfree(row);
                delete writer;
                delete imgStr;
                fclose(f1);
                return;
            }
        }
        gfree(row);
        imgStr->close();
        delete imgStr;
    } else { // isMask == true
        int size = (width + 7) / 8;

        // PDF masks use 0 = draw current color, 1 = leave unchanged.
        // We invert this to provide the standard interpretation of alpha
        // (0 = transparent, 1 = opaque). If the colorMap already inverts
        // the mask we leave the data unchanged.
        int invert_bits = 0xff;
        if (colorMap) {
            GfxGray gray;
            unsigned char zero[gfxColorMaxComps];
            memset(zero, 0, sizeof(zero));
            colorMap->getGray(zero, &gray);
            if (colToByte(gray) == 0)
                invert_bits = 0x00;
        }

        if (!str->rewind()) {
            error(errInternal, -1, "Can't rewind image stream");
            delete writer;
            fclose(f1);
            return;
        }
        unsigned char *png_row = (unsigned char *)gmalloc(size);

        for (int ri = 0; ri < height; ++ri) {
            for (int i = 0; i < size; i++)
                png_row[i] = str->getChar() ^ invert_bits;

            if (!writer->writeRow(&png_row)) {
                error(errIO, -1, "Failed to write into in-memory PNG file");
                delete writer;
                fclose(f1);
                gfree(png_row);
                return;
            }
        }
        str->close();
        gfree(png_row);
    }

    str->close();

    writer->close();
    delete writer;
    fclose(f1);

    pages->addImage(std::string("data:image/png;base64,") + gbase64Encode(ims.getBuffer()), state);
#else
    return;
#endif
}

void HtmlOutputDev::drawImageMask(GfxState *state, Object *ref, Stream *str, int width, int height, bool invert, bool interpolate, bool inlineImg)
{

    if (ignoreImages) {
        OutputDev::drawImageMask(state, ref, str, width, height, invert, interpolate, inlineImg);
        return;
    }

    // dump JPEG file
    if (dumpJPEG && str->getKind() == strDCT) {
        drawJpegImage(state, str);
    } else {
#if ENABLE_LIBPNG
        drawPngImage(state, str, width, height, nullptr, true);
#else
        OutputDev::drawImageMask(state, ref, str, width, height, invert, interpolate, inlineImg);
#endif
    }
}

void HtmlOutputDev::drawImage(GfxState *state, Object *ref, Stream *str, int width, int height, GfxImageColorMap *colorMap, bool interpolate, const int *maskColors, bool inlineImg)
{

    if (ignoreImages) {
        OutputDev::drawImage(state, ref, str, width, height, colorMap, interpolate, maskColors, inlineImg);
        return;
    }

    // dump JPEG file
    if (dumpJPEG && str->getKind() == strDCT && (colorMap->getNumPixelComps() == 1 || colorMap->getNumPixelComps() == 3) && !inlineImg) {
        drawJpegImage(state, str);
    } else {
#if ENABLE_LIBPNG
        drawPngImage(state, str, width, height, colorMap);
#else
        OutputDev::drawImage(state, ref, str, width, height, colorMap, interpolate, maskColors, inlineImg);
#endif
    }
}

void HtmlOutputDev::doProcessLink(AnnotLink *link)
{
    double _x1, _y1, _x2, _y2;
    int x1, y1, x2, y2;

    link->getRect(&_x1, &_y1, &_x2, &_y2);
    cvtUserToDev(_x1, _y1, &x1, &y1);

    cvtUserToDev(_x2, _y2, &x2, &y2);

    HtmlLink t((double)x1, (double)y2, (double)x2, (double)y1, getLinkDest(link));
    pages->AddLink(t);
}

std::string HtmlOutputDev::getLinkDest(AnnotLink *link)
{
    if (!link->getAction())
        return std::string();
    switch (link->getAction()->getKind()) {
    case actionGoTo: {
        int destPage = 1;
        const LinkGoTo *ha = static_cast<const LinkGoTo *>(link->getAction());
        std::unique_ptr<LinkDest> dest;
        if (ha->getDest() != nullptr)
            dest = std::make_unique<LinkDest>(*ha->getDest());
        else if (ha->getNamedDest() != nullptr)
            dest = catalog->findDest(ha->getNamedDest());

        if (dest) {
            std::string file = gbasename(Docname.c_str());

            if (dest->isPageRef()) {
                const Ref pageref = dest->getPageRef();
                destPage = catalog->findPage(pageref);
            } else {
                destPage = dest->getPageNum();
            }

            file += "#";
            file += std::to_string(destPage);

            return file;
        } else {
            return std::string();
        }
    }
    case actionGoToR: {
        const LinkGoToR *ha = static_cast<const LinkGoToR *>(link->getAction());
        std::unique_ptr<LinkDest> dest;
        int destPage = 1;
        std::string file;
        if (ha->getFileName()) {
            file = ha->getFileName()->c_str();
        }
        if (ha->getDest() != nullptr)
            dest = std::make_unique<LinkDest>(*ha->getDest());
        if (dest) {
            if (!(dest->isPageRef()))
                destPage = dest->getPageNum();
            file += '#';
            file += std::to_string(destPage);
        }
        return file;
    }
    case actionURI: {
        const LinkURI *ha = static_cast<const LinkURI *>(link->getAction());
        return ha->getURI();
    }
    case actionLaunch: {
        const LinkLaunch *ha = static_cast<const LinkLaunch *>(link->getAction());
        return ha->getFileName() ? std::string(ha->getFileName()->c_str()) : std::string();
    }
    default:
        return std::string();
    }
}

void HtmlOutputDev::generateOutline(PDFDoc *doc, std::vector<ReadPDFOutputs::OutlineItem> &dest) {
  Outline *outline = doc->getOutline();
  if (!outline) return;
  const std::vector<OutlineItem *> *outlines = outline->getItems();
  if (!outlines) return;
  generateOutlineLevel(outlines, dest);
}

void HtmlOutputDev::generateOutlineLevel(const std::vector<OutlineItem *> *outlines,
                                         std::vector<ReadPDFOutputs::OutlineItem> &dest) {
  for (OutlineItem *item : *outlines) {
    const std::vector<Unicode> &title = item->getTitle();
    std::string titleStr = HtmlFont::codepointsEncode(title.data(), title.size());
    const int itemPage = getOutlinePageNum(item);

    dest.push_back(ReadPDFOutputs::OutlineItem());
    ReadPDFOutputs::OutlineItem &serItem = dest.back();
    if (itemPage > 0) serItem.page = itemPage;
    serItem.title = titleStr;

    item->open();
    if (item->hasKids() && item->getKids()) generateOutlineLevel(item->getKids(), serItem.children);
  }
}

int HtmlOutputDev::getOutlinePageNum(OutlineItem *item)
{
    const LinkAction *action = item->getAction();
    const LinkGoTo *link = nullptr;
    std::unique_ptr<LinkDest> linkdest;
    int pagenum = -1;

    if (!action || action->getKind() != actionGoTo)
        return pagenum;

    link = static_cast<const LinkGoTo *>(action);

    if (!link || !link->isOk())
        return pagenum;

    if (link->getDest())
        linkdest = std::make_unique<LinkDest>(*link->getDest());
    else if (link->getNamedDest())
        linkdest = catalog->findDest(link->getNamedDest());

    if (!linkdest)
        return pagenum;

    if (linkdest->isPageRef()) {
        const Ref pageref = linkdest->getPageRef();
        pagenum = catalog->findPage(pageref);
    } else {
        pagenum = linkdest->getPageNum();
    }

    return pagenum;
}
