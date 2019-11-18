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

#ifndef __ATHEOS_KERNEL_H__
#define __ATHEOS_KERNEL_H__

#include <atheos/kdebug.h>
#include <atheos/stdlib.h>
#include <atheos/schedule.h>
#include <atheos/v86.h>
#include <atheos/areas.h>

#ifdef __cplusplus
extern "C" {
#if 0  
} /*make emacs indention work */
#endif
#endif

#define MAX_CPU_COUNT	16

typedef struct
{
  uint64	nCoreSpeed;
  uint64	nBusSpeed;
  bigtime_t	nActiveTime;
} CPUInfo_s;

enum { SYS_INFO_VERSION = 2 };

typedef struct
{
    bigtime_t nBootTime;				/* time of boot (# usec since 1/1/70) */
    int	      nCPUCount;
    int	      nCPUType;
    CPUInfo_s asCPUInfo[MAX_CPU_COUNT];
    int       nMaxPages;				/* total # physical pages		*/
    int       nFreePages;				/* Number of free physical pages	*/
    int	      nCommitedPages;				/* Total number of allocated pages	*/
    int       nPageFaults;				/* Number of page faults		*/
    int       nUsedSemaphores;				/* Number of semaphores in use		*/
    int       nUsedPorts;				/* Number of message ports in use	*/
    int       nUsedThreads;			 	/* Number of living threads		*/
    int       nUsedProcesses;			 	/* Number of living processes		*/

    char      zKernelName[ OS_NAME_LENGTH ];	 	/* Name of kernel image		*/
    char      zKernelBuildDate[ OS_NAME_LENGTH ];	/* Date of kernel built		*/
    char      zKernelBuildTime[ OS_NAME_LENGTH ];	/* Time of kernel built		*/
    int64     nKernelVersion;
} system_info_v1;

typedef struct
{
    int64     nKernelVersion;
    char      zKernelName[ OS_NAME_LENGTH ];	 	/* Name of kernel image		*/
    char      zKernelBuildDate[ OS_NAME_LENGTH ];	/* Date of kernel built		*/
    char      zKernelBuildTime[ OS_NAME_LENGTH ];	/* Time of kernel built		*/
    bigtime_t nBootTime;				/* time of boot (# usec since 1/1/70) */
    int	      nCPUCount;
    int	      nCPUType;
    CPUInfo_s asCPUInfo[MAX_CPU_COUNT];
    int       nMaxPages;				/* total # physical pages		*/
    int       nFreePages;				/* Number of free physical pages	*/
    int	      nCommitedPages;				/* Total number of allocated pages	*/
    int	      nKernelMemSize;
    int       nPageFaults;				/* Number of page faults		*/
    int       nUsedSemaphores;				/* Number of semaphores in use		*/
    int       nUsedPorts;				/* Number of message ports in use	*/
    int       nUsedThreads;			 	/* Number of living threads		*/
    int       nUsedProcesses;			 	/* Number of living processes		*/

    int	      nLoadedImageCount;
    int	      nImageInstanceCount;

    int	      nOpenFileCount;
    int       nAllocatedInodes;
    int       nLoadedInodes;
    int       nUsedInodes;
    int	      nBlockCacheSize;
    int	      nDirtyCacheSize;
    int	      nLockedCacheBlocks;
} system_info;


/* this struct defines the way the registers are stored on the
   stack during a system call. */

typedef struct
{
    long 	ebx;
    long 	ecx;
    long 	edx;
    long 	esi;
    long	edi;
    long 	ebp;
    long 	eax;
    uint16	ds, __dsu;
    uint16	es, __esu;
    uint16	fs, __fsu;
    uint16	gs, __gsu;
    long 	orig_eax;
    long 	eip;
    uint16	cs, __csu;
    long 	eflags;
    long 	oldesp;
    uint16	oldss, __ssu;
} SysCallRegs_s;



void print_registers( SysCallRegs_s* psRegs );

// Sendt to the app server when a thread dies

typedef	struct
{
    thread_id	hThread;
} DR_ThreadDied_s;


