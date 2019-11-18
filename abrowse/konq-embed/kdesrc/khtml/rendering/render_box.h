/**
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * $Id: render_box.h,v 1.31 2001/02/18 00:31:01 mueller Exp $
 */
#ifndef RENDER_BOX_H
#define RENDER_BOX_H

#include "render_object.h"
#include "loader.h"

namespace khtml {
    class CachedObject;

class RenderBox : public RenderObject
{


// combines ElemImpl & PosElImpl (all rendering objects are positioned)
// should contain all border and padding handling

public:
    RenderBox();
    virtual ~RenderBox();

    virtual const char *renderName() const { return "RenderBox"; }

    virtual void setStyle(RenderStyle *style);
    virtual void addChild(RenderObject *newChild, RenderObject *beforeChild = 0);

    virtual void print(QPainter *p, int _x, int _y, int _w, int _h,
                       int _tx, int _ty);

    virtual void updateSize();
    virtual void updateHeight();
    virtual void close();

    virtual QSize contentSize() const;
    virtual QSize contentOffset() const;
    virtual QSize paddingSize() const;
    virtual QSize size() const;

    virtual short minWidth() const { return m_minWidth; }
    virtual short maxWidth() const { return m_maxWidth; }

    virtual short contentWidth() const;
    virtual int contentHeight() const;

    virtual bool absolutePosition(int &xPos, int &yPos, bool f = false);

    virtual void setPos( int xPos, int yPos );

    virtual int xPos() const { return m_x; }
    virtual int yPos() const { return m_y; }
    virtual short width() const;
    virtual int height() const;

    virtual short marginTop() const { return m_marginTop; }
    virtual short marginBottom() const { return m_marginBottom; }
    virtual short marginLeft() const { return m_marginLeft; }
    virtual short marginRight() const { return m_marginRight; }

    virtual void setSize( int width, int height ) { m_width = width; m_height = height; }
    virtual void setWidth( int width ) { m_width = width; }
    virtual void setHeight( int height ) { m_height = height; }

    // for table cells
    virtual short baselineOffset() const;

    virtual short verticalPositionHint() const;
    virtual int bidiHeight() const;

    virtual void position(int x, int y, int from, int len, int width, bool reverse, bool firstLine);
    virtual unsigned int width( int, int) const { return width(); }

    virtual int lowestPosition() const;
    virtual int rightmostPosition() const;

    void repaint();

    virtual void repaintRectangle(int x, int y, int w, int h);

    virtual void setPixmap(const QPixmap &, const QRect&, CachedImage *, bool *manualUpdate);

    virtual short containingBlockWidth() const;

    virtual void calcWidth();
    virtual void calcHeight();

protected:
    virtual void printBoxDecorations(QPainter *p,int _x, int _y,
                                       int _w, int _h, int _tx, int _ty);
    void printBackground(QPainter *p, const QColor &c, CachedImage *bg, int clipy, int cliph, int _tx, int _ty, int w, int h);
    void outlineBox(QPainter *p, int _tx, int _ty, const char *color = "red");

    virtual int borderTopExtra() { return 0; }
    virtual int borderBottomExtra() { return 0; }

    void relativePositionOffset(int &tx, int &ty);

    void calcAbsoluteHorizontal();
    void calcAbsoluteVertical();

    void calcHorizontalMargins(const Length& ml, const Length& mr, int cw);

    // the actual height of the contents + borders + padding
    int m_height;

    int m_y;
    short m_x;

    // the actual width of the contents + borders + padding
    short m_width;

    short m_marginTop;
    short m_marginBottom;
    short m_marginLeft;
    short m_marginRight;

    /*
     * the minimum width the element needs, to be able to render
     * it's content without clipping
     */
    short m_minWidth;
    /* The maximum width the element can fill horizontally
     * ( = the width of the element with line breaking disabled)
     */
    short m_maxWidth;
};


}; //namespace

#endif
