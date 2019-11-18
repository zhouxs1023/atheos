/*
 *  The AtheOS kernel
 *  Copyright (C) 1999 - 2000 Kurt Skauen
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

#include <atheos/types.h>
#include <atheos/kernel.h>
#include <atheos/smp.h>
#include <atheos/kdebug.h>
#include <posix/errno.h>

#include <macros.h>

#include "inc/array.h"

static void*	g_pMagic[ 256 ];
static int	g_nUsedMem = 0;
static int	g_nTotObjects = 0;

static void dbg_print_stats( int argc, char** argv )
{
    dbprintf( DBP_DEBUGGER, "Memory used: %d\n", g_nUsedMem );
    dbprintf( DBP_DEBUGGER, "Num objects: %d\n", g_nTotObjects );
}

/*****************************************************************************
 * NAME:
 * DESC:
 * NOTE:
 * SEE ALSO:
 ****************************************************************************/

static void MArray_IncCount( MultiArray_s* psArray )
{
  psArray->nTabCount++;

  psArray->nAvgCount = (psArray->nAvgCount + psArray->nTabCount) / 2;

  if ( psArray->nTabCount > psArray->nMaxCount ) {
    psArray->nMaxCount = psArray->nTabCount;
  }
  g_nTotObjects++;
}

/*****************************************************************************
 * NAME:
 * DESC:
 * NOTE:
 * SEE ALSO:
 ****************************************************************************/

static void MArray_DecCount( MultiArray_s* psArray )
{
  psArray->nTabCount--;

  psArray->nAvgCount = (psArray->nAvgCount + psArray->nTabCount) / 2;
  g_nTotObjects--;
}

/*****************************************************************************
 * NAME:
 * DESC:
 * NOTE:
 * SEE ALSO:
 ****************************************************************************/

void MArray_Init( MultiArray_s* psArray )
{
    static bool bInititated = false;
    int	i;
    psArray->nLastID	= 0;

    psArray->nTabCount	=	0;
    psArray->nMaxCount	=	0;
    psArray->nAvgCount	=	0;

    if ( bInititated == false ) {
	bInititated = true;
	register_debug_cmd( "array_stat", "print stats about the multi-dimentional arrays tracking kernel-handles", dbg_print_stats );
	
	for ( i = 0 ; i < 256 ; ++i )	{
	    g_pMagic[ i ] = (void*) g_pMagic;
	}
    }
    for ( i = 0 ; i < 256 ; ++i )	{
	psArray->pArray[ i ] = (void***) g_pMagic;
    }
}

static void* ma_alloc_block( bool bNoBlock )
{
    uint32 nFlags = MEMF_KERNEL | MEMF_OKTOFAILHACK;

    if ( bNoBlock ) {
	nFlags |= MEMF_NOBLOCK;
    }
    return( kmalloc( sizeof( void* ) *  256, nFlags ) );
}


static void ma_free_block( void* pBlock )
{
    kfree( pBlock );
}

/*****************************************************************************
 * NAME:
 * DESC:
 * NOTE:
 * SEE ALSO:
 ****************************************************************************/

void MArray_Destroy( MultiArray_s* psArray )
{
}

/*****************************************************************************
 * NAME:
 * DESC:
 * NOTE:
 * SEE ALSO:
 ****************************************************************************/

int MArray_Insert( MultiArray_s* psArray, void* pObject, bool bNoBlock )
{
  kassertw( pObject != (void*) g_pMagic );
  kassertw( NULL != pObject );

  for(;;)
  {
    int	   nIndex = (psArray->nLastID++) & 0xffffff;
    void** pObj   = psArray->pArray[ (nIndex >> 16) & 0xff ][ (nIndex >> 8) & 0xff ];

    if ( pObj == g_pMagic )
    {
      if ( psArray->pArray[ (nIndex >> 16) & 0xff ] == (void***) g_pMagic )
      {
	int	i;
	void*   pBlock1;
	void*   pBlock2;


	pBlock1 = ma_alloc_block( bNoBlock );
	if ( pBlock1 == NULL ) {
	    return( -ENOMEM );
	}
	pBlock2 = ma_alloc_block( bNoBlock );
	if ( pBlock2 == NULL ) {
	    ma_free_block( pBlock1 );
	    return( -ENOMEM );
	}
	
	psArray->pArray[ (nIndex >> 16) & 0xff ] = pBlock1;
	g_nUsedMem += sizeof( void* ) *  256;
	MArray_IncCount( psArray );

	for ( i = 0 ; i < 256 ; ++i ) {
	  psArray->pArray[ (nIndex >> 16) & 0xff ][i] = g_pMagic;
	}
	psArray->pArray[ (nIndex >> 16) & 0xff ][(nIndex >> 8) & 0xff] = pBlock2;
	MArray_IncCount( psArray );
	g_nUsedMem += sizeof( void* ) *  256;

	for ( i = 0 ; i < 256 ; ++i ) {
	  psArray->pArray[ (nIndex >> 16) & 0xff ][ (nIndex >> 8) & 0xff ][i] = (void*) g_pMagic;
	}
      }
      else
      {
	void* pBlock;
	int   i;

	pBlock = ma_alloc_block( bNoBlock );
	if ( pBlock == NULL ) {
	    return( -ENOMEM );
	}
	
	psArray->pArray[ (nIndex >> 16) & 0xff ][(nIndex >> 8) & 0xff] = pBlock;
	MArray_IncCount( psArray );
	g_nUsedMem += sizeof( void* ) *  256;

	for ( i = 0 ; i < 256 ; ++i ) {
	  psArray->pArray[ (nIndex >> 16) & 0xff ][ (nIndex >> 8) & 0xff ][i] = (void*) g_pMagic;
	}
      }
      psArray->pArray[ (nIndex >> 16) & 0xff ][(nIndex >> 8) & 0xff][ nIndex & 0xff ] = pObject;
      return( nIndex );
    }
    else
    {
      if ( pObj[ nIndex & 0xff ] != (void*) g_pMagic ) {
	continue;
      }
      pObj[ nIndex & 0xff ] = pObject;
      return( nIndex );
    }
  }
}