int  isa_readb( int nPort );
int  isa_readw( int nPort );
int  isa_readl( int nPort );
int  isa_writeb( int nPort, int nValue );
int  isa_writew( int nPort, int nValue );
int  isa_writel( int nPort, int nValue );


uint32	GetCMOSTime( void );
//void	SetColor( int Color, int R, int G, int B );

void	InitSemaphores( void );
void	InitMsgPorts( void );
void	InitProcesses( void );
void	InitThreads( void );
void	InitAreas( void );
void	InitTimer( void );
void	init_pci_module( void );

void	SwitchCont( int nDesc );

uint32 Delay( uint32	nMicros );

status_t snooze( bigtime_t nTimeout );

int	exit_from_sys_call( void );


void	flush_tlb( void );			  /* Invalidate local page table 		*/
void	flush_tlb_global( void );		  /* Invalidate page table on all processors	*/
void	flush_tlb_page( uint32 nVirtualAddress ); /* Flush one local pte 			*/

int	realint( int num, struct RMREGS *rm );

status_t get_system_info( system_info* psInfo );
int	 set_app_server_port( port_id hPort );
port_id	 get_app_server_port( void );
port_id	 sys_get_app_server_port( void );

int	sys_get_system_path( char* pzBuffer, int nBufLen );
int	get_system_path( char* pzBuffer, int nBufLen );


void protect_dos_mem( void );
void unprotect_dos_mem( void );

void*	kmalloc( size_t nSize, int nFlags );
int 	__kfree( void* pBlock );

#define kfree(p) kassertw( __kfree(p) == 0 )

void*	alloc_real( uint32 RequestSize, uint32	Flags );
void	free_real( void* Block );

int	Fork( const char* pzName );
int	reboot( void );
void    hard_reset( void );

int	sys_exit( int nCode );
int	exit_thread( int nCode );
int	do_exit( int nCode );

void	SetColor( int Color, int R, int G, int B );

int	get_area_for( MemArea_s** ppsArea, const void* pAddress, uint32 nSize );
void	release_area( MemArea_s* psArea );


thread_id	create_init_proc( const char* pzName );
void	init_kmalloc( void );
void	make_kernel_image( void );

int	sys_killpg( const pid_t nGrp, const int nSigNum );
int	sys_kill_proc( proc_id hProcess, int nSigNum );

int	killpg( pid_t nGrp, int nSigNum );
int	kill_proc( proc_id hProcess, int nSigNum );

int 	initialize_fs( const char* pzDevPath, const char* pzFsType, const char* pzVolName, void* pArgs, int nArgLen );
int 	mount( const char* pzDevName, const char* pzDirName,
	   const char* pzFSName, int nFlags, void* pData );
int	sys_unmount( const char* pzPath, bool bForce );

int	sys_initialize_fs( const char* pzDevPath, const char* pzFsType, const char* pzVolName, void* pArgs, int nArgLen );

int	sys_mount( const char* pzDevName, const char* pzDirName,
		   const char* pzFSName, int nFlags, void* pData );
int	unmount( const char* pzPath, bool bForce );


/* Process groups */

#ifdef __KERNEL__

int handle_signals( int dummy );
int is_orphaned_pgrp( int nProcessGroup );

int	setpgid( pid_t a_hDest, pid_t a_hGroup );
int	getpgid( pid_t hPid );
pid_t	getpgrp(void);
pid_t	getppid(void);
int	getsid( pid_t hPid );
int	setsid(void);
gid_t	getegid(void);
uid_t	geteuid(void);
gid_t	getfsgid(void);
uid_t	getfsuid(void);
gid_t	getgid(void);
int	getgroups(int _size, gid_t *grouplist);
uid_t	getuid(void);
int	setgid(gid_t _gid);
int	setuid( uid_t uid );


bool get_str_arg( char* pzValue, const char* pzName, const char* pzArg, int nArgLen );
bool get_num_arg( uint32* pnValue, const char* pzName, const char* pzArg, int nArgLen );


#endif /* __KERNEL__ */


#ifdef __cplusplus
}
#endif

#endif	/* __ATHEOS_KERNEL_H__	*/

