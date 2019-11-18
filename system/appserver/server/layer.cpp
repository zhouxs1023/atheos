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

#include <string.h>
#include <atheos/types.h>
#include <atheos/time.h>

#include <macros.h>

#include "ddriver.h"
#include "server.h"
#include "layer.h"
#include "swindow.h"
#include "sfont.h"
#include "bitmap.h"

#include <gui/view.h>
#include <gui/guidefines.h>
#include <gui/font.h>
#include <util/locker.h>
#include <util/messenger.h>
#include <util/message.h>

Gate g_cLayerGate( "layer_gate", false );


//----------------------------------------------------------------------------
// NAME:
// DESC:
// NOTE:
// SEE ALSO:
//----------------------------------------------------------------------------

void Layer::Init()
{
    m_pcDrawConstrainReg  = NULL;
    m_pcShapeConstrainReg = NULL;
    m_pcVisibleReg	  = NULL;
    m_pcPrevVisibleReg	  = NULL;
    m_pcDrawReg		  = NULL;
    m_pcDamageReg	  = NULL;
    m_pcActiveDamageReg	  = NULL;
    m_pcFullReg		  = NULL;
    m_pcPrevFullReg	  = NULL;
    m_bIsUpdating	  = false;
    m_bHasInvalidRegs	  = true;
    m_nLevel		  = 0;
    m_cFontViewListIterator = NULL;
    m_bIsAddedToFont	    = false;

    m_nHideCount	= 0;
  
    m_pcLowerSibling	= m_pcHigherSibling = NULL;
    m_pcTopChild	= m_pcBottomChild   = NULL;

    m_pcBitmap = (NULL == m_pcParent) ? NULL : m_pcParent->m_pcBitmap;

    m_pcFont		= NULL;
    m_nDrawingMode	= DM_COPY;
    m_sFgColor.red	= 255;
    m_sFgColor.green	= 255;
    m_sFgColor.blue	= 255;

    m_sBgColor.red	= 170;
    m_sBgColor.green	= 170;
    m_sBgColor.blue	= 170;

    m_nRegionUpdateCount= 0;
    m_nMouseOffCnt	= 0;
    m_bIsMouseOn	= true;
    m_bFontPalletteValid= false;
    m_hHandle = g_pcLayers->Insert( this );
    m_bBackdrop = false;
  
    memset( m_asFontPallette, 255, sizeof( m_asFontPallette ) );
}

//----------------------------------------------------------------------------
// NAME:
// DESC:	Class constructor. Used for initializing of normal widgets.
// NOTE:
// SEE ALSO:
//----------------------------------------------------------------------------

Layer::Layer( SrvWindow* pcWindow, Layer* pcParent, const char* pzName,
	      const Rect& cFrame, int nFlags, void* pUserObj ) : m_cName( pzName ), m_cFrame(cFrame), m_cIFrame(cFrame)
{
    m_pcWindow	  = pcWindow;
    m_pcParent	  = pcParent;
    m_pUserObject = pUserObj;
    m_nFlags	  = nFlags;

    Init();
  
    if ( NULL != pcParent ) {
	pcParent->AddChild( this, true );
    }
}

//----------------------------------------------------------------------------
// NAME:
// DESC: Class constructor. Used for initializing of the screens top widget.
// NOTE:
// SEE ALSO:
//----------------------------------------------------------------------------

Layer::Layer( SrvBitmap* pcBitmap ) : m_cName( "bitmap_layer" )
{
    m_pcWindow = NULL;
    m_cIFrame  = IRect( 0, 0, pcBitmap->m_nWidth - 1, pcBitmap->m_nHeight - 1 );
    m_cFrame   = static_cast<Rect>(m_cIFrame);
    m_pcParent = NULL;
    m_nFlags   = 0;
    Init();
    SetBitmap( pcBitmap );
}

//----------------------------------------------------------------------------
// NAME:
// DESC:
// NOTE:
// SEE ALSO:
//----------------------------------------------------------------------------

Layer::~Layer()
{
    Layer*	pcChild;
    Layer*	pcNext;

    for ( pcChild	= m_pcTopChild ; NULL != pcChild ; pcChild = pcNext )
    {
	pcNext = pcChild->m_pcLowerSibling;
	delete pcChild;
    }
    g_pcLayers->Remove( m_hHandle );
    DeleteRegions();

    delete m_pcDrawConstrainReg;
    delete m_pcShapeConstrainReg;
    
    if ( m_pcFont != NULL ) {
	if ( m_bIsAddedToFont ) {
	    m_pcFont->RemoveDependency( m_cFontViewListIterator );
	}
	m_pcFont->Release();
    }
}

//----------------------------------------------------------------------------
// NAME:
// DESC:
// NOTE:
// SEE ALSO:
//----------------------------------------------------------------------------

void Layer::Show( bool bFlag )
{
    if ( m_pcParent == NULL || m_pcWindow == NULL ) {
	dbprintf( "Error: Layer::Show() attempt to hide root layer\n" );
	return;
    }
    if ( bFlag ) {
	m_nHideCount--;
    } else {
	m_nHideCount++;
    }

    m_pcParent->m_bHasInvalidRegs = true;
    SetDirtyRegFlags();
    
    Layer* pcSibling;
    for ( pcSibling = m_pcParent->m_pcBottomChild ; pcSibling != NULL ; pcSibling = pcSibling->m_pcHigherSibling ) {
	if ( pcSibling->m_cIFrame.DoIntersect( m_cIFrame ) ) {
	    pcSibling->MarkModified( m_cIFrame - pcSibling->m_cIFrame.LeftTop() );
	}
    }

    Layer* pcChild;
    for ( pcChild	= m_pcTopChild ; NULL != pcChild ; pcChild = pcChild->m_pcLowerSibling ) {
	pcChild->Show( bFlag );
    }
}

//----------------------------------------------------------------------------
// NAME:
// DESC:
// NOTE:
// SEE ALSO:
//----------------------------------------------------------------------------

void Layer::SetWindow( SrvWindow* pcWindow )
{
    m_pcWindow = pcWindow;
}

//----------------------------------------------------------------------------
// NAME:
// DESC:
// NOTE:
// SEE ALSO:
//----------------------------------------------------------------------------

