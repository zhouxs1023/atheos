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

#include <stdio.h>
#include <limits.h>
#include <signal.h>
#include <atheos/types.h>
#include <atheos/time.h>

#include "ddriver.h"
#include "server.h"
#include "swindow.h"
#include "layer.h"
#include "sfont.h"
#include "sapplication.h"
#include "wndborder.h"
#include "bitmap.h"
#include "sprite.h"
#include "keyboard.h"
#include "inputnode.h"
#include "windowdecorator.h"
#include "config.h"

#include <atheos/kernel.h>

#include <gui/window.h>
#include <gui/guidefines.h>
#include <util/messenger.h>
#include <util/locker.h>
#include <util/string.h>
#include <appserver/protocol.h>

#include <macros.h>

SrvWindow* SrvWindow::s_pcLastMouseWindow = NULL;
SrvWindow* SrvWindow::s_pcDragWindow      = NULL;
SrvSprite* SrvWindow::s_pcDragSprite	  = NULL;
Message	   SrvWindow::s_cDragMessage(0);
bool	   SrvWindow::s_bIsDragging = false;

//----------------------------------------------------------------------------
// NAME:
// DESC:
// NOTE:
// SEE ALSO:
//----------------------------------------------------------------------------

SrvWindow::SrvWindow( const char* pzTitle, void* pTopView, uint32 nFlags, uint32 nDesktopMask,
		      const Rect& cRect, SrvApplication* pcApp, port_id hEventPort ) :
    m_cTitle( pzTitle ), m_cMutex( "swindow_lock" )
{
    m_pcApp	= pcApp;
    m_hEventPort	= hEventPort;
	
    m_hMsgPort	= create_port( "swindow_port", DEFAULT_PORT_SIZE );

    m_pcAppTarget = new Messenger( hEventPort, -1, -1 );

    m_nFlags     = nFlags;
    m_bOffscreen = false;
    m_bNeedRegionUpdate = false;
    Rect cBorderFrame = cRect;
  
    m_pcWndBorder = new WndBorder( this, NULL, "wnd_border", (nFlags & WND_BACKMOST) );
    m_pcDecorator = AppServer::GetInstance()->CreateWindowDecorator( m_pcWndBorder, nFlags );
    m_pcWndBorder->SetDecorator( m_pcDecorator );

    m_pcTopView = m_pcWndBorder->GetClient();
    m_pcTopView->SetUserObject( pTopView );

    Rect cBorders = m_pcDecorator->GetBorderSize();
    cBorderFrame.Resize( -cBorders.left, -cBorders.top, cBorders.right, cBorders.bottom );
    m_pcWndBorder->SetFrame( cBorderFrame );
    m_pcDecorator->SetTitle( pzTitle );
  
    m_bBorderHit = false;

    if ( nDesktopMask == 0 ) {
	m_nDesktopMask = 1 << get_active_desktop();
    } else {
	m_nDesktopMask = nDesktopMask;
    }

    for ( int i = 0 ; i < 32 ; ++i ) {
	m_asDTState[i].m_cFrame = cRect;
    }

    add_window_to_desktop( this );
  
    m_hThread = Run();
}

void SrvWindow::ReplaceDecorator()
{
    if ( m_pcWndBorder == NULL ) {
	return;
    }
    Rect cFrame = GetFrame();

    m_pcDecorator = AppServer::GetInstance()->CreateWindowDecorator( m_pcWndBorder, m_nFlags );
    m_pcWndBorder->SetDecorator( m_pcDecorator );
    m_pcDecorator->SetTitle( m_cTitle.c_str() );
    if ( HasFocus() ) {
	m_pcDecorator->SetFocusState( true );
    }
    SetFrame( cFrame );
}

void SrvWindow::NotifyWindowFontChanged( bool bToolWindow )
{
    if ( m_pcDecorator == NULL ) {
	return;
    }
    Rect cFrame = GetFrame();
    m_pcDecorator->FontChanged();
    SetFrame( cFrame );
}

//----------------------------------------------------------------------------
// NAME:
// DESC:
// NOTE:
// SEE ALSO:
//----------------------------------------------------------------------------

SrvWindow::SrvWindow( SrvApplication* pcApp, SrvBitmap* pcBitmap ) : m_cMutex( "swindow_lock" )
{
    m_pcApp	= pcApp;
    m_hEventPort= -1;
    m_hMsgPort	= -1;

    m_pcAppTarget = NULL;
    m_nFlags	  = WND_NO_BORDER;
    m_bOffscreen  = true;
    m_nDesktopMask= ~0;
  
    m_bBorderHit = false;
    
    m_pcTopView = new Layer( NULL, NULL, "bitmap_layer",
			     Rect( 0, 0, pcBitmap->m_nWidth - 1, pcBitmap->m_nHeight - 1 ), 0, NULL );
    m_pcTopView->SetWindow( this );
    m_pcWndBorder = NULL;
    m_pcDecorator = NULL;
    
    SetBitmap( pcBitmap );
    m_hThread = -1;
}

//----------------------------------------------------------------------------
// NAME:
// DESC:
// NOTE:
// SEE ALSO:
//----------------------------------------------------------------------------

SrvWindow::~SrvWindow()
{
    if ( this == s_pcLastMouseWindow ) {
	s_pcLastMouseWindow = NULL;
    }
    if ( this == s_pcDragWindow ) {
	s_pcDragWindow = NULL;
    }

    if ( NULL != m_pcWndBorder ) {
	Layer* pcParent = m_pcWndBorder->GetParent();
	remove_window_from_desktop( this );
	if ( pcParent != NULL ) {
	    pcParent->UpdateRegions();
	}
	delete m_pcWndBorder;
    } else {
	delete m_pcTopView;
    }
    delete m_pcAppTarget;
    if ( m_hMsgPort != -1 ) {
	delete_port( m_hMsgPort );
    }
}

bool SrvWindow::HasFocus() const
{
    return( this == get_active_window(true) );
}

void SrvWindow::DesktopActivated( int nNewDesktop, const IPoint cNewRes, color_space eColorSpace )
{
    if ( m_pcAppTarget != NULL ) {
	Message cMsg( M_DESKTOP_ACTIVATED );
	cMsg.AddInt32( "_new_desktop", nNewDesktop );
	cMsg.AddIPoint( "_new_resolution", cNewRes );
	cMsg.AddInt32( "_new_color_space", eColorSpace );
	if ( m_pcAppTarget->SendMessage( &cMsg ) < 0 ) {
	    dbprintf( "Error: SrvWindow::DesktopActivated() failed to send message to target %s\n", m_cTitle.c_str() );
	}
    }
}

