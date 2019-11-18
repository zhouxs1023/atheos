/*
 *  The AtheOS kernel
 *  Copyright (C) 1999 - 2001  Kurt Skauen
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
#include <atheos/types.h>
#include <atheos/kernel.h>
#include <atheos/semaphore.h>
#include <atheos/spinlock.h>

#include <macros.h>

#include "inc/scheduler.h"
#include "inc/sysbase.h"
#include "inc/global.h"
#include "inc/areas.h"
#include "inc/bcache.h"
#include "inc/swap.h"

/*
 * Read only:
 *	Both the area, and the pte is marked read only
 * Cloned as copy-on-write:
 *	The pte is marked as read-only while the area is marked as read-write
 * Mem mapped/not present:
 *	The area has a non NULL a_psFile entry, and the pte has the value 0.
 * Mem mapped/present:
 *	The area has a non NULL a_psFile entry, and the pte is marked present.
 * Shared/not-present:
 *	The area has the AREA_SHARED flags set, and the pte is marked not present.
 * Swapped out:
 * 	The pte is marked not-present, but the address is non-null.
 * 
 */
MultiArray_s g_sAreas;
sem_id g_hAreaTableSema = -1;


static MemAreaOps_s sMemMapOps =
{
    NULL,	/* open */
    NULL,	/* close */
    NULL,	/* unmap	*/
    NULL,	/* protect */
    NULL,	/* sync */
    NULL,	/* advise */
    memmap_no_page,
    NULL,	/* wppage	*/
    NULL,	/* swapout	*/
    NULL,	/* swapin	*/
};

/*****************************************************************************
 * NAME:
 * DESC:
 * NOTE:
 * SEE ALSO:
 ****************************************************************************/

void list_areas( MemContext_s* psCtx )
{
    MemArea_s* psArea;
    int	     i;
  
//  LOCK( g_hAreaTableSema );
    for ( i = 0 ; i < psCtx->mc_nAreaCount ; ++i )
    {
	psArea = psCtx->mc_apsAreas[i];

	if ( NULL == psArea ) {
	    printk( "Error: print_areas() index %d of %d has a NULL pointer!\n", i, psCtx->mc_nAreaCount );
	    break;
	}
	if ( i > 0 && psArea->a_psPrev != psCtx->mc_apsAreas[i - 1] ) {
	    printk( "Error: print_areas() area at index %d (%p) has invalid prev pointer %p\n", i, psArea, psArea->a_psPrev );
	}
	if ( i < psCtx->mc_nAreaCount - 1 && psArea->a_psNext != psCtx->mc_apsAreas[i + 1] ) {
	    printk( "Error: print_areas() area at index %d (%p) has invalid next pointer %p\n", i, psArea, psArea->a_psNext );
	}
	printk( "Area %04d (%d) -> %08lx-%08lx %p %02d %s\n", i, psArea->a_nAreaID, psArea->a_nStart, psArea->a_nEnd,
		psArea->a_psFile, psArea->a_nRefCount, psArea->a_zName );
    }
//  UNLOCK( g_hAreaTableSema );
}

/*****************************************************************************
 * NAME:
 * DESC:
 * NOTE:
 * SEE ALSO:
 ****************************************************************************/

static int clear_pagedir( pgd_t* pPgd, int nAddress, int nSize )
{
    int	 nEnd;
    pte_t* pPte = pte_offset( pPgd, nAddress );

    nAddress &= ~PGDIR_MASK;
    nEnd = nAddress + nSize -1;

    if ( nEnd > PGDIR_SIZE ) {
	nEnd = PGDIR_SIZE;
    }

    if ( 0 == PGD_PAGE( *pPgd ) ) {
	printk( "Error: clear_pagedir() No page directory for address %08x\n", nAddress );
	return( -EINVAL );
    }

    do
    {
	if ( PTE_ISPRESENT( *pPte ) )
	{
	    uint32 nPage = PTE_PAGE( *pPte );
	    int	   nPageNr = PAGE_NR( nPage );
	
	    if ( 0 == nPage ) {
		printk( "ERROR : Page table referenced physical address 0\n" );
		PTE_VALUE( *pPte++ ) = ((uint32)g_sSysBase.ex_pNullPage) | PTE_PRESENT | PTE_USER;
		nAddress += PAGE_SIZE;
		continue;
	    }
	    if ( nPage == (uint32) g_sSysBase.ex_pNullPage ) {
		PTE_VALUE( *pPte++ ) = ((uint32)g_sSysBase.ex_pNullPage) | PTE_PRESENT | PTE_USER;
		nAddress += PAGE_SIZE;
		continue;
	    }
	    if ( nPageNr >= g_sSysBase.ex_nTotalPageCount ) {
		PTE_VALUE( *pPte++ ) = ((uint32)g_sSysBase.ex_pNullPage) | PTE_PRESENT | PTE_USER;
		nAddress += PAGE_SIZE;
		continue;
	    }
	    if ( g_psFirstPage[ nPageNr ].p_nCount == 1 ) {
		unregister_swap_page( nPage );
	    }
	    if ( g_psFirstPage[ nPageNr ].p_nCount < 1 ) {
		printk( "panic: clear_pagedir() page %08lx got ref count of %d\n",
			nPage, g_psFirstPage[ nPageNr ].p_nCount - 1 );
		g_psFirstPage[ nPageNr ].p_nCount = 1000;
	    }
	    free_pages( nPage, 1 );
	}
	else
	{
	    if ( PTE_PAGE( *pPte ) != 0 ) {
		free_swap_page( PTE_PAGE( *pPte ) );
	    }
	}
	PTE_VALUE( *pPte++ ) = ((uint32)g_sSysBase.ex_pNullPage) | PTE_PRESENT | PTE_USER;
	nAddress += PAGE_SIZE;
    } while( nAddress < nEnd );
    return( 0 );
}

/*****************************************************************************
 * NAME:
 * DESC:
 * NOTE:
 * SEE ALSO:
 ****************************************************************************/

static int free_area_pages( MemArea_s* psArea, uint32 nStart, uint32 nEnd )
{
    pgd_t* pPgd;
    pPgd = pgd_offset( psArea->a_psContext, nStart );

    while( nStart - 1 < nEnd ) {
	clear_pagedir( pPgd++, nStart, nEnd - nStart );
	nStart = (nStart + PGDIR_SIZE) & PGDIR_MASK;
    }
    return( 0 );
}

/** 
 * \par Description:
 * \par Note:
 * \par Warning:
 * \param
 * \return
 * \sa
 * \author	Kurt Skauen (kurt@atheos.cx)
 *****************************************************************************/

static void free_area_page_tables( MemArea_s* psArea, uint32 nStart, uint32 nEnd, bool bResized )
{
    MemContext_s* psCtx = psArea->a_psContext;
    uint32 nFirstTab = nStart >> PGDIR_SHIFT;
    uint32 nLastTab  = nEnd >> PGDIR_SHIFT;
    MemArea_s* psPrev = psArea->a_psPrev;
    MemArea_s* psNext = psArea->a_psNext;
    uint32     i;

    for ( i = nFirstTab ; i <= nLastTab ; ++i ) {
	uint32 nCurAddr = i << PGDIR_SHIFT;
	uint32 nCurEnd  = nCurAddr + PGDIR_SIZE - 1;
	uint32 nPage;

	  // Skip tables still used by the resized area
	if ( bResized && nCurAddr <= psArea->a_nEnd ) {
	    continue;
	}
	  // Skip tables shared by the previous area
	if ( psPrev != NULL && nCurAddr <= psPrev->a_nEnd ) {
	    continue;
	}
	  // Skip tables shared by the next area
	if ( psNext != NULL && nCurEnd >= psNext->a_nStart ) {
	    continue;
	}
	nPage = PGD_PAGE( psCtx->mc_pPageDir[i] );
	if ( nPage == 0 ) {
	    printk( "Error: free_area_page_tables() found NULL pointer in page table\n" );
	    continue;
	}
	free_pages( nPage, 1 );
	PGD_VALUE( psCtx->mc_pPageDir[i] ) = 0;
	if ( nCurEnd < AREA_FIRST_USER_ADDRESS ) {
	    MemContext_s* psTmp;
	    for ( psTmp = psCtx->mc_psNext ; psTmp != psCtx ; psTmp = psTmp->mc_psNext ) {
		psTmp->mc_pPageDir[i] = psCtx->mc_pPageDir[i];
	    }
	}
    }
}

/*****************************************************************************
 * NAME:
 * DESC:
 * NOTE:
 * SEE ALSO:
 ****************************************************************************/

