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
// Copyright (C) 2008 Boris Toloknov <tlknv@yandex.ru>
// Copyright (C) 2010 Albert Astals Cid <aacid@kde.org>
// Copyright (C) 2013 Julien Nabet <serval2412@yahoo.fr>
//
// To see a description of the changes please see the Changelog file that
// came with your tarball or type make ChangeLog if you are building from git
//
//========================================================================

#include "HtmlLinks.h"

#include <utility>

HtmlLink::HtmlLink(double xmin, double ymin, double xmax, double ymax, std::string _dest) : dest(std::move(_dest))
{
    if (xmin < xmax) {
        Xmin = xmin;
        Xmax = xmax;
    } else {
        Xmin = xmax;
        Xmax = xmin;
    }
    if (ymin < ymax) {
        Ymin = ymin;
        Ymax = ymax;
    } else {
        Ymin = ymax;
        Ymax = ymin;
    }
}

bool HtmlLink::isEqualDest(const HtmlLink &x) const
{
    return dest == x.dest;
}

bool HtmlLink::inLink(double xmin, double ymin, double xmax, double ymax) const
{
    double y = (ymin + ymax) / 2;
    if (y > Ymax)
        return false;
    return (y > Ymin) && (xmin < Xmax) && (xmax > Xmin);
}

bool HtmlLinks::inLink(double xmin, double ymin, double xmax, double ymax, int &p) const
{
    for (size_t i = 0; i < accu.size(); ++i) {
        if (accu[i].inLink(xmin, ymin, xmax, ymax)) {
            p = static_cast<int>(i);
            return true;
        }
    }
    return false;
}

const HtmlLink *HtmlLinks::getLink(int i) const
{
    return &accu[i];
}
