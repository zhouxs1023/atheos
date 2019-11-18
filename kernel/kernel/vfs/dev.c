/*
 *  The AtheOS kernel
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


#include <posix/errno.h>
#include <posix/stat.h>
#include <posix/fcntl.h>
#include <posix/dirent.h>

#include <atheos/types.h>
#include <atheos/time.h>
#include <atheos/filesystem.h>
#include <atheos/kernel.h>
#include <atheos/device.h>
#include <atheos/image.h>
#include <atheos/semaphore.h>

#include <macros.h>

#include "inc/global.h"
#include "vfs.h"

typedef	struct	FileNode	FileNode_s;
typedef	struct	SuperInfo	SuperInfo_s;

#define	DFS_MAX_NAME_LEN	64

typedef status_t device_init_t( int nDeviceID );
typedef status_t device_uninit_t( int nDeviceID );

static int 	g_nDeviceID = BUILTIN_DEVICE_ID + 1;
static sem_id	g_hMutex;

typedef struct _Device Device_s;
struct _Device
{
    Device_s*	      	      d_psNext;
    const DeviceOperations_s* d_psOps;
    int		      	      d_nDeviceID;
    atomic_t	      	      d_nNodeCount;
    atomic_t	      	      d_nOpenCount;
    char		      d_zPath[512];
    int		      	      d_nDriverImage;
    ino_t	      	      d_nImgInode;
    int		       	      d_nImgDeviceNum;
    FileNode_s*	      	      d_psFirstNode;
};

typedef struct
{
    uint8  p_nStatus;
    uint8  p_nFirstHead;
    uint16 p_nFirstCyl;
    uint8  p_nType;
    uint8  p_nLastHead;
    uint16 p_nLastCyl;
    uint32 p_nStartLBA;
    uint32 p_nSize;
} PartitionRecord_s;



struct FileNode
{
    FileNode_s*		      fn_psNextSibling;
    FileNode_s*		      fn_psNextInDevice;
    FileNode_s*		      fn_psParent;
    FileNode_s*		      fn_psFirstChild;
    const DeviceOperations_s* fn_psOps;
    void*		      fn_pDevNode;
    Device_s*		      fn_psDevice;
    int			      fn_nInodeNum;
    int			      fn_nMode;
    int			      fn_nSize;
    int			      fn_nLinkCount;
    bool		      fn_bIsLoaded;	/* TRUE between read_inode(), and write_inode() */
    bool		      fn_bIsScanned;
    char		      fn_zName[ DFS_MAX_NAME_LEN ];
    char*		      fn_pzSymLinkPath;
    bigtime_t		      fn_nCTime;
    bigtime_t		      fn_nLastScanTime;
};

typedef struct
{
    kdev_t	nDevNum;
    FileNode_s*	psRootNode;
    FileNode_s*	psDevNullNode;
    FileNode_s*	psDevZeroNode;
    FileNode_s*	psDevTTYNode;
} DevVolume_s;


static Device_s*    g_psFirstDevice = NULL;
static DevVolume_s* g_psVolume = NULL;

/*****************************************************************************
 * NAME:
 * DESC:
 * NOTE:
 * SEE ALSO:
 ****************************************************************************/

static FileNode_s* dfs_create_node( FileNode_s* psParent, const char* pzName, int nNameLen, int nMode )
{
    FileNode_s*	psNewNode;

    if ( nNameLen < 1 || nNameLen >= DFS_MAX_NAME_LEN ) {
	return( NULL );
    }

    if ( (psNewNode = kmalloc( sizeof( FileNode_s ), MEMF_KERNEL | MEMF_CLEAR ) ) )
    {
	memcpy( psNewNode->fn_zName, pzName, nNameLen );
	psNewNode->fn_zName[ nNameLen ] = 0;

	psNewNode->fn_nMode	= nMode;


	psNewNode->fn_psNextSibling = psParent->fn_psFirstChild;
	psParent->fn_psFirstChild 	= psNewNode;
	psNewNode->fn_psParent	= psParent;
	psNewNode->fn_nLinkCount	= 1;
		
	psNewNode->fn_nInodeNum	= (int) psNewNode;

	return( psNewNode );
    }
    return( NULL );
}

/*****************************************************************************
 * NAME:
 * DESC:
 * NOTE:
 * SEE ALSO:
 ****************************************************************************/

static void unload_device( Device_s* psDevice )
{
    int nError;
    device_uninit_t* pUninitFunc;
  
    nError =  get_symbol_address( psDevice->d_nDriverImage, "device_uninit", -1 , (void**) &pUninitFunc );

    if ( nError >= 0 ) {
	pUninitFunc( psDevice->d_nDeviceID );
    } else {
	printk( "Error: unload_device() device %s does not export device_uninit()\n", psDevice->d_zPath );
    }
    
    while( psDevice->d_psFirstNode != NULL ) {
	FileNode_s*  psNode = psDevice->d_psFirstNode;
	FileNode_s** ppsTmp;

	psDevice->d_psFirstNode = psNode->fn_psNextInDevice;

	if ( psNode->fn_nLinkCount > 0 )
	{
	    for ( ppsTmp = &psNode->fn_psParent->fn_psFirstChild ;
		  NULL != *ppsTmp ; ppsTmp = &(*ppsTmp)->fn_psNextSibling )
	    {
		if ( *ppsTmp == psNode ) {
		    *ppsTmp = psNode->fn_psNextSibling;
		    break;
		}
	    }

	    psNode->fn_psParent = NULL;
	    psNode->fn_nLinkCount--;
	    psNode->fn_psOps = NULL;
  
	    kassertw( 0 == psNode->fn_nLinkCount );

	    if ( psNode->fn_bIsLoaded == false )
	    {
		printk( "unload_device() : Delete node <%s>\n", psNode->fn_zName );
		if ( NULL != psNode->fn_pzSymLinkPath ) {
		    kfree( psNode->fn_pzSymLinkPath );
		}
		kfree( psNode );
	    }
	}
    }
    unload_kernel_driver( psDevice->d_nDriverImage );
    psDevice->d_nDriverImage = -1;
}

/*****************************************************************************
 * NAME:
 * DESC:
 *	Appends the nodes path relative to /dev/ in pzPath.
 * NOTE:
 *	pzPath should end with a / when passed inn, and will end
 *	with a / when returned.
 * SEE ALSO:
 ****************************************************************************/

static void get_node_path( FileNode_s* psNode, char* pzPath )
{
    FileNode_s* psTmp;
    int nPathLen = strlen( pzPath );
  
    for ( psTmp = psNode ; psTmp->fn_psParent != NULL ; psTmp = psTmp->fn_psParent ) {
	nPathLen += strlen( psTmp->fn_zName ) + 1;
    }

    pzPath[nPathLen] = '\0';
  
    for ( psTmp = psNode ; psTmp->fn_psParent != NULL ; psTmp = psTmp->fn_psParent ) {
	int nLen = strlen( psTmp->fn_zName );
	nPathLen -= nLen + 1;
	memcpy( pzPath + nPathLen, psTmp->fn_zName, nLen );
	pzPath[ nPathLen + nLen ] = '/';
    }
}