static int do_delete_area( MemContext_s* psCtx, MemArea_s* psArea )
{
    if ( psArea->a_nRefCount != 0 ) {
	panic( "do_delete_area() area ref count = %d\n", psArea->a_nRefCount );
	return( -EINVAL );
    }
    if ( psArea->a_psFile != NULL ) {
	if ( psArea->a_psNextShared == psArea ) {
	    psArea->a_psFile->f_psInode->i_psAreaList = NULL;
	} else if ( psArea->a_psFile->f_psInode->i_psAreaList == psArea ) {
	    psArea->a_psFile->f_psInode->i_psAreaList = psArea->a_psNextShared;
	}
    }

      // Unlink from shared list.
    psArea->a_psPrevShared->a_psNextShared = psArea->a_psNextShared;
    psArea->a_psNextShared->a_psPrevShared = psArea->a_psPrevShared;

    psArea->a_bDeleted = true; // Make sure nobody maps in pages while we waits for IO

    while ( psArea->a_nIOPages > 0 ) {
	UNLOCK( g_hAreaTableSema );
	printk( "do_delete_area() wait for IO to finnish\n" );
	snooze( 20000 );
	LOCK( g_hAreaTableSema );
    }

    if ( psArea->a_psFile != NULL ) {
	UNLOCK( g_hAreaTableSema );
	put_fd( psArea->a_psFile );
	LOCK( g_hAreaTableSema );
    }
  
    free_area_pages( psArea, psArea->a_nStart, psArea->a_nEnd );
    free_area_page_tables( psArea, psArea->a_nStart, psArea->a_nEnd, false );
    flush_tlb_global();

//  delete_semaphore( psArea->a_hMutex1 );
  
    remove_area( psCtx, psArea );
    kfree( psArea );
    return( 0 );
}

/*****************************************************************************
 * NAME:
 * DESC:
 * NOTE:
 * SEE ALSO:
 ****************************************************************************/

int put_area( MemArea_s* psArea )
{
    int        nCount;
    int	       nError = 0;
  
//  kassertw( is_semaphore_locked( g_hAreaTableSema ) == false );
    LOCK( g_hAreaTableSema );

    if ( psArea != NULL ) {
	nCount = atomic_add( &psArea->a_nRefCount, -1 );
	if ( nCount == 1 ) {
	    lock_area( psArea, LOCK_AREA_WRITE );
	    nError = do_delete_area( psArea->a_psContext, psArea );
	}
    }
    UNLOCK( g_hAreaTableSema );
    return( nError );
//    return( delete_area( psArea ) );
}

/*****************************************************************************
 * NAME:
 * DESC:
 * NOTE:
 * SEE ALSO:
 ****************************************************************************/

static int alloc_area_page_tables( MemArea_s* psArea, uint32 nStart, uint32 nEnd )
{
    MemContext_s*	psCtx	  = psArea->a_psContext;
    uint32	nFirstPTE = nStart >> PAGE_SHIFT;
    uint32	nLastPTE  = nEnd   >> PAGE_SHIFT;
    int		i;

    for ( i = nFirstPTE ; i <= nLastPTE ; ++i )
    {
	int	nDir = i >> 10;
	uint32* pDir = (uint32*) PGD_PAGE( psCtx->mc_pPageDir[ nDir ] );

	if ( NULL == pDir ) {
	    pDir =  (uint32*) get_free_page( GFP_CLEAR );
	    if ( pDir == NULL ) {
		return( -ENOMEM );
	    }
	    PGD_VALUE( psCtx->mc_pPageDir[ nDir ] ) = MK_PGDIR( (uint32) pDir );
	    if ( nStart < AREA_FIRST_USER_ADDRESS ) {
		MemContext_s* psTmp;
		for ( psTmp = psCtx->mc_psNext ; psTmp != psCtx ; psTmp = psTmp->mc_psNext ) {
		    psTmp->mc_pPageDir[ nDir ] = psCtx->mc_pPageDir[ nDir ];
		}
	    }
	}
	if ( NULL != pDir ) {
	    pDir[ i % 1024 ] = ((uint32)g_sSysBase.ex_pNullPage) | PTE_PRESENT | PTE_USER;
	}
    }
    flush_tlb_global();
    return( 0 );
}


