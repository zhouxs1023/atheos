/*
 *  The AtheOS application server
 *  Copyright (C) 1999 - 2001 Kurt Skauen
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of version 2 of the GNU Library
 *  General Public License as published by the Free Software
 *  Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "amigadecorator.h"
#include <layer.h>

#include <gui/window.h>

using namespace os;

AmigaDecorator::AmigaDecorator( Layer* pcLayer, uint32 nWndFlags ) :
    WindowDecorator( pcLayer, nWndFlags )
{
    m_nFlags = nWndFlags;

    m_bHasFocus   = false;
    m_bCloseState = false;
    m_bZoomState  = false;
    m_bDepthState = false;
  
    m_sFontHeight = pcLayer->GetFontHeight();
    
    CalculateBorderSizes();
}

AmigaDecorator::~AmigaDecorator()
{
}

void AmigaDecorator::CalculateBorderSizes()
{
    if ( m_nFlags & WND_NO_BORDER ) {
	m_vLeftBorder   = 0;
	m_vRightBorder  = 0;
	m_vTopBorder    = 0;
	m_vBottomBorder = 0;
    } else {
	if ( (m_nFlags & (WND_NO_TITLE | WND_NO_CLOSE_BUT | WND_NO_ZOOM_BUT | WND_NO_DEPTH_BUT | WND_NOT_MOVEABLE)) ==
	     (WND_NO_TITLE | WND_NO_CLOSE_BUT | WND_NO_ZOOM_BUT | WND_NO_DEPTH_BUT | WND_NOT_MOVEABLE) ) {
	    m_vTopBorder = 4;
	} else {
	    m_vTopBorder = float(m_sFontHeight.ascender + m_sFontHeight.descender + 6);
	}
	m_vLeftBorder   = 4;
	m_vRightBorder  = 4;
	m_vBottomBorder = 4;
    }
    
}

void AmigaDecorator::SetFlags( uint32 nFlags )
{
    Layer* pcView = GetLayer();
    m_nFlags = nFlags;
    CalculateBorderSizes();
    pcView->Invalidate();
    Layout();
    
}

void AmigaDecorator::FontChanged()
{
    Layer* pcView = GetLayer();
    m_sFontHeight = pcView->GetFontHeight();
    CalculateBorderSizes();
    pcView->Invalidate();
    Layout();
}

void AmigaDecorator::Layout()
{
    if ( m_nFlags & WND_NO_CLOSE_BUT ) {
	m_cCloseRect.left   = 0;
	m_cCloseRect.right  = 0;
	m_cCloseRect.top    = 0;
	m_cCloseRect.bottom = 0;
    } else {
	m_cCloseRect.left = 0;
	m_cCloseRect.right = m_vTopBorder - 1;
	m_cCloseRect.top = 0;
	m_cCloseRect.bottom = m_vTopBorder - 1;
    }

    m_cToggleRect.right = m_cBounds.right;
    if ( m_nFlags & WND_NO_DEPTH_BUT ) {
	m_cToggleRect.left = m_cToggleRect.right;
    } else {
	m_cToggleRect.left = ceil(m_cToggleRect.right - m_vTopBorder * 1.5f);
    }
    m_cToggleRect.top = 0;
    m_cToggleRect.bottom = m_vTopBorder - 1;
    

    if ( m_nFlags & WND_NO_ZOOM_BUT ) {
	m_cZoomRect.left  = m_cToggleRect.left;
	m_cZoomRect.right = m_cToggleRect.left;
    } else {
	if ( m_nFlags & WND_NO_DEPTH_BUT ) {
	    m_cZoomRect.right = m_cBounds.right;
	} else {
	    m_cZoomRect.right  = m_cToggleRect.left - 1.0f;
	}
	m_cZoomRect.left   = ceil( m_cZoomRect.right - m_vTopBorder * 1.5f);
    }
    m_cZoomRect.top  = 0;
    m_cZoomRect.bottom  = m_vTopBorder - 1;

    if ( m_nFlags & WND_NO_CLOSE_BUT ) {
	m_cDragRect.left  = 0.0f;
    } else {
	m_cDragRect.left  = m_cCloseRect.right + 1.0f;
    }
    if ( m_nFlags & WND_NO_ZOOM_BUT ) {
	if ( m_nFlags & WND_NO_DEPTH_BUT ) {
	    m_cDragRect.right = m_cBounds.right;
	} else {
	    m_cDragRect.right = m_cToggleRect.left - 1.0f;
	}
    } else {
	m_cDragRect.right  = m_cZoomRect.left - 1.0f;
    }
    m_cDragRect.top  = 0;
    m_cDragRect.bottom  = m_vTopBorder - 1;
}

Point AmigaDecorator::GetMinimumSize()
{
    Point cMinSize( 0.0f, m_vTopBorder + m_vBottomBorder );

    if ( (m_nFlags & WND_NO_CLOSE_BUT) == 0 ) {
	cMinSize.x += m_cCloseRect.Width();
    }
    if ( (m_nFlags & WND_NO_ZOOM_BUT) == 0 ) {
	cMinSize.x += m_cZoomRect.Width();
    }
    if ( (m_nFlags & WND_NO_DEPTH_BUT) == 0 ) {
	cMinSize.x += m_cToggleRect.Width();
    }
    if ( cMinSize.x < m_vLeftBorder + m_vRightBorder ) {
	cMinSize.x = m_vLeftBorder + m_vRightBorder;
    }
    return( cMinSize );
}

WindowDecorator::hit_item AmigaDecorator::HitTest( const Point& cPosition )
{
    if ( cPosition.x < 4 ) {
	if ( cPosition.y < 4 ) {
	    return( HIT_SIZE_LT );
	} else if ( cPosition.y > m_cBounds.bottom - 4 ) {
	    return( HIT_SIZE_LB );
	} else {
	    return( HIT_SIZE_LEFT );
	}
    } else if ( cPosition.x > m_cBounds.right - 4 ) {
	if ( cPosition.y < 4 ) {
	    return( HIT_SIZE_RT );
	} else if ( cPosition.y > m_cBounds.bottom - 4 ) {
	    return( HIT_SIZE_RB );
	} else {
	    return( HIT_SIZE_RIGHT );
	}
    } else if ( cPosition.y < 4 ) {
	return( HIT_SIZE_TOP );
    } else if ( cPosition.y > m_cBounds.bottom - 4 ) {
	return( HIT_SIZE_BOTTOM );
    }
    if ( m_cCloseRect.DoIntersect( cPosition ) ) {
	return( HIT_CLOSE );
    } else if ( m_cZoomRect.DoIntersect( cPosition ) ) {
	return( HIT_ZOOM );
    } else if ( m_cToggleRect.DoIntersect( cPosition ) ) {
	return( HIT_DEPTH );
    } else if ( m_cDragRect.DoIntersect( cPosition ) ) {
	return( HIT_DRAG );
    }
    return( HIT_NONE );
}

Rect AmigaDecorator::GetBorderSize()
{
    return( Rect( m_vLeftBorder, m_vTopBorder, m_vRightBorder, m_vBottomBorder ) );
}

void AmigaDecorator::SetTitle( const char* pzTitle )
{
    m_cTitle = pzTitle;
    Render( m_cBounds );
}

void AmigaDecorator::SetFocusState( bool bHasFocus )
{
    m_bHasFocus = bHasFocus;
    Render( m_cBounds );
}

void AmigaDecorator::SetWindowFlags( uint32 nFlags )
{
    m_nFlags = nFlags;
}

void AmigaDecorator::FrameSized( const Rect& cFrame )
{
    Layer* pcView = GetLayer();
    Point cDelta( cFrame.Width() - m_cBounds.Width(), cFrame.Height() - m_cBounds.Height() );
    m_cBounds = cFrame.Bounds();

    Layout();
    if ( cDelta.x != 0.0f ) {
	Rect cDamage = m_cBounds;

	cDamage.left = m_cZoomRect.left - fabs(cDelta.x)  - 2.0f;
	pcView->Invalidate( cDamage );
    }
    if ( cDelta.y != 0.0f ) {
	Rect cDamage = m_cBounds;

	cDamage.top = cDamage.bottom - max( m_vBottomBorder, m_vBottomBorder + cDelta.y ) - 1.0f;
	pcView->Invalidate( cDamage );
    }
}

void AmigaDecorator::SetCloseButtonState( bool bPushed )
{
    m_bCloseState = bPushed;
    if ( (m_nFlags & WND_NO_CLOSE_BUT) == 0 ) {
	Color32_s sFillColor =  m_bHasFocus ? GetDefaultColor( PEN_SELWINTITLE ) : GetDefaultColor( PEN_WINTITLE );
	DrawClose( m_cCloseRect, sFillColor, m_bCloseState == 1 );
    }
}

void AmigaDecorator::SetZoomButtonState( bool bPushed )
{
    m_bZoomState = bPushed;
    if ( (m_nFlags & WND_NO_ZOOM_BUT) == 0 ) {
	Color32_s sFillColor =  m_bHasFocus ? GetDefaultColor( PEN_SELWINTITLE ) : GetDefaultColor( PEN_WINTITLE );
	DrawZoom( m_cZoomRect, sFillColor, m_bZoomState == 1 );
    }
}

void AmigaDecorator::SetDepthButtonState( bool bPushed )
{
    m_bDepthState = bPushed;
    if ( (m_nFlags & WND_NO_DEPTH_BUT) == 0 ) {
	Color32_s sFillColor =  m_bHasFocus ? GetDefaultColor( PEN_SELWINTITLE ) : GetDefaultColor( PEN_WINTITLE );
	DrawDepth( m_cToggleRect, sFillColor, m_bDepthState == 1 );
    }
}


void AmigaDecorator::DrawDepth( const Rect& cRect, const Color32_s& sFillColor, bool bRecessed )
{
    Rect	L,R;

    L.left = cRect.left + ((cRect.Width()+1.0f) / 7);
    L.top = cRect.top + ((cRect.Height()+1.0f) / 7);

    L.right = L.left + ((cRect.Width()+1.0f) * 4 / 7);
    L.bottom = L.top + ((cRect.Height()+1.0f) / 2);

    R.right = cRect.right - ((cRect.Width()+1.0f) / 7);
    R.bottom = cRect.bottom - ((cRect.Height()+1.0f) / 7);

    R.left = R.right - ((cRect.Width()+1.0f) * 4 / 7);
    R.top = R.bottom - ((cRect.Height()+1.0f) / 2);

    Layer* pcView = GetLayer();
  
    if ( bRecessed )
    {
	pcView->FillRect( cRect, sFillColor );
	pcView->DrawFrame( cRect, FRAME_RECESSED | FRAME_TRANSPARENT );
	pcView->DrawFrame( L, FRAME_RAISED | FRAME_TRANSPARENT );
	pcView->FillRect( R, sFillColor );
	pcView->DrawFrame( R, FRAME_RAISED | FRAME_TRANSPARENT );
    }
    else
    {
	pcView->FillRect( cRect, sFillColor );
	pcView->DrawFrame( cRect, FRAME_RAISED | FRAME_TRANSPARENT );
	pcView->DrawFrame( L, FRAME_RECESSED | FRAME_TRANSPARENT );
	pcView->FillRect( R, sFillColor );
	pcView->DrawFrame( R, FRAME_RAISED | FRAME_TRANSPARENT );
    }
}

//----------------------------------------------------------------------------
// NAME:
// DESC:
// NOTE:
// SEE ALSO:
//----------------------------------------------------------------------------

void AmigaDecorator::DrawZoom( const Rect& cRect, const Color32_s& sFillColor, bool bRecessed )
{
    Rect	L,R;

    L.left = cRect.left + ((cRect.Width()+1.0f) / 6);
    L.top = cRect.top + ((cRect.Height()+1.0f) / 6);

    L.right = cRect.right - ((cRect.Width()+1.0f) / 6);
    L.bottom = cRect.bottom - ((cRect.Height()+1.0f) / 6);

    R.left = L.left + 1;
    R.top = L.top + 1;

    R.right = R.left + ((cRect.Width()+1.0f) / 3);
    R.bottom = R.top + ((cRect.Height()+1.0f) / 3);

    Layer* pcView = GetLayer();
  
    if ( bRecessed )
    {
	pcView->FillRect( cRect, sFillColor );
	pcView->DrawFrame( cRect, FRAME_RECESSED | FRAME_TRANSPARENT );
	pcView->DrawFrame( R, FRAME_RAISED | FRAME_TRANSPARENT  );
	pcView->DrawFrame( L, FRAME_RECESSED | FRAME_TRANSPARENT );

	pcView->FillRect( Rect( L.left + 1, L.top + 1, L.right - 1, L.bottom - 1 ), sFillColor );
    }
    else
    {
	pcView->FillRect( cRect, sFillColor );
	pcView->DrawFrame( cRect, FRAME_RAISED | FRAME_TRANSPARENT );
	pcView->DrawFrame( L, FRAME_RAISED | FRAME_TRANSPARENT );
	pcView->DrawFrame( R, FRAME_RECESSED | FRAME_TRANSPARENT  );
    }
}

//----------------------------------------------------------------------------
// NAME:
// DESC:
// NOTE:
// SEE ALSO:
//----------------------------------------------------------------------------

void AmigaDecorator::DrawClose(  const Rect& cRect, const Color32_s& sFillColor, bool bRecessed  )
{
    Rect L;

    L.left = cRect.left + ((cRect.Width()+1.0f) / 3);
    L.top = cRect.top + ((cRect.Height()+1.0f) / 3);

    L.right = cRect.right - ((cRect.Width()+1.0f) / 3);
    L.bottom = cRect.bottom - ((cRect.Height()+1.0f) / 3);

    Layer* pcView = GetLayer();
  
    if ( bRecessed ) {
	pcView->FillRect( cRect, sFillColor );
	pcView->DrawFrame( cRect, FRAME_RECESSED | FRAME_TRANSPARENT );
	pcView->DrawFrame( L, FRAME_RAISED | FRAME_TRANSPARENT );
    } else {
	pcView->FillRect( cRect, sFillColor );
	pcView->DrawFrame( cRect, FRAME_RAISED | FRAME_TRANSPARENT );
	pcView->DrawFrame( L, FRAME_RECESSED | FRAME_TRANSPARENT );
    }
}

void AmigaDecorator::Render( const Rect& cUpdateRect )
{
    if ( m_nFlags & WND_NO_BORDER ) {
	return;
    }
  
    Color32_s sFillColor =  m_bHasFocus ? GetDefaultColor( PEN_SELWINTITLE ) : GetDefaultColor( PEN_WINTITLE );

    Layer* pcView = GetLayer();
  
    Rect  cOBounds = pcView->GetBounds();
    Rect  cIBounds = cOBounds;
  
    cIBounds.left += m_vLeftBorder - 1;
    cIBounds.right -= m_vRightBorder - 1;
    cIBounds.top += m_vTopBorder - 1;
    cIBounds.bottom -= m_vBottomBorder - 1;

    pcView->DrawFrame( cOBounds, FRAME_RAISED | FRAME_THIN | FRAME_TRANSPARENT );
    pcView->DrawFrame( cIBounds, FRAME_RECESSED | FRAME_THIN | FRAME_TRANSPARENT );
  
      // Bottom
    pcView->FillRect( Rect( cOBounds.left + 1, cIBounds.bottom + 1, cOBounds.right - 1, cOBounds.bottom - 1 ), sFillColor );
      // Left
    pcView->FillRect( Rect( cOBounds.left + 1, cOBounds.top + m_vTopBorder,
			    cIBounds.left - 1, cIBounds.bottom ), sFillColor );
      // Right
    pcView->FillRect( Rect( cIBounds.right + 1, cOBounds.top + m_vTopBorder,
			    cOBounds.right - 1, cIBounds.bottom ), sFillColor );

    if ( (m_nFlags & WND_NO_CLOSE_BUT) == 0 ) {
	DrawClose( m_cCloseRect, sFillColor, m_bCloseState == 1 );
    }

    if ( (m_nFlags & WND_NO_TITLE) == 0 ) {
	pcView->FillRect( m_cDragRect, sFillColor );
	pcView->SetFgColor( 255, 255, 255, 0 );
	pcView->SetBgColor( sFillColor );
	pcView->MovePenTo( m_cDragRect.left + 5,
			   (m_cDragRect.Height()+1.0f) / 2 -
			   (m_sFontHeight.ascender + m_sFontHeight.descender) / 2 + m_sFontHeight.ascender +
			   m_sFontHeight.line_gap * 0.5f );
    
	pcView->DrawString( m_cTitle.c_str(), -1 );
	pcView->DrawFrame( m_cDragRect, FRAME_RAISED | FRAME_TRANSPARENT );
    } else {
	pcView->FillRect( Rect( cOBounds.left + 1, cOBounds.top - 1, cOBounds.right - 1, cIBounds.top + 1 ), sFillColor );
    }

    if ( (m_nFlags & WND_NO_ZOOM_BUT) == 0 ) {
	DrawZoom( m_cZoomRect, sFillColor, m_bZoomState == 1 );
    }
    if ( (m_nFlags & WND_NO_DEPTH_BUT) == 0 ) {
	DrawDepth( m_cToggleRect, sFillColor, m_bDepthState == 1 );
    }
}

extern "C" int get_api_version()
{
    return( WNDDECORATOR_APIVERSION );
}

extern "C" WindowDecorator* create_decorator( Layer* pcLayer, uint32 nFlags )
{
    return( new AmigaDecorator( pcLayer, nFlags ) );
}
