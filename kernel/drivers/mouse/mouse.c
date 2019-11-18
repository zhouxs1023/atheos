/*
 *  The AtheOS kernel
 *  Copyright (C) 1999  Kurt Skauen
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

#include <posix/errno.h>
#include <posix/stat.h>
#include <posix/fcntl.h>

#include <atheos/types.h>
#include <atheos/time.h>
#include <atheos/kernel.h>
#include <atheos/device.h>
#include <atheos/mouse.h>

#include <macros.h>


typedef struct
{
    thread_id 	hWaitThread;
    MouseEvent_s	asEvents[ 256 ];
    int		nOutPos;
    int		nInPos;
    int		nEventsReceived;
    int		nOpenCount;
    int		nDevNum;
} MouseVolume_s;

#define	MOUSE_INODE_NUM		342

static MouseVolume_s	g_sVolume;

/*****************************************************************************
 * NAME:
 * DESC:
 * NOTE:
 * SEE ALSO:
 ****************************************************************************/

static bool InitMouse()
{
    struct	RMREGS	rm;

    return( TRUE );

    memset( &rm, 0, sizeof( struct RMREGS ) );

    rm.EAX	= 0x0000;
    realint( 0x33, &rm );

    return( rm.EAX & 0xFFFF );
}

/*****************************************************************************
 * NAME:
 * DESC:
 * NOTE:
 * SEE ALSO:
 ****************************************************************************/

static uint32	GetMouseButtons()
{
    struct	RMREGS	rm;

    memset( &rm, 0, sizeof( struct RMREGS ) );

    rm.EAX	= 0x0003;
    realint( 0x33, &rm );

    return( rm.EBX & 0x07 );
}

/*****************************************************************************
 * NAME:
 * DESC:
 * NOTE:
 * SEE ALSO:
 ****************************************************************************/

static uint32	ReadMouseTicks()
{
    struct	RMREGS	rm;

    memset( &rm, 0, sizeof( struct RMREGS ) );

    rm.EAX	= 0x000B;
    realint( 0x33, &rm );

    return( (rm.ECX << 16) | (rm.EDX & 0xffff) );
}

/*****************************************************************************
 * NAME:
 * DESC:
 * NOTE:
 * SEE ALSO:
 ****************************************************************************/

void msd_add_event( MouseEvent_s* psEvent )
{
    if ( 0 != psEvent->me_nDeltaX || 0 != psEvent->me_nDeltaY || 0 != psEvent->me_nButton ) {
	psEvent->me_nTimeStamp = get_real_time();
	g_sVolume.asEvents[ atomic_add( &g_sVolume.nInPos, 1 ) & 0xff ] = *psEvent;
	atomic_add( &g_sVolume.nEventsReceived, 1 );
    }
}

/*****************************************************************************
 * NAME:
 * DESC:
 * NOTE:
 * SEE ALSO:
 ****************************************************************************/

void msd_poll_mouse()
{
    static uint16 nLastButtons = 0;
    uint16	  nButtons,nButtonFlg;
    uint32	  nMouseMove;
    MouseEvent_s  sEvent;
	
    nMouseMove	= ReadMouseTicks();
    nButtons	= GetMouseButtons();
	
    nButtonFlg	 = nButtons ^ nLastButtons;
    nLastButtons = nButtons;


    sEvent.me_nDeltaX = ((int16)(nMouseMove >> 16)); /* Update mouse counters.	*/
    sEvent.me_nDeltaY = ((int16)(nMouseMove & 0xFFFF));

    if ( 0 != nButtonFlg )
    {
	if ( nButtonFlg & 0x01 ) {
	    sEvent.me_nButton = 1 | ((nButtons & 0x01) ? 0x00 : 0x80 );
	    msd_add_event( &sEvent );
	}
	if ( nButtonFlg & 0x04 ) {
	    sEvent.me_nButton = 3 | ((nButtons & 0x04) ? 0x00 : 0x80 );
	    msd_add_event( &sEvent );
	}
	if ( nButtonFlg & 0x02 ) {
	    sEvent.me_nButton = 2 | ((nButtons & 0x02) ? 0x00 : 0x80 );
	    msd_add_event( &sEvent );
	}
    }
    else
    {
	sEvent.me_nButton = 0;
	msd_add_event( &sEvent );
    }
}