void Layer::AddChild( Layer* pcChild, bool bTopmost )
{
    if ( NULL == m_pcBottomChild && NULL == m_pcTopChild )
    {
	m_pcBottomChild		= pcChild;
	m_pcTopChild		= pcChild;
	pcChild->m_pcLowerSibling	= NULL;
	pcChild->m_pcHigherSibling	= NULL;
    }
    else
    {
	if ( bTopmost )
	{
	    if ( pcChild->m_bBackdrop == false ) {
		if ( NULL != m_pcTopChild ) {
		    m_pcTopChild->m_pcHigherSibling = pcChild;
		}
		__assertw( m_pcTopChild != NULL );
		pcChild->m_pcLowerSibling = m_pcTopChild;
		m_pcTopChild		  = pcChild;
		pcChild->m_pcHigherSibling= NULL;
	    } else {
		Layer* pcTmp;
		for ( pcTmp = m_pcBottomChild ; pcTmp != NULL ; pcTmp = pcTmp->m_pcHigherSibling ) {
		    if ( pcTmp->m_bBackdrop == false ) {
			break;
		    }
		}
		if ( pcTmp != NULL ) {
		    pcChild->m_pcHigherSibling = pcTmp;
		    pcChild->m_pcLowerSibling  = pcTmp->m_pcLowerSibling;
		    pcTmp->m_pcLowerSibling = pcChild;
		    if ( pcChild->m_pcLowerSibling != NULL ) {
			pcChild->m_pcLowerSibling->m_pcHigherSibling = pcChild;
		    } else {
			m_pcBottomChild = pcChild;
		    }
		} else {
		    m_pcTopChild->m_pcHigherSibling = pcChild;
		    pcChild->m_pcLowerSibling 	  = m_pcTopChild;
		    m_pcTopChild		    	  = pcChild;
		    pcChild->m_pcHigherSibling	  = NULL;
		}
	    }
	}
	else
	{
	    if ( pcChild->m_bBackdrop ) {
		if ( NULL != m_pcBottomChild ) {
		    m_pcBottomChild->m_pcLowerSibling = pcChild;
		}
		__assertw( m_pcBottomChild != NULL );
		pcChild->m_pcHigherSibling = m_pcBottomChild;
		m_pcBottomChild		 = pcChild;
		pcChild->m_pcLowerSibling	 = NULL;
	    } else {
		Layer* pcTmp;
		for ( pcTmp = m_pcBottomChild ; pcTmp != NULL ; pcTmp = pcTmp->m_pcHigherSibling ) {
		    if ( pcTmp->m_bBackdrop == false ) {
			break;
		    }
		}
		if ( pcTmp != NULL ) {
		    pcChild->m_pcHigherSibling = pcTmp;
		    pcChild->m_pcLowerSibling  = pcTmp->m_pcLowerSibling;
		    pcTmp->m_pcLowerSibling = pcChild;
		    if ( pcChild->m_pcLowerSibling != NULL ) {
			pcChild->m_pcLowerSibling->m_pcHigherSibling = pcChild;
		    } else {
			m_pcBottomChild = pcChild;
		    }
		} else {
		    m_pcTopChild->m_pcHigherSibling = pcChild;
		    pcChild->m_pcLowerSibling 	  = m_pcTopChild;
		    m_pcTopChild		    	  = pcChild;
		    pcChild->m_pcHigherSibling	  = NULL;
		}
	    }
	}
    }
    pcChild->SetBitmap( m_pcBitmap );
    pcChild->m_pcParent = this;

    pcChild->Added( m_nHideCount );	// Alloc clipping regions
}

//----------------------------------------------------------------------------
// NAME:
// DESC:
// NOTE:
// SEE ALSO:
//----------------------------------------------------------------------------

void Layer::RemoveChild( Layer* pcChild )
{
    __assertw( pcChild->m_pcParent == this );

    if ( pcChild == m_pcTopChild ) {
	m_pcTopChild = pcChild->m_pcLowerSibling;
    }
    if ( pcChild == m_pcBottomChild ) {
	m_pcBottomChild = pcChild->m_pcHigherSibling;
    }
    if ( NULL != pcChild->m_pcLowerSibling ) {
	pcChild->m_pcLowerSibling->m_pcHigherSibling = pcChild->m_pcHigherSibling;
    }
    if ( NULL != pcChild->m_pcHigherSibling ) {
	pcChild->m_pcHigherSibling->m_pcLowerSibling = pcChild->m_pcLowerSibling;
    }
    pcChild->SetBitmap( NULL );
    pcChild->m_pcLowerSibling  = NULL;
    pcChild->m_pcHigherSibling = NULL;
    pcChild->m_pcParent	     = NULL;
}

//----------------------------------------------------------------------------
// NAME:
// DESC:
// NOTE:
// SEE ALSO:
//----------------------------------------------------------------------------

void Layer::Added( int nHideCount )
{
    Layer* pcChild;

    m_nHideCount += nHideCount;
    if ( m_pcParent != NULL ) {
	m_nLevel = m_pcParent->m_nLevel + 1;
    }
    for ( pcChild	= m_pcTopChild ; NULL != pcChild ; pcChild = pcChild->m_pcLowerSibling ) {
	pcChild->Added( nHideCount );
    }
}

//----------------------------------------------------------------------------
// NAME:
// DESC:
// NOTE:
// SEE ALSO:
//----------------------------------------------------------------------------

void Layer::DeleteRegions()
{
    __assertw( m_pcPrevVisibleReg == NULL );
    __assertw( m_pcPrevFullReg == NULL );

    delete m_pcVisibleReg;
    delete m_pcFullReg;
    delete m_pcDamageReg;
    delete m_pcActiveDamageReg;
    delete m_pcDrawReg;
  
    m_pcVisibleReg     	= NULL;
    m_pcFullReg	     	= NULL;
    m_pcDrawReg	     	= NULL;
    m_pcDamageReg	= NULL;
    m_pcActiveDamageReg = NULL;
    m_pcDrawReg	     	= NULL;
  
    Layer* pcChild;
    for ( pcChild	= m_pcTopChild ; NULL != pcChild ; pcChild = pcChild->m_pcLowerSibling ) {
	pcChild->DeleteRegions();
    }
}

//----------------------------------------------------------------------------
// NAME:
// DESC:
// NOTE:
// SEE ALSO:
//----------------------------------------------------------------------------