void SrvWindow::ScreenModeChanged( const IPoint cNewRes, color_space eColorSpace )
{
    if ( m_pcAppTarget != NULL ) {
	Message cMsg( M_SCREENMODE_CHANGED );
	cMsg.AddIPoint( "_new_resolution", cNewRes );
	cMsg.AddInt32( "_new_color_space", eColorSpace );
	if ( m_pcAppTarget->SendMessage( &cMsg ) < 0 ) {
	    dbprintf( "Error: SrvWindow::ScreenModeChanged() failed to send message to target %s\n", m_cTitle.c_str() );
	}
    }
}

void SrvWindow::PostUsrMessage( Message* pcMsg )
{
    if ( m_pcAppTarget != NULL ) {
	m_pcAppTarget->SendMessage( pcMsg );
    }
}

//----------------------------------------------------------------------------
// NAME:
// DESC:
// NOTE:
// SEE ALSO:
//----------------------------------------------------------------------------

void SrvWindow::SetFrame( const Rect& cFrame )
{
    if ( NULL != m_pcWndBorder ) {
	Rect cBorders = m_pcDecorator->GetBorderSize();
	m_pcWndBorder->SetFrame( Rect( cFrame.left - cBorders.left, cFrame.top - cBorders.top,
				       cFrame.right + cBorders.right, cFrame.bottom + cBorders.bottom ) );
    }
}

//----------------------------------------------------------------------------
// NAME:
// DESC:
// NOTE:
// SEE ALSO:
//----------------------------------------------------------------------------

Rect SrvWindow::GetFrame( bool bClient ) const
{
    Rect cRect;
    if ( NULL != m_pcWndBorder ) {
	cRect = m_pcWndBorder->GetFrame();
	if ( bClient ) {
	    Rect cBorders = m_pcDecorator->GetBorderSize();
	    cRect.Resize( cBorders.left, cBorders.top, -cBorders.right, -cBorders.bottom );
	}
    } else { // Bitmap window
	cRect = m_pcTopView->GetFrame();
    }
    return( cRect );
}

//----------------------------------------------------------------------------
// NAME:
// DESC:
// NOTE:
// SEE ALSO:
//----------------------------------------------------------------------------

Layer* SrvWindow::GetTopView() const
{
    return( m_pcWndBorder );
}

//----------------------------------------------------------------------------
// NAME:
// DESC:
// NOTE:
// SEE ALSO:
//----------------------------------------------------------------------------

uint32 SrvWindow::GetDesktopMask() const
{
    return( m_nDesktopMask );
}


//----------------------------------------------------------------------------
// NAME:
// DESC:
// NOTE:
// SEE ALSO:
//----------------------------------------------------------------------------

void SrvWindow::SetDesktopMask( uint32 nMask )
{
    remove_window_from_desktop( this );
    m_nDesktopMask = nMask;
    add_window_to_desktop( this );
}

//----------------------------------------------------------------------------
// NAME:
// DESC:
// NOTE:
// SEE ALSO:
//----------------------------------------------------------------------------

void SrvWindow::Quit()
{
    if ( m_hMsgPort != -1 ) {
	send_msg( m_hMsgPort, M_QUIT, NULL, 0 );
    } else {
	SrvApplication*	pcApp = GetApp();
	__assertw( NULL != pcApp );
	AR_DeleteWindow_s sReq;
	sReq.pcWindow	= this;
	send_msg( pcApp->GetReqPort(), AR_DELETE_WINDOW, &sReq, sizeof(sReq) );
    }
}

//----------------------------------------------------------------------------
// NAME:
// DESC:
// NOTE:
// SEE ALSO:
//----------------------------------------------------------------------------

status_t SrvWindow::Lock()
{
    return( m_cMutex.Lock() );
}

//----------------------------------------------------------------------------
// NAME:
// DESC:
// NOTE:
// SEE ALSO:
//----------------------------------------------------------------------------

status_t SrvWindow::Unlock()
{
    return( m_cMutex.Unlock() );
}

//----------------------------------------------------------------------------
// NAME:
// DESC:
// NOTE:
// SEE ALSO:
//----------------------------------------------------------------------------

bool SrvWindow::IsLocked( void )
{
    return( m_cMutex.IsLocked() );
}

bool SrvWindow::HasPendingSizeEvents( Layer* pcLayer )
{
    return( m_pcWndBorder != NULL && pcLayer != m_pcWndBorder && m_pcWndBorder->HasPendingSizeEvents() );
}

//----------------------------------------------------------------------------
// NAME:
// DESC:
// NOTE:
// SEE ALSO:
//----------------------------------------------------------------------------

void SrvWindow::Show( bool bShow )
{
    if ( m_pcWndBorder != NULL ) {
	m_pcWndBorder->Show( bShow );
    }
}

//----------------------------------------------------------------------------
// NAME:
// DESC:
// NOTE:
// SEE ALSO:
//----------------------------------------------------------------------------

void SrvWindow::MakeFocus( bool bFocus )
{
    if ( bFocus ) {
	set_active_window( this );
    } else {
	set_active_window( NULL );
    }
}


void SrvWindow::WindowActivated( bool bFocus )
{
    if ( m_pcApp != NULL ) {
	if ( m_pcAppTarget != NULL ) {
	    Message cMsg( M_WINDOW_ACTIVATED );

	    cMsg.AddBool( "_is_active", bFocus );
	    cMsg.AddPoint( "_scr_pos", InputNode::GetMousePos() );
	    if ( m_pcAppTarget->SendMessage( &cMsg ) < 0 ) {
		dbprintf( "Error: SrvWindow::WindowActivated() failed to send message to target %s\n", m_cTitle.c_str() );
	    }
	}
	if ( m_pcWndBorder != NULL ) {
	    m_pcDecorator->SetFocusState( bFocus );
	}
    }
}


//----------------------------------------------------------------------------
// NAME:
// DESC:
// NOTE:
// SEE ALSO:
//----------------------------------------------------------------------------

