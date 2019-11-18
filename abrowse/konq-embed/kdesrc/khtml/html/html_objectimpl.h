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
 * $Id: html_objectimpl.h,v 1.29 2001/02/13 10:30:25 pmk Exp $
 */
#ifndef HTML_OBJECTIMPL_H
#define HTML_OBJECTIMPL_H

#include "html_elementimpl.h"
#include "xml/dom_stringimpl.h"

#include <qstringlist.h>

class KHTMLView;

// -------------------------------------------------------------------------
namespace DOM {

class HTMLFormElementImpl;
class DOMStringImpl;

class HTMLAppletElementImpl : public HTMLElementImpl
{
public:
    HTMLAppletElementImpl(DocumentImpl *doc);

    ~HTMLAppletElementImpl();

    virtual const DOMString nodeName() const;
    virtual ushort id() const;

    virtual tagStatus startTag() const { return APPLETStartTag; }
    virtual tagStatus endTag() const { return APPLETEndTag; }

    virtual void parseAttribute(AttrImpl *token);

    virtual void attach(KHTMLView *w);
    virtual void detach();

protected:
    DOMStringImpl *codeBase;
    DOMStringImpl *name;
    DOMStringImpl *code;
    DOMStringImpl *archive;

    KHTMLView *view;
    khtml::VAlign valign;
};

// -------------------------------------------------------------------------

class HTMLEmbedElementImpl : public HTMLElementImpl
{
public:
    HTMLEmbedElementImpl(DocumentImpl *doc);

    ~HTMLEmbedElementImpl();

    virtual const DOMString nodeName() const;
    virtual ushort id() const;

    virtual tagStatus startTag() const { return OBJECTStartTag; }
    virtual tagStatus endTag() const { return OBJECTEndTag; }

    virtual void parseAttribute(AttrImpl *attr);

    virtual void attach(KHTMLView *w);
    virtual void detach();

    QString url;
    QString pluginPage;
    QString serviceType;
    bool hidden;
    QStringList param;
};

// -------------------------------------------------------------------------

class HTMLObjectElementImpl : public HTMLElementImpl
{
public:
    HTMLObjectElementImpl(DocumentImpl *doc);

    ~HTMLObjectElementImpl();

    virtual const DOMString nodeName() const;
    virtual ushort id() const;

    virtual tagStatus startTag() const { return OBJECTStartTag; }
    virtual tagStatus endTag() const { return OBJECTEndTag; }

    HTMLFormElementImpl *form() const;

    virtual void parseAttribute(AttrImpl *token);

    virtual void attach(KHTMLView *w);

    virtual void applyChanges(bool = true, bool = true);

    QString serviceType;
    QString url;
    QString classId;
    bool needWidgetUpdate;
};

// -------------------------------------------------------------------------

class HTMLParamElementImpl : public HTMLElementImpl
{
    friend class HTMLAppletElementImpl;
public:
    HTMLParamElementImpl(DocumentImpl *doc);

    ~HTMLParamElementImpl();

    virtual const DOMString nodeName() const;
    virtual ushort id() const;

    virtual tagStatus startTag() const { return PARAMStartTag; }
    virtual tagStatus endTag() const { return PARAMEndTag; }

    virtual void parseAttribute(AttrImpl *token);

    QString name() const { if(!m_name) return QString::null; return QConstString(m_name->s, m_name->l).string(); }
    QString value() const { if(!m_value) return QString::null; return QConstString(m_value->s, m_value->l).string(); }

 protected:
    DOMStringImpl *m_name;
    DOMStringImpl *m_value;
};

};
#endif
