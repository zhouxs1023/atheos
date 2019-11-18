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

#ifndef __F_SMP_H__
#define __F_SMP_H__

#include <atheos/types.h>
#include <atheos/atomic.h>
#include <macros.h>

#ifdef __cplusplus
extern "C" {
#if 0  
} /*make emacs indention work */
#endif
#endif



#ifdef __KERNEL__

#include <atheos/typedefs.h>
#include <atheos/kdebug.h>
#include <atheos/kernel.h>

#include "i82489.h"

extern int		g_pAPICPageDir;

#define SMP_MAGIC_IDENT	(('_'<<24)|('P'<<16)|('M'<<8)|'_')


typedef struct
{
  uint32 tc_nJmpInstr;
  uint32 tc_nKernelEntry;
  uint32 tc_nKernelStack;
  uint32 tc_nKernelDS;
  uint32 tc_nKernelCS;
  uint32 tc_nGdt;	  /* Pointer to global descriptor table */
} SmpTrampolineCfg_s;



#define MPC_SIGNATURE "PCMP"

typedef struct
{
    char   mpc_anSignature[4];
    uint16 mpc_nSize;	/* Size of table */
    char   mpc_nVersion;	/* Table version (0x01 or 0x04) */
    char   mpc_checksum;
    char   mpc_anOEMString[8];
    char   mpc_anProductID[12];
    void*  mpc_pOEMPointer;	/* 0 if not present */
    uint16 mpc_nOEMSize;	/* 0 if not present */
    uint16 mpc_nOEMCount;
    uint32 mpc_nAPICAddress;	/* APIC address */
    uint32 mpc_reserved;
} MpConfigTable_s;

typedef struct
{
    char   	     mpf_anSignature[4];	/* "_MP_" 				*/
    MpConfigTable_s* mpf_psConfigTable;		/* Configuration table address		*/
    uint8	     mpf_nLength;		/* Length of struct in paragraphs	*/
    uint8	     mpf_nVersion;		/* Specification version		*/
    uint8	     mpf_nChecksum;		/* Checksum (makes sum 0)		*/
    uint8	     mpf_nFeature1;		/* Standard or configuration ? 		*/
    uint8	     mpf_nFeature2;		/* Bit7 set for IMCR|PIC		*/
    uint8	     mpf_unused[3];		/* Unused (0)				*/
} MpFloatingPointer_s;

/* Followed by entries */

#define	MP_PROCESSOR	0
#define	MP_BUS		1
#define	MP_IOAPIC	2
#define	MP_INTSRC	3
#define	MP_LINTSRC	4

/* CPU flags */
#define CPU_ENABLED		1	/* Processor is available */
#define CPU_BOOTPROCESSOR	2	/* Processor is the BP */

/* CPU features */
#define CPU_STEPPING_MASK 0x0F
#define CPU_MODEL_MASK	0xF0
#define CPU_FAMILY_MASK	0xF00

typedef struct
{
  uint8  mpc_nType;
  uint8  mpc_nAPICID;	/* Local APIC number */
  uint8  mpc_nAPICVer;	/* Its versions */
  uint8  mpc_cpuflag;
  uint32 mpc_cpufeature;		
  uint32 mpc_featureflag;	/* CPUID feature value */
  uint32 mpc_reserved[2];
} MpConfigProcessor_s;

typedef struct
{
  uint8 mpc_nType;
  uint8 mpc_busid;
  uint8 mpc_bustype[6] __attribute((packed));
} MpConfigBus_s;

#define BUSTYPE_EISA	"EISA"
#define BUSTYPE_ISA	"ISA"
#define BUSTYPE_INTERN	"INTERN"	/* Internal BUS */
#define BUSTYPE_MCA	"MCA"
#define BUSTYPE_VL	"VL"		/* Local bus */
#define BUSTYPE_PCI	"PCI"
#define BUSTYPE_PCMCIA	"PCMCIA"

/* We don't understand the others */

#define MPC_APIC_USABLE		0x01