void SrvWindow::R_Render( WR_Render_s* psPkt )
{
    bool	bViewsMoved = false;
    g_cLayerGate.Lock();
    int	 nLowest = INT_MAX;
    Layer* pcLowestLayer = NULL;
     uint   nBufPos = 0;
  
    for ( int i = 0 ; i < psPkt->nCount ; ++i )
    {
	GRndHeader_s* psHdr = (GRndHeader_s*) &psPkt->aBuffer[ nBufPos ];
	nBufPos += psHdr->nSize;
	if ( nBufPos > RENDER_BUFFER_SIZE ) {
	    dbprintf( "Error: SrvWindow::R_Render() invalid message size %ld\n", psHdr->nSize );
	    break;
	}

	if ( bViewsMoved && (psHdr->nCmd != DRC_SET_FRAME ||
			     psHdr->nCmd != DRC_SHOW_VIEW ||
			     psHdr->nCmd != DRC_SET_DRAW_REGION ||
			     psHdr->nCmd != DRC_SET_SHAPE_REGION ) ) {
	    bViewsMoved = false;

	    if ( pcLowestLayer != NULL )
	    {
		g_cLayerGate.Unlock();
		g_cLayerGate.Close();
		SrvSprite::Hide( /*pcLowestLayer->ConvertToRoot( pcLowestLayer->GetBounds() )*/ );
		pcLowestLayer->UpdateRegions( false );
		SrvWindow::HandleMouseTransaction();
		SrvSprite::Unhide();
		nLowest = INT_MAX;
		g_cLayerGate.Open();
		g_cLayerGate.Lock();
	    }
	    bViewsMoved = false;
	}
    
	Layer* pcView = FindLayer( psHdr->hViewToken );
      
	if ( pcView == NULL  ) {
	    continue;
	}

	switch( psHdr->nCmd )
	{
	    case DRC_BEGIN_UPDATE:
		pcView->BeginUpdate();
		break;
	    case DRC_END_UPDATE:
		pcView->EndUpdate();
		break;
	    case DRC_LINE32:
	    {
		GRndLine32_s* psMsg = (GRndLine32_s*) psHdr;
		pcView->DrawLine( psMsg->sToPos );
		break;
	    }
	    case DRC_FILL_RECT32:
	    {
		GRndFillRect32_s* psMsg = (GRndFillRect32_s*) psHdr;
		pcView->FillRect( psMsg->sRect, psMsg->sColor );
		break;
	    }
	    case DRC_COPY_RECT:
		if ( NULL != GetBitmap() ) {
		    g_cLayerGate.Unlock();
		    g_cLayerGate.Close();
		    pcView->CopyRect( GetBitmap(), (GRndCopyRect_s*) psHdr );
		    g_cLayerGate.Open();
		    g_cLayerGate.Lock();
		}
		break;
	    case DRC_DRAW_BITMAP:
		if ( NULL != GetBitmap() ) {
		    GRndDrawBitmap_s*	psReq = (GRndDrawBitmap_s*) psHdr;

		    BitmapNode*	pcNode = g_pcBitmaps->GetObj( psReq->hBitmapToken );
		    if ( NULL != pcNode )
		    {
			pcView->DrawBitMap( GetBitmap(), pcNode->m_pcBitmap, psReq->cSrcRect, psReq->cDstRect.LeftTop() );
		    }
		}
		break;
	    case DRC_SET_COLOR32:
	    {
		GRndSetColor32_s* psMsg = (GRndSetColor32_s*) psHdr;
		switch( psMsg->nWhichPen )
		{
		    case PEN_HIGH:  pcView->SetFgColor( psMsg->sColor );		break;
		    case PEN_LOW:	  pcView->SetBgColor( psMsg->sColor );		break;
		    case PEN_ERASE: pcView->SetEraseColor( psMsg->sColor );	break;
		}
		break;
	    }
	    case DRC_SET_PEN_POS:
	    {
		GRndSetPenPos_s* psMsg = (GRndSetPenPos_s*) psHdr;
		if ( psMsg->bRelative ) {
		    pcView->MovePenBy( Point( psMsg->sPosition ) );
		} else {
		    pcView->MovePenTo( Point( psMsg->sPosition ) );
		}
		break;
	    }
	    case DRC_SET_FONT:
	    {
		GRndSetFont_s* psMsg  = (GRndSetFont_s*) psHdr;
		g_cLayerGate.Unlock();
		g_cLayerGate.Close();
		pcView->SetFont( m_pcApp->GetFont( psMsg->hFontID ) );
		g_cLayerGate.Open();
		g_cLayerGate.Lock();
		break;
	    }
	    case DRC_DRAW_STRING:
	    {
		GRndDrawString_s* psMsg = (GRndDrawString_s*) psHdr;
		pcView->DrawString( psMsg->zString, psMsg->nLength  );
		break;
	    }
	    case DRC_SET_FRAME:
	    {
		GRndSetFrame_s* psMsg = static_cast<GRndSetFrame_s*>(psHdr);

		if ( m_pcWndBorder != NULL && pcView == m_pcTopView ) {
		    Layer* pcParent = m_pcWndBorder->GetParent();
		    if ( pcParent != NULL ) { // Cant move windows not attached to a desktop
			SetFrame( psMsg->cFrame );
			nLowest = 0;
			pcLowestLayer = pcParent;
		    }
		} else {
		    Layer* pcParent = pcView->GetParent();
		    if ( pcParent != NULL ) {
			pcView->SetFrame( psMsg->cFrame + pcParent->GetScrollOffset() );
			if ( pcView->GetLevel() < nLowest ) {
			    nLowest = pcParent->GetLevel();
			    pcLowestLayer = pcParent;
			}
		    }
		}
		bViewsMoved = true;
		break;
	    }
	    case DRC_SHOW_VIEW:
	    {
		GRndShowView_s* psMsg = static_cast<GRndShowView_s*>(psHdr);

		if ( m_pcWndBorder != NULL && pcView == m_pcTopView ) {
		    m_pcWndBorder->Show( psMsg->bVisible );
		    if ( psMsg->bVisible ) {
			m_pcWndBorder->MoveToFront();
		    }
		    Layer* pcParent = m_pcWndBorder->GetParent();
		    if ( pcParent != NULL ) { // Cant show not attached to a desktop
			nLowest = 0;
			pcLowestLayer = pcParent;
		    }
		} else {
		    Layer* pcParent = pcView->GetParent();
		    if ( pcParent != NULL ) {
			pcView->Show( psMsg->bVisible );
			if ( pcParent->GetLevel() < nLowest ) {
			    nLowest = pcParent->GetLevel();
			    pcLowestLayer = pcParent;
			}
		    }
		}
		bViewsMoved = true;
		break;
	    }
      
	    case DRC_SCROLL_VIEW:
	    {
		GRndScrollView_s* psMsg = static_cast<GRndScrollView_s*>(psHdr);

		g_cLayerGate.Unlock();
		g_cLayerGate.Close();
		SrvSprite::Hide( static_cast<IRect>(pcView->ConvertToRoot( pcView->GetBounds() )) );
		pcView->ScrollBy( psMsg->cDelta );
		SrvSprite::Unhide();
		g_cLayerGate.Open();
		g_cLayerGate.Lock();
		break;
	    }
	    case DRC_SET_DRAWING_MODE:
		pcView->SetDrawindMode( static_cast<GRndSetDrawingMode_s*>(psHdr)->nDrawingMode );
		break;
	    case DRC_INVALIDATE_RECT:
	    {
		IRect cRect( static_cast<GRndInvalidateRect_s*>(psHdr)->m_cRect );
		pcView->Invalidate( cRect + pcView->m_cIScrollOffset/*, static_cast<GRndInvalidateRect_s*>(psHdr)->m_bRecurse*/ );
		if ( pcView->GetLevel() < nLowest ) {
		    nLowest = pcView->GetLevel();
		    pcLowestLayer = pcView;
		}
		bViewsMoved = true;
		break;
	    }
	    case DRC_INVALIDATE_VIEW:
		pcView->Invalidate( static_cast<GRndInvalidateView_s*>(psHdr)->m_bRecurse );
		if ( pcView->GetLevel() < nLowest ) {
		    nLowest = pcView->GetLevel();
		    pcLowestLayer = pcView;
		}
		bViewsMoved = true;
		break;
	    case DRC_SET_DRAW_REGION:
	    case DRC_SET_SHAPE_REGION:
	    {
		GRndRegion_s* psMsg = static_cast<GRndRegion_s*>(psHdr);
		IRect* pasRects = reinterpret_cast<IRect*>(psMsg+1);
		Region* pcReg = NULL;

		if ( psMsg->m_nClipCount >= 0 ) {
		    pcReg  = new Region;
		    for ( int i = 0 ; i < psMsg->m_nClipCount ; ++i ) {
			pcReg->AddRect( pasRects[i] );
		    }
		}
		if ( psHdr->nCmd == DRC_SET_DRAW_REGION ) {
		    pcView->SetDrawRegion( pcReg );
		    if ( pcView->GetLevel() < nLowest ) {
			nLowest = pcView->GetLevel();
			pcLowestLayer = pcView;
		    }
		} else {
		    Layer* pcParent;
		    if ( m_pcWndBorder != NULL && pcView == m_pcTopView ) {
			pcParent = m_pcWndBorder->GetParent();
			m_pcWndBorder->SetShapeRegion( pcReg );
		    } else {
			pcParent = pcView->GetParent();
			pcView->SetShapeRegion( pcReg );
		    }
		    if ( pcParent != NULL && pcParent->GetLevel() < nLowest ) {
			nLowest = pcParent->GetLevel();
			pcLowestLayer = pcParent;
		    }		    
		}
		bViewsMoved = true;
		break;
	    }
	}
//    g_cLayerGate.Unlock();
    }
    g_cLayerGate.Unlock();
  
    if ( bViewsMoved ) {
	bViewsMoved = false;
	if ( pcLowestLayer != NULL ) {
	    g_cLayerGate.Close();
	    SrvSprite::Hide();
	    pcLowestLayer->UpdateRegions( false );
	    SrvWindow::HandleMouseTransaction();
	    SrvSprite::Unhide();
	    nLowest = INT_MAX;
	    g_cLayerGate.Open();
	}
    }
    if ( psPkt->hReply != -1 ) {
	send_msg( psPkt->hReply, 0, NULL, 0 );
    }
}