/*****************************************************************************
 * NAME:
 * DESC:
 * NOTE:
 * SEE ALSO:
 ****************************************************************************/

void MArray_Remove( MultiArray_s* psArray, int nIndex )
{
  int	i;

  if ( psArray->pArray[ (nIndex >> 16) & 0xff ][ (nIndex >> 8) & 0xff ][ nIndex & 0xff ] == (void*) g_pMagic ) {
    printk( "Error : Array_Remove() Attempt to release empty entry %08x\n", nIndex );
    return;
  }

  psArray->pArray[ (nIndex >> 16) & 0xff ][ (nIndex >> 8) & 0xff ][ nIndex & 0xff ] = (void*) g_pMagic;

  for ( i = 0 ; i < 256 ; ++i ) {
    if ( psArray->pArray[ (nIndex >> 16) & 0xff ][ (nIndex >> 8) & 0xff ][i] != (void*)g_pMagic ) {
      return;
    }
  }

  ma_free_block( psArray->pArray[ (nIndex >> 16) & 0xff ][ (nIndex >> 8) & 0xff ] );
  MArray_DecCount( psArray );
  g_nUsedMem -= sizeof( void* ) *  256;

  psArray->pArray[ (nIndex >> 16) & 0xff ][ (nIndex >> 8) & 0xff ] = g_pMagic;


  for ( i = 0 ; i < 256 ; ++i ) {
    if ( psArray->pArray[ (nIndex >> 16) & 0xff ][i] != g_pMagic ) {
      return;
    }
  }

  ma_free_block( psArray->pArray[ (nIndex >> 16) & 0xff ] );
  MArray_DecCount( psArray );
  g_nUsedMem -= sizeof( void* ) *  256;


  psArray->pArray[ (nIndex >> 16) & 0xff ] = (void***)g_pMagic;
}

/*****************************************************************************
 * NAME:
 * DESC:
 * NOTE:
 * SEE ALSO:
 ****************************************************************************/

void* MArray_GetObj( const MultiArray_s* psArray, int nIndex )
{
  void*	pObj = psArray->pArray[ (nIndex >> 16) & 0xff ][ (nIndex >> 8) & 0xff ][ nIndex & 0xff ];

  if ( pObj == (void*) g_pMagic ) {
    return( NULL );
  }
  return( pObj );
}

/*****************************************************************************
 * NAME:
 * DESC:
 * NOTE:
 * SEE ALSO:
 ****************************************************************************/

int MArray_GetNextIndex( MultiArray_s* psArray, int nIndex )
{
    int	nLev1;
    int	nLev2;
    int	nLev3;
    int	i,j,k;

    if ( -1 == nIndex ) {
	nIndex = 0;
    }	else {
	nIndex++;
    }

    nLev1 = (nIndex >> 16) & 0xff;
    nLev2 = (nIndex >> 8) & 0xff;
    nLev3 = nIndex & 0xff;

    for ( i = nLev1 ; i < 256 ; ++i ) {
	if ( psArray->pArray[ i ] != (void***) g_pMagic ) {
	    for ( j = nLev2 ; j < 256 ; ++j ) {
		if ( psArray->pArray[ i ][ j ] != g_pMagic ) {
		    for ( k = nLev3 ; k < 256 ; ++k ) {
			if ( psArray->pArray[ i ][ j ][ k ] != g_pMagic ) {
			    return( (i << 16) | (j << 8) | k );
			}
		    }
		}
		nLev3 = 0;
	    }
	}
	nLev2 = 0;
	nLev3 = 0;
    }
    return( -1 );
}

/*****************************************************************************
 * NAME:
 * DESC:
 * NOTE:
 * SEE ALSO:
 ****************************************************************************/

int MArray_GetPrevIndex( MultiArray_s* psArray, int nIndex )
{
    int	nLev1;
    int	nLev2;
    int	nLev3;
    int	i,j,k;

    if ( -1 == nIndex ) {
	nIndex = 0xffffff;
    }	else {
	if ( 0 == nIndex ) {
	    return( -1 );
	} else {
	    nIndex--;
	}
    }

    nLev1 = (nIndex >> 16) & 0xff;
    nLev2 = (nIndex >> 8) & 0xff;
    nLev3 = nIndex & 0xff;

    for ( i = nLev1 ; i >= 0 ; --i ) {
	if ( psArray->pArray[ i ] != (void***) g_pMagic ) {
	    for ( j = nLev2 ; j >= 0 ; --j ) {
		if ( psArray->pArray[ i ][ j ] != g_pMagic ) {
		    for ( k = nLev3 ; k >= 0 ; --k ) {
			if ( psArray->pArray[ i ][ j ][ k ] != g_pMagic ) {
			    return( (i << 16) | (j << 8) | k );
			}
		    }
		}
		nLev3 = 255;
	    }
	}
	nLev2 = 255;
	nLev3 = 255;
    }
    return( -1 );
}
