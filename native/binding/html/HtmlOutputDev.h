//========================================================================
//
// HtmlOutputDev.h
//
// Copyright 1997 Derek B. Noonburg
//
// Changed 1999 by G.Ovtcharov
//========================================================================

//========================================================================
//
// Modified under the Poppler project - http://poppler.freedesktop.org
//
// All changes made under the Poppler project to this file are licensed
// under GPL version 2 or later
//
// Copyright (C) 2006, 2007, 2009, 2012, 2018, 2019 Albert Astals Cid <aacid@kde.org>
// Copyright (C) 2008, 2009 Warren Toomey <wkt@tuhs.org>
// Copyright (C) 2009, 2011 Carlos Garcia Campos <carlosgc@gnome.org>
// Copyright (C) 2009 Kovid Goyal <kovid@kovidgoyal.net>
// Copyright (C) 2010 Hib Eris <hib@hiberis.nl>
// Copyright (C) 2011 Joshua Richardson <jric@chegg.com>
// Copyright (C) 2011 Stephen Reichling <sreichling@chegg.com>
// Copyright (C) 2012 Igor Slepchin <igor.redhat@gmail.com>
// Copyright (C) 2012 Fabio D'Urso <fabiodurso@hotmail.it>
// Copyright (C) 2013 Thomas Freitag <Thomas.Freitag@alfa.de>
// Copyright (C) 2018 Klarälvdalens Datakonsult AB, a KDAB Group company, <info@kdab.com>. Work sponsored by the LiMux
// project of the city of Munich Copyright (C) 2019 Oliver Sander <oliver.sander@tu-dresden.de>
//
// To see a description of the changes please see the Changelog file that
// came with your tarball or type make ChangeLog if you are building from git
//
//========================================================================

#pragma once

#include <cstdio>

#include <goo/gbasename.h>
#include <napi.h>
#include <poppler/Catalog.h>
#include <poppler/GfxFont.h>
#include <poppler/Link.h>
#include <poppler/OutputDev.h>
#include <poppler/UnicodeMap.h>

#include "HtmlFonts.h"
#include "HtmlLinks.h"

class GfxState;
class GooString;
class HtmlImage;
class PDFDoc;
class OutlineItem;

//------------------------------------------------------------------------
// HtmlString
//------------------------------------------------------------------------

enum UnicodeTextDirection { textDirUnknown, textDirLeftRight, textDirRightLeft, textDirTopBottom };

class HtmlString {
 public:
  // Constructor.
  HtmlString(GfxState *state, double fontSize, HtmlFontAccu *fonts);

  // Destructor.
  ~HtmlString();

  HtmlString(const HtmlString &) = delete;
  HtmlString &operator=(const HtmlString &) = delete;

  // Add a character to the string.
  void addChar(GfxState *state, double x, double y, double dx, double dy, Unicode u);
  HtmlLink *getLink() { return link; }
  const HtmlFont &getFont() const { return *fonts->Get(fontpos); }
  void endString();  // postprocessing

 private:
  // aender die text variable
  HtmlLink *link;
  double xMin, xMax;   // bounding box x coordinates
  double yMin, yMax;   // bounding box y coordinates
  int col;             // starting column
  Unicode *text;       // the text
  double *xRight;      // right-hand x coord of each char
  HtmlString *yxNext;  // next string in y-major order
  HtmlString *xyNext;  // next string in x-major order
  int fontpos;
  GooString *htext;
  int len;                   // length of text and xRight
  int size;                  // size of text and xRight arrays
  UnicodeTextDirection dir;  // direction (left to right/right to left)
  HtmlFontAccu *fonts;

  friend class HtmlPage;
};

//------------------------------------------------------------------------
// HtmlPage
//------------------------------------------------------------------------

class HtmlPage {
 public:
  // Constructor.
  HtmlPage(bool newMergeLines, double newWordBreakThreshold);

  // Destructor.
  ~HtmlPage();

  HtmlPage(const HtmlPage &) = delete;
  HtmlPage &operator=(const HtmlPage &) = delete;

  // Begin a new string.
  void beginString(GfxState *state, const GooString *s);

  // Add a character to the current string.
  void addChar(GfxState *state, double x, double y, double dx, double dy, double ox, double oy, const Unicode *u,
               int uLen);  // unsigned char c);

  void updateFont(GfxState *state);

  // End the current string, sorting it into the list of strings.
  void endString();

  // Coalesce strings that look like parts of the same line.
  void coalesce();

  // Find a string.  If <top> is true, starts looking at top of page;
  // otherwise starts looking at <xMin>,<yMin>.  If <bottom> is true,
  // stops looking at bottom of page; otherwise stops looking at
  // <xMax>,<yMax>.  If found, sets the text bounding rectangle and
  // returns true; otherwise returns false.

  // new functions
  void AddLink(const HtmlLink &x) { links->AddLink(x); }