//----------------------------------------------------------------------------
// NAME:
// DESC:
// NOTE:
// SEE ALSO:
//----------------------------------------------------------------------------

void SrvWindow::SendKbdEvent( int nKeyCode, uint32 nQualifiers, const char* pzConvString, const char* pzRawString )
{
    g_cLayerGate.Lock();
    SrvWindow* pcWnd = get_active_window(false);
    if ( pcWnd != NULL ) {
	Message cMsg( (nKeyCode & 0x80) ? M_KEY_UP : M_KEY_DOWN );

	cMsg.AddPointer( "_widget", pcWnd->m_pcTopView->GetUserObject() );
	cMsg.AddInt32( "_raw_key", nKeyCode );
	cMsg.AddInt32( "_qualifiers", nQualifiers );
	cMsg.AddString( "_string", pzConvString );
	cMsg.AddString( "_raw_string", pzRawString );

	pcWnd->PostUsrMessage( &cMsg );
    }
    g_cLayerGate.Unlock();
}

//----------------------------------------------------------------------------
// NAME:
// DESC:
// NOTE:
// SEE ALSO:
//----------------------------------------------------------------------------

void SrvWindow::MouseMoved( Message* pcEvent, int nTransit )
{
    __assertw( NULL != m_pcWndBorder );
    __assertw( NULL != m_pcTopView );

    if ( /*m_bBorderHit &&*/ (m_nFlags & WND_SYSTEM) == 0 ) {
	Point cNewPos;
	pcEvent->FindPoint( "_scr_pos", &cNewPos );
	if ( m_pcWndBorder->MouseMoved( m_pcAppTarget, cNewPos - m_pcWndBorder->GetLeftTop(), nTransit ) ) {
	    return;
	}
    }
  
    Message cMsg( *pcEvent );
  
    cMsg.AddPointer( "_widget", m_pcTopView->GetUserObject() );
    cMsg.AddInt32( "_transit", nTransit );

    if ( s_bIsDragging ) {
	cMsg.AddMessage( "_drag_message", &s_cDragMessage );
    }
    if ( m_pcAppTarget != NULL ) {
	if ( m_pcAppTarget->SendMessage( &cMsg ) < 0 ) {
	    dbprintf( "Error: SrvWindow::MouseMoved() failed to send message to %s\n", m_cTitle.c_str() );
	}
    }
}

//----------------------------------------------------------------------------
// NAME:
// DESC:
// NOTE:
// SEE ALSO:
//----------------------------------------------------------------------------