/*****************************************************************************
 * NAME:
 * DESC:
 * NOTE:
 * SEE ALSO:
 ****************************************************************************/

static int msd_open( void* pNode, uint32 nFlags, void **ppCookie )
{
    int	nError;
	
    if ( 0 != g_sVolume.nOpenCount )
    {
	nError = -EBUSY;
	printk( "ERROR : Attempt to reopen mouse device\n" );
    }
    else
    {
	g_sVolume.nOpenCount++;
	nError = 0;
    }
    return( nError );
}

/*****************************************************************************
 * NAME:
 * DESC:
 * NOTE:
 * SEE ALSO:
 ****************************************************************************/

static int msd_close( void* pNode, void* pCookie )
{
    g_sVolume.nOpenCount--;
    return( 0 );
}

/*****************************************************************************
 * NAME:
 * DESC:
 * NOTE:
 * SEE ALSO:
 ****************************************************************************/

static int msd_read( void* pNode, void* pCookie, off_t nPos, void* pBuf, size_t nLen )
{
    MouseVolume_s* psVolume = &g_sVolume;
    int		 nError;

    if ( 0 == nLen ) {
	return( 0 );
    }

    if ( (nLen % sizeof( MouseEvent_s )) != 0 ) {
	return( -EINVAL );
    }

    msd_poll_mouse();
	
    for ( ;; )
    {
	if ( psVolume->nEventsReceived > 0 )
	{
	    int	nSize = min( nLen / sizeof( MouseEvent_s ), psVolume->nEventsReceived );
	    int	nEFlg;
	    int	i;
	    MouseEvent_s* psBuf = pBuf;

	    for ( i = 0 ; i < nSize ; ++i )
	    {
		psBuf[ i ] = psVolume->asEvents[ psVolume->nOutPos & 0xff ];
		psVolume->nOutPos++;
	    }
			
	    nEFlg = cli();
	    psVolume->nEventsReceived -= nSize;
	    put_cpu_flags( nEFlg );
			
	    nError = nSize * sizeof( MouseEvent_s );
	}
	else
	{
	    int	nEFlg = cli();

	    if ( -1 != psVolume->hWaitThread )
	    {
		nError = -EBUSY;
		printk( "ERROR : two threads attempted to read from keyboard device!\n" );
	    }
	    else
	    {
		if ( 1 )
		{
		    nError = -EWOULDBLOCK;
		}
		else
		{
		    psVolume->hWaitThread = get_thread_id( NULL );
		    nError = suspend();
		    psVolume->hWaitThread = -1;
		}
	    }
	    put_cpu_flags( nEFlg );
	}
	if ( 0 != nError ) {
	    break;
	}
    }
    return( nError );
}

/*****************************************************************************
 * NAME:
 * DESC:
 * NOTE:
 * SEE ALSO:
 ****************************************************************************/

DeviceOperations_s g_sOperations = {
    msd_open,
    msd_close,
    NULL,		/* ioctl*/
    msd_read,
    NULL		/* write */
};

/*****************************************************************************
 * NAME:
 * DESC:
 * NOTE:
 * SEE ALSO:
 ****************************************************************************/

status_t device_init( int nDeviceID )
{
    int nError;
  
    g_sVolume.hWaitThread     = -1;
    g_sVolume.nInPos 	    = 0;
    g_sVolume.nOutPos 	    = 0;
    g_sVolume.nEventsReceived = 0;
    g_sVolume.nOpenCount 	    = 0;

    printk( "initialize mouse driver\n" );

    InitMouse();
  
    nError = create_device_node( nDeviceID, "mouse", &g_sOperations, NULL );
    return( nError );
}

/*****************************************************************************
 * NAME:
 * DESC:
 * NOTE:
 * SEE ALSO:
 ****************************************************************************/

status_t device_uninit( int nDeviceID )
{
    return( 0 );
}