void Layer::RemoveThis()
{
    Layer* pcParent = GetParent();

    if ( pcParent != NULL ) {
	pcParent->RemoveChild( this );
    }
}

//----------------------------------------------------------------------------
// NAME:
// DESC:
// NOTE:
// SEE ALSO:
//----------------------------------------------------------------------------

Layer* Layer::GetChildAt( Point cPos )
{
    Layer* pcChild;

    for ( pcChild	= m_pcTopChild ; NULL != pcChild ; pcChild = pcChild->m_pcLowerSibling )
    {
	if ( pcChild->m_nHideCount > 0 ) {
	    continue;
	}
	if ( !((cPos.x < pcChild->m_cFrame.left) ||
	       (cPos.x > pcChild->m_cFrame.right) ||
	       (cPos.y < pcChild->m_cFrame.top) ||
	       (cPos.y > pcChild->m_cFrame.bottom)) )
	{
	    return( pcChild );	// May ask the child to hit test first ???
	}
    }
    return( NULL );
}

//----------------------------------------------------------------------------
// NAME:
// DESC:
// NOTE:
// SEE ALSO:
//----------------------------------------------------------------------------

void Layer::UpdateIfNeeded( bool bForce )
{
    Layer* pcChild;

    if ( m_nHideCount > 0 ) {
	return;
    }
    if ( m_pcWindow != NULL && ((m_pcWindow->GetDesktopMask() & (1 << get_active_desktop())) == 0 ||
				m_pcWindow->HasPendingSizeEvents( this )) ) {
	return;
    }
  
    if ( m_pcDamageReg != NULL ) {
	if ( m_pcActiveDamageReg == NULL ) {
	    m_pcActiveDamageReg = m_pcDamageReg;
	    m_pcDamageReg = NULL;
	    m_pcActiveDamageReg->Optimize();
	    Paint( static_cast<Rect>(m_pcActiveDamageReg->GetBounds()), true );
	}
    }
    for ( pcChild = m_pcBottomChild ; NULL != pcChild ; pcChild = pcChild->m_pcHigherSibling ) {
//    if ( m_nLevel != 0 || pcChild->m_pcWindow == NULL ) {
	pcChild->UpdateIfNeeded( bForce );
//    }
    }
}

//----------------------------------------------------------------------------
// NAME:
// DESC:
// NOTE:
// SEE ALSO:
//----------------------------------------------------------------------------

void Layer::SetBitmap( SrvBitmap* pcBitmap )
{
    Layer* pcChild;

    m_bFontPalletteValid  = false;
    m_pcBitmap = pcBitmap;

    for ( pcChild	= m_pcTopChild ; NULL != pcChild ; pcChild = pcChild->m_pcLowerSibling ) {
	pcChild->SetBitmap( pcBitmap );
    }
}

//----------------------------------------------------------------------------
// NAME:
// DESC:
// NOTE:
// SEE ALSO:
//----------------------------------------------------------------------------

int Layer::ToggleDepth()
{
    if ( m_pcParent != NULL )
    {
	Layer* pcParent = m_pcParent;
    
	if ( pcParent->m_pcTopChild == this )
	{
	    pcParent->RemoveChild( this );
	    pcParent->AddChild( this, false );
	}
	else
	{
	    pcParent->RemoveChild( this );
	    pcParent->AddChild( this, true );
	}

	m_pcParent->m_bHasInvalidRegs = true;
	SetDirtyRegFlags();
    
	Layer* pcSibling;
	for ( pcSibling = m_pcParent->m_pcBottomChild ; pcSibling != NULL ; pcSibling = pcSibling->m_pcHigherSibling ) {
	    if ( pcSibling->m_cIFrame.DoIntersect( m_cIFrame ) ) {
		pcSibling->MarkModified( m_cIFrame - pcSibling->m_cIFrame.LeftTop() );
	    }
	}
    }
    return( false );
}

void Layer::MoveToFront()
{
    if ( m_pcParent == NULL ) {
	return;
    }
    Layer* pcParent = m_pcParent;
    
    if ( pcParent->m_pcTopChild == this ) {
	return;
    }
    pcParent->RemoveChild( this );
    pcParent->AddChild( this, true );
    

    m_pcParent->m_bHasInvalidRegs = true;
    SetDirtyRegFlags();
    
    Layer* pcSibling;
    for ( pcSibling = m_pcParent->m_pcBottomChild ; pcSibling != NULL ; pcSibling = pcSibling->m_pcHigherSibling ) {
	if ( pcSibling->m_cIFrame.DoIntersect( m_cIFrame ) ) {
	    pcSibling->MarkModified( m_cIFrame - pcSibling->m_cIFrame.LeftTop() );
	}
    }
}

void Layer::MoveToBack()
{
    if ( m_pcParent == NULL ) {
	return;
    }
    Layer* pcParent = m_pcParent;
    
    if ( pcParent->m_pcBottomChild == this ) {
	return;
    }
    pcParent->RemoveChild( this );
    pcParent->AddChild( this, false );
    

    m_pcParent->m_bHasInvalidRegs = true;
    SetDirtyRegFlags();
    
    Layer* pcSibling;
    for ( pcSibling = m_pcParent->m_pcBottomChild ; pcSibling != NULL ; pcSibling = pcSibling->m_pcHigherSibling ) {
	if ( pcSibling->m_cIFrame.DoIntersect( m_cIFrame ) ) {
	    pcSibling->MarkModified( m_cIFrame - pcSibling->m_cIFrame.LeftTop() );
	}
    }
}

//----------------------------------------------------------------------------
// NAME:
// DESC:
// NOTE:
// SEE ALSO:
//----------------------------------------------------------------------------

void Layer::MarkModified( const IRect& cRect )
{
    if ( GetBounds().DoIntersect( cRect ) ) {
	m_bHasInvalidRegs = true;
    
	Layer* pcChild;
	for ( pcChild = m_pcBottomChild ; NULL != pcChild ; pcChild = pcChild->m_pcHigherSibling ) {
	    pcChild->MarkModified( cRect - pcChild->m_cIFrame.LeftTop() );
	}
    }
}

//----------------------------------------------------------------------------
// NAME:
// DESC:
//	Mark the clipping region for ourself and all our childs as modified.
// NOTE:
// SEE ALSO:
//----------------------------------------------------------------------------