void SrvWindow::MouseDown( Message* pcEvent )
{
    Point  cPos;
    __assertw( NULL != m_pcWndBorder );
    __assertw( NULL != m_pcTopView );

    pcEvent->FindPoint( "_scr_pos", &cPos );

    if ( m_pcTopView->GetFrame().DoIntersect( cPos - m_pcWndBorder->GetLeftTop() ) == false && (m_nFlags & WND_SYSTEM) == 0 ) {
	int32 nButton;
	pcEvent->FindInt32( "_button", &nButton );
	m_bBorderHit = true;
	if ( m_pcWndBorder->MouseDown( m_pcAppTarget, cPos - m_pcWndBorder->GetLeftTop(), nButton ) ) {
	    s_pcDragWindow = this;
	    return;
	}
    }
  
    Message cMsg( *pcEvent );

    cMsg.AddPointer( "_widget", m_pcTopView->GetUserObject() );

    if ( m_pcAppTarget != NULL ) {
	if ( m_pcAppTarget->SendMessage( &cMsg ) < 0 ) {
	    dbprintf( "Error: SrvWindow::MouseDown() failed to send message to %s\n", m_cTitle.c_str() );
	}
    }
}

//----------------------------------------------------------------------------
// NAME:
// DESC:
// NOTE:
// SEE ALSO:
//----------------------------------------------------------------------------

void SrvWindow::MouseUp( Message* pcEvent, bool bSendDragMsg )
{
    __assertw( NULL != m_pcWndBorder );
    __assertw( NULL != m_pcTopView );

    Message cMsg( *pcEvent );

    cMsg.AddPointer( "_widget", m_pcTopView->GetUserObject() );
    if ( bSendDragMsg && s_bIsDragging ) {
	cMsg.AddMessage( "_drag_message", &s_cDragMessage );
    }
    if ( m_pcAppTarget != NULL ) {
	if ( m_pcAppTarget->SendMessage( &cMsg ) < 0 ) {
	    dbprintf( "Error: SrvWindow::MouseUp() failed to send message to %s\n", m_cTitle.c_str() );
	}
    }
}


void SrvWindow::HandleMouseMoved( const Point& cMousePos, Message* pcEvent )
{
    SrvWindow* pcActiveWindow = get_active_window(false);
    SrvWindow* pcMouseWnd = NULL;

    if ( s_pcDragWindow != NULL ) {
	s_pcDragWindow->m_pcWndBorder->MouseMoved( s_pcDragWindow->m_pcAppTarget,
						   cMousePos - s_pcDragWindow->m_pcWndBorder->GetLeftTop(), MOUSE_INSIDE );
	return;
    }
    
    if ( pcActiveWindow == NULL || (pcActiveWindow->GetFlags() & WND_SYSTEM) == 0 ) {
	Layer* pcView = g_pcTopView->GetChildAt( cMousePos );

	if ( pcView != NULL ) {
	    pcMouseWnd = pcView->GetWindow();
	} else {
	    pcMouseWnd = NULL;
	}
    }
    if ( s_pcLastMouseWindow != pcMouseWnd ) {
	if ( s_pcLastMouseWindow != NULL ) {
	      // Give the window a chance to deliver a MOUSE_EXITED to it's views
	    s_pcLastMouseWindow->MouseMoved( pcEvent, MOUSE_EXITED );
	    if ( s_pcLastMouseWindow == pcActiveWindow ) {
		pcActiveWindow = NULL;
	    }
	}
	if ( pcMouseWnd != NULL ) {
	    pcMouseWnd->MouseMoved( pcEvent, MOUSE_ENTERED );
	    if ( pcMouseWnd == pcActiveWindow ) {
		pcActiveWindow = NULL;
	    }
	}
	s_pcLastMouseWindow = pcMouseWnd;
    } else {
	if ( pcMouseWnd != NULL ) {
	    pcMouseWnd->MouseMoved( pcEvent, MOUSE_INSIDE );
	    if ( pcMouseWnd == pcActiveWindow ) {
		pcActiveWindow = NULL;
	    }
	}
    }
    if ( pcActiveWindow != NULL ) {
	pcActiveWindow->MouseMoved( pcEvent, MOUSE_OUTSIDE );
    }
    if ( pcMouseWnd != NULL && pcMouseWnd != pcActiveWindow ) {
	pcMouseWnd->m_pcWndBorder->MouseMoved( pcMouseWnd->m_pcAppTarget, cMousePos - pcMouseWnd->m_pcWndBorder->GetLeftTop(), MOUSE_INSIDE );
    }
    if ( s_bIsDragging && s_pcDragSprite != NULL ) {
	s_pcDragSprite->MoveTo( static_cast<IPoint>(cMousePos) );
    }
}

void SrvWindow::HandleMouseDown( const Point& cMousePos, int nButton, Message* pcEvent )
{
    SrvWindow* pcActiveWindow = get_active_window(false);
    SrvWindow* pcMouseWnd = NULL;

    if ( pcActiveWindow == NULL || (pcActiveWindow->GetFlags() & WND_SYSTEM) == 0 ) {
	pcMouseWnd = s_pcLastMouseWindow;
    }
    
    if ( pcMouseWnd != NULL ) {
	bigtime_t nCurTime;
	if ( pcEvent->FindInt64( "_timestamp", &nCurTime ) != 0 ) {
	    dbprintf( "Error: SrvWindow::HandleInputEvent() M_MOUSE_DOWN message has no _timestamp field\n" );
	    nCurTime = get_system_time();
	}
	pcMouseWnd->MakeFocus( true );
	
	int32 nQualifiers;
	if ( pcEvent->FindInt32( "_qualifiers", &nQualifiers ) != 0 ) {
	    dbprintf( "Error: SrvWindow::HandleInputEvent() M_MOUSE_DOWN message has no _qualifiers field\n" );
	    nQualifiers = GetQualifiers();
	}
	
	AppserverConfig* pcConfig = AppserverConfig::GetInstance();
	if ( (pcConfig->GetPopoupSelectedWindows() || (nQualifiers & QUAL_CTRL)) ||
	     (pcMouseWnd->m_pcTopView->GetFrame().DoIntersect( cMousePos - pcMouseWnd->m_pcWndBorder->GetFrame().LeftTop() ) == false &&
	      pcMouseWnd->m_nLastHitTime + pcConfig->GetDoubleClickTime()  > nCurTime) ) {
	    pcMouseWnd->m_pcWndBorder->MoveToFront();
	    pcMouseWnd->m_pcWndBorder->GetParent()->UpdateRegions( false );
	    SrvWindow::HandleMouseTransaction();
	}
	pcMouseWnd->m_nLastHitTime = nCurTime;
    } else if ( get_active_window(true) != NULL ) {
	get_active_window(true)->MakeFocus( false );
    }
    
    pcActiveWindow = get_active_window(false);
    if ( pcActiveWindow != NULL ) {
	pcActiveWindow->MouseDown( pcEvent );
    }
}