/*****************************************************************************
 * NAME:
 * DESC:
 * NOTE:
 * SEE ALSO:
 ****************************************************************************/

static Device_s* find_device_by_path( const char* pzPath )
{
    Device_s* psDevice;
  
    for ( psDevice = g_psFirstDevice ; psDevice != NULL ; psDevice = psDevice->d_psNext ) {
	if ( strcmp( psDevice->d_zPath, pzPath ) == 0 ) {
	    return( psDevice );
	}
    }
    return( NULL );
}

/*****************************************************************************
 * NAME:
 * DESC:
 * NOTE:
 * SEE ALSO:
 ****************************************************************************/

static Device_s* find_device_by_id( int nDeviceID )
{
    Device_s* psDevice;
  
    for ( psDevice = g_psFirstDevice ; psDevice != NULL ; psDevice = psDevice->d_psNext ) {
	if ( psDevice->d_nDeviceID == nDeviceID ) {
	    return( psDevice );
	}
    }
    return( NULL );
}

/*****************************************************************************
 * NAME:
 * DESC:
 * NOTE:
 * SEE ALSO:
 ****************************************************************************/

static int load_device( Device_s* psDevice )
{
    device_init_t* pInitFunc;
    struct       stat sStat;
  
    int	    nError;

    if ( g_bRootFSMounted ) {
	nError = stat( psDevice->d_zPath, &sStat );

	if ( nError < 0 ) {
	    printk( "Error: load_device() failed to stat driver image %s\n", psDevice->d_zPath );
	    goto error1;
	}
	if ( psDevice->d_nOpenCount > 0 ) {
	    nError = 0;
	    goto error1;
	}
	if ( psDevice->d_nDriverImage >= 0 ) {
	    if ( psDevice->d_nImgDeviceNum == sStat.st_dev && psDevice->d_nImgInode == sStat.st_ino ) {
		nError = 0;
		goto error1;
	    }
	    unload_device( psDevice );
	}
    }
  
    psDevice->d_nDriverImage = load_kernel_driver( psDevice->d_zPath );
  
    if ( psDevice->d_nDriverImage < 0 ) {
	nError = psDevice->d_nDriverImage;
	printk( "Error: load_device() failed to load image %s\n", psDevice->d_zPath );
	goto error1;
    }

    nError =  get_symbol_address( psDevice->d_nDriverImage, "device_init", -1 , (void**) &pInitFunc );

    if ( nError < 0 ) {
	printk( "Error: load_device() device %s does not export device_init()\n", psDevice->d_zPath );
	goto error2;
    }

    if ( g_bRootFSMounted ) {
	psDevice->d_nImgInode     = sStat.st_ino;
	psDevice->d_nImgDeviceNum = sStat.st_dev;
    } else {
	psDevice->d_nImgInode     = -1;
	psDevice->d_nImgDeviceNum = -1;
    }

    nError = pInitFunc( psDevice->d_nDeviceID );

    if ( nError < 0 ) {
	printk( "Error: device %s failed to initialize itself\n", psDevice->d_zPath );
	goto error2;
    }
    return( 1 );
error2:
    unload_kernel_driver( psDevice->d_nDriverImage );
    psDevice->d_nDriverImage = -1;
error1:
    return( nError );
}

/*****************************************************************************
 * NAME:
 * DESC:
 * NOTE:
 * SEE ALSO:
 ****************************************************************************/

static void scan_driver_dir( FileNode_s* psParentNode, char* pzPath, int nPathBase )
{
    int nDir;
    struct kernel_dirent sDirEnt;
    int		nPathLen = strlen( pzPath );
    int		nError;
    bigtime_t	nCurTime;

    nCurTime = get_system_time();
  
    if ( psParentNode->fn_bIsScanned && nCurTime < psParentNode->fn_nLastScanTime + 1000000 ) {
	return;
    }
    psParentNode->fn_bIsScanned = true;
    psParentNode->fn_nLastScanTime = nCurTime;
  
    if ( pzPath[nPathLen - 1] == '/' ) {
	nPathLen--;
	pzPath[nPathLen] = '\0';
    }
  
    nDir = open( pzPath, O_RDONLY );

    if ( nDir < 0 ) {
	return;
    }

    strcat( pzPath, "/" );
    nPathLen++;
  
    while( getdents( nDir, &sDirEnt, 1 ) == 1 ) {
	struct stat sStat;
    
	if ( strcmp( sDirEnt.d_name, "." ) == 0 || strcmp( sDirEnt.d_name, ".." ) == 0 ) {
	    continue;
	}
	strcat( pzPath, sDirEnt.d_name );
	nError = stat( pzPath, &sStat );
	if ( nError < 0 ) {
	    printk( "Error: scan_driver_dir() failed to stat %s\n", pzPath );
	    continue;
	}
	if ( S_ISDIR( sStat.st_mode ) ) {
	    FileNode_s* psTmp;
      
	    for ( psTmp = psParentNode->fn_psFirstChild ; psTmp != NULL ; psTmp = psTmp->fn_psNextSibling ) {
		if ( strcmp( psTmp->fn_zName, sDirEnt.d_name ) == 0 ) {
		    break;
		}
	    }
	    if ( psTmp == NULL ) {
		dfs_create_node( psParentNode, sDirEnt.d_name, strlen( sDirEnt.d_name ), S_IFDIR | S_IRWXUGO );
	    }
	} else {
	    Device_s* psDevice;

	    psDevice = find_device_by_path( pzPath );
	    if ( psDevice == NULL ) {
		psDevice = kmalloc( sizeof( Device_s ), MEMF_KERNEL | MEMF_CLEAR );
		if ( psDevice == NULL ) {
		    printk( "Error: scan_driver_dir() no memory for device entry\n" );
		    break;
		}

		strcpy( psDevice->d_zPath, pzPath );
		psDevice->d_psOps         = NULL;
		psDevice->d_nDeviceID     = g_nDeviceID++;
		psDevice->d_nNodeCount    = 0;
		psDevice->d_nOpenCount    = 0;
		psDevice->d_nDriverImage  = -1;
		psDevice->d_nImgInode     = sStat.st_ino;
		psDevice->d_nImgDeviceNum = sStat.st_dev;
		psDevice->d_psFirstNode   = NULL;
      
		psDevice->d_psNext        = g_psFirstDevice;
		g_psFirstDevice	        = psDevice;
	    }
	    load_device( psDevice );
	}
	pzPath[ nPathLen ] = '\0'; // Remove leaf name
    }
    close( nDir );
  
}