void Layer::SetDirtyRegFlags()
{
    m_bHasInvalidRegs = true;
    
    Layer* pcChild;
    for ( pcChild = m_pcBottomChild ; NULL != pcChild ; pcChild = pcChild->m_pcHigherSibling ) {
	pcChild->SetDirtyRegFlags();
    }
}

//----------------------------------------------------------------------------
// NAME:
// DESC:
// NOTE:
// SEE ALSO:
//----------------------------------------------------------------------------

int SortCmp( const void* pNode1, const void* pNode2 )
{
    ClipRect* pcClip1 = *((ClipRect**)pNode1);
    ClipRect* pcClip2 = *((ClipRect**)pNode2);


    if ( pcClip1->m_cBounds.left > pcClip2->m_cBounds.right && pcClip1->m_cBounds.right < pcClip2->m_cBounds.left ) {
	if ( pcClip1->m_cMove.x < 0 ) {
	    return( pcClip1->m_cBounds.left - pcClip2->m_cBounds.left );
	} else {
	    return( pcClip2->m_cBounds.left - pcClip1->m_cBounds.left );
	}
    } else {
	if ( pcClip1->m_cMove.y < 0 ) {
	    return( pcClip1->m_cBounds.top - pcClip2->m_cBounds.top );
	} else {
	    return( pcClip2->m_cBounds.top - pcClip1->m_cBounds.top );
	}
    }
}

//----------------------------------------------------------------------------
// NAME:
// DESC:
// NOTE:
// SEE ALSO:
//----------------------------------------------------------------------------

void Layer::ScrollBy( const Point& cOffset )
{
    if ( m_pcParent == NULL ) {
	return;
    }
    IPoint cOldOffset = m_cIScrollOffset;
    m_cScrollOffset += cOffset;
    m_cIScrollOffset = static_cast<IPoint>(m_cScrollOffset);

    if ( m_cIScrollOffset == cOldOffset ) {
	return;
    }
    
    IPoint cIOffset = m_cIScrollOffset - cOldOffset;

    Layer* pcChild;
    for ( pcChild = m_pcBottomChild ; NULL != pcChild ; pcChild = pcChild->m_pcHigherSibling ) {
	pcChild->m_cIFrame += cIOffset;
	pcChild->m_cFrame  += static_cast<Point>(cIOffset);
    }
    if ( m_nHideCount > 0 ) {
	return;
    }

    UpdateRegions();
    SrvWindow::HandleMouseTransaction();
    
    if ( NULL == m_pcFullReg || m_pcBitmap == NULL ) {
	return;
    }

    Rect	 cBounds = GetBounds();
    IRect	 cIBounds( cBounds );
    ClipRect*	 pcSrcClip;
    ClipRect*	 pcDstClip;
    ClipRectList cBltList;
    Region	 cDamage( *m_pcVisibleReg );

    ENUMCLIPLIST( &m_pcFullReg->m_cRects, pcSrcClip ) {
	  /***	Clip to source rectangle	***/
	IRect cSRect	= cIBounds & pcSrcClip->m_cBounds;

	  /***	Transform into destination space	***/

	if ( cSRect.IsValid() == false ) {
	    continue;
	}
	cSRect += cIOffset;

	ENUMCLIPLIST( &m_pcFullReg->m_cRects, pcDstClip ) {
	    IRect cDRect = cSRect & pcDstClip->m_cBounds;

	    if ( cDRect.IsValid() == false ) {
		continue;
	    }
	    cDamage.Exclude( cDRect );

	    ClipRect* pcClips = Region::AllocClipRect();
	    pcClips->m_cBounds  = cDRect;
	    pcClips->m_cMove    = cIOffset;

	    cBltList.AddRect( pcClips );
	}
    }
  
    int nCount = cBltList.GetCount();
    
    if ( nCount == 0 ) {
	Invalidate( cIBounds );
	UpdateIfNeeded( true );
	return;
    }
    IPoint cTopLeft( ConvertToRoot( Point( 0, 0 ) ) );

    ClipRect** apsClips = new ClipRect*[nCount];

    for ( int i = 0 ; i < nCount ; ++i )
    {
	apsClips[i] = cBltList.RemoveHead();
	__assertw( apsClips[i] != NULL );
    }
    qsort( apsClips, nCount, sizeof( ClipRect* ), SortCmp );

    for ( int i = 0 ; i < nCount ; ++i )
    {
	ClipRect* pcClip = apsClips[i];
	
	pcClip->m_cBounds += cTopLeft;	// Convert into screen space
	
	m_pcBitmap->m_pcDriver->BltBitmap( m_pcBitmap, m_pcBitmap, pcClip->m_cBounds - pcClip->m_cMove,
					   IPoint( pcClip->m_cBounds.left, pcClip->m_cBounds.top ), DM_COPY );
	Region::FreeClipRect( pcClip );
    }
    delete[] apsClips;

    if ( m_pcDamageReg != NULL ) {
	ENUMCLIPLIST( &m_pcDamageReg->m_cRects, pcDstClip ) {
	    pcDstClip->m_cBounds += cIOffset;
	}
    }

    if ( m_pcActiveDamageReg != NULL ) {
	ENUMCLIPLIST( &m_pcActiveDamageReg->m_cRects, pcDstClip ) {
	    pcDstClip->m_cBounds += cIOffset;
	}
    }
    ENUMCLIPLIST( &cDamage.m_cRects, pcDstClip ) {
	Invalidate( pcDstClip->m_cBounds );
    }
    UpdateIfNeeded( true );
}

//----------------------------------------------------------------------------
// NAME:
// DESC:
// NOTE:
// SEE ALSO:
//----------------------------------------------------------------------------

void Layer::SetFrame( const Rect& cRect )
{
    IRect cIRect( cRect );
    m_cFrame = cRect;
    
    if ( m_nHideCount == 0 ) {
	if ( m_cIFrame == cIRect ) {
	    return;
	}
	m_cDeltaMove += IPoint( cIRect.left, cIRect.top ) - IPoint( m_cIFrame.left, m_cIFrame.top );
	m_cDeltaSize += IPoint( cIRect.Width(), cIRect.Height() ) - IPoint( m_cIFrame.Width(), m_cIFrame.Height() );

	if ( m_pcParent != NULL ) {
	    m_pcParent->m_bHasInvalidRegs = true;
	}

	SetDirtyRegFlags();
	Layer* pcSibling;
	for ( pcSibling = m_pcLowerSibling ; pcSibling != NULL ; pcSibling = pcSibling->m_pcLowerSibling ) {
	    if ( pcSibling->m_cIFrame.DoIntersect( m_cIFrame ) || pcSibling->m_cIFrame.DoIntersect( cIRect ) ) {
		pcSibling->MarkModified( m_cIFrame - pcSibling->m_cIFrame.LeftTop() );
		pcSibling->MarkModified( cIRect - pcSibling->m_cIFrame.LeftTop() );
	    }
	}
    }
    m_cIFrame = cIRect;
    if ( m_pcParent == NULL ) {
	Invalidate();
    }
}