void SrvWindow::HandleMouseUp( const Point& cMousePos, int nButton, Message* pcEvent )
{
    if ( s_pcDragWindow != NULL ) {
	Point cPos;
	int32 nButton;
	pcEvent->FindPoint( "_scr_pos", &cPos );
	pcEvent->FindInt32( "_button", &nButton );
	s_pcDragWindow->m_pcWndBorder->MouseUp( s_pcDragWindow->m_pcAppTarget, cPos - s_pcDragWindow->m_pcWndBorder->GetLeftTop(), nButton );
	s_pcDragWindow = NULL;
	return;
    }
    
    SrvWindow* pcActiveWindow = get_active_window(false);
    SrvWindow* pcMouseWnd = NULL;

    if ( pcActiveWindow == NULL || (pcActiveWindow->GetFlags() & WND_SYSTEM) == 0 ) {
	pcMouseWnd = s_pcLastMouseWindow;
    }
    
    if ( pcMouseWnd != NULL ) {
	pcMouseWnd->MouseUp( pcEvent, true );
    }
    if ( pcActiveWindow != NULL && pcMouseWnd != pcActiveWindow ) {
	pcActiveWindow->MouseUp( pcEvent, false );
    }
    if ( s_bIsDragging ) {
	SrvSprite::Hide();
	delete s_pcDragSprite;
	s_cDragMessage.MakeEmpty();
	s_bIsDragging = false;
	SrvSprite::Unhide();
    }
}

//----------------------------------------------------------------------------
// NAME:
// DESC:
// NOTE:
// SEE ALSO:
//----------------------------------------------------------------------------

void SrvWindow::HandleInputEvent( Message* pcEvent )
{
    g_cLayerGate.Close();
    Point cMousePos;

    pcEvent->FindPoint( "_scr_pos", &cMousePos );

    if ( pcEvent->GetCode() == M_MOUSE_MOVED ) {
	HandleMouseMoved( cMousePos, pcEvent );
    } else if ( pcEvent->GetCode() == M_MOUSE_DOWN ) {
	int32 nButton;
	if ( pcEvent->FindInt32( "_button", &nButton ) != 0 ) {
	    dbprintf( "Error: SrvWindow::HandleInputEvent() M_MOUSE_DOWN message has no _button field\n" );
	}
	HandleMouseDown( cMousePos, nButton, pcEvent );
    } else if ( pcEvent->GetCode() == M_MOUSE_UP ) {
	int32 nButton;
	if ( pcEvent->FindInt32( "_button", &nButton ) != 0 ) {
	    dbprintf( "Error: SrvWindow::HandleInputEvent() M_MOUSE_UP message has no _button field\n" );
	}
	HandleMouseUp( cMousePos, nButton, pcEvent );
    } else if ( pcEvent->GetCode() == M_WHEEL_MOVED ) {
	if ( s_pcLastMouseWindow != NULL && s_pcLastMouseWindow->m_pcAppTarget != NULL ) {
	    Message cMsg( *pcEvent );
	    cMsg.AddPointer( "_widget", s_pcLastMouseWindow->m_pcTopView->GetUserObject() );
	    if ( s_pcLastMouseWindow->m_pcAppTarget->SendMessage( &cMsg ) < 0 ) {
		dbprintf( "Error: SrvWindow::HandleInputEvent() failed to send M_WHEEL_MOVED event to %s\n",
			  s_pcLastMouseWindow->m_cTitle.c_str() );
	    }
	}
    } else {
	SrvWindow* pcWnd = get_active_window(false);
	if ( pcWnd != NULL && pcWnd->m_pcAppTarget != NULL ) {
	    Message cMsg( *pcEvent );
	    cMsg.AddPointer( "_widget", pcWnd->m_pcTopView->GetUserObject() );
	    if ( pcWnd->m_pcAppTarget->SendMessage( &cMsg ) < 0 ) {
		dbprintf( "Error: SrvWindow::HandleInputEvent() failed to send event %d to %s\n",
			  pcEvent->GetCode(), pcWnd->m_cTitle.c_str() );
	    }
	}
    }
    g_cLayerGate.Open();
}

void SrvWindow::HandleMouseTransaction()
{
    Point cMousePos = InputNode::GetMousePos();
    Layer* pcView = g_pcTopView->GetChildAt( cMousePos );
    SrvWindow* pcWnd = (pcView != NULL) ? pcView->GetWindow() : NULL;

    if ( pcWnd != s_pcLastMouseWindow ) {
	InputNode::SetMousePos( cMousePos );
    }
}

//----------------------------------------------------------------------------
// NAME:
// DESC:
// NOTE:
// SEE ALSO:
//----------------------------------------------------------------------------

void SrvWindow::BeginRegionUpdate()
{
    m_bNeedRegionUpdate = true;
}