void init_boot_device( const char* pzPath )
{
    Device_s* psDevice = kmalloc( sizeof( Device_s ), MEMF_KERNEL | MEMF_CLEAR );
    if ( psDevice == NULL ) {
	printk( "Error: init_boot_device() no memory for device entry\n" );
	return;
    }

    strcpy( psDevice->d_zPath, pzPath );
    psDevice->d_psOps         = NULL;
    psDevice->d_nDeviceID     = g_nDeviceID++;
    psDevice->d_nNodeCount    = 0;
    psDevice->d_nOpenCount    = 0;
    psDevice->d_nDriverImage  = -1;
    psDevice->d_nImgInode     = -1;
    psDevice->d_nImgDeviceNum = -1;
    psDevice->d_psFirstNode   = NULL;
      
    psDevice->d_psNext        = g_psFirstDevice;
    g_psFirstDevice	        = psDevice;

    load_device( psDevice );
}


/*****************************************************************************
 * NAME:
 * DESC:
 * NOTE:
 * SEE ALSO:
 ****************************************************************************/

static void dfs_DeleteNode( FileNode_s* psNode )
{
    FileNode_s* psParent = psNode->fn_psParent;
    FileNode_s** ppsTmp;
	
    if ( NULL == psParent ) {
	printk( "iiik, sombody tried to delete '/dev' !!!\n" );
	return;
    }

    for ( ppsTmp = &psParent->fn_psFirstChild ; NULL != *ppsTmp ; ppsTmp = &((*ppsTmp)->fn_psNextSibling) )
    {
	if ( *ppsTmp == psNode )
	{
	    *ppsTmp = psNode->fn_psNextSibling;
	    break;
	}
    }
    kfree( psNode );
}

/*****************************************************************************
 * NAME:
 * DESC:
 * NOTE:
 * SEE ALSO:
 ****************************************************************************/

static int dfs_mount( kdev_t nDevNum, const char* pzDevPath,
		      uint32 nFlags, void* pArgs, int nArgLen, void** ppVolData, ino_t* pnRootIno )
{
    DevVolume_s*	psVolume = kmalloc( sizeof( DevVolume_s ), MEMF_KERNEL | MEMF_CLEAR );
    FileNode_s*	psRootNode;

    if ( (psRootNode = kmalloc( sizeof( FileNode_s ), MEMF_KERNEL | MEMF_CLEAR ) ) )
    {
	psRootNode->fn_zName[ 0 ] = '\0';
	psRootNode->fn_nLinkCount = 1;

	psRootNode->fn_nMode	 = S_IFDIR;
	psRootNode->fn_nInodeNum = DEVFS_ROOT;

	psRootNode->fn_psNextSibling = NULL;
	psRootNode->fn_psParent	 = NULL;

	psVolume->psRootNode = psRootNode;
	psVolume->nDevNum 	 = nDevNum;

	psVolume->psDevNullNode = dfs_create_node( psRootNode, "null", 4, S_IFCHR | S_IRUGO | S_IWUGO );
	psVolume->psDevNullNode->fn_nInodeNum = DEVFS_DEVNULL;

	psVolume->psDevZeroNode = dfs_create_node( psRootNode, "zero", 4, S_IFCHR | S_IRUGO | S_IWUGO );
	psVolume->psDevZeroNode->fn_nInodeNum = DEVFS_DEVZERO;

	psVolume->psDevTTYNode = dfs_create_node( psRootNode, "tty", 4, S_IFCHR | S_IRUGO | S_IWUGO );
	psVolume->psDevTTYNode->fn_nInodeNum = DEVFS_DEVTTY;
	
	g_psVolume = psVolume;

	*pnRootIno = DEVFS_ROOT;
	*ppVolData = psVolume;

	return( true );
    }
    return( false );
}

/*****************************************************************************
 * NAME:
 * DESC:
 * NOTE:
 * SEE ALSO:
 ****************************************************************************/

static int dfs_lookup( void* pVolume, void* pParent, const char* pzName, int nNameLen, ino_t* pnResInode )
{
    FileNode_s*	psParentNode = pParent;
    FileNode_s*	psNode;
    char zPath[256];
    int	 nPathLen;

    *pnResInode = 0;
    if ( nNameLen == 2 && '.' == pzName[0] && '.' == pzName[1] ) {
	if ( NULL != psParentNode->fn_psParent ) {
	    *pnResInode = psParentNode->fn_psParent->fn_nInodeNum;
	} else {
	    return( -ENOENT );
	}
    }

    if ( g_bRootFSMounted ) {
	sys_get_system_path( zPath, 256 );

	nPathLen = strlen( zPath );
	
	if ( '/' != zPath[ nPathLen - 1 ] ) {
	    zPath[ nPathLen ] = '/';
	    zPath[ nPathLen + 1 ] = '\0';
	}

	strcat( zPath, "sys/drivers/dev/" );
	get_node_path( psParentNode, zPath );
	scan_driver_dir( psParentNode, zPath, strlen( zPath ) );
    }
  
    for ( psNode = psParentNode->fn_psFirstChild ; NULL != psNode ; psNode = psNode->fn_psNextSibling ) {
	if ( strlen(psNode->fn_zName ) == nNameLen && strncmp( psNode->fn_zName, pzName, nNameLen ) == 0 ) {
	    *pnResInode = psNode->fn_nInodeNum;
	    break;
	}
    }
    if ( 0L != *pnResInode ) {
	return( 0 );
    } else {
	return( -ENOENT );
    }
}

/*****************************************************************************
 * NAME:
 * DESC:
 * NOTE:
 * SEE ALSO:
 ****************************************************************************/

static int dfs_read( void* pVolume, void* pNode, void* pCookie, off_t nPos, void* pBuf, size_t nLen )
{
    FileNode_s* psNode = pNode;
    int	      nError = 0;

    if ( psNode->fn_nInodeNum == DEVFS_DEVNULL ) {
	return( 0 );
    } else if ( psNode->fn_nInodeNum == DEVFS_DEVZERO ) {
	memset( pBuf, 0, nLen );
	return( nLen );
    } else if ( psNode->fn_psOps != NULL && psNode->fn_psOps->read != NULL ) {
	nError = psNode->fn_psOps->read( psNode->fn_pDevNode, pCookie, nPos, pBuf, nLen );
    }
    return( nError );
}

static int dfs_readv( void* pVolume, void* pNode, void* pCookie, off_t nPos, const struct iovec* psVector, size_t nCount )
{
    FileNode_s* psNode = pNode;
    int	      nError = 0;

    if ( psNode->fn_nInodeNum == DEVFS_DEVNULL ) {
	return( 0 );
    } else if ( psNode->fn_nInodeNum == DEVFS_DEVZERO ) {
	int nLen = 0;
	int i;
	for ( i = 0 ; i < nCount ; ++i ) {
	    memset( psVector[i].iov_base, 0, psVector[i].iov_len );
	    nLen += psVector[i].iov_len;
	}
	return( nLen );
    } else if ( psNode->fn_psOps != NULL && psNode->fn_psOps->readv != NULL ) {
	nError = psNode->fn_psOps->readv( psNode->fn_pDevNode, pCookie, nPos, psVector, nCount );
    } else {
	nError = -ENOSYS;
    }
    return( nError );
}