void Layer::SetDrawRegion( Region* pcReg )
{
    delete m_pcDrawConstrainReg;
    m_pcDrawConstrainReg = pcReg;

    if ( m_nHideCount == 0 ) {
	m_bHasInvalidRegs = true;
    }
    delete m_pcDrawReg;
    m_pcDrawReg = NULL;
}

void Layer::SetShapeRegion( Region* pcReg )
{
    delete m_pcShapeConstrainReg;
    m_pcShapeConstrainReg = pcReg;

    if ( m_nHideCount == 0 ) {
	if ( m_pcParent != NULL ) {
	    m_pcParent->m_bHasInvalidRegs = true;
	}
	SetDirtyRegFlags();
	Layer* pcSibling;
	for ( pcSibling = m_pcLowerSibling ; pcSibling != NULL ; pcSibling = pcSibling->m_pcLowerSibling ) {
	    if ( pcSibling->m_cIFrame.DoIntersect( m_cIFrame ) ) {
		pcSibling->MarkModified( m_cIFrame - pcSibling->m_cIFrame.LeftTop() );
	    }
	}
    }
}

//----------------------------------------------------------------------------
// NAME:
// DESC:
// NOTE: The region gate must be closed prior to calling this function
// SEE ALSO:
//----------------------------------------------------------------------------

void Layer::Invalidate( const IRect& cRect )
{
    if ( m_nHideCount == 0 ) {
	if ( m_pcDamageReg == NULL ) {
	    m_pcDamageReg = new Region( cRect );
	} else {
	    m_pcDamageReg->Include( cRect );
	}
    }
}

void Layer::Invalidate( bool bReqursive )
{
    if ( m_nHideCount == 0 ) {
	delete m_pcDamageReg;
	m_pcDamageReg = new Region( static_cast<IRect>(GetBounds()) );
	if ( bReqursive ) {
	    for ( Layer* pcChild = m_pcBottomChild ; NULL != pcChild ; pcChild = pcChild->m_pcHigherSibling ) {
		pcChild->Invalidate( true );
	    }
	}
    }
}


//----------------------------------------------------------------------------
// NAME:
// DESC:
// NOTE:
// SEE ALSO:
//----------------------------------------------------------------------------

void Layer::InvalidateNewAreas( void )
{
    Region* pcRegion;

    if ( m_nHideCount > 0 ) {
	return;
    }
    if ( m_pcWindow != NULL && (m_pcWindow->GetDesktopMask() & (1 << get_active_desktop())) == 0 ) {
	return;
    }
  
    if ( m_bHasInvalidRegs )
    {
	if ( ((m_nFlags & WID_FULL_UPDATE_ON_H_RESIZE) && m_cDeltaSize.x != 0) ||
	     ((m_nFlags & WID_FULL_UPDATE_ON_V_RESIZE) && m_cDeltaSize.y != 0) ) {
	    Invalidate( false );
	} else {
	    if ( m_pcVisibleReg != NULL ) {
		pcRegion = new Region( *m_pcVisibleReg );
    
		if ( pcRegion != NULL ) {
		    if ( m_pcPrevVisibleReg != NULL ) {
			pcRegion->Exclude( *m_pcPrevVisibleReg );
		    }
		    if ( m_pcDamageReg == NULL ) {
			if ( pcRegion->IsEmpty() == false ) {
			    m_pcDamageReg = pcRegion;
			} else {
			    delete pcRegion;
			}
		    } else {
			ClipRect* pcClip;
			ENUMCLIPLIST( &pcRegion->m_cRects, pcClip ) {
			    Invalidate( pcClip->m_cBounds );
			}
			delete pcRegion;
		    }
		}
	    }
	}
	delete m_pcPrevVisibleReg;
	m_pcPrevVisibleReg = NULL;
	
	m_cDeltaSize = IPoint( 0, 0 );
	m_cDeltaMove = IPoint( 0, 0 );
    }
  
    for ( Layer* pcChild = m_pcBottomChild ; NULL != pcChild ; pcChild = pcChild->m_pcHigherSibling ) {
//    if ( m_nLevel != 0 || pcChild->m_pcWindow == NULL ) {
	pcChild->InvalidateNewAreas();
//    }
    }
}

//----------------------------------------------------------------------------
// NAME:
// DESC:
// NOTE:
// SEE ALSO:
//----------------------------------------------------------------------------