bool SrvWindow::DispatchMessage( Message* pcReq )
{
    switch( pcReq->GetCode() ) {
	case WR_CREATE_VIEW:
	{
	    Layer* pcParent;
	    int32 hParentView = -1;
      
	    pcReq->FindInt32( "parent_view", &hParentView );
      
	    g_cLayerGate.Close();
	    pcParent = FindLayer( hParentView );

	    if ( pcParent != NULL ) {
		const char* pzName;
		Rect	    cFrame;
		uint32	    nFlags;
		void*	    pUserObject;
		int	    nHideCount = 0;
	
		pcReq->FindString( "name", &pzName );
		pcReq->FindInt( "flags", &nFlags );
		pcReq->FindPointer( "user_object", &pUserObject );
		pcReq->FindRect( "frame", &cFrame );

	
		pcReq->FindInt( "hide_count", &nHideCount );
	
		Layer* pcView = new Layer( this, pcParent, pzName, cFrame, nFlags, pUserObject );

		pcReq->FindColor32( "fg_color", &pcView->m_sFgColor );
		pcReq->FindColor32( "bg_color", &pcView->m_sBgColor );
		pcReq->FindColor32( "er_color", &pcView->m_sEraseColor );
		pcReq->FindPoint( "scroll_offset", &pcView->m_cScrollOffset );
		pcView->m_cIScrollOffset = static_cast<IPoint>(pcView->m_cScrollOffset);
		
		pcView->m_nHideCount += nHideCount;
	
		Message cReply;
		cReply.AddInt32( "handle", pcView->GetHandle() );
		cReply.AddInt32( "error", 0 );
	
		pcReq->SendReply( &cReply );
		pcParent->UpdateRegions();
		SrvWindow::HandleMouseTransaction();
	    } else {
		dbprintf( "Error: Attempt to create view with invalid parent %ld\n", hParentView );
		Message cReply;
		cReply.AddInt32( "error", -EINVAL );
		pcReq->SendReply( &cReply );
	    }
	    g_cLayerGate.Open();
	    break;
	}
	case WR_DELETE_VIEW:
	{
	    Layer* pcView;
	    int	   hView = -1;

	    pcReq->FindInt( "handle", &hView );
	    g_cLayerGate.Close();
	    SrvSprite::Hide();
	    if ( m_pcTopView->GetHandle() != hView ) {
		pcView = FindLayer( hView );
		if ( pcView != NULL ) {
		    if ( pcView->GetWindow() != this ) {
			dbprintf( "Error : Attempt to delete widget not belonging to this window!\n" );
			g_cLayerGate.Open();
			break;
		    }
		    Layer* pcParent = pcView->GetParent();
		    pcView->RemoveThis();

		    delete pcView;

		    if ( pcParent != NULL ) {
			pcParent->UpdateRegions( true );
		    }
		    SrvWindow::HandleMouseTransaction();
		} else {
		    dbprintf( "Error : Attempt to delete invalid view %d\n", hView );
		}
	    } else {
		m_pcTopView->SetUserObject( NULL );
	    }
	    SrvSprite::Unhide();
	    g_cLayerGate.Open();
	    break;
	}
	case WR_SET_TITLE:
	{
	    const char* pzTitle;

	    if ( pcReq->FindString( "title", &pzTitle ) < 0 ) {
		dbprintf( "Error: WR_SET_TITLE message have no 'title' field\n" );
		break;
	    }
	    g_cLayerGate.Close();
	    m_cTitle = pzTitle;
	    if ( m_pcDecorator != NULL ) {
		m_pcDecorator->SetTitle( m_cTitle.c_str() );
	    }
	    rename_thread( -1, String().Format( "W:%.62s", pzTitle ).c_str() );
	    g_cLayerGate.Open();
	    break;
	}
	case WR_SET_FLAGS:
	{
	    if ( m_pcWndBorder != NULL ) {
		int32 nFlags = m_nFlags;
		pcReq->FindInt32( "flags", &nFlags );

		g_cLayerGate.Close();
		
		m_nFlags = nFlags;
		Rect cFrame = GetFrame();
		m_pcWndBorder->SetFlags( nFlags );
		SetFrame( cFrame );
		if ( cFrame != GetFrame() ) {
		    SrvSprite::Hide();
		    g_pcTopView->UpdateRegions( false );
		    SrvWindow::HandleMouseTransaction();
		    SrvSprite::Unhide();
		} else {
		    m_pcWndBorder->UpdateIfNeeded( false );
		}
		g_cLayerGate.Open();
	    }
	    break;
	}
	case WR_SET_SIZE_LIMITS:
	{
	    if ( m_pcWndBorder != NULL ) {
		Point cMinSize(0.0f,0.0f);
		Point cMaxSize(FLT_MAX,FLT_MAX);
		pcReq->FindPoint( "min_size", &cMinSize );
		pcReq->FindPoint( "max_size", &cMaxSize );
		m_pcWndBorder->SetSizeLimits( cMinSize, cMaxSize );
	    }
	    break;
	}
	case WR_SET_ALIGNMENT:
	{
	    if ( m_pcWndBorder != NULL ) {
		IPoint cSize(1,1);
		IPoint cSizeOff(0,0);
		IPoint cPos(1,1);
		IPoint cPosOff(0,0);

		pcReq->FindIPoint( "size", &cSize );
		pcReq->FindIPoint( "size_off", &cSizeOff );
		pcReq->FindIPoint( "pos", &cPos );
		pcReq->FindIPoint( "pos_off", &cPosOff );
		if ( cSize.x < 1 ) cSize.x = 1;
		if ( cSize.y < 1 ) cSize.y = 1;
		if ( cPos.x < 1 )  cPos.x  = 1;
		if ( cPos.y < 1 )  cPos.y  = 1;
		
		m_pcWndBorder->SetAlignment( cSize, cSizeOff, cPos, cPosOff );
	    }
	    break;
	}
	case WR_MAKE_FOCUS:
	{
	    bool bFocus = true;
	    
	    if ( pcReq->FindBool( "focus", &bFocus ) < 0 ) {
		dbprintf( "Error: WR_MAKE_FOCUS message have no 'focus' field\n" );
		break;
	    }
	    g_cLayerGate.Close();
	    MakeFocus( bFocus );
	    g_cLayerGate.Open();
	    break;
	}
	case WR_TOGGLE_VIEW_DEPTH:
	{
	    Layer* pcView;
	    int	   hView = -1;

	    pcReq->FindInt( "view", &hView );
	    
	    g_cLayerGate.Close();
	    pcView = FindLayer( hView );
  
	    if ( pcView != NULL  ) {
		Layer* pcParent = pcView->GetParent();
		if ( pcParent != NULL ) {
		    if ( m_pcWndBorder != NULL && pcView == m_pcTopView ) {
			pcView = m_pcWndBorder;
			pcView->ToggleDepth();
			pcParent->UpdateRegions( false );
		    }
		}
	    }
	    SrvWindow::HandleMouseTransaction();
	    g_cLayerGate.Open();
	    
	    Message cReply;
	    pcReq->SendReply( &cReply );
	    break;
	}
	case WR_BEGIN_DRAG:
	{
	    BitmapNode* pcNode;
	    SrvBitmap*  pcBitmap = NULL;
	    int		hBitmap = -1;
	    Rect	cBounds(0,0,0,0);
	    Point	cHotSpot(0,0);
	    
	    if ( pcReq->FindInt( "bitmap", &hBitmap ) < 0 ) {
		dbprintf( "No 'bitmap' field in WR_BEGIN_DRAG message\n" );
		break;
	    }
	    if ( pcReq->FindPoint( "hot_spot", &cHotSpot ) < 0 ) {
		dbprintf( "No 'hot_spot' field in WR_BEGIN_DRAG message\n" );
		break;
	    }
	    
	    if ( hBitmap != -1 ) {
		pcNode = g_pcBitmaps->GetObj( hBitmap );
		if ( pcNode == NULL ) {
		    dbprintf( "Error: Attempt to start drag operation with invalid bitmap %d\n", hBitmap );
		    break;
		}
		pcBitmap = pcNode->m_pcBitmap;
	    } else {
		if ( pcReq->FindRect( "bounds", &cBounds ) < 0 ) {
		    dbprintf( "No 'bounds' field in WR_BEGIN_DRAG message\n" );
		    break;
		}
	    }
      
	    g_cLayerGate.Close();
	    if ( s_bIsDragging == false ) {
		s_pcDragSprite = new SrvSprite( static_cast<IRect>(cBounds),
						static_cast<IPoint>(InputNode::GetMousePos()),
						static_cast<IPoint>(cHotSpot),
						g_pcTopView->GetBitmap(), pcBitmap );
		pcReq->FindMessage( "data", &s_cDragMessage );
		s_cDragMessage.AddPoint( "_hot_spot", cHotSpot );
		s_bIsDragging = true;
	    }
	    g_cLayerGate.Open();
	    Message cReply;
	    pcReq->SendReply( &cReply );
	    break;
	}
	case WR_GET_MOUSE:
	{
	    Message cReply;
	    cReply.AddPoint( "position", InputNode::GetMousePos() );
	    cReply.AddInt32( "buttons", InputNode::GetMouseButtons() );
	    pcReq->SendReply( &cReply );
	    break;
	}
	case WR_SET_MOUSE_POS:
	{
	    Point cPos;
	    if ( pcReq->FindPoint( "position", &cPos ) == 0 ) {
		InputNode::SetMousePos( cPos );
	    }
	    break;
	}
    }
    return( true );
}