typedef struct
{
  uint8  mpc_nType;
  uint8  mpc_nAPICID;
  uint8  mpc_nAPICVer;
  uint8  mpc_nFlags;
  uint32 mpc_nAPICAddress;
} MpConfigIOAPIC_s;

typedef struct
{
  uint8  mpc_nType;
  uint8  mpc_nIRQType;
  uint16 mpc_nIRQFlags;
  uint8  mpc_nSrcBusID;
  uint8  mpc_nSrcBusIRQ;
  uint8  mpc_nDstAPIC;
  uint8  mpc_nDstIRQ;
} MpConfigIntSrc_s;

#define MP_INT_VECTORED		0
#define MP_INT_NMI		1
#define MP_INT_SMI		2
#define MP_INT_EXTINT		3

#define MP_IRQDIR_DEFAULT	0
#define MP_IRQDIR_HIGH		1
#define MP_IRQDIR_LOW		3


#define MP_APIC_ALL	0xFF

typedef struct
{
    uint8  mpc_nType;
    uint8  mpc_nIRQType;
    uint16 mpc_nIRQFlags;
    uint8  mpc_nSrcBusID;
    uint8  mpc_nSrcBusIRQ;
    uint8  mpc_nDstAPIC;
    uint8  mpc_nDstAPICLocalInt;
} MpConfigIntLocal_s;


/*
 *	Default configurations
 *
 *	1	2 CPU ISA 82489DX
 *	2	2 CPU EISA 82489DX no IRQ 8 or timer chaining
 *	3	2 CPU EISA 82489DX
 *	4	2 CPU MCA 82489DX
 *	5	2 CPU ISA+PCI
 *	6	2 CPU EISA+PCI
 *	7	2 CPU MCA+PCI
 */

typedef struct
{
    char      pi_anVendorID[16];
    uint32    pi_nCoreSpeed;
    uint32    pi_nBusSpeed;
    uint32    pi_nDelayCount;
    uint32    pi_nGS;		/* GS segment, used for thread specific data */
    int	      pi_nFamily;
    int	      pi_nModel;
    int	      pi_nAPICVersion;
    Thread_s* pi_psCurrentThread;
    Thread_s* pi_psIdleThread;
    bigtime_t pi_nIdleTime;
    bool      pi_bIsPresent;
    bool      pi_bIsRunning;
    bool      pi_bHaveCPUID;
    bool      pi_bHavePERFCnt;
    bool      pi_bHaveAPIC;
} ProcessorInfo_s;


extern bool		g_bAPICPresent;
extern int		g_nActiveCPUCount;
extern int		g_nBootCPU;
extern ProcessorInfo_s	g_asProcessorDescs[MAX_CPU_COUNT];
extern uint32 		g_nVirtualAPICAddr;


/*
 *	APIC handlers: Note according to the Intel specification update
 *	you should put reads between APIC writes.
 *	Intel Pentium processor specification update [11AP, pg 64]
 *		"Back to Back Assertions of HOLD May Cause Lost APIC Write Cycle"
 */


extern __inline void apic_write( uint32 nReg, uint32 nValue )
{
  kassertw( g_bAPICPresent );
  *((vuint32*)(g_nVirtualAPICAddr + nReg)) = nValue;
}

extern __inline unsigned long apic_read( uint32 nReg)
{
  return( *((vuint32*)(g_nVirtualAPICAddr + nReg)) );
}

#ifdef __BUILD_KERNEL__
extern __inline int get_processor_id(void)
{
  return( GET_APIC_ID(apic_read(APIC_ID)) );
}
#endif

int logig_to_physical_cpu_id( int nLogicID );


#define NO_PROC_ID		0xFF		/* No processor magic marker */


bool init_smp( bool bInitSMP );
void boot_ap_processors( void );

void forward_irq( int nTarget, int nIrqNum );

#define MSG_ALL_BUT_SELF	0x8000		/* Assume < 32768 CPU's */
#define MSG_ALL			0x8001

#endif /* __KERNEL__ */

#ifdef __cplusplus
}
#endif


#endif /* __F_SMP_H__ */