void Layer::MoveChilds()
{
    if ( m_nHideCount > 0 || NULL == m_pcBitmap ) {
	return;
    }
    if ( m_pcWindow != NULL && (m_pcWindow->GetDesktopMask() & (1 << get_active_desktop())) == 0 ) {
	return;
    }
  
    Layer* pcChild;
    if ( m_bHasInvalidRegs )
    {
	Rect cBounds = GetBounds();
	IRect cIBounds( cBounds );
	
	for ( pcChild = m_pcBottomChild ; NULL != pcChild ; pcChild = pcChild->m_pcHigherSibling )
	{
	    if ( pcChild->m_cDeltaMove.x == 0.0f && pcChild->m_cDeltaMove.y == 0.0f ) {
		continue;
	    }
	    if ( pcChild->m_pcFullReg == NULL || pcChild->m_pcPrevFullReg == NULL ) {
		continue;
	    }

	    Region* pcRegion = new Region( *pcChild->m_pcPrevFullReg );
	    pcRegion->Intersect( *pcChild->m_pcFullReg );

	    ClipRect* pcClip;
	    int nCount = 0;
    
	    IPoint cTopLeft( ConvertToRoot( Point( 0, 0 ) ) );

	    IPoint cChildOffset( IPoint( pcChild->m_cIFrame.left, pcChild->m_cIFrame.top ) + cTopLeft );
	    IPoint cChildMove( pcChild->m_cDeltaMove );
	    ENUMCLIPLIST( &pcRegion->m_cRects, pcClip ) {
		  // Transform into parents coordinate system
		pcClip->m_cBounds += cChildOffset;
		pcClip->m_cMove    = cChildMove;
		nCount++;
		__assertw( pcClip->m_cBounds.IsValid() );
	    }
	    if ( nCount == 0 ) {
		delete pcRegion;
		continue;
	    }

	    ClipRect** apsClips = new ClipRect*[nCount];

	    for ( int i = 0 ; i < nCount ; ++i ) {
		apsClips[i] = pcRegion->m_cRects.RemoveHead();
		__assertw( apsClips[i] != NULL );
	    }
	    qsort( apsClips, nCount, sizeof( ClipRect* ), SortCmp );

	    for ( int i = 0 ; i < nCount ; ++i ) {
		ClipRect* pcClip = apsClips[i];
	
		m_pcBitmap->m_pcDriver->BltBitmap( m_pcBitmap, m_pcBitmap, pcClip->m_cBounds - pcClip->m_cMove,
						   IPoint( pcClip->m_cBounds.left, pcClip->m_cBounds.top ), DM_COPY );
		m_pcVisibleReg->FreeClipRect( pcClip );
	    }
	    delete[] apsClips;
	    delete pcRegion;
	}
	  /*	Since the parent window is shrinked before the childs is moved
	   *	we may need to redraw the right and bottom edges.
	   */
	if ( NULL != m_pcParent && (m_cDeltaMove.x != 0.0f || m_cDeltaMove.y != 0.0f) ) {
	    if ( m_pcParent->m_cDeltaSize.x < 0 ) {
		IRect	cRect = cIBounds;

		cRect.left = cRect.right + int( m_pcParent->m_cDeltaSize.x + m_pcParent->m_cIFrame.right -
						m_pcParent->m_cIFrame.left - m_cIFrame.right );

		if ( cRect.IsValid() ) {
		    Invalidate( cRect );
		}
	    }
	    if ( m_pcParent->m_cDeltaSize.y < 0 ) {
		IRect	cRect = cIBounds;

		cRect.top = cRect.bottom + int( m_pcParent->m_cDeltaSize.y + m_pcParent->m_cIFrame.bottom -
						m_pcParent->m_cIFrame.top - m_cIFrame.bottom );

		if ( cRect.IsValid() ) {
		    Invalidate( cRect );
		}
	    }
	}
	delete m_pcPrevFullReg;
	m_pcPrevFullReg = NULL;
    }
    for ( pcChild = m_pcBottomChild ; NULL != pcChild ; pcChild = pcChild->m_pcHigherSibling ) {
//    if ( m_nLevel != 0 || pcChild->m_pcWindow == NULL ) {
	pcChild->MoveChilds();
//    }
    }
}

//----------------------------------------------------------------------------
// NAME:
// DESC:
//	Stores the previous visible region in m_pcPrevVisibleReg and then
//	rebuilds m_pcVisibleReg, starting with whatever is left of our parent
//	and then removing areas covered by siblings.
// NOTE:
//	Areas covered by childrens are not removed.
// SEE ALSO:
//----------------------------------------------------------------------------

void Layer::SwapRegions( bool bForce )
{
    if ( bForce ) {
	m_bHasInvalidRegs = true;
    }
  
    Layer* pcChild;
    for ( pcChild = m_pcBottomChild ; NULL != pcChild ; pcChild = pcChild->m_pcHigherSibling ) {
	pcChild->SwapRegions( bForce );
    }
}


void Layer::RebuildRegion( bool bForce )
{
    Layer*	pcSibling;
    Layer*	pcChild;

    if ( m_pcWindow != NULL && (m_pcWindow->GetDesktopMask() & (1 << get_active_desktop())) == 0 ) {
	return;
    }

    if ( m_nHideCount > 0 ) {
	if ( m_pcVisibleReg != NULL ) {
	    DeleteRegions();
	}
	return;
    }
  
    if ( bForce ) {
	m_bHasInvalidRegs = true;
    }
  
    if ( m_bHasInvalidRegs )
    {
	delete m_pcDrawReg;
	m_pcDrawReg = NULL;

	__assertw( m_pcPrevVisibleReg == NULL );
	__assertw( m_pcPrevFullReg == NULL );
    
	m_pcPrevVisibleReg = m_pcVisibleReg;
	m_pcPrevFullReg    = m_pcFullReg;

	if ( m_pcParent == NULL ) {
	    m_pcFullReg = new Region( m_cIFrame );
	} else {
	    __assertw( m_pcParent->m_pcFullReg != NULL );
	    if ( m_pcParent->m_pcFullReg == NULL ) {
		m_pcFullReg = new Region( m_cIFrame );
	    } else {
		m_pcFullReg = new Region( *m_pcParent->m_pcFullReg, m_cIFrame, true );
	    }
	    if ( m_pcShapeConstrainReg != NULL ) {
		m_pcFullReg->Intersect( *m_pcShapeConstrainReg );
	    }
	}
//    if ( m_nLevel == 1 ) {
	IPoint cLeftTop( m_cIFrame.LeftTop() );
	for ( pcSibling = m_pcHigherSibling ; NULL != pcSibling ; pcSibling = pcSibling->m_pcHigherSibling ) {
	    if ( pcSibling->m_nHideCount == 0 ) {
		if ( pcSibling->m_cIFrame.DoIntersect( m_cIFrame ) ) {
		    if ( pcSibling->m_pcShapeConstrainReg == NULL ) {
			m_pcFullReg->Exclude( pcSibling->m_cIFrame - cLeftTop );
		    } else {
			m_pcFullReg->Exclude( *pcSibling->m_pcShapeConstrainReg, pcSibling->m_cIFrame.LeftTop() - cLeftTop );
		    }
		}
	    }
	}
//    }
	m_pcFullReg->Optimize();
	m_pcVisibleReg = new Region( *m_pcFullReg );

	if ( (m_nFlags & WID_DRAW_ON_CHILDREN) == 0 ) {
	    bool bRegModified = false;
	    for ( pcChild = m_pcBottomChild ; NULL != pcChild ; pcChild = pcChild->m_pcHigherSibling ) {
		  /***	Remove children from child region	***/
		if ( pcChild->m_nHideCount == 0 && (pcChild->m_nFlags & WID_TRANSPARENT) == 0 ) {
		    if ( pcChild->m_pcShapeConstrainReg == NULL ) {
			m_pcVisibleReg->Exclude( pcChild->m_cIFrame );
		    } else {
			m_pcVisibleReg->Exclude( *pcChild->m_pcShapeConstrainReg, pcChild->m_cIFrame.LeftTop() );
		    }
		    bRegModified = true;
		}
	    }
	    if ( bRegModified ) {
		m_pcVisibleReg->Optimize();
	    }
	}
    }
    for ( pcChild = m_pcBottomChild ; NULL != pcChild ; pcChild = pcChild->m_pcHigherSibling ) {
//    if ( m_nLevel != 0 || pcChild->m_pcWindow == NULL ) {
	pcChild->RebuildRegion( bForce );
//    }
    }
}