static int alloc_area_pages( MemArea_s* psArea, uint32 nStart, uint32 nEnd, bool bWriteAccess )
{
    MemContext_s* psSeg = psArea->a_psContext;
    int nFlags = PTE_PRESENT;
    int nError = 0;
    uint32 nAddr;
    
    if ( psArea->a_nProtection & AREA_WRITE ) {
	nFlags |= PTE_WRITE;
    }
    if ( (psArea->a_nProtection & AREA_KERNEL) == 0 ) {
	nFlags |= PTE_USER;
    }
    for ( nAddr = nStart ; nAddr < nEnd ; nAddr += PAGE_SIZE )
    {
	pgd_t* pPgd = pgd_offset( psSeg, nAddr );
	pte_t* pPte = pte_offset( pPgd, nAddr );

	if ( PTE_ISPRESENT( *pPte ) ) {
	    if  ( bWriteAccess == false || PTE_ISWRITE( *pPte ) ) {
		continue;	// It is present and writable so nothing to do here.
	    }
	}
	nError = load_area_page( psArea, nAddr, bWriteAccess );
//	    nError = handle_copy_on_write( psArea, pPte, nAddr );
	if ( nError <  0 ) {
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

static MemArea_s* do_create_area( MemContext_s* psCtx, uint32 nAddress, uint32 nSize, uint32 nMaxSize,
				  bool bAllocTables, uint32 nProtection, uint32 nLockMode )
{
    MemArea_s* psArea;
    int	     nError;
  
    psArea = kmalloc( sizeof( MemArea_s ), MEMF_KERNEL | MEMF_NOBLOCK | MEMF_CLEAR );

    if ( NULL == psArea ) {
	printk( "Error: do_create_area() out of memory\n" );
	goto error1;
    }
      // FIXME: create_semaphore() allocates memory, and can dedlock the system.
    
    psArea->a_nStart  = nAddress;
    psArea->a_nEnd    = nAddress + nSize - 1;
    psArea->a_nMaxEnd = nAddress + nMaxSize - 1;

    psArea->a_psNext	 = NULL;
    psArea->a_psPrev	 = NULL;
    psArea->a_psNextShared = psArea;
    psArea->a_psPrevShared = psArea;
    psArea->a_psContext	   = psCtx;
    psArea->a_psOps	   = NULL;
    psArea->a_nProtection  = nProtection;
    psArea->a_nLockMode	   = nLockMode;
    psArea->a_nRefCount    = 1;
  
    nError = insert_area( psCtx, psArea );
 
    if ( nError < 0 ) {
	goto error2;
    }
    if ( bAllocTables ) {
	nError = alloc_area_page_tables( psArea, psArea->a_nStart, psArea->a_nEnd );
	if ( nError < 0 ) {
	    goto error3;
	}
	if ( nLockMode == AREA_FULL_LOCK ) {
	    nError = alloc_area_pages( psArea, psArea->a_nStart, psArea->a_nEnd, true );
	    if ( nError < 0 ) {
		goto error4;
	    }
	}
    }
    return( psArea );
error4:
    free_area_pages( psArea, psArea->a_nStart, psArea->a_nEnd );
    free_area_page_tables( psArea, psArea->a_nStart, psArea->a_nEnd, false );
error3:
    remove_area( psCtx, psArea );
error2:
    kfree( psArea );
error1:
    return( NULL );
}  

/*****************************************************************************
 * NAME:
 * DESC:
 * NOTE:
 * SEE ALSO:
 ****************************************************************************/

static int alloc_area( const char* pzName, MemContext_s* psCtx, uint32 nProtection, uint32 nLockMode,
		       uint32 nAddress, uint32 nSize, uint32 nMaxSize, area_id* pnArea )
{
    MemArea_s* psArea;
    uint32  nNewAddr;
    int	  nError;

    kassertw( is_semaphore_locked( g_hAreaTableSema ) );
  
    nNewAddr = find_unmapped_area( psCtx, nProtection, nMaxSize, nAddress );

    if ( nNewAddr == -1 && (nProtection & AREA_ADDR_SPEC_MASK) == AREA_BASE_ADDRESS ) {
	nNewAddr = find_unmapped_area( psCtx, (nProtection & ~AREA_ADDR_SPEC_MASK) | AREA_ANY_ADDRESS, nMaxSize, nAddress );
    }
  
    if ( nNewAddr == -1 ) {
	printk( "Error: alloc_area() out of virtual space\n" );
	nError = -ENOADDRSPC;
	goto error;
    }

    if ( (nProtection & AREA_ADDR_SPEC_MASK) == AREA_EXACT_ADDRESS ) {
	if ( nNewAddr != nAddress ) {
	    printk( "PANIC: find_unmapped_area() returned %08lx during attempt to alloc at excact %08lx\n", nNewAddr, nAddress );
	    nNewAddr = nAddress;
	}
    }
    if ( nNewAddr < AREA_FIRST_USER_ADDRESS && (nProtection & AREA_KERNEL) == 0 ) {
	printk( "alloc_area() set AREA_KERNEL flag\n" );
	nProtection |= AREA_KERNEL;
    }
  
    psArea = do_create_area( psCtx, nNewAddr, nSize, nMaxSize, true, nProtection, nLockMode );

    if ( NULL == psArea ) {
	nError = -ENOMEM;
	goto error;
    }
    strcpy( psArea->a_zName, pzName );
    *pnArea = psArea->a_nAreaID;
  
    return( 0 );
error:
    return( nError );
}

/*****************************************************************************
 * NAME:
 * DESC:
 * NOTE:
 * SEE ALSO:
 ****************************************************************************/

int alloc_area_list( uint32 nProtection, uint32 nLockMode, uint32 nAddress, uint32 nCount,
		     const char* const * apzNames, uint32* panOffsets, uint32* panSizes, area_id* panAreas )
{
    MemContext_s* psCtx;
    uint32 nTotSize = panOffsets[ nCount - 1 ] + panSizes[ nCount - 1 ];
    uint32 nNewAddr;
    int	 nError;
    int	 i;

    if ( nProtection & AREA_KERNEL) {
	psCtx = g_psKernelSeg;
    } else {
	psCtx = CURRENT_PROC->tc_psMemSeg;
    }
  
      // Verify parameters.
    for ( i = 0 ; i < nCount - 1 ; ++i ) {
	if ( panOffsets[i] + panSizes[i] > panOffsets[i + 1] ) {
	    printk( "Error: alloc_area_list() area %d (%08lx, %08lx) overlaps area %d (%08lx,%08lx)\n",
		    i, panOffsets[i], panSizes[i], i + 1, panOffsets[i+1], panSizes[i+1] );
	    return( -EINVAL );
	}
	if ( panOffsets[i] > panOffsets[i+1] ) {
	    printk( "Error: alloc_area_list() area %d (%08lx, %08lx) addressed after area %d (%08lx,%08lx)\n",
		    i, panOffsets[i], panSizes[i], i + 1, panOffsets[i+1], panSizes[i+1] );
	    return( -EINVAL );
	}
    }
again:  
    kassertw( is_semaphore_locked( g_hAreaTableSema ) == false );
    LOCK( g_hAreaTableSema );
    if ( psCtx->mc_bBusy ) {
	UNLOCK( g_hAreaTableSema );
	printk( "alloc_area_list() wait for segment to become ready\n" );
	snooze( 10000 );
	goto again;
    }
    nNewAddr = find_unmapped_area( psCtx, nProtection, nTotSize, nAddress );

    if ( nNewAddr == -1 && (nProtection & AREA_ADDR_SPEC_MASK) == AREA_BASE_ADDRESS ) {
	nNewAddr = find_unmapped_area( psCtx, (nProtection & ~AREA_ADDR_SPEC_MASK) | AREA_ANY_ADDRESS, nTotSize, nAddress );
	printk( "alloc_area_list() failed to alloc %ld areas at %p, got %p\n", nCount, (void*)nAddress, (void*)nNewAddr );
    }
    if ( nNewAddr == -1 ) {
	printk( "Error: alloc_area_list() out of address space\n" );
	nError = -ENOADDRSPC;
	goto error;
    }

    for ( i = 0 ; i < nCount ; ++i ) {
	MemArea_s* psArea = do_create_area( psCtx, nNewAddr + panOffsets[i], panSizes[i], panSizes[i], true, nProtection, nLockMode );
	
	if ( psArea == NULL ) {
	    int j;
      
	    for ( j = 0 ; j < i ; ++j ) {
		psArea = get_area_from_handle( panAreas[j] );
		if ( psArea == NULL ) {
		    panic( "alloc_area_list() could not find newly created are!\n" );
		}
		kassertw( psArea->a_psFile == NULL );
		do_delete_area( psCtx, psArea );
	    }
	    nError = -ENOMEM;
	    goto error;
	}
	if ( apzNames != NULL && apzNames[i] != NULL ) {
	    strcpy( psArea->a_zName, apzNames[i] );
	}
	panAreas[i] = psArea->a_nAreaID;
    }
    UNLOCK( g_hAreaTableSema );

    return( 0 );
error:
    UNLOCK( g_hAreaTableSema );
    if ( nError == -ENOMEM ) {
	shrink_caches( 65536 );
	goto again;
    }
    return( nError );
}

/*****************************************************************************
 * NAME:
 * DESC:
 * NOTE:
 * SEE ALSO:
 ****************************************************************************/

status_t resize_area( area_id hArea, uint32 nNewSize, bool bAtomic )
{
    MemArea_s* psArea;
    status_t nError = 0;
  
again:
    kassertw( is_semaphore_locked( g_hAreaTableSema ) == false );
    LOCK( g_hAreaTableSema );

    psArea = get_area_from_handle( hArea );

    if ( psArea == NULL ) {
	printk( "Error: resize_area() invalid handle %d\n", hArea );
	nError = -EINVAL;
	goto error1;
    }
    
      // Check that the area won't wrap around the 4G limit
    if ( psArea->a_nStart + nNewSize < psArea->a_nStart ) {
	nError = -ENOADDRSPC;
	printk( "resize_area() area to big %p -> %08lx\n", (void*)psArea->a_nStart, nNewSize );
	goto error2;
    }
      // Check that a kernel area won't extend into user-space.
    if ( psArea->a_nStart < AREA_FIRST_USER_ADDRESS ) {
	if ( psArea->a_nStart + nNewSize > AREA_FIRST_USER_ADDRESS ) {
	    nError = -ENOADDRSPC;
	    printk( "resize_area() area extends out of the kernel %08lx\n", nNewSize );
	    goto error2;
	}
    }
  
    if ( psArea->a_psNext != NULL ) {
	  // If it's not the last area, we must verify that it won't collide with the next.
	if ( psArea->a_nStart + nNewSize > psArea->a_psNext->a_nStart ) {
	    nError = -ENOADDRSPC;
//	    printk( "resize_area() no space after area %p -> %08lx (%p)\n", (void*)psArea->a_nStart, nNewSize, (void*)psArea->a_psNext->a_nStart );
	    goto error2;
	}
    }
    if ( nNewSize == psArea->a_nEnd - psArea->a_nStart + 1 ) {
	goto error2;
    }
    if ( psArea->a_nEnd - psArea->a_nStart + 1 < nNewSize ) {
	nError = alloc_area_page_tables( psArea, psArea->a_nEnd + 1, psArea->a_nStart + nNewSize - 1 );
	if ( nError < 0 ) {
	    free_area_page_tables( psArea, psArea->a_nEnd + 1, psArea->a_nStart + nNewSize - 1, true );
	    goto error2;
	}
	if ( psArea->a_nLockMode == AREA_FULL_LOCK ) {
	    nError = alloc_area_pages( psArea, psArea->a_nEnd + 1, psArea->a_nStart + nNewSize - 1, true );
	    if ( nError < 0 ) {
		free_area_pages( psArea, psArea->a_nEnd + 1, psArea->a_nStart + nNewSize - 1 );
		free_area_page_tables( psArea, psArea->a_nEnd + 1, psArea->a_nStart + nNewSize - 1, true );
		goto error2;
	    }
	}
	psArea->a_nEnd = psArea->a_nStart + nNewSize - 1;
	if ( psArea->a_nEnd > psArea->a_nMaxEnd ) {
	    psArea->a_nMaxEnd = psArea->a_nEnd;
	}
    } else {
	uint32 nOldEnd = psArea->a_nEnd;
	
	if ( psArea->a_nIOPages > 0 ) {
	    UNLOCK( g_hAreaTableSema );
	    printk( "resize_area() wait for IO to finnish\n" );
	    snooze( 20000 );
	    goto again;;
	}
	psArea->a_nEnd = psArea->a_nStart + nNewSize - 1;
	free_area_pages( psArea, psArea->a_nStart + nNewSize, nOldEnd ); 
	free_area_page_tables( psArea, psArea->a_nStart + nNewSize, nOldEnd, true );
    }
    flush_tlb_global();
  
error2:
    put_area( psArea );
error1:
    UNLOCK( g_hAreaTableSema );
    if ( nError == -ENOMEM && bAtomic ) {
	shrink_caches( 65536 );
	goto again;
    }
    return( nError );
}

/*****************************************************************************
 * NAME:
 * DESC:
 * NOTE:
 * SEE ALSO:
 ****************************************************************************/

status_t delete_area( area_id hArea )
{
    MemArea_s* psArea;
    int        nCount;
    int	       nError = 0;
  
//  kassertw( is_semaphore_locked( g_hAreaTableSema ) == false );
again:
    LOCK( g_hAreaTableSema );

    psArea = get_area_from_handle( hArea );

    if ( psArea != NULL ) {
	if ( psArea->a_psContext->mc_bBusy ) {
	    atomic_add( &psArea->a_nRefCount, -1 );
	    UNLOCK( g_hAreaTableSema );
	    printk( "delete_area(): wait for segment to become ready\n" );
	    snooze( 10000 );
	    goto again;
	}
	
	nCount = atomic_add( &psArea->a_nRefCount, -2 );
  
	if ( nCount == 2 ) {
//	    lock_semaphore_ex( psArea->a_hMutex, AREA_MUTEX_COUNT, SEM_NOSIG, INFINITE_TIMEOUT );
	    lock_area( psArea, LOCK_AREA_WRITE );
	    nError = do_delete_area( psArea->a_psContext, psArea );
	}
    }
    UNLOCK( g_hAreaTableSema );
    return( nError );
}

/*****************************************************************************
 * NAME:
 * DESC:
 * NOTE:
 * SEE ALSO:
 ****************************************************************************/

static void delete_all_areas( MemContext_s* psCtx )
{
    int i;

    kassertw( is_semaphore_locked( g_hAreaTableSema ) );

    while( psCtx->mc_nAreaCount > 0 )
    {
	int nCount;

	lock_area( psCtx->mc_apsAreas[0], LOCK_AREA_WRITE );

	nCount = atomic_add( &psCtx->mc_apsAreas[0]->a_nRefCount, -1 );

	if ( 1 != nCount ) {
	    printk( "Error : delete_all_areas() area '%s' (%d) for addr %08lx has ref count of %d\n",
		    psCtx->mc_apsAreas[0]->a_zName, psCtx->mc_apsAreas[0]->a_nAreaID, psCtx->mc_apsAreas[0]->a_nStart, nCount );

	    psCtx->mc_apsAreas[0]->a_nRefCount = 0;
	}
	do_delete_area( psCtx, psCtx->mc_apsAreas[0] );
    }

    for ( i = 512 ; i < 1024 ; ++i )
    {
	uint32 nPage = PGD_PAGE( psCtx->mc_pPageDir[i] );

	if ( 0 != nPage ) {
	    printk( "delete_all_areas() page-table at %d forgotten\n", i );
	    free_pages( nPage, 1 );
	}
	PGD_VALUE( psCtx->mc_pPageDir[i] ) = 0;
    }
    flush_tlb_global();
}

/** 
 * \par Description:
 *	Deletes all areas in the given context
 * \par Note:
 * \par Warning:
 * \param
 * \return
 * \sa
 * \author	Kurt Skauen (kurt@atheos.cx)
 *****************************************************************************/

void empty_mem_context( MemContext_s* psCtx )
{
    LOCK( g_hAreaTableSema );
    delete_all_areas( psCtx );
    UNLOCK( g_hAreaTableSema );
}

/*****************************************************************************
 * NAME:
 * DESC:
 * NOTE:
 * SEE ALSO:
 ****************************************************************************/

void delete_mem_context( MemContext_s* psCtx )
{
    MemContext_s*	psTmp;
  
    kassertw( is_semaphore_locked( g_hAreaTableSema ) == false );
    LOCK( g_hAreaTableSema );

    for ( psTmp = psCtx->mc_psNext ; NULL != psTmp ; psTmp = psTmp->mc_psNext )
    {
	if ( psTmp->mc_psNext == psCtx ) {
	    psTmp->mc_psNext = psCtx->mc_psNext;
	    break;
	}
    }

    delete_all_areas( psCtx );
    UNLOCK( g_hAreaTableSema );

    free_pages( (uint32) psCtx->mc_pPageDir, 1 );
    if ( psCtx->mc_apsAreas != NULL ) {
	kfree( psCtx->mc_apsAreas );
    }
    kfree( psCtx );
}


/*****************************************************************************
 * NAME:
 * DESC:
 * NOTE:
 * SEE ALSO:
 ****************************************************************************/

int clone_page_pte( pte_t* pDst, pte_t* pSrc, bool bCow )
{
    pte_t nPte;
    int	nPageNr;
    int	nPageAddress = PTE_PAGE( *pSrc );

    if ( bCow ) {
	PTE_VALUE( *pSrc ) &= ~PTE_WRITE;
    }
  
    nPageNr = PAGE_NR( nPageAddress );

    nPte = *pSrc;
  
    if ( PTE_ISPRESENT( nPte ) ) {
	atomic_add( &g_psFirstPage[ nPageNr ].p_nCount, 1 );
    } else if ( PTE_PAGE( nPte ) != 0 ) {
	dup_swap_page( PTE_PAGE( nPte ) );
    }
    *pDst = nPte;
    return( 0 );
}

/*****************************************************************************
 * NAME:
 * DESC:
 * NOTE:
 * SEE ALSO:
 ****************************************************************************/

static void do_clone_area( MemContext_s* psDstSeg, MemContext_s* psSrcSeg, MemArea_s* psArea )
{
    uint32 nSrcAddr;
    bool	 bCow;

    bCow = (psArea->a_nProtection & (AREA_SHARED | AREA_WRITE)) == AREA_WRITE;


    for ( nSrcAddr = psArea->a_nStart ; (nSrcAddr-1) != psArea->a_nEnd ; nSrcAddr += PAGE_SIZE )
    {
	pgd_t* pSrcPgd = pgd_offset( psSrcSeg, nSrcAddr );
	pte_t* pSrcPte = pte_offset( pSrcPgd,  nSrcAddr );
	pgd_t* pDstPgd = pgd_offset( psDstSeg, nSrcAddr );
	pte_t* pDstPte = pte_offset( pDstPgd,  nSrcAddr );


	kassertw( PGD_PAGE( *pDstPgd ) != 0 );
    
	if ( bCow ) {
	    PTE_VALUE( *pSrcPte ) &= ~PTE_WRITE;
	}
    
	*pDstPte = *pSrcPte;

	if ( PTE_ISPRESENT( *pSrcPte ) ) {
	    atomic_add( &g_psFirstPage[ PAGE_NR( PTE_PAGE( *pSrcPte ) ) ].p_nCount, 1 );
	} else if ( PTE_PAGE( *pSrcPte ) != 0 ) {
	    dup_swap_page( PTE_PAGE( *pSrcPte ) );
	}
    }
}

/*****************************************************************************
 * NAME:
 * DESC:
 * NOTE:
 * SEE ALSO:
 ****************************************************************************/

MemContext_s* clone_mem_context( MemContext_s* psOrig )
{
    MemContext_s* psNewSeg;
    int		i;
    int		nError;
again:
    kassertw( is_semaphore_locked( g_hAreaTableSema ) == false );
    LOCK( g_hAreaTableSema );
    if ( psOrig->mc_bBusy ) {
	UNLOCK( g_hAreaTableSema );
	printk( "clone_mem_context(): wait for segment to become ready\n" );
	snooze( 10000 );
	goto again;
    }
    psOrig->mc_bBusy = true; // Make sure nobody creates or deletes any areas while we are cloning
    
    psNewSeg = kmalloc( sizeof( MemContext_s ), MEMF_KERNEL | MEMF_NOBLOCK | MEMF_CLEAR );

    if ( NULL == psNewSeg ) {
	printk( "Error: clone_mem_context() out of memory\n" );
	nError = -ENOMEM;
	goto error1;
    }

    psNewSeg->mc_pPageDir = (pgd_t*) get_free_page( GFP_CLEAR );

    if ( psNewSeg->mc_pPageDir == NULL ) {
	printk( "Error: clone_mem_context() out of memory\n" );
	nError = -ENOMEM;
	goto error2;
    }

    memcpy( psNewSeg->mc_pPageDir, psOrig->mc_pPageDir, 2048 ); /* Copy shared page table pointers */
  
    for ( i = 0 ; i < (volatile int)psOrig->mc_nAreaCount ; ++i )
    {
	MemArea_s* psOldArea = psOrig->mc_apsAreas[i];
	MemArea_s* psNewArea;

	if ( psOldArea->a_nProtection & AREA_KERNEL ) {
	    continue;
	}

	  // Wait til all IO performed on pages in the area is finnished
	while( psOldArea->a_nIOPages > 0 ) {
	    Thread_s*	psThread = CURRENT_THREAD;
	    WaitQueue_s sWaitNode;
	    int nFlg;
	    printk( "clone_mem_context() Wait for area %s to finnish IO on %d pages\n", psOldArea->a_zName, psOldArea->a_nIOPages );
	    sWaitNode.wq_hThread = psThread->tr_hThreadID;

	    nFlg = cli(); // Make sure we are not pre-empted until we are added to the waitlist
	    add_to_waitlist( &psOldArea->a_psIOThreads, &sWaitNode );
	    psThread->tr_nState = TS_WAIT;
	    UNLOCK( g_hAreaTableSema );
	    put_cpu_flags( nFlg );
	    Schedule();
	    LOCK( g_hAreaTableSema );
	    remove_from_waitlist( &psOldArea->a_psIOThreads, &sWaitNode );
	}
	
	lock_area( psOldArea, LOCK_AREA_READ );
    
	psNewArea = do_create_area( psNewSeg, psOldArea->a_nStart,
				    psOldArea->a_nEnd - psOldArea->a_nStart + 1,
				    psOldArea->a_nMaxEnd - psOldArea->a_nStart + 1,
				    true, psOldArea->a_nProtection, psOldArea->a_nLockMode );
	if ( psNewArea == NULL ) {
	    printk( "clone_mem_context() failed to alloc area\n" );
	    unlock_area( psOldArea, LOCK_AREA_READ );
	    nError = -ENOMEM;
	    goto error3;
	}
	memcpy( psNewArea->a_zName, psOldArea->a_zName, sizeof( psNewArea->a_zName ) );
	psNewArea->a_psFile	 = psOldArea->a_psFile;
	psNewArea->a_nFileOffset = psOldArea->a_nFileOffset;
	psNewArea->a_nFileLength = psOldArea->a_nFileLength;
	psNewArea->a_psOps	 = psOldArea->a_psOps;
    
	do_clone_area( psNewSeg, psOrig, psNewArea );

	  // If the source area is mapped to a file the new better be to.
	if ( psOldArea->a_psFile != NULL )
	{
	    MemArea_s* psTmp;

	    psNewArea->a_psNextShared = psOldArea->a_psNextShared;
	    psNewArea->a_psNextShared->a_psPrevShared = psNewArea;
	    psOldArea->a_psNextShared = psNewArea;
	    psNewArea->a_psPrevShared = psOldArea;

	    for ( psTmp = psNewArea->a_psNextShared ; psTmp != psNewArea ; psTmp = psTmp->a_psNextShared ) {
		kassertw( psTmp->a_psFile != NULL );
		kassertw( psTmp->a_psFile->f_psInode->i_nInode == psNewArea->a_psFile->f_psInode->i_nInode );
	    }
	    atomic_add( &psNewArea->a_psFile->f_nRefCount, 1 );
	}
//	unlock_semaphore_ex( psOldArea->a_hMutex, 1 );
	unlock_area( psOldArea, LOCK_AREA_READ );
    }
    psNewSeg->mc_psNext = psOrig->mc_psNext;
    psOrig->mc_psNext = psNewSeg;
    flush_tlb_global();
    psOrig->mc_bBusy = false;
    UNLOCK( g_hAreaTableSema );

    return( psNewSeg );
error3:
    delete_all_areas( psNewSeg );
    free_pages( (uint32) psNewSeg->mc_pPageDir, 1 );
error2:
    kfree( psNewSeg );
error1:
    psOrig->mc_bBusy = false;
    UNLOCK( g_hAreaTableSema );
    if ( nError == -ENOMEM ) {
	shrink_caches( 65536 );
	goto again;
    }
    return( NULL );
}

/*****************************************************************************
 * NAME:
 * DESC:
 * NOTE:
 * SEE ALSO:
 ****************************************************************************/

static int unmap_pagedir( pgd_t* pPgd, uint32 nAddress, int nSize, bool bFreeOldPages )
{
    int	 nEnd;

    pte_t* pPte = pte_offset( pPgd, nAddress );

    nAddress &= ~PGDIR_MASK;
    nEnd = nAddress + nSize - 1;

    if ( nEnd > PGDIR_SIZE ) {
	nEnd = PGDIR_SIZE;
    }
    do
    {
	if ( bFreeOldPages )
	{
	    if ( PTE_ISPRESENT( *pPte ) )
	    {
		uint32 nOldPage = PTE_PAGE( *pPte );
		int    nPageNr  = PAGE_NR( nOldPage );
	
		if ( 0 != nOldPage && nOldPage != (uint32) g_sSysBase.ex_pNullPage ) {
		    if ( g_psFirstPage[ nPageNr ].p_nCount == 1 ) {
			unregister_swap_page( nOldPage );
		    }
		    free_pages( nOldPage, 1 );
		}
	    }
	}
	PTE_VALUE( *pPte ) = 0; // PTE_USER;
	pPte++;

	nAddress += PAGE_SIZE;
    } while( nAddress < nEnd );

    return( 0 );
}

/*****************************************************************************
 * NAME:
 * DESC:
 * NOTE:
 * SEE ALSO:
 ****************************************************************************/

int map_area_to_file( area_id hArea, File_s* psFile, uint32 nProt, off_t nOffset, size_t nLength )
{
    MemContext_s* psCtx;
    MemArea_s*    psArea;
    Inode_s*	  psInode  = psFile->f_psInode;
    uint32 	  nAddress;
    uint32 	  nEnd;
    pgd_t* 	  pPgd;
    bool 	  bFreeOldPages;
    int	 	  nError   = 0;
    MemArea_s*    psTmp;

    kassertw( psFile != NULL );
    kassertw( is_semaphore_locked( g_hAreaTableSema ) == false );
    LOCK( g_hAreaTableSema );

    psArea = get_area_from_handle( hArea );
    if ( psArea == NULL ) {
	printk( "Error: map_area_to_file() called with invalid area %d\n", hArea );
	UNLOCK( g_hAreaTableSema );
	return( -EINVAL );
    }
    psCtx = psArea->a_psContext;

    nAddress = psArea->a_nStart;
    nEnd     = psArea->a_nEnd;
    
    if ( NULL == psCtx ) {
	printk( "PANIC: map_area_to_file() area has no memcontext!\n" );
	UNLOCK( g_hAreaTableSema );
	return( -EINVAL );
    }
    
//    lock_semaphore_ex( psArea->a_hMutex, AREA_MUTEX_COUNT, SEM_NOSIG, INFINITE_TIMEOUT );
    lock_area( psArea, LOCK_AREA_WRITE );
  
    bFreeOldPages = (psArea->a_nProtection & AREA_REMAPPED) == 0;
    pPgd = pgd_offset( psArea->a_psContext, nAddress );

    psArea->a_nProtection &= ~AREA_REMAPPED;
    psArea->a_psFile      = psFile;
    psArea->a_nFileOffset = nOffset;
    psArea->a_nFileLength = nLength;
    psArea->a_psOps       = &sMemMapOps;


    while( nAddress - 1 < nEnd )
    {
	int nSkipSize;

	unmap_pagedir( pPgd++, nAddress, nEnd - nAddress, bFreeOldPages );
	
	nSkipSize = ((nAddress + PGDIR_SIZE) & PGDIR_MASK) - nAddress;
	nAddress += nSkipSize;
    }
  
    if ( psInode->i_psAreaList == NULL )
    {
	psInode->i_psAreaList = psArea;
	psArea->a_psNextShared = psArea;
	psArea->a_psPrevShared = psArea;
    }
    else
    {
	MemArea_s* psSrcArea = psInode->i_psAreaList;

	psArea->a_psNextShared = psSrcArea->a_psNextShared;
	psArea->a_psNextShared->a_psPrevShared = psArea;
	psSrcArea->a_psNextShared = psArea;
	psArea->a_psPrevShared = psSrcArea;
    }

    for ( psTmp = psArea->a_psNextShared ; psTmp != psArea ; psTmp = psTmp->a_psNextShared ) {
	kassertw( psTmp->a_psFile != NULL );
	kassertw( psTmp->a_psFile->f_psInode->i_nInode == psArea->a_psFile->f_psInode->i_nInode );
    }
//    unlock_semaphore_ex( psArea->a_hMutex, AREA_MUTEX_COUNT );
    unlock_area( psArea, LOCK_AREA_WRITE );
    put_area( psArea );
    UNLOCK( g_hAreaTableSema );
  
    atomic_add( &psFile->f_nRefCount, 1 );
    flush_tlb_global();
    return( nError );
}


/*****************************************************************************
 * NAME:
 * DESC:
 * NOTE:
 * SEE ALSO:
 ****************************************************************************/

area_id	sys_clone_area( const char* pzName, void** ppAddress, uint32 nProtection, int nLockMode, area_id hSrcArea )
{
    Process_s*	psProc = CURRENT_PROC;
    MemArea_s*	psSrcArea;
    MemArea_s*	psNewArea;
    area_id	nNewArea;
    uint32	nAddress;
    uint32	nSrcAddr;
    uint32	nDstAddr;
    int		nError;
    MemContext_s* psDstSeg;
    MemContext_s* psSrcSeg;
    char 	  zName[64];

    if ( pzName != NULL ) {
	if ( strncpy_from_user( zName, pzName, 64 ) < 0 ) {
	    return( -EFAULT );
	}
	zName[63] = '\0';
    } else {
	zName[0] = '\0';
    }
    if ( NULL == ppAddress ) {
	nAddress = AREA_FIRST_USER_ADDRESS + 512 * 1024 * 1024;
	nProtection = (nProtection & ~AREA_ADDR_SPEC_MASK) | AREA_BASE_ADDRESS;
    } else {
	nAddress = (uint32) *ppAddress;
	if ( nAddress < AREA_FIRST_USER_ADDRESS ) {
	    nAddress = AREA_FIRST_USER_ADDRESS + 512 * 1024 * 1024;
	    nProtection = (nProtection & ~AREA_ADDR_SPEC_MASK) | AREA_BASE_ADDRESS;
	} else if ( (nAddress & ~PAGE_MASK) != 0 ) {
	    printk( "Error: sys_clone_area() unaligned address %08lx\n", nAddress );
	    return( -EINVAL );
	}
    }
again:
    kassertw( is_semaphore_locked( g_hAreaTableSema ) == false );
    LOCK( g_hAreaTableSema );

    if ( psProc->tc_psMemSeg->mc_bBusy ) {
	UNLOCK( g_hAreaTableSema );
	printk( "sys_clone_area(): wait for segment to be ready\n" );
	snooze( 10000 );
	goto again;
    }
    
    psSrcArea = get_area_from_handle( hSrcArea );

    if ( NULL == psSrcArea ) {
	nError = -EINVAL;
	goto error;
    }
    lock_area( psSrcArea, LOCK_AREA_READ );


    nError = alloc_area( zName, psProc->tc_psMemSeg, nProtection, nLockMode,
			 nAddress, psSrcArea->a_nEnd - psSrcArea->a_nStart + 1, psSrcArea->a_nMaxEnd - psSrcArea->a_nStart + 1, &nNewArea );
    if ( nError < 0 ) {
	unlock_area( psSrcArea, LOCK_AREA_READ );
	put_area( psSrcArea );
	UNLOCK( g_hAreaTableSema );

	if ( nError == -ENOMEM ) {
	    shrink_caches( 65536 );
	    goto again;
	}
	return( nError );
    }
    psNewArea = get_area_from_handle( nNewArea );
    atomic_add( &psNewArea->a_nRefCount, -1 );
    
    psDstSeg = psNewArea->a_psContext;
    psSrcSeg = psSrcArea->a_psContext;
  
    nDstAddr = psNewArea->a_nStart;
    for ( nSrcAddr = psSrcArea->a_nStart ; nSrcAddr < psSrcArea->a_nEnd ; nSrcAddr += PAGE_SIZE, nDstAddr += PAGE_SIZE )
    {
	pgd_t* pSrcPgd = pgd_offset( psSrcSeg, nSrcAddr );
	pte_t* pSrcPte = pte_offset( pSrcPgd, nSrcAddr );
	pgd_t* pDstPgd = pgd_offset( psDstSeg, nDstAddr );
	pte_t* pDstPte = pte_offset( pDstPgd, nDstAddr );

	*pDstPte = *pSrcPte;

	if ( PTE_ISPRESENT( *pSrcPte ) ) {
	    int nPageNum = PAGE_NR( PTE_PAGE( *pSrcPte ) );
	    if ( nPageNum < g_sSysBase.ex_nTotalPageCount ) {
		atomic_add( &g_psFirstPage[ nPageNum ].p_nCount, 1 );
	    }
	}
    }
    psSrcArea->a_nProtection |= AREA_SHARED;
    psNewArea->a_nProtection |= AREA_SHARED;
    if ( psSrcArea->a_nProtection & AREA_REMAPPED ) {
	psNewArea->a_nProtection |= AREA_REMAPPED;
    }
    
    psNewArea->a_psNextShared = psSrcArea->a_psNextShared;
    psNewArea->a_psNextShared->a_psPrevShared = psNewArea;
    psSrcArea->a_psNextShared = psNewArea;
    psNewArea->a_psPrevShared = psSrcArea;
  
    if ( NULL != ppAddress ) {
	*ppAddress =  (void*) psNewArea->a_nStart;
    }
    unlock_area( psSrcArea, LOCK_AREA_READ );
    put_area( psSrcArea );
    flush_tlb_global();
    UNLOCK( g_hAreaTableSema );
    return( psNewArea->a_nAreaID );
error:
    UNLOCK( g_hAreaTableSema );
    return( nError );
}


/*****************************************************************************
 * NAME:
 * DESC:
 * NOTE:
 * SEE ALSO:
 ****************************************************************************/

area_id	create_area( const char* pzName, void** ppAddress, size_t nSize, size_t nMaxSize, uint32 nProtection, int nLockMode )
{
    MemContext_s* psSeg;
    area_id	hArea;
    uint32	nAddress;
    int		nError;
  
    if ( NULL == ppAddress ) {
	nAddress = 0;
	nProtection = (nProtection & ~AREA_ADDR_SPEC_MASK) | AREA_ANY_ADDRESS;
    } else {
	nAddress = (uint32) *ppAddress;
	if ( (nAddress & ~PAGE_MASK) != 0 ) {
	    printk( "Error: create_area() unaligned address %08lx\n", nAddress );
	    return( -EINVAL );
	}
    }

    if ( nProtection & AREA_KERNEL ) {
	psSeg = g_psKernelSeg;
    } else {
	psSeg = CURRENT_PROC->tc_psMemSeg;
    }
again:
    LOCK( g_hAreaTableSema );
    if ( psSeg->mc_bBusy ) { // True while context is being cloned
	UNLOCK( g_hAreaTableSema );
	printk( "create_area(): wait for segment to be ready\n" );
	snooze( 10000 );
	goto again;
    }
    nError = alloc_area( pzName, psSeg, nProtection, nLockMode, nAddress, nSize, nMaxSize, &hArea );
    UNLOCK( g_hAreaTableSema );
    if ( nError == -ENOMEM ) {
	shrink_caches( 65536 );
	goto again;
    }
    if ( nError < 0 ) {
	return( nError );
    }
	
    if ( ppAddress != NULL ) {
	AreaInfo_s sInfo;
	get_area_info( hArea, &sInfo );
	*ppAddress =  sInfo.pAddress;
    }
    return( hArea );
}


area_id	sys_create_area( const char* pzName, void** ppAddress, size_t nSize, uint32 nProtection, int nLockMode )
{
    char 	zName[64];
    area_id	hArea;
    void*	pAddress;

    if ( nProtection & AREA_KERNEL ) {
	printk( "Error: sys_create_area() attempt to create kernel area from user-space\n" );
	return( -EINVAL );
    }
    
    if ( pzName != NULL ) {
	if ( strncpy_from_user( zName, pzName, 64 ) < 0 ) {
	    return( -EFAULT );
	}
	zName[63] = '\0';
    } else {
	zName[0] = '\0';
    }

    if ( NULL == ppAddress ) {
	pAddress = (void*)(AREA_FIRST_USER_ADDRESS + 512 * 1024 * 1024);
	nProtection = (nProtection & ~AREA_ADDR_SPEC_MASK) | AREA_BASE_ADDRESS;
    } else {
	if ( memcpy_from_user( &pAddress, ppAddress, sizeof( pAddress ) ) < 0 ) {
	    return( -EFAULT );
	}
	if ( (uint32)pAddress < AREA_FIRST_USER_ADDRESS ) {
	    pAddress = (void*)(AREA_FIRST_USER_ADDRESS + 512 * 1024 * 1024);
	    nProtection = (nProtection & ~AREA_ADDR_SPEC_MASK) | AREA_BASE_ADDRESS;
	} else if ( (((uint32)pAddress) & ~PAGE_MASK) != 0 ) {
	    printk( "Error: sys_create_area() unaligned address %p\n", pAddress );
	    return( -EINVAL );
	}
    }
    
    hArea = create_area( zName, &pAddress, nSize, nSize, nProtection, nLockMode );
    
    if ( hArea >= 0 && ppAddress != NULL ) {
	memcpy_to_user( ppAddress, &pAddress, sizeof( pAddress ) );
    }
    return( hArea );
}

/*****************************************************************************
 * NAME:
 * DESC:
 * NOTE:
 * SEE ALSO:
 ****************************************************************************/

int sys_delete_area( area_id hArea )
{
    MemContext_s* psCtx = CURRENT_PROC->tc_psMemSeg;
    MemArea_s*    psArea;
    int 		nCount;
    int		nError = 0;
    
again:  
    kassertw( is_semaphore_locked( g_hAreaTableSema ) == false );
    LOCK( g_hAreaTableSema );

    psArea = get_area_from_handle( hArea );
    
    if ( psArea == NULL || psArea->a_psContext != psCtx ) {
	if ( psArea == NULL ) {
	    printk( "Error: sys_delete_area() attempt to delete invalid area %d\n", hArea );
	} else {
	    printk( "Error: sys_delete_area() attempt to delete area %d not belonging to us\n", hArea );
	}
	nError = -EINVAL;
	goto error;
    }

    if ( psCtx->mc_bBusy ) {
	atomic_add( &psArea->a_nRefCount, -1 );
	UNLOCK( g_hAreaTableSema );
	printk( "sys_delete_area(): wait for segment to become ready\n" );
	snooze( 10000 );
	goto again;
    }
    
    nCount = atomic_add( &psArea->a_nRefCount, -2 );

    if ( nCount == 2 ) {
	lock_area( psArea, LOCK_AREA_WRITE );
    
	nError = do_delete_area( psCtx, psArea );
    }
error:
    UNLOCK( g_hAreaTableSema );
    return( nError );
  
}

/*****************************************************************************
 * NAME:
 * DESC:
 * NOTE:
 * SEE ALSO:
 ****************************************************************************/

static status_t do_get_area_info( area_id hArea, AreaInfo_s* psInfo )
{
    MemArea_s* psArea;
    int	nError = 0;
	
    kassertw( is_semaphore_locked( g_hAreaTableSema ) );

    psArea = get_area_from_handle( hArea );

    if ( NULL == psArea ) {
	nError = -EINVAL;
	goto error;
    }

    psInfo->zName[0]    = '\0';
    psInfo->hAreaID     = psArea->a_nAreaID;
    psInfo->nSize	= psArea->a_nEnd - psArea->a_nStart + 1;
    psInfo->nLock	= psArea->a_nLockMode;
    psInfo->nProtection = psArea->a_nProtection;
//	psInfo->hProcess		=	psArea->;
//	psInfo->nAllocSize;
    psInfo->pAddress    = (void*) psArea->a_nStart;
  
    put_area( psArea );
error:
    return( nError );
}

status_t get_area_info( area_id hArea, AreaInfo_s* psInfo )
{
    int	       nError;

    kassertw( is_semaphore_locked( g_hAreaTableSema ) == false );
    LOCK( g_hAreaTableSema );
    nError = do_get_area_info( hArea, psInfo );
    UNLOCK( g_hAreaTableSema );
    
    return( nError );
}

status_t sys_get_area_info( area_id hArea, AreaInfo_s* psInfo )
{
    AreaInfo_s sInfo;
    int	       nError;

    kassertw( is_semaphore_locked( g_hAreaTableSema ) == false );
    LOCK( g_hAreaTableSema );
    nError = do_get_area_info( hArea, &sInfo );
    UNLOCK( g_hAreaTableSema );
    if ( nError >= 0 ) {
	nError = memcpy_to_user( psInfo, &sInfo, sizeof(sInfo) );
    }
    
    return( nError );
}


/*****************************************************************************
 * NAME:
 * DESC:
 * NOTE:
 * SEE ALSO:
 ****************************************************************************/

static int remap_pagedir( pgd_t* pPgd, int nAddress, int nSize, int nPhysAddress, bool bFreeOldPages )
{
    int	 nEnd;
    pte_t* pPte = pte_offset( pPgd, nAddress );

    nAddress &= ~PGDIR_MASK;
    nEnd = nAddress + nSize - 1;

    if ( nEnd > PGDIR_SIZE ) {
	nEnd = PGDIR_SIZE;
    }

    do
    {
	if ( bFreeOldPages )
	{
	    if ( PTE_ISPRESENT( *pPte ) )
	    {
		uint32	nOldPage = PTE_PAGE( *pPte );
		int	nPageNr  = PAGE_NR( nOldPage );
	
		if ( 0 != nOldPage && nOldPage != (uint32) g_sSysBase.ex_pNullPage ) {
		    if ( g_psFirstPage[ nPageNr ].p_nCount == 1 ) {
			unregister_swap_page( nOldPage );
		    }
		    free_pages( nOldPage, 1 );
		}
	    }
	}
	PTE_VALUE( *pPte ) = nPhysAddress | PTE_PRESENT | PTE_WRITE | PTE_USER;
	pPte++;

	nPhysAddress += PAGE_SIZE;
	nAddress += PAGE_SIZE;
    } while( nAddress < nEnd );
	
    return( 0 );
}

/*****************************************************************************
 * NAME:
 * DESC:
 * NOTE:
 * SEE ALSO:
 ****************************************************************************/

static int do_remap_area( MemArea_s* psArea, uint32 nPhysAddress )
{
    uint32 nAddress = psArea->a_nStart;
    uint32 nEnd	  = psArea->a_nEnd;
    pgd_t* pPgd;
    int	 nError   = 0;
    bool	 bFreeOldPages = (psArea->a_nProtection & AREA_REMAPPED) == 0;

    if ( nPhysAddress & ~PAGE_MASK ) {
	printk( "Error : Attempt to remap area to unaligned physical address %lx\n", nPhysAddress );
	return( -EINVAL );
    }

    pPgd = pgd_offset( psArea->a_psContext, nAddress );

    while( nAddress - 1 < nEnd )
    {
	int	nOffset;
		
	nError = remap_pagedir( pPgd++, nAddress, nEnd - nAddress, nPhysAddress, bFreeOldPages );
	if ( 0 != nError ) {
	    break;
	}
	nOffset = ((nAddress + PGDIR_SIZE) & PGDIR_MASK) - nAddress;
	nPhysAddress += nOffset;
	nAddress 	 += nOffset;
    }
    psArea->a_nProtection |= AREA_REMAPPED;
    flush_tlb_global();
    return( nError );
}

/*****************************************************************************
 * NAME:
 * DESC:
 * NOTE:
 * SEE ALSO:
 ****************************************************************************/

status_t sys_remap_area( area_id nArea, void* pPhysAddress )
{
    MemArea_s* psArea;
    int 	     nError = 0;

    kassertw( is_semaphore_locked( g_hAreaTableSema ) == false );
    LOCK( g_hAreaTableSema );

    psArea = get_area_from_handle( nArea );
  
    if ( NULL == psArea ) {
	nError = -EINVAL;
	goto error;
    }
    lock_area( psArea, LOCK_AREA_WRITE );
    nError = do_remap_area( psArea, (uint32) pPhysAddress );
    unlock_area( psArea, LOCK_AREA_WRITE );
    put_area( psArea );
error:
    UNLOCK( g_hAreaTableSema );
    return( nError );
}

/*****************************************************************************
 * NAME:
 * DESC:
 * NOTE:
 * SEE ALSO:
 ****************************************************************************/

status_t remap_area( area_id nArea, void* pPhysAddress )
{
    MemArea_s* psArea;
    int 	     nError = 0;

    kassertw( is_semaphore_locked( g_hAreaTableSema ) == false );
    LOCK( g_hAreaTableSema );

    psArea = get_area_from_handle( nArea );
  
    if ( NULL == psArea ) {
	nError = -EINVAL;
	goto error;
    }
    lock_area( psArea, LOCK_AREA_WRITE );
    nError = do_remap_area( psArea, (uint32) pPhysAddress );
    unlock_area( psArea, LOCK_AREA_WRITE );
    put_area( psArea );
error:
    UNLOCK( g_hAreaTableSema );
    return( nError );
}

/*****************************************************************************
 * NAME:
 * DESC:
 * NOTE:
 * SEE ALSO:
 ****************************************************************************/

int memcpy_to_user( void* pDst, const void* pSrc, int nSize )
{
    if ( g_bKernelInitiated ) {
	int nError = lock_mem_area( pDst, nSize, true );

	if ( nError < 0 ) {
	    return( nError );
	}
	memcpy( pDst, pSrc, nSize );
	unlock_mem_area( pDst, nSize );
	return( 0 );
    } else {
	memcpy( pDst, pSrc, nSize );
	return( 0 );
    }
}

/*****************************************************************************
 * NAME:
 * DESC:
 * NOTE:
 * SEE ALSO:
 ****************************************************************************/

int memcpy_from_user( void* pDst, const void* pSrc, int nSize )
{
    if ( g_bKernelInitiated ) {
	int nError = lock_mem_area( pSrc, nSize, false );
	if ( nError < 0 ) {
	    return( nError );
	}
	memcpy( pDst, pSrc, nSize );
	unlock_mem_area( pSrc, nSize );
	return( 0 );
    } else {
	memcpy( pDst, pSrc, nSize );
	return( 0 );
    }
}

/*****************************************************************************
 * NAME:
 * DESC:
 * NOTE:
 * SEE ALSO:
 ****************************************************************************/

int strncpy_from_user( char* pzDst, const char* pzSrc, int nMaxLen )
{
    if ( g_bKernelInitiated ) {
	int nMaxSize = ((((uint32)pzSrc) + PAGE_SIZE) & PAGE_MASK) - ((uint32)pzSrc);
	int nLen = 0;
	bool bDone = false;

	if ( nMaxSize > nMaxLen ) {
	    nMaxSize = nMaxLen;
	}
    
	while ( bDone == false && nLen < nMaxLen )
	{
	    int nError = lock_mem_area( pzSrc, nMaxSize, false );
	    int i;
	
	    if ( nError < 0 ) {
		return( nError );
	    }
	    for ( i = 0 ; i < nMaxSize ; ++i ) {
		pzDst[i] = pzSrc[i];
		if ( pzSrc[i] == '\0' ) {
		    bDone = true;
		    break;
		}
		nLen++;
	    }
	    unlock_mem_area( pzSrc, nMaxSize );
	    pzSrc += nMaxSize;
	    pzDst += nMaxSize;
	    nMaxSize = PAGE_SIZE;
	    if ( nMaxSize > nMaxLen - nLen ) {
		nMaxSize = nMaxLen - nLen;
	    }
	}
	return( nLen );
    } else {
	strncpy( pzDst, pzSrc, nMaxLen );
	return( strlen( pzDst ) );
    }
}

int strndup_from_user( const char* pzSrc, int nMaxLen, char** ppzDst )
{
    int nLen = strlen_from_user( pzSrc );
    int nError;
    char* pzDst;
    if ( nLen < 0 ) {
	return( nLen );
    }
    if ( nLen > nMaxLen - 1 ) {
	return( -ENAMETOOLONG );
    }
    pzDst = kmalloc( nLen + 1, MEMF_KERNEL | MEMF_OKTOFAILHACK );
    if ( pzDst == NULL ) {
	return( -ENOMEM );
    }
    nError = strncpy_from_user( pzDst, pzSrc, nLen + 1 );
    if ( nError != nLen ) {
	kfree( pzDst );
	if ( nError < 0 ) {
	    return( nError );
	} else {
	    return( -EFAULT ); // The string was modified before we was able to copy it.
	}
    }
    *ppzDst = pzDst;
    return( nLen );
}

/*****************************************************************************
 * NAME:
 * DESC:
 * NOTE:
 * SEE ALSO:
 ****************************************************************************/

int strcpy_to_user( char* pzDst, const char* pzSrc )
{
    return( memcpy_to_user( pzDst, pzSrc, strlen( pzSrc ) ) );
}

int strlen_from_user( const char* pzstring )
{
    if ( g_bKernelInitiated ) {
	int nMaxSize = ((((uint32)pzstring) + PAGE_SIZE) & PAGE_MASK) - ((uint32)pzstring);
	int nLen = 0;
	bool bDone = false;
    
	while ( bDone == false )
	{
	    int nError = lock_mem_area( pzstring, nMaxSize, false );
	    int i;
	
	    if ( nError < 0 ) {
		return( nError );
	    }
	    for ( i = 0 ; i < nMaxSize ; ++i ) {
		if ( pzstring[i] == '\0' ) {
		    bDone = true;
		    break;
		}
		nLen++;
	    }
	    unlock_mem_area( pzstring, nMaxSize );
	    pzstring += nMaxSize;
	    nMaxSize = PAGE_SIZE;
	}
	return( nLen );
    } else {
	return( strlen( pzstring ) );
    }
}



int lock_mem_area( const void* pAddress, uint32 nSize, bool bWriteAccess )
{
    MemArea_s* psArea;
    iaddr_t nAddr = (iaddr_t) pAddress;
    int	    nError;

    if ( nAddr < AREA_FIRST_USER_ADDRESS ) {
	printk( "lock_mem_area() got kernel address %08lx\n", nAddr );
	return( -EFAULT );
    }
again:    
    psArea = get_area( CURRENT_PROC->tc_psMemSeg, nAddr );

    if ( psArea == NULL ) {
	printk( "lock_mem_area() no area for address %08lx\n", nAddr );
	return( -EFAULT );
    }

    LOCK( g_hAreaTableSema );
    
    if ( nAddr + nSize - 1 > psArea->a_nEnd ) {
	printk( "lock_mem_area() adr=%08lx size=%ld does not fit in area %08lx-%08lx\n",
		nAddr, nSize, psArea->a_nStart, psArea->a_nEnd );
	UNLOCK( g_hAreaTableSema );
	put_area( psArea );
	return( -EFAULT );
    }

    nError = alloc_area_pages( psArea, nAddr & PAGE_MASK, (nAddr + nSize + PAGE_SIZE - 1) & PAGE_MASK, bWriteAccess );

    UNLOCK( g_hAreaTableSema );
    
    if ( nError < 0 ) {
	put_area( psArea );
	if ( nError == -ENOMEM ) {
	    shrink_caches( 65536 );
	    goto again;
	}
    }
    return( nError );
}

int unlock_mem_area( const void* pAddress, uint32 nSize )
{
    MemArea_s* psArea;
    
    iaddr_t nAddr = (iaddr_t) pAddress;

    if ( nAddr < AREA_FIRST_USER_ADDRESS ) {
	printk( "unlock_mem_area() got kernel address %08lx\n", nAddr );
	return( -EFAULT );
    }
    
    psArea = get_area( CURRENT_PROC->tc_psMemSeg, nAddr );

    if ( psArea == NULL ) {
	printk( "unlock_mem_area() no area for address %08lx\n", nAddr );
	return( -EFAULT );
    }

    if ( nAddr + nSize - 1 > psArea->a_nEnd ) {
	printk( "unlock_mem_area() adr=%08lx size=%ld does not fit in area %08lx-%08lx\n",
		nAddr, nSize, psArea->a_nStart, psArea->a_nEnd );
	put_area( psArea );
	return( -EFAULT );
    }
    
    put_area( psArea ); // The lock we just got
    put_area( psArea ); // The lock taken by lock_mem_area()
    
    return( 0 );
}

/*****************************************************************************
 * NAME:
 * DESC:
 * NOTE:
 * SEE ALSO:
 ****************************************************************************/

void* sys_sbrk( int nDelta )
{
    Process_s* psProc = CURRENT_PROC;
    void*      pOldBrk = (void*) -1;
    int	       nError = 0;
    uint32     nOldSize;
  

    if ( nDelta > 0 ) {
	nDelta = (nDelta + PAGE_SIZE - 1) & PAGE_MASK;
    } else {
	nDelta = -(((-nDelta) + PAGE_SIZE - 1) & PAGE_MASK);
    }
again:
    LOCK( g_hAreaTableSema );
    if ( psProc->tc_psMemSeg->mc_bBusy ) { // True while context is being cloned
	UNLOCK( g_hAreaTableSema );
	printk( "sys_sbrk(): wait for segment to be ready\n" );
	snooze( 10000 );
	goto again;
    }

    
    if ( psProc->pr_nHeapArea == -1 ) {
	nOldSize = 0;
    } else {
	AreaInfo_s sInfo;
	do_get_area_info( psProc->pr_nHeapArea, &sInfo );
	nOldSize = sInfo.nSize;
    }

  
    if ( (nOldSize + nDelta) == 0 ) {
	if ( psProc->pr_nHeapArea != -1 ) {
	    AreaInfo_s sInfo;
	    do_get_area_info( psProc->pr_nHeapArea, &sInfo );
	    
	    pOldBrk = (void*)(((uint32)sInfo.pAddress) + sInfo.nSize);
	    printk( "Delete proc area, old address = %p\n", pOldBrk );
	    delete_area( psProc->pr_nHeapArea );
	    psProc->pr_nHeapArea = -1;
	} else {
	    printk( "sbrk() Search for address\n" );
	    pOldBrk = (void*) find_unmapped_area( psProc->tc_psMemSeg, AREA_ANY_ADDRESS, PAGE_SIZE, 0 );
	    printk( "sbrk() next address whould be %p\n", pOldBrk );
	}
	goto error;
    }
  
    if ( psProc->pr_nHeapArea == -1 )
    {
	uint32 nReserveSize = 512 * 1024 * 1024;
	uint32 nAddress = -1;
    
	if ( nDelta < 0 ) {
	    pOldBrk = (void*)-1;
	    goto error;
	}
	while( nReserveSize >= PAGE_SIZE ) {
	    nAddress = find_unmapped_area( psProc->tc_psMemSeg, AREA_ANY_ADDRESS, nReserveSize, 0 );
	    if ( nAddress != -1 ) {
		break;
	    }
	    nReserveSize >>= 1;
	}
	if ( nAddress == -1 ) {
	    nAddress = AREA_FIRST_USER_ADDRESS;
	}
	nAddress = AREA_FIRST_USER_ADDRESS + 128 * 1024 * 1024;
	nError = alloc_area( "heap", psProc->tc_psMemSeg, AREA_BASE_ADDRESS | AREA_FULL_ACCESS, AREA_NO_LOCK,
			     nAddress, nDelta /*PGDIR_SIZE * 16*/, nDelta, &psProc->pr_nHeapArea );
	if ( nError == -ENOMEM ) {
	    UNLOCK( g_hAreaTableSema );
	    shrink_caches( 65536 );
	    goto again;
	}
	if ( nError >= 0 ) {
	    AreaInfo_s sInfo;
	    do_get_area_info( psProc->pr_nHeapArea, &sInfo );
	    pOldBrk = sInfo.pAddress;
	}
	goto error;
    } else {
	AreaInfo_s sInfo;
	do_get_area_info( psProc->pr_nHeapArea, &sInfo );
	pOldBrk = (void*)(((uint32)sInfo.pAddress) + sInfo.nSize);
	
	UNLOCK( g_hAreaTableSema );
	nError = resize_area( psProc->pr_nHeapArea, sInfo.nSize + nDelta, true );
	LOCK( g_hAreaTableSema );

	if ( nError < 0 ) {
	    pOldBrk = (void*) -1;
	}
    }
error:
    UNLOCK( g_hAreaTableSema );
    return( pOldBrk );
}

/*****************************************************************************
 * NAME:
 * DESC:
 * NOTE:
 * SEE ALSO:
 ****************************************************************************/

void db_list_proc_areas( MemContext_s* psCtx )
{
    MemArea_s* psArea;
    int	     i;
  
    for ( i = 0 ; i < psCtx->mc_nAreaCount ; ++i )
    {
	psArea = psCtx->mc_apsAreas[i];

	if ( NULL == psArea ) {
	    dbprintf( DBP_DEBUGGER, "Error: print_areas() index %d of %d has a NULL pointer!\n", i, psCtx->mc_nAreaCount );
	    break;
	}
	if ( i > 0 && psArea->a_psPrev != psCtx->mc_apsAreas[i - 1] ) {
	    dbprintf( DBP_DEBUGGER, "Error: print_areas() area at index %d (%p) has invalid prev pointer %p\n",
		      i, psArea, psArea->a_psPrev );
	}
	if ( i < psCtx->mc_nAreaCount - 1 && psArea->a_psNext != psCtx->mc_apsAreas[i + 1] ) {
	    dbprintf( DBP_DEBUGGER, "Error: print_areas() area at index %d (%p) has invalid next pointer %p\n",
		      i, psArea, psArea->a_psNext );
	}
	dbprintf( DBP_DEBUGGER, "Area %04d (%p) -> %08lx-%08lx %p %02d\n",
		  i, psArea, psArea->a_nStart, psArea->a_nEnd,
		  psArea->a_psFile, psArea->a_nRefCount );
    }
}

/*****************************************************************************
 * NAME:
 * DESC:
 * NOTE:
 * SEE ALSO:
 ****************************************************************************/

static void db_list_areas( int argc, char** argv )
{
  if ( argc == 3 && strcmp( argv[1], "-p" ) == 0 ) {
    proc_id hProc = atol( argv[2] );
    Process_s* psProc = get_proc_by_handle( hProc );

    if ( NULL == psProc ) {
      dbprintf( DBP_DEBUGGER, "Process %04d does not exist\n", hProc );
      return;
    }
    dbprintf( DBP_DEBUGGER, "Listing areas of %s (%d)\n", psProc->tc_zName, hProc );
    
    db_list_proc_areas( psProc->tc_psMemSeg );
  }
}

/*****************************************************************************
 * NAME:
 * DESC:
 * NOTE:
 * SEE ALSO:
 ****************************************************************************/

void InitAreaManager( void )
{
    register_debug_cmd( "ls_area", "list all memory areas created by a given process.", db_list_areas );
    MArray_Init( &g_sAreas );
    g_hAreaTableSema = create_semaphore( "area_table", 1, SEM_REQURSIVE );

    do_create_area( g_psKernelSeg, 0, 1024 * 1024, 1024 * 1024, false,
		    AREA_KERNEL | AREA_READ | AREA_WRITE | AREA_EXEC | AREA_CONTIGUOUS, AREA_FULL_LOCK );
    do_create_area( g_psKernelSeg, 1024 * 1024, g_sSysBase.ex_nTotalPageCount * PAGE_SIZE - 1024*1024,
		    g_sSysBase.ex_nTotalPageCount * PAGE_SIZE - 1024*1024, false,
		    AREA_KERNEL | AREA_READ | AREA_WRITE | AREA_EXEC | AREA_CONTIGUOUS, AREA_FULL_LOCK );
    flush_tlb();
}
