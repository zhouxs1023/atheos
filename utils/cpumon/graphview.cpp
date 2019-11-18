/*  CPUMon - Small AtheOS utility for displaying the CPU load.
 *  Copyright (C) 2001 Kurt Skauen
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
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

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#include "main.h"
#include "graphview.h"

#include <atheos/time.h>
#include <w8378x_driver.h>

#include <gui/window.h>
#include <gui/stringview.h>
#include <gui/tableview.h>

enum { TIMER_ID = 1 };

using namespace os;

//----------------------------------------------------------------------------
// NAME:
// DESC:
// NOTE:
// SEE ALSO:
//----------------------------------------------------------------------------

GraphView::GraphView( const os::Rect& cRect, int nCPUCount ) : View( cRect, "graph_view", CF_FOLLOW_ALL )
{
    m_nW8378xDevice = open( "/dev/misc/w8378x", O_RDWR );

    m_nCPUCount = nCPUCount;
    m_pcTableView = new TableView( cRect.Bounds(), "_cpumon_table", NULL, m_nCPUCount, 2, CF_FOLLOW_ALL );
    m_pcTableView->SetCellBorders( 2, 2, 2, 2 );
    for ( int i = 0 ; i < m_nCPUCount ; ++i ) {
	m_apcTempViews[i] = new StringView( Rect( 0, 0, 1, 1 ), "cpu_tmp",
					    "50.0f/10.0f%", ALIGN_CENTER, WID_WILL_DRAW );
    
	m_apcMeters[i] = new MultiMeter( Rect( 0, 0, 1, 1 ), "", 0 , WID_WILL_DRAW | WID_FULL_UPDATE_ON_RESIZE );
	m_apcTempViews[i]->SetFgColor( 0, 0, 0 );
	m_pcTableView->SetColAlignment( i, ALIGN_CENTER );

	m_pcTableView->SetChild( m_apcTempViews[i], i, 0 );
	m_pcTableView->SetChild( m_apcMeters[i], i, 1 );
    }
    AddChild( m_pcTableView );
  
}

GraphView::~GraphView()
{
    if ( m_nW8378xDevice >= 0 ) {
	close( m_nW8378xDevice );
    }
}

void GraphView::AttachedToWindow()
{
    GetWindow()->AddTimer( this, TIMER_ID, 500000, false );
}

void GraphView::DetachedFromWindow()
{
    GetWindow()->RemoveTimer( this, TIMER_ID );
}


void GraphView::TimerTick( int nID )
{
    static int nCnt = 0;
  
    int nTempSensors[] = { W8378x_READ_TEMP1, W8378x_READ_TEMP2 };

    float avLoads[MAX_CPU_COUNT];
    GetLoad( m_nCPUCount, avLoads );
    
    for ( int i = 0 ; i < m_nCPUCount ; ++i ) {
	m_apcMeters[i]->AddValue( avLoads[i] );

	if ( (nCnt % 2) == 0 ) {
	    char zTmp[128];

	    if ( m_nW8378xDevice >= 0 ) {
		int nTemp = 0;
		ioctl( m_nW8378xDevice, nTempSensors[i%2], &nTemp );
		sprintf( zTmp, "%.1fc/%.1f%%", float(nTemp)/256.0f, avLoads[i] * 100.0 );
	    } else {
		sprintf( zTmp, "%.1f%%", avLoads[i] * 100.0 );
	    }
	    m_apcTempViews[i]->SetString( zTmp );
	    m_pcTableView->Layout();
	}
    }
    nCnt++;
}







//----------------------------------------------------------------------------
// NAME:
// DESC:
// NOTE:
// SEE ALSO:
//----------------------------------------------------------------------------

MultiMeter::MultiMeter(  const Rect& cFrame, const char* pzTitle, uint32 nResizeMask, uint32 nFlags  )
    : View( cFrame, pzTitle, nResizeMask, nFlags  )
{
    memset( m_avValues, 0, sizeof( m_avValues ) );
}

//----------------------------------------------------------------------------
// NAME:
// DESC:
// NOTE:
// SEE ALSO:
//----------------------------------------------------------------------------

MultiMeter::~MultiMeter()
{

}

//----------------------------------------------------------------------------
// NAME:
// DESC:
// NOTE:
// SEE ALSO:
//----------------------------------------------------------------------------

void MultiMeter::Paint( const Rect& cUpdateRect )
{
    Rect cBounds = GetBounds();
    float	 x;
    float	 y;
    int	 i;

    DrawFrame( cBounds, FRAME_RECESSED | FRAME_TRANSPARENT );
  
    cBounds.left += 2;
    cBounds.top += 2;
    cBounds.right -= 2;
    cBounds.bottom -= 2;
  
    SetFgColor( 255, 255, 255, 0 );
    FillRect( cBounds );
  
    SetFgColor( 0, 0, 0, 0 );
    for ( i = 0, x = cBounds.right ; x >= cBounds.left ; ++i )
    {
	y = (1.0f - m_avValues[ i ]) * (cBounds.Height()+1.0f);
	DrawLine( Point( x, cBounds.bottom ), Point( x, y ) );
	x -= 1.0f;
    }
}

//----------------------------------------------------------------------------
// NAME:
// DESC:
// NOTE:
// SEE ALSO:
//----------------------------------------------------------------------------

void MultiMeter::AddValue( float vValue )
{
    for ( int i = 1023 ; i > 0 ; --i )	{
	m_avValues[ i ] = m_avValues[ i - 1 ];
    }

    m_avValues[ 0 ] = vValue; //min( 1000, nValue );

    Rect cBounds = GetBounds();
  
    cBounds.left += 2;
    cBounds.top += 2;
    cBounds.right -= 2;
    cBounds.bottom -= 2;
 
    ScrollRect( cBounds + Point( 1, 0 ), cBounds );
    float y = (1.0f - m_avValues[ 0 ]) * (cBounds.Height()+1.0f);
  
    SetFgColor( 0, 0, 0, 0 );
    DrawLine( Point( cBounds.right, cBounds.bottom ), Point( cBounds.right, y ) );
  
    SetFgColor( 255, 255, 255, 0 );
    DrawLine( Point( cBounds.right, 0 ), Point( cBounds.right, y ) );
  
    Flush();
}