//----------------------------------------------------------------------------
// NAME:
// DESC:
// NOTE:
// SEE ALSO:
//----------------------------------------------------------------------------

void Layer::ClearDirtyRegFlags()
{
    m_bHasInvalidRegs = false;
    for ( Layer* pcChild = m_pcBottomChild ; NULL != pcChild ; pcChild = pcChild->m_pcHigherSibling ) {
//    if ( m_nLevel != 0 || pcChild->m_pcWindow == NULL ) {
	pcChild->ClearDirtyRegFlags();
//    }
    }
}

//----------------------------------------------------------------------------
// NAME:
// DESC:
// NOTE:
// SEE ALSO:
//----------------------------------------------------------------------------

void Layer::UpdateRegions( bool bForce, bool bRoot )
{
    RebuildRegion( bForce );
    MoveChilds();
    InvalidateNewAreas();

    if ( m_bHasInvalidRegs && m_pcParent == NULL && m_pcBitmap != NULL && m_pcDamageReg != NULL )
    {
	if ( m_pcBitmap == g_pcScreenBitmap ) {
	    ClipRect* pcClip;
	    Color32_s sColor( 0x00, 0x60, 0x6b, 0 );
	    IPoint    cTopLeft( ConvertToRoot( Point( 0, 0 ) ) );

//	    Region cTmpReg( static_cast<IRect>(GetBounds()) );

	    Region cDrawReg( *m_pcVisibleReg );
	    cDrawReg.Intersect( *m_pcDamageReg );
//	    cTmpReg.Exclude( *m_pcDamageReg );
//	    cDrawReg.Exclude( cTmpReg );
	
	    ENUMCLIPLIST( &cDrawReg.m_cRects, pcClip ) {
		m_pcBitmap->m_pcDriver->FillRect( m_pcBitmap, pcClip->m_cBounds + cTopLeft, sColor );
	    }
	}
	delete m_pcDamageReg;
	m_pcDamageReg = NULL;
    }
    UpdateIfNeeded( false );
    ClearDirtyRegFlags();
}

//----------------------------------------------------------------------------
// NAME:
// DESC:
// NOTE:
// SEE ALSO:
//----------------------------------------------------------------------------

Region* Layer::GetRegion()
{
    if ( m_nHideCount > 0 ) {
	return( NULL );
    }
    if ( m_bIsUpdating && m_pcActiveDamageReg == NULL ) {
	return( NULL );
    }

    if ( m_bIsUpdating == false ) {
	if ( m_pcDrawConstrainReg == NULL ) {
	    return( m_pcVisibleReg );
	} else {
	    if ( m_pcDrawReg == NULL ) {
		m_pcDrawReg = new Region( *m_pcVisibleReg );
		m_pcDrawReg->Intersect( *m_pcDrawConstrainReg );
	    }
	}
    } else if ( m_pcDrawReg == NULL && m_pcVisibleReg != NULL ) {
//	Region cTmpReg( static_cast<IRect>(GetBounds()) );

	m_pcDrawReg = new Region( *m_pcVisibleReg );

	__assertw( m_pcActiveDamageReg != NULL );
	
//	cTmpReg.Exclude( *m_pcActiveDamageReg );

//	m_pcDrawReg->Exclude( cTmpReg );
	m_pcDrawReg->Intersect( *m_pcActiveDamageReg );
	if ( m_pcDrawConstrainReg != NULL ) {
	    m_pcDrawReg->Intersect( *m_pcDrawConstrainReg );
	}
	m_pcDrawReg->Optimize();
    }
    __assertw( NULL != m_pcDrawReg );
    return( m_pcDrawReg );
}

//----------------------------------------------------------------------------
// NAME:
// DESC:
// NOTE:
// SEE ALSO:
//----------------------------------------------------------------------------

void Layer::PutRegion( Region* pcReg )
{
}

//----------------------------------------------------------------------------
// NAME:
// DESC:
// NOTE:
// SEE ALSO:
//----------------------------------------------------------------------------

void Layer::BeginUpdate()
{
    if ( m_pcVisibleReg != NULL ) {
	m_bIsUpdating = true;
    }
}

//----------------------------------------------------------------------------
// NAME:
// DESC:
// NOTE:
// SEE ALSO:
//----------------------------------------------------------------------------

void Layer::EndUpdate( void )
{
    delete m_pcActiveDamageReg;
    m_pcActiveDamageReg = NULL;
    delete m_pcDrawReg;
    m_pcDrawReg = NULL;
    m_bIsUpdating	= false;
  
    if ( m_pcDamageReg != NULL ) {
	m_pcActiveDamageReg = m_pcDamageReg;
	m_pcDamageReg = NULL;
	Paint( static_cast<Rect>(m_pcActiveDamageReg->GetBounds()), true );
    }
}

//----------------------------------------------------------------------------
// NAME:
// DESC:
// NOTE:
// SEE ALSO:
//----------------------------------------------------------------------------

void Layer::SetFont( FontNode* pcFont )
{
    if ( pcFont == m_pcFont ) {
	return;
    }
    if ( pcFont != NULL ) {
	pcFont->AddRef();
	if ( m_pcFont != NULL ) {
	    if ( m_bIsAddedToFont ) {
		m_pcFont->RemoveDependency( m_cFontViewListIterator );
		m_bIsAddedToFont = false;
	    }
	    m_pcFont->Release();
	}
	m_pcFont = pcFont;
	if ( m_pUserObject != NULL && m_pcWindow != NULL && m_pcWindow->GetAppTarget() != NULL ) {
	    m_cFontViewListIterator = m_pcFont->AddDependency( m_pcWindow->GetAppTarget(), m_pUserObject );
	    m_bIsAddedToFont = true;
	}
    } else {
	dbprintf( "ERROR : Layer::SetFont() called with NULL pointer\n" );
    }
}