//----------------------------------------------------------------------------
// NAME:
// DESC:
// NOTE:
// SEE ALSO:
//----------------------------------------------------------------------------

bool SrvWindow::DispatchMessage( const void* psMsg, int nCode )
{
    bool bDoLoop = true;

    switch( nCode )
    {
	case WR_CREATE_VIEW:
	case WR_DELETE_VIEW:
	case WR_SET_TITLE:
	case WR_SET_FLAGS:
	case WR_SET_SIZE_LIMITS:
	case WR_SET_ALIGNMENT:
	case WR_MAKE_FOCUS:
	case WR_TOGGLE_VIEW_DEPTH:
	case WR_BEGIN_DRAG:
	case WR_GET_MOUSE:
	case WR_SET_MOUSE_POS:
	{
	    try {
		Message cReq( psMsg );
		DispatchMessage( &cReq );
	    } catch (...) {
		dbprintf( "Error: DispatchMessage() catched exception while unflatting message\n" );
	    }
	    break;
	}
	case WR_RENDER:
	    R_Render( (WR_Render_s*) psMsg );
	    break;
	case WR_WND_MOVE_REPLY:
	    if ( m_pcWndBorder != NULL ) {
		m_pcWndBorder->WndMoveReply( m_pcAppTarget );
	    }
	    break;
	case WR_GET_PEN_POSITION:
	{
	    WR_GetPenPosition_s* psReq = static_cast<WR_GetPenPosition_s*>(psMsg);
	    WR_GetPenPositionReply_s sReply;

	    Layer* pcView = FindLayer( psReq->m_hViewHandle );

	    if ( pcView != NULL ) {
		sReply.m_cPos = pcView->GetPenPosition();
	    } else {
		dbprintf( "Error: SrvWindow::DispatchMessage() request for pen position in invalid view %08x\n", psReq->m_hViewHandle );
	    }
	    send_msg( psReq->m_hReply, 0, &sReply, sizeof( sReply ) );
	    break;
	}
	case WR_UPDATE_REGIONS:
	    g_cLayerGate.Close();
	    if ( m_pcWndBorder != NULL ) {
		m_pcWndBorder->UpdateRegions();
	    } else {
		m_pcTopView->UpdateRegions();
	    }
	    g_cLayerGate.Open();
	    break;
	case M_QUIT:
	case AR_CLOSE_WINDOW:
	{
	    bDoLoop = false;
	    delete m_pcAppTarget;
	    m_pcAppTarget = NULL;
	    break;
	}
	default:
	    dbprintf( "Warning : SrvWindow::DispatchMessage() Unknown command %d\n", nCode );
	    break;
    }
    return( bDoLoop );
}

//----------------------------------------------------------------------------
// NAME:
// DESC:
// NOTE:
// SEE ALSO:
//----------------------------------------------------------------------------

void SrvWindow::Loop( void )
{
    void*	psMsg	= new char[8192];
    bool	bDoLoop	= true;

    while( bDoLoop )
    {
	uint32	nCode;
		
	if ( get_msg( m_hMsgPort, &nCode, psMsg, 8192 ) >= 0 )
	{
//      Lock();
	    if ( m_bNeedRegionUpdate ) {
		g_cLayerGate.Lock();
		if ( m_pcWndBorder != NULL ) {
		    m_pcWndBorder->UpdateRegions();
		} else {
		    m_pcTopView->UpdateRegions();
		}
		m_bNeedRegionUpdate = false;
		g_cLayerGate.Unlock();
	    }
      
	    bDoLoop = DispatchMessage( psMsg, nCode );
//      Unlock();
	}
    }
    delete[] (char*)psMsg;
}

//----------------------------------------------------------------------------
// NAME:
// DESC:
// NOTE:
// SEE ALSO:
//----------------------------------------------------------------------------

uint32	SrvWindow::Entry( void* pData )
{
    SrvWindow*	pcWindow = (SrvWindow*) pData;

    signal( SIGINT, SIG_IGN );
    signal( SIGQUIT, SIG_IGN );
    signal( SIGTERM, SIG_IGN );
	
    pcWindow->Loop();

    SrvApplication*	pcApp = pcWindow->GetApp();

    __assertw( NULL != pcApp );

    AR_DeleteWindow_s sReq;

    sReq.pcWindow	= pcWindow;

    send_msg( pcApp->GetReqPort(), AR_DELETE_WINDOW, &sReq, sizeof(sReq) );
    exit_thread( 0 );
    return( 0 );
}

//----------------------------------------------------------------------------
// NAME:
// DESC:
// NOTE:
// SEE ALSO:
//----------------------------------------------------------------------------

thread_id SrvWindow::Run()
{
    thread_id hThread = spawn_thread( String().Format( "W:%.62s", m_cTitle.c_str() ).c_str(), Entry, DISPLAY_PRIORITY, 0, this );

    if ( hThread != -1 ) {
	resume_thread( hThread );
    }
    return( hThread );
}