  // add an image to the current page
  void addImage(GooString *fname, GfxState *state);

  // number of images on the current page
  int getNumImages() { return imgList->size(); }

  Napi::Object dump(Napi::Env &env, int pageNumber);

  // Clear the page.
  void clear();

  void conv();

 private:
  HtmlFont *getFont(HtmlString *hStr) { return fonts->Get(hStr->fontpos); }

  double fontSize;  // current font size

  HtmlString *curStr;  // currently active string

  HtmlString *yxStrings;        // strings in y-major order
  HtmlString *xyStrings;        // strings in x-major order
  HtmlString *yxCur1, *yxCur2;  // cursors for yxStrings list

  void setDocName(const char *fname);

  // marks the position of the fonts that belong to current page (for noframes)
  int fontsPageMarker;
  HtmlFontAccu *fonts;
  HtmlLinks *links;
  std::vector<HtmlImage *> *imgList;

  GooString *DocName;
  int pageWidth;
  int pageHeight;
  int firstPage;  // used to begin the numeration of pages
  bool mergeLines;
  double wordBreakThreshold;

  friend class HtmlOutputDev;
};

//------------------------------------------------------------------------
// HtmlOutputDev
//------------------------------------------------------------------------

class HtmlOutputDev : public OutputDev {
 public:
  // Open a text output file.  If <fileName> is nullptr, no file is written
  // (this is useful, e.g., for searching text).  If <useASCII7> is true,
  // text is converted to 7-bit ASCII; otherwise, text is converted to
  // 8-bit ISO Latin-1.  <useASCII7> should also be set for Japanese
  // (EUC-JP) text.  Because <rawOrder> is always true, the text is kept in
  // content stream order.
  HtmlOutputDev(Napi::Env &newEnv, Catalog *catalogA, const char *fileName, int numPages, bool newIgnoreImages,
                bool newShowHiddenCharacters, bool mergeLines, double wordBreakThreshold);

  // Destructor.
  virtual ~HtmlOutputDev();

  //---- get info about output device

  // Does this device use upside-down coordinates?
  // (Upside-down means (0,0) is the top left corner of the page.)
  bool upsideDown() override { return true; }

  // Does this device use drawChar() or drawString()?
  bool useDrawChar() override { return true; }

  // Does this device use beginType3Char/endType3Char?  Otherwise,
  // text in Type 3 fonts will be drawn with drawChar/drawString.
  bool interpretType3Chars() override { return false; }

  // Does this device need non-text content?
  bool needNonText() override { return true; }

  //----- initialization and control

  bool checkPageSlice(Page *p, double hDPI, double vDPI, int rotate, bool useMediaBox, bool crop, int sliceX,
                      int sliceY, int sliceW, int sliceH, bool printing, bool (*abortCheckCbk)(void *data) = nullptr,
                      void *abortCheckCbkData = nullptr,
                      bool (*annotDisplayDecideCbk)(Annot *annot, void *user_data) = nullptr,
                      void *annotDisplayDecideCbkData = nullptr) override {
    docPage = p;
    return true;
  }

  void startPage(int pageNum, GfxState *state, XRef *xref) override;
  void endPage() override;
  void updateFont(GfxState *state) override;

  //----- text drawing
  void beginString(GfxState *state, const GooString *s) override;
  void endString(GfxState *state) override;
  void drawChar(GfxState *state, double x, double y, double dx, double dy, double originX, double originY,
                CharCode code, int nBytes, const Unicode *u, int uLen) override;

  void drawImageMask(GfxState *state, Object *ref, Stream *str, int width, int height, bool invert, bool interpolate,
                     bool inlineImg) override;
  void drawImage(GfxState *state, Object *ref, Stream *str, int width, int height, GfxImageColorMap *colorMap,
                 bool interpolate, const int *maskColors, bool inlineImg) override;

  int getPageWidth() { return maxPageWidth; }
  int getPageHeight() { return maxPageHeight; }

  Napi::Value outlineAsNapiArray(PDFDoc *doc);

 private:
  void doProcessLink(AnnotLink *link);
  GooString *getLinkDest(AnnotLink *link);
  Napi::Array newOutlineLevel(const std::vector<OutlineItem *> *outlines);
  int getOutlinePageNum(OutlineItem *item);
  void drawJpegImage(GfxState *state, Stream *str);
  void drawPngImage(GfxState *state, Stream *str, int width, int height, GfxImageColorMap *colorMap,
                    bool isMask = false);

  HtmlPage *pages;  // text for the current page
  bool dumpJPEG;
  int pageNum;
  int currPageIndex = 0;
  int maxPageWidth;
  int maxPageHeight;
  GooString *Docname;
  Catalog *catalog;
  Page *docPage;
  bool ignoreImages;
  bool showHiddenCharacters;

  Napi::Env &env;

  friend class HtmlPage;

 public:
  Napi::Array allPages;
};