//----------------------------------------------------------------------------
// NAME:
// DESC:
// NOTE:
// SEE ALSO:
//----------------------------------------------------------------------------

bool Layer::MouseOverlap( Point cOffset )
{
    SrvBitmap* pcBitmap = GetBitmap();

    if ( NULL != pcBitmap ) {
	if ( NULL != pcBitmap->m_pcDriver ) {
	    return( pcBitmap->m_pcDriver->IntersectWithMouse( static_cast<IRect>(ConvertToRoot( GetBounds() ) - cOffset )) );
	}
    }
    return( false );
}

//----------------------------------------------------------------------------
// NAME:
// DESC:
// NOTE:
// SEE ALSO:
//----------------------------------------------------------------------------

void Layer::MouseOff()
{
/*  
  if ( m_bIsMouseOn )
  {
    if ( MouseOverlap( Point( 0, 0 ) ) )
    {
      SrvBitmap* pcBitmap = GetBitmap();

      if ( NULL != pcBitmap )
      {
	if ( NULL != pcBitmap->m_pcDriver ) {
	  pcBitmap->m_pcDriver->MouseOff();
	  m_bIsMouseOn = false;
	}
      }
    }
  }
  m_nMouseOffCnt++;
  */  
}

//----------------------------------------------------------------------------
// NAME:
// DESC:
// NOTE:
// SEE ALSO:
//----------------------------------------------------------------------------

void Layer::MouseOn()
{
/*  
  m_nMouseOffCnt--;

  if ( m_nMouseOffCnt == 0 && m_bIsMouseOn == false )
  {
    m_bIsMouseOn = true;

    SrvBitmap* pcBitmap = GetBitmap();

    if ( NULL != pcBitmap )
    {
      if ( NULL != pcBitmap->m_pcDriver )
      {
	pcBitmap->m_pcDriver->MouseOn();
      }
    }
  }
  */  
}

font_height Layer::GetFontHeight() const
{
    if ( m_pcFont != NULL ) {
	SFontInstance* pcFontInst = m_pcFont->GetInstance();
	if ( pcFontInst != NULL ) {
	    font_height sHeight;
	    sHeight.ascender  = pcFontInst->GetAscender();
	    sHeight.descender = -pcFontInst->GetDescender();
	    sHeight.line_gap   = pcFontInst->GetLineGap();
	    return( sHeight );
	}
    }
    font_height sHeight;
    sHeight.ascender  = 0;
    sHeight.descender = 0;
    sHeight.line_gap   = 0;
    return( sHeight );
}

//----------------------------------------------------------------------------
// NAME:
// DESC:
// NOTE:
// SEE ALSO:
//----------------------------------------------------------------------------

void Layer::SetFgColor( int nRed, int nGreen, int nBlue, int nAlpha )
{
    m_sFgColor.red = nRed;
    m_sFgColor.green = nGreen;
    m_sFgColor.blue = nBlue;
    m_sFgColor.alpha = nAlpha;
    m_bFontPalletteValid = false;
}

//----------------------------------------------------------------------------
// NAME:
// DESC:
// NOTE:
// SEE ALSO:
//----------------------------------------------------------------------------

void Layer::SetFgColor( Color32_s sColor )
{
    m_sFgColor = sColor;
    m_bFontPalletteValid  = false;
}

//----------------------------------------------------------------------------
// NAME:
// DESC:
// NOTE:
// SEE ALSO:
//----------------------------------------------------------------------------

void Layer::SetBgColor( int nRed, int nGreen, int nBlue, int nAlpha )
{
    m_sBgColor.red = nRed;
    m_sBgColor.green = nGreen;
    m_sBgColor.blue = nBlue;
    m_sBgColor.alpha = nAlpha;
    m_bFontPalletteValid  = false;
}

//----------------------------------------------------------------------------
// NAME:
// DESC:
// NOTE:
// SEE ALSO:
//----------------------------------------------------------------------------

void Layer::SetBgColor( Color32_s sColor )
{
    m_sBgColor = sColor;
    m_bFontPalletteValid  = false;
}

//----------------------------------------------------------------------------
// NAME:
// DESC:
// NOTE:
// SEE ALSO:
//----------------------------------------------------------------------------

void Layer::SetEraseColor( int nRed, int nGreen, int nBlue, int nAlpha )
{
    m_sEraseColor.red   = nRed;
    m_sEraseColor.green = nGreen;
    m_sEraseColor.blue  = nBlue;
    m_sEraseColor.alpha = nAlpha;
}

//----------------------------------------------------------------------------
// NAME:
// DESC:
// NOTE:
// SEE ALSO:
//----------------------------------------------------------------------------

void Layer::SetEraseColor( Color32_s sColor )
{
    m_sEraseColor	= sColor;
}

//----------------------------------------------------------------------------
// NAME:
// DESC:
// NOTE:
// SEE ALSO:
//----------------------------------------------------------------------------

void Layer::Paint( const IRect& cUpdateRect, bool bUpdate )
{
    if ( m_nHideCount > 0 || m_bIsUpdating == true || NULL == m_pUserObject ) {
	return;
    }
    Messenger*	pcTarget = m_pcWindow->GetAppTarget();

    if ( NULL != pcTarget ) {
	Message cMsg( M_PAINT );

	cMsg.AddPointer( "_widget", GetUserObject() );
	cMsg.AddRect( "_rect", cUpdateRect - m_cIScrollOffset );

	if ( pcTarget->SendMessage( &cMsg ) < 0 ) {
	    dbprintf( "Layer::Paint() failed to send M_PAINT message to %s\n", m_cName.c_str() );
	}
    }
}

//----------------------------------------------------------------------------
// NAME:
// DESC:
// NOTE:
// SEE ALSO:
//----------------------------------------------------------------------------

Layer* FindLayer( int nToken )
{
    Layer* pcLayer = g_pcLayers->GetObj( nToken );
    return( pcLayer );
}