/*****************************************************************************
 * NAME:
 * DESC:
 * NOTE:
 * SEE ALSO:
 ****************************************************************************/

static int dfs_write( void* pVolume, void* pNode, void* pCookie, off_t nPos, const void* pBuf, size_t nLen )
{
    FileNode_s* psNode = pNode;
    int	      nError = 0;
  
    if ( psNode->fn_nInodeNum == DEVFS_DEVNULL || psNode->fn_nInodeNum == DEVFS_DEVZERO ) {
	return( nLen );
    } else if ( psNode->fn_psOps != NULL && psNode->fn_psOps->write != NULL ) {
	nError = psNode->fn_psOps->write( psNode->fn_pDevNode, pCookie, nPos, pBuf, nLen );
    } else {
	nError = -ENOSYS;
    }
    return( nError );
}

static int dfs_writev( void* pVolume, void* pNode, void* pCookie, off_t nPos, const struct iovec* psVector, size_t nCount )
{
    FileNode_s* psNode = pNode;
    int	      nError = 0;
  
    if ( psNode->fn_nInodeNum == DEVFS_DEVNULL || psNode->fn_nInodeNum == DEVFS_DEVZERO ) {
	int nLen = 0;
	int i;
	for ( i = 0 ; i < nCount ; ++i ) {
	    nLen += psVector[i].iov_len;
	}
	return( nLen );
    } else if ( psNode->fn_psOps != NULL && psNode->fn_psOps->writev != NULL ) {
	nError = psNode->fn_psOps->writev( psNode->fn_pDevNode, pCookie, nPos, psVector, nCount );
    } else {
	nError = -ENOSYS;
    }
    return( nError );
}

/*****************************************************************************
 * NAME:
 * DESC:
 * NOTE:
 * SEE ALSO:
 ****************************************************************************/

static int dfs_ioctl( void* pVolume, void* pNode, void* pCookie, int nCmd, void* pBuf, bool bFromKernel )
{
    FileNode_s* psNode = pNode;
    int	      nError = -ENOTTY;

    switch( nCmd )
    {
	case IOCTL_GET_DEVICE_PATH:
	{
	    FileNode_s* psTmp;
	    int nNameLen = 4;
	    char zPath[PATH_MAX];

	    strcpy( zPath, "/dev" );
	    
	    LOCK( g_hMutex );
	    for ( psTmp = psNode ; psTmp->fn_psParent != NULL ; psTmp = psTmp->fn_psParent ) {
		nNameLen += strlen( psTmp->fn_zName ) + 1;
	    }
	    if ( nNameLen >= PATH_MAX ) {
		nError = -ENAMETOOLONG;
		UNLOCK( g_hMutex );
		break;
	    }
	    nError = nNameLen;
	    zPath[nNameLen] = '\0';

	    for ( psTmp = psNode ; psTmp->fn_psParent != NULL ; psTmp = psTmp->fn_psParent ) {
		int nLen = strlen( psTmp->fn_zName );
		nNameLen -= nLen;
		memcpy( zPath + nNameLen, psTmp->fn_zName, nLen );
		zPath[--nNameLen] = '/';
	    }
	    UNLOCK( g_hMutex );
	    if ( bFromKernel ) {
		memcpy( pBuf, zPath, nError + 1 );
	    } else {
		if ( memcpy_to_user( pBuf, zPath, nError + 1 ) < 0 ) {
		    nError = -EFAULT;
		}
	    }
	    break;
	}
	default:
	    if ( psNode->fn_psOps != NULL && psNode->fn_psOps->ioctl != NULL ) {
		nError = psNode->fn_psOps->ioctl( psNode->fn_pDevNode, pCookie, nCmd, pBuf, bFromKernel );
	    }
	    break;
    }
    return( nError );
}

/*****************************************************************************
 * NAME:
 * DESC:
 * NOTE:
 * SEE ALSO:
 ****************************************************************************/

int dfs_read_dir( void* pVolume, void* pNode, void* pCookie, int nPos,
		  struct kernel_dirent* psFileInfo, size_t nBufSize )
{
    FileNode_s*	psParentNode	= pNode;
    FileNode_s*	psNode;
    int		nCurPos		= nPos;

    for ( psNode = psParentNode->fn_psFirstChild ; NULL != psNode ; psNode = psNode->fn_psNextSibling )
    {
	if ( nCurPos == 0 )
	{
	    strcpy( psFileInfo->d_name, psNode->fn_zName );
	    psFileInfo->d_namlen = strlen( psFileInfo->d_name );
	    psFileInfo->d_ino = psNode->fn_nInodeNum;
	    return( 1 );
	}
	nCurPos--;
    }
    return( 0 );
}

/*****************************************************************************
 * NAME:
 * DESC:
 * NOTE:
 * SEE ALSO:
 ****************************************************************************/

static int dfs_rstat( void* pVolume, void* pNode, struct stat* psStat )
{
    DevVolume_s* psVolume = pVolume;
    FileNode_s*  psNode   = pNode;

    psStat->st_ino 	= psNode->fn_nInodeNum;
    psStat->st_dev	= psVolume->nDevNum;
    psStat->st_size 	= psNode->fn_nSize;
    psStat->st_mode	= psNode->fn_nMode;
    psStat->st_nlink	= psNode->fn_nLinkCount;
    psStat->st_atime	= 0;
    psStat->st_mtime	= 0;
    psStat->st_ctime	= 0;
    psStat->st_uid	= 0;
    psStat->st_gid	= 0;
	
    return( 0 );
}

/*****************************************************************************
 * NAME:
 * DESC:
 * NOTE:
 * SEE ALSO:
 ****************************************************************************/

