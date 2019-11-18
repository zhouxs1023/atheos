/*  libatheos.so - the highlevel API library for AtheOS
 *  Copyright (C) 1999 - 2001  Kurt Skauen
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of version 2 of the GNU Library
 *  General Public License as published by the Free Software
 *  Foundation.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public
 *  License along with this library; if not, write to the Free
 *  Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
 *  MA 02111-1307, USA
 */

#ifndef	__F_GUI_WINDOW_H
#define	__F_GUI_WINDOW_H

#include <gui/view.h>
#include <util/looper.h>

namespace os
{
#if 0 // Fool Emacs auto-indent
}
#endif

class Menu;
class View;
struct WR_Render_s;

enum
{
    WND_NO_CLOSE_BUT    = 0x00000001,
    WND_NO_ZOOM_BUT     = 0x00000002,
    WND_NO_DEPTH_BUT    = 0x00000004,
    WND_NOT_MOVEABLE    = 0x00000008,
    WND_NOT_H_RESIZABLE = 0x00000010,
    WND_NOT_V_RESIZABLE = 0x00000020,
    WND_NOT_RESIZABLE   = 0x00000030,
    WND_NO_TITLE	= 0x00000040,
    WND_NO_BORDER	= 0x00000080,
    WND_FRONTMOST	= 0x00000100,
    WND_BACKMOST	= 0x00000200,
    WND_MODAL		= 0x00000400,
    WND_SYSTEM		= 0x00010000
};

enum
{
    CURRENT_DESKTOP = 0,
    ALL_DESKTOPS	  = ~0
};

/** 
 * \ingroup gui
 * \par Description:
 *
 * \sa
 * \author	Kurt Skauen (kurt@atheos.cx)
 *****************************************************************************/

class	Window : public Looper
{
public:
    Window( const Rect& cFrame, const std::string& cName, const std::string& cTitle,
	    uint32 nFlags = 0, uint32 nDesktopMask = CURRENT_DESKTOP );
    ~Window();

      // Hook functions
    virtual void	FrameMoved( const Point& cDelta );
    virtual void	FrameSized( const Point& cDelta );
    virtual void	ScreenModeChanged( const IPoint& cNewRes, color_space eColorSpace );
    virtual void	DesktopActivated( int nDesktop, bool bActive );
    virtual void	DesktopsChanged( uint32 nOldDesktops, uint32 nNewDesktops );

    virtual void	__WI_reserved1__();
    virtual void	__WI_reserved2__();
    virtual void	__WI_reserved3__();
    virtual void	__WI_reserved4__();
    virtual void	__WI_reserved5__();
    virtual void	__WI_reserved6__();
    
    void		SetFlags( uint32 nFlags );
    uint32		GetFlags() const;

    void		SetSizeLimits( const Point& cMinSize, const Point& cMaxSize );
    void		SetAlignment( const IPoint& cSize = IPoint(1,1), const IPoint& cSizeOffset = IPoint(0,0),
				      const IPoint& cPos = IPoint(1,1), const IPoint& cPosOffset = IPoint(0,0) );
  
    void		AddChild( View* pcChild, bool bAssignTabOrder = false );
    void		RemoveChild( View* pcChild );

    void		MakeFocus( bool bFocus = true );
    void		Close() { Quit(); }
    void		Show( bool bMakeVisible = true );
    void		Hide() { Show( false ); }
    void		Start();
    void		SetTitle( const std::string& cTitle );
    std::string		GetTitle( void ) const;

//  void		SetMenuBar( Menu* pcMenu );


    void		Flush();
    void		Sync();

    View*		FindView( const std::string& cName ) const	{ return( dynamic_cast<View*>(FindHandler( cName )) );	}
    View*		FindView( const Point& cPos ) const;
    View*		GetFocusChild( void ) const			{ return( dynamic_cast<View*>(GetDefaultHandler()) );	}
    View*		SetFocusChild( View* pcChild );
    void		SetDefaultButton( View* pcView );
    View*		GetDefaultButton() const;

    void		SetDefaultWheelView( View* pcView );
    View*		GetDefaultWheelView() const;
  
    Rect		GetFrame() const;
    Rect		GetBounds() const;

    void		SetFrame( const Rect& cRect, bool bNotifyServer = true );
  
    void		MoveBy( const Point& cDelta );
    void		MoveBy( float vDeltaX, float vDeltaY );
    void		MoveTo( const Point& cPos );
    void		MoveTo( float x, float y );

    void		ResizeBy( const Point& cDelta );
    void		ResizeBy( float vDeltaW, float vDeltaH );
    void		ResizeTo( const Point& cSize );
    void		ResizeTo( float w, float h );

    void 		SetShapeRegion( const Region& cReg );
    void		ClearShapeRegion();
  
    bool		IsActive() const;
    int			ToggleDepth();

      //	From Looper:

    void		DispatchMessage( Message* pcMessage, Handler* pcHandler );

private:
    friend	class	View;
    friend	class	Bitmap;
    friend	class	MenuItem;
  
    Window( Bitmap* pcBitmap );
    void	_ViewDeleted( View* pcView );
//    void	Init();
    void	_Cleanup();
    void*	_AllocRenderCmd( uint32 nCmd, View* pcView, uint32 nSize );
    void	_HandleActivate( bool bIsActive, const Point& cMousePos );
    void	_CallMouseMoved( View* pcView, uint32 nButtons, int nWndTransit, Message* pcData );
    void	_MouseEvent( const Point& cNewPos, uint32 nButtons, Message* pcData, bool bReEntered );

    void	_SetMenuOpen( bool bOpen );

    void	_DeleteViewFromServer( View* pcView );
    Point	_GetPenPosition( int nViewHandle );
    View*	_GetPrevTabView( View* pcCurrent );
    View*	_GetNextTabView( View* pcCurrent );
    void	_DefaultColorsChanged();

    View*	_GetTopView() const;
    port_id	_GetAppserverPort() const;

    class Private;
    Private* m;
};

} // end of namespace

#endif	// __F_GUI_WINDOW_H