static int dfs_open( void* pVolume, void* pNode, int nMode, void** ppCookie )
{
    FileNode_s* psNode = pNode;
    int	      nError = 0;

    if ( psNode->fn_psOps != NULL && psNode->fn_psOps->open != NULL ) {
	nError = psNode->fn_psOps->open( psNode->fn_pDevNode, nMode, ppCookie );
	if ( nError >= 0 && psNode->fn_psDevice != NULL ) {
	    psNode->fn_psDevice->d_nOpenCount++;
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

static int dfs_Close( void* pVolume, void* pNode, void* pCookie )
{
    FileNode_s* psNode = pNode;
    int	      nError = 0;
  
    if ( psNode->fn_psOps != NULL && psNode->fn_psOps->close != NULL ) {
	nError = psNode->fn_psOps->close( psNode->fn_pDevNode, pCookie );
	if ( psNode->fn_psDevice != NULL ) {
	    psNode->fn_psDevice->d_nOpenCount--;
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

static int dfs_read_inode( void* pVolume, ino_t nInodeNum, void** ppNode )
{
    DevVolume_s*	psVolume = pVolume;
    FileNode_s*	psNode;

    switch( nInodeNum )
    {
	case DEVFS_ROOT:
	    psNode = psVolume->psRootNode;
	    break;
	case DEVFS_DEVNULL:
	    psNode = psVolume->psDevNullNode;
	    break;
	case DEVFS_DEVZERO:
	    psNode = psVolume->psDevZeroNode;
	    break;
	case DEVFS_DEVTTY:
	    psNode = psVolume->psDevTTYNode;
	    break;
	default:
	    psNode = (FileNode_s*) ((int)nInodeNum);
	    if ( psNode->fn_nInodeNum != (int) psNode ) {
		printk( "dfs_read_inode() invalid inode %Ld\n", nInodeNum );
		psNode = NULL;
	    }
	    break;
    }
	
    if ( NULL != psNode )
    {
	if ( g_bRootFSMounted ) {
	    char zPath[256];
	    int  nPathLen;
      
	    sys_get_system_path( zPath, 256 );

	    nPathLen = strlen( zPath );
	
	    if ( '/' != zPath[ nPathLen - 1 ] ) {
		zPath[ nPathLen ] = '/';
		zPath[ nPathLen + 1 ] = '\0';
	    }

	    strcat( zPath, "sys/drivers/dev/" );
	    get_node_path( psNode, zPath );
	    scan_driver_dir( psNode, zPath, strlen( zPath ) );
	}
    
	kassertw( false == psNode->fn_bIsLoaded );
	psNode->fn_bIsLoaded = true;
	*ppNode = psNode;
	return( 0 );
    }
    else
    {
	*ppNode = NULL;
	return( -EINVAL );
    }
}

/*****************************************************************************
 * NAME:
 * DESC:
 * NOTE:
 * SEE ALSO:
 ****************************************************************************/

static int dfs_write_inode( void* pVolData, void* pNode )
{
    FileNode_s* psNode = pNode;
	
    psNode->fn_bIsLoaded	= false;

    if ( 0 == psNode->fn_nLinkCount )
    {
	printk( "dfs_write_inode() : Deleting node <%s>\n", psNode->fn_zName );

	if ( NULL != psNode->fn_pzSymLinkPath ) {
	    kfree( psNode->fn_pzSymLinkPath );
	}
	kfree( psNode );
    }
    return( 0 );
}

/*****************************************************************************
 * NAME:
 * DESC:
 * NOTE:
 * SEE ALSO:
 ****************************************************************************/

static int dfs_mkdir( void* pVolume, void* pParent, const char *pzName, int nNameLen, int nPerms )
{
    FileNode_s*	psParentNode = pParent;
    FileNode_s*	psNewNode;

    if ( (psNewNode = dfs_create_node( psParentNode, pzName, nNameLen, S_IFDIR | S_IRWXUGO )) )
    {
	return( 0 );
    }
    return( -1 );
}

/*****************************************************************************
 * NAME:
 * DESC:
 * NOTE:
 * SEE ALSO:
 ****************************************************************************/

static int dfs_rmdir( void* pVolData, void* pParentNode, const char* pzName, int nNameLen )
{
    FileNode_s* 	psParent = pParentNode;
    FileNode_s*	psNode	 = NULL;
    FileNode_s**	ppsTmp;
    int		nError 	 = -ENOENT;
	
    for ( ppsTmp = &psParent->fn_psFirstChild ; NULL != *ppsTmp ; ppsTmp = &((*ppsTmp)->fn_psNextSibling) )
    {
	if ( strlen( (*ppsTmp)->fn_zName ) == nNameLen && strncmp( (*ppsTmp)->fn_zName, pzName, nNameLen ) == 0 )
	{
	    psNode = *ppsTmp;
	    if ( psNode->fn_psFirstChild != NULL || S_ISDIR( psNode->fn_nMode ) == false ) {
		psNode = NULL;
		break;
	    }
			
	    *ppsTmp = psNode->fn_psNextSibling;
	    nError = 0;
	    break;
	}
    }

    if ( NULL != psNode )
    {
	psNode->fn_psParent = NULL;
	psNode->fn_nLinkCount--;

	kassertw( 0 == psNode->fn_nLinkCount );
		
	if ( FALSE == psNode->fn_bIsLoaded )
	{
	    printk( "dfs_rmdir() : Delete node <%s>\n", pzName );
	    if ( NULL != psNode->fn_pzSymLinkPath ) {
		kfree( psNode->fn_pzSymLinkPath );
	    }
	    kfree( psNode );
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

static int dfs_unlink( void* pVolData, void* pParentNode, const char* pzName, int nNameLen )
{
    return( -ENOSYS );
}

/*****************************************************************************
 * NAME:
 * DESC:
 * NOTE:
 * SEE ALSO:
 ****************************************************************************/

static int dfs_symlink( void* pVolume, void* pParentNode, const char* pzName, int nNameLen, const char* pzNewPath )
{
    FileNode_s* psParent = pParentNode;
    FileNode_s* psNode;
    int		nError = 0;
	
    psNode = dfs_create_node( psParent, pzName, nNameLen, S_IFLNK | S_IRWXUGO );

    if ( NULL != psNode )
    {
	psNode->fn_pzSymLinkPath = kmalloc( strlen( pzNewPath ) + 1, MEMF_KERNEL );

	if ( NULL != psNode->fn_pzSymLinkPath )
	{
	    strcpy( psNode->fn_pzSymLinkPath, pzNewPath );
	}
	else
	{
	    nError = -ENOMEM;
	    dfs_DeleteNode( psNode );
	}
    }
    else
    {
	nError = -ENOMEM;
    }
    return( nError );
}

/*****************************************************************************
 * NAME:
 * DESC:
 * NOTE:
 * SEE ALSO:
 ****************************************************************************/

static int dfs_readlink( void* pVolume, void* pNode, char* pzBuf, size_t nBufSize )
{
    FileNode_s* psNode = pNode;

    if ( NULL != psNode->fn_pzSymLinkPath )
    {
	strncpy( pzBuf, psNode->fn_pzSymLinkPath, nBufSize );
	pzBuf[ nBufSize - 1 ] = '\0';
	return( strlen( pzBuf ) );
    }
    else
    {
	return( -EINVAL );
    }
}

/*****************************************************************************
 * NAME:
 * DESC:
 * NOTE:
 * SEE ALSO:
 ****************************************************************************/

static FSOperations_s	g_sOperations =
{
    NULL,		// op_probe
    dfs_mount,		// op_mount
    NULL, 		// op_unmount
    dfs_read_inode,
    dfs_write_inode,
    dfs_lookup, 	/* Lookup		*/
    NULL,		/* access 		*/
    NULL, 		/* create, 		*/
    dfs_mkdir, 		/* mkdir 		*/
    NULL,		/* mknod		*/
    dfs_symlink,	/* symlink 		*/
    NULL,		/* link 		*/
    NULL, 		/* rename 		*/
    dfs_unlink,		/* unlink 		*/
    dfs_rmdir, 		/* rmdir 		*/
    dfs_readlink,	/* readlink 		*/
    NULL, 		/* ppendir 		*/
    NULL,		/* closedir 		*/
    NULL, 		/* RewindDir		*/
    dfs_read_dir,	/* ReadDir 		*/
    dfs_open,		/* open			*/
    dfs_Close,		/* close 		*/
    NULL, 		/* FreeCookie		*/
    dfs_read, 		/* Read 		*/
    dfs_write,		/* Write 		*/
    dfs_readv,		// op_readv
    dfs_writev,		// op_writev
    dfs_ioctl,		/* ioctl 		*/
    NULL, 		/* setflags 		*/
    dfs_rstat,
    NULL,		/* wstat 		*/
    NULL,		/* fsync 		*/
    NULL, 		/* initialize 		*/
    NULL, 		/* sync 		*/
    NULL, 		/* rfsstat		*/
    NULL, 		/* wfsstat 		*/
    NULL		/* isatty		*/
};


static int locate_parent_Node( const char* pzPath, bool bCreateDirs, const char** ppzName, int* pnNameLen, FileNode_s** ppsRes )
{
    FileNode_s*	psParent = g_psVolume->psRootNode;
    const char*	pzStart = pzPath;
    int		nError;
    int		i;

    for ( i = 0 ; pzPath[i] != '\0' ; ++i ) {
	if ( pzPath[i] == '/' ) {
	    FileNode_s*	psTmp;
	    int nLen;
	    nLen = &pzPath[i] - pzStart;

	    if ( nLen == 0 ) {
		continue;
	    }
      
	    for ( psTmp = psParent->fn_psFirstChild ; psTmp != NULL ; psTmp = psTmp->fn_psNextSibling ) {
		if ( strlen( psTmp->fn_zName ) == nLen && strncmp( psTmp->fn_zName, pzStart, nLen ) == 0 ) {
		    break;
		}
	    }
	    if ( psTmp != NULL && S_ISDIR( psTmp->fn_nMode ) == false ) {
		printk( "Error: create_device_node() node %s already exist\n", pzPath );
		nError = -EEXIST;
		goto error1;
	    }
	    if ( psTmp == NULL ) {
		if ( bCreateDirs ) {
		    psTmp = dfs_create_node( psParent, pzStart, nLen, S_IFDIR | S_IRWXUGO );
		    if ( psTmp == NULL ) {
			printk( "Error: create_device_node() no memory for directory node %s\n", pzPath );
			nError = -ENOMEM;
			goto error1;
		    }
		} else {
		    nError = -ENOENT;
		    goto error1;
		}
	    }
	    psParent = psTmp;
	    pzStart = pzPath + i + 1;
	}
    }
    *pnNameLen = &pzPath[i] - pzStart;
    *ppzName = pzStart;
    *ppsRes  = psParent;
    return( 0 );
error1:
    return( nError );
    
}

static int link_device_node( const char* pzPath, FileNode_s* psNode )
{
    FileNode_s*	psParent;
    const char*	pzName;
    int  	nLen;
    FileNode_s*	psTmp;
    int		nError;

    nError = locate_parent_Node( pzPath, true, &pzName, &nLen, &psParent );
    
    if ( nError < 0 ) {
	goto error1;
    }
    for ( psTmp = psParent->fn_psFirstChild ; psTmp != NULL ; psTmp = psTmp->fn_psNextSibling ) {
	if ( strlen( psTmp->fn_zName ) == nLen && strncmp( psTmp->fn_zName, pzName, nLen ) == 0 ) {
	    printk( "Error: create_device_node() node %s already exist\n", pzName );
	    nError = -EEXIST;
	    goto error1;
	}
    }


    memcpy( psNode->fn_zName, pzName, nLen );
    psNode->fn_zName[ nLen ] = 0;

    psNode->fn_psNextSibling = psParent->fn_psFirstChild;
    psParent->fn_psFirstChild 	= psNode;
    psNode->fn_psParent	= psParent;
    psNode->fn_nLinkCount	= 1;
    return( 0 );
error1:
    return( nError );
    
}

/** Publish a device-node inside the /dev/ directory
 * \ingroup DriverAPI
 * \par Description:
 *	Device drivers can call this function to publish their device nodes
 *	inside the /dev/ directory. AtheOS have a very flexible and dynamic
 *	system for managing device nodes. A device driver can create, rename
 *	and delete nodes inside the /dev/ directory at any time.
 *	Normally a device driver will call create_device_node() during
 *	initialization and delete_device_node() when being deinitialized, but
 *	it can also create and delete nodes at other points if it for example
 *	controll some form of removable device that might be added or removed
 *	after the device driver has been loaded.
 *
 * \par Note:
 *
 * \param nDeviceID
 *	This is the unique device ID passed to the driver in the "device_init()"
 *	funcion.
 *
 * \param pzPath
 *	The path is relative to /dev/ and can specify directories in addition
 *	to the node-name. For example "disk/scsi/hda/raw" will create a device
 *	node name "raw" inside /dev/disk/scsi/hda/". The path should not have
 *	any leading or trailing slashes.
 *
 * \param psOps
 *	Pointer to a DeviceOperations_s structure containing a list of
 *	function pointers that will be called by the kernel on behalf
 *	of the user of the device. Functions like read(), write(), ioctl(),
 *	etc etc, will be mapped more or less directly into calls into the
 *	device driver through these function pointers.
 *
 * \param pCookie
 *	This is just a void pointer that the kernel will be associating with
 *	the device-node and that will be passed back to the driver when
 *	calling functions in psOps. This can be used by the driver to associate
 *	some private data with the device node.
 *
 * \return
 *	create_device_node() returns a handle that can later be used to rename
 *	the node with rename_device_node() or to delete the node with
 *	delete_device_node(). If the node is not deleted before the driver
 *	is unloaded it will be deleted automatically by the kernel.
 *
 * \sa delete_device_node(), rename_device_node()
 * \author	Kurt Skauen (kurt@atheos.cx)
 *****************************************************************************/

int create_device_node( int nDeviceID, const char* pzPath, const DeviceOperations_s* psOps, void* pCookie )
{
    Device_s*	psDevice;
    FileNode_s*	psTmp;
    int		nError;
//    int		i;

    if ( nDeviceID != BUILTIN_DEVICE_ID ) {
	psDevice = find_device_by_id( nDeviceID );
	if ( psDevice == NULL ) {
	    printk( "create_device_node() cant find device %d\n", nDeviceID );
	    return( -EINVAL );
	}
    } else {
	psDevice = NULL;
    }

    LOCK( g_hMutex );

    psTmp = kmalloc( sizeof( FileNode_s ), MEMF_KERNEL | MEMF_CLEAR | MEMF_OKTOFAILHACK );
    if ( psTmp == NULL ) {
	printk( "Error: create_device_node() no memory for device node %s\n", pzPath );
	nError = -ENOMEM;
	goto error1;
    }
    nError = link_device_node( pzPath, psTmp );
    if ( nError < 0 ) {
	goto error2;
    }
    psTmp->fn_nMode	= S_IFCHR | S_IRUGO | S_IWUGO;
    psTmp->fn_nInodeNum	= (int) psTmp;
    
    psTmp->fn_psOps    = psOps;
    psTmp->fn_pDevNode = pCookie;

    if ( psDevice != NULL ) {
	psTmp->fn_psDevice = psDevice;
	psTmp->fn_psNextInDevice = psDevice->d_psFirstNode;
	psDevice->d_psFirstNode  = psTmp;
    }
    UNLOCK( g_hMutex );
    return( psTmp->fn_nInodeNum );
error2:
    kfree( psTmp );
error1:
    UNLOCK( g_hMutex );
    return( nError );
}

static int unlink_device_node( FileNode_s* psNode )
{
    FileNode_s*  psParent = psNode->fn_psParent;
    FileNode_s** ppsTmp;
    int		 nError = -ENOENT;

    for ( ppsTmp = &psParent->fn_psFirstChild ; NULL != *ppsTmp ; ppsTmp = &((*ppsTmp)->fn_psNextSibling) ) {
	if ( *ppsTmp == psNode ) {
	    *ppsTmp = psNode->fn_psNextSibling;
	    psNode->fn_psNextSibling = NULL;
	    psNode->fn_psParent = NULL;
	    nError = 0;
	    break;
	}
    }
    return( nError );
}

/** Remove a device node from the /dev/ directory.
 * \ingroup DriverAPI
 * \par Description:
 *	Delete a node previously created with create_device_node().
 *	The device node will be removed from /dev/ directory and
 *	the DeviceOperations_s structure and the cookie passed to
 *	create_device_node() can now be destroyed by the driver.
 * \param nHandle
 *	A device-node handle returned by a previous call to create_device_node()
 * \return
 *	0 on success, a negative error code on failure
 * \sa create_device_node(), rename_device_node()
 * \author	Kurt Skauen (kurt@atheos.cx)
 *****************************************************************************/

int delete_device_node( int nHandle )
{
    FileNode_s*	 psNode	 = (FileNode_s*) nHandle;
    Device_s*	 psDevice = psNode->fn_psDevice;
    int		 nError = -ENOENT;

    LOCK( g_hMutex );

    nError = unlink_device_node( psNode );
    if ( psDevice != NULL ) {
	FileNode_s** ppsTmp;
	for ( ppsTmp = &psDevice->d_psFirstNode ; NULL != *ppsTmp ; ppsTmp = &((*ppsTmp)->fn_psNextInDevice) ) {
	    if ( *ppsTmp == psNode ) {
		*ppsTmp = psNode->fn_psNextInDevice;
		break;
	    }
	}
    }
    psNode->fn_nLinkCount--;
    psNode->fn_psOps = NULL;
  
    kassertw( 0 == psNode->fn_nLinkCount );

    if ( psNode->fn_bIsLoaded == false ) {
	printk( "delete_device_node() : Delete node <%s>\n", psNode->fn_zName );
	if ( NULL != psNode->fn_pzSymLinkPath ) {
	    kfree( psNode->fn_pzSymLinkPath );
	}
	kfree( psNode );
    }
    UNLOCK( g_hMutex );
    return( nError );
}

/** Rename and/or move a device node.
 * \ingroup DriverAPI
 * \par Description:
 *	A device driver can rename and/or move a device node inside the /dev/
 *	hierarchy. The new path should be relative to /dev/ and have the same
 *	format as the path passed to create_device_node().
 *
 * \param nHandle
 *	A device-node handle returned by a previous call to create_device_node()
 * \param pzNewPath
 *	The new device-node path.
 * \return
 * \sa create_device_node(), delete_device_node()
 * \author	Kurt Skauen (kurt@atheos.cx)
 *****************************************************************************/

int rename_device_node( int nHandle, const char* pzNewPath )
{
    FileNode_s* psNode = (FileNode_s*)nHandle;
    FileNode_s* psOldParent = psNode->fn_psParent;
    FileNode_s* psNewParent;
    FileNode_s* psTmp;
    const char* pzNewName;
    int		nNameLen;
    int		nError;

    LOCK( g_hMutex );
    nError = locate_parent_Node( pzNewPath, true, &pzNewName, &nNameLen, &psNewParent );
    if ( nError < 0 ) {
	goto error;
    }

    if ( psNewParent != psOldParent || (strlen( psNode->fn_zName ) == nNameLen && strncmp( psNode->fn_zName, pzNewName, nNameLen ) == 0) ) {
	for ( psTmp = psNewParent->fn_psFirstChild ; psTmp != NULL ; psTmp = psTmp->fn_psNextSibling ) {
	    if ( strlen( psTmp->fn_zName ) == nNameLen && strncmp( psTmp->fn_zName, pzNewName, nNameLen ) == 0 ) {
		printk( "Warning: rename_device_node() node %s already exist\n", pzNewPath );
		nError = -EEXIST;
		goto error;
	    }
	}
    }

    if ( psNewParent != psOldParent ) {
	unlink_device_node( psNode );
	psNode->fn_psNextSibling = psNewParent->fn_psFirstChild;
	psNewParent->fn_psFirstChild = psNode;
	psNode->fn_psParent	     = psNewParent;
    }
    
    memcpy( psNode->fn_zName, pzNewName, nNameLen );
    psNode->fn_zName[ nNameLen ] = '\0';

    UNLOCK( g_hMutex );
    return( 0 );
error:
    UNLOCK( g_hMutex );
    return( nError );
}

/** Decode a hard-disk partition table.
 * \ingroup DriverAPI
 * \par Description:
 *	decode_disk_partitions() can be called by block-device drivers to
 *	decode a disk's partition table. It will return both primary
 *	partitions and logical partitions within the extended partition if
 *	one exists. The extended partition itself will not be returned.
 *
 *	The caller must provide the device-geometry and a callback that
 *	will be called to read the primary partition table and any existing
 *	nested extended partition tabless.
 *
 *	The partition table is validated and the function will fail if
 *	it is found invalid. Checks performed includes overlaping partitions,
 *	partitions ending outside the disk and the primary table containing
 *	more than one extended partition.
 *
 * \par Note:
 *	Primary partitions are numbered 0-3 based on their position inside
 *	the primary table. Logical partition are numbered from 4 and up.
 *	This might leave "holes" in the returned array of partitions.
 *	The returned count only indicate the highest used partition number
 *	and the caller must check each returned partition and filter out
 *	partitions where the type-field is '0'.
 * \param psDevice
 *	Pointer to a device_geometry structure describing the disk's geometry.
 * \param pasPartitions
 *	Pointer to an array of partition descriptors that will be filled in.
 * \param nMaxPartitions
 *	Number of entries allocated for pasPartitions. Maximum nMaxPartitions
 *	number of partitions will be returned. If the function return exactly
 *	nMaxPartitions it might have overflowed and the call should be repeated
 *	with a larger buffer.
 * \param pCookie
 *	Pointer private to the caller that will be passed back to the pfReadCallback
 *	call-back function.
 * \param pfReadCallback
 *	Pointer to a function that will be called to read in partition tables.
 * 
 * \return
 *	A positive maximum partition-count on success or a negative error
 *	code on failure.
 * \sa create_device_node(), delete_device_node()
 * \author	Kurt Skauen (kurt@atheos.cx)
 *****************************************************************************/


int decode_disk_partitions( device_geometry* psDiskGeom, Partition_s* pasPartitions, int nMaxPartitions, void* pCookie, disk_read_op* pfReadCallback )
{
    char anBuffer[512];
    PartitionRecord_s* pasTable = (PartitionRecord_s*)(anBuffer + 0x1be);
    int	  i;
    int	  nCount = 0;
    int   nRealCount = 0;
    off_t nDiskSize = psDiskGeom->sector_count * psDiskGeom->bytes_per_sector;
    off_t nTablePos = 0;
    off_t nExtStart = 0;
    off_t nFirstExtended = 0;
    int	  nNumExtended;
    int	  nNumActive;
    int   nError;
    
repeat:
    if ( pfReadCallback( pCookie, nTablePos, anBuffer, 512 ) != 512 ) {
	printk( "Error: decode_disk_partitions() failed to read MBR\n" );
	nError = 0; //-EINVAL;
	goto error;
    }
    if ( *((uint16*)(anBuffer + 0x1fe)) != 0xaa55 ) {
	printk( "Error: decode_disk_partitions() Invalid partition table signature %04x\n", *((uint16*)(anBuffer + 0x1fe)) );
	nError = 0; //-EINVAL;
	goto error;
    }

    nNumActive   = 0;
    nNumExtended = 0;
    
    for ( i = 0 ; i < 4 ; ++i ) {
	if ( pasTable[i].p_nStatus & 0x80 ) {
	    nNumActive++;
	}
	if ( pasTable[i].p_nType == 0x05 || pasTable[i].p_nType == 0x0f || pasTable[i].p_nType == 0x85 ) {
	    nNumExtended++;
	}
	if ( nNumActive > 1 ) {
	    printk( "Warning: decode_disk_partitions() more than one active partitions\n" );
	}
	if ( nNumExtended > 1 ) {
	    printk( "Error: decode_disk_partitions() more than one extended partitions\n" );
	    nError = -EINVAL;
	    goto error;
	}
    }
    for ( i = 0 ; i < 4 ; ++i ) {
	int j;
    
	if ( nCount >= nMaxPartitions ) {
	    break;
	}
	if ( pasTable[i].p_nType == 0 ) {
	    continue;
	}
	if ( pasTable[i].p_nType == 0x05 || pasTable[i].p_nType == 0x0f || pasTable[i].p_nType == 0x85 ) {
	    nExtStart = ((uint64)pasTable[i].p_nStartLBA) * ((uint64)psDiskGeom->bytes_per_sector); // + nTablePos;
	    if ( nFirstExtended == 0 ) {
		memset( &pasPartitions[nCount], 0, sizeof(pasPartitions[nCount]) );
		nCount++;
	    }
	    continue;
	}

	pasPartitions[nCount].p_nType   = pasTable[i].p_nType;
	pasPartitions[nCount].p_nStatus = pasTable[i].p_nStatus;
	pasPartitions[nCount].p_nStart  = ((uint64)pasTable[i].p_nStartLBA) * ((uint64)psDiskGeom->bytes_per_sector) + nTablePos;
	pasPartitions[nCount].p_nSize   = ((uint64)pasTable[i].p_nSize) * ((uint64)psDiskGeom->bytes_per_sector);

	if ( pasPartitions[nCount].p_nStart + pasPartitions[nCount].p_nSize > nDiskSize ) {
	    printk( "Error: Partition %d extend outside the disk/extended partition\n", nCount );
	    nError = -EINVAL;
	    goto error;
	}
	
	for ( j = 0 ; j < nCount ; ++j ) {
	    if ( pasPartitions[nCount].p_nType == 0 ) {
		continue;
	    }
	    if ( pasPartitions[j].p_nStart + pasPartitions[j].p_nSize > pasPartitions[nCount].p_nStart &&
		 pasPartitions[j].p_nStart <  pasPartitions[nCount].p_nStart + pasPartitions[nCount].p_nSize ) {
		printk( "Error: decode_disk_partitions() partition %d overlap partition %d\n", j, nCount );
		nError = -EINVAL;
		goto error;
	    }
	    if ( (pasPartitions[nCount].p_nStatus & 0x80) != 0 && (pasPartitions[j].p_nStatus & 0x80) != 0 ) {
		printk( "Error: decode_disk_partitions() more than one active partitions\n" );
		nError = -EINVAL;
		goto error;
	    }
	    if ( pasPartitions[nCount].p_nType == 0x05 && pasPartitions[j].p_nType == 0x05 ) {
		printk( "Error: decode_disk_partitions() more than one extended partitions\n" );
		nError = -EINVAL;
		goto error;
	    }
	}
	nCount++;
	nRealCount++;
    }
    if ( nExtStart != 0 ) {
	nTablePos = nFirstExtended + nExtStart;
	if ( nFirstExtended == 0 ) {
	    nFirstExtended = nExtStart;
	}
	nExtStart = 0;
	if ( nCount < 4 ) {
	    while( nCount & 0x03 ) {
		memset( &pasPartitions[nCount++], 0, sizeof(Partition_s) );
	    }
	}
	goto repeat;
    }
    return( nCount );
error:
    if ( nCount < 4 || nRealCount == 0 ) {
	return( nError );
    } else {
	return( nCount & ~3 );
    }
}


/*****************************************************************************
 * NAME:
 * DESC:
 * NOTE:
 * SEE ALSO:
 ****************************************************************************/

bool init_dev_fs( void )
{
    g_hMutex = create_semaphore( "devfs_lock", 1, 0 );
    kassertw( g_hMutex >= 0 );
    register_file_system( "_device_fs", &g_sOperations );
    return( true );
}
