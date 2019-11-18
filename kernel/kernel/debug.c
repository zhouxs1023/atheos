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

#include <atheos/ctype.h>
#include <posix/errno.h>

#include <atheos/types.h>
#include <atheos/isa_io.h>
#include <atheos/kernel.h>
#include <atheos/irq.h>
#include <atheos/spinlock.h>

#include <macros.h>

#include "inc/sysbase.h"
#include "inc/intel.h"
#include "inc/swap.h"

typedef struct _DbgCmd DbgCmd_s;

static int  g_nDebugBaudRate  = 0;
static int  g_nDebugPort      = 2;
static int  g_nPortBase       = 0x2f8;
static int  g_nPortIRQ        = 3;
static int  g_nIRQHandle      = -1;
static bool g_bPlainTextDebug = false;
static bool g_bInitialized    = false;

#define UART_RX		0	/* In:  Receive buffer (DLAB=0) */
#define UART_TX		0	/* Out: Transmit buffer (DLAB=0) */
#define UART_DLL	0	/* Out: Divisor Latch Low (DLAB=1) */
#define UART_DLM	1	/* Out: Divisor Latch High (DLAB=1) */
#define UART_IER	1	/* Out: Interrupt Enable Register */
#define UART_IIR	2	/* In:  Interrupt ID Register */
#define UART_FCR	2	/* Out: FIFO Control Register */
#define UART_EFR	2	/* I/O: Extended Features Register */
				/* (DLAB=1, 16C660 only) */
#define UART_LCR	3	/* Out: Line Control Register */
#define UART_MCR	4	/* Out: Modem Control Register */
#define UART_LSR	5	/* In:  Line Status Register */
#define UART_MSR	6	/* In:  Modem Status Register */
#define UART_SCR	7	/* I/O: Scratch Register */

/*
 * These are the definitions for the FIFO Control Register
 * (16650 only)
 */
#define UART_FCR_ENABLE_FIFO	0x01 /* Enable the FIFO */
#define UART_FCR_CLEAR_RCVR	0x02 /* Clear the RCVR FIFO */
#define UART_FCR_CLEAR_XMIT	0x04 /* Clear the XMIT FIFO */
#define UART_FCR_DMA_SELECT	0x08 /* For DMA applications */
#define UART_FCR_TRIGGER_MASK	0xC0 /* Mask for the FIFO trigger range */
#define UART_FCR_TRIGGER_1	0x00 /* Mask for trigger set at 1 */
#define UART_FCR_TRIGGER_4	0x40 /* Mask for trigger set at 4 */
#define UART_FCR_TRIGGER_8	0x80 /* Mask for trigger set at 8 */
#define UART_FCR_TRIGGER_14	0xC0 /* Mask for trigger set at 14 */
/* 16650 redefinitions */
#define UART_FCR6_R_TRIGGER_8	0x00 /* Mask for receive trigger set at 1 */
#define UART_FCR6_R_TRIGGER_16	0x40 /* Mask for receive trigger set at 4 */
#define UART_FCR6_R_TRIGGER_24  0x80 /* Mask for receive trigger set at 8 */
#define UART_FCR6_R_TRIGGER_28	0xC0 /* Mask for receive trigger set at 14 */
#define UART_FCR6_T_TRIGGER_16	0x00 /* Mask for transmit trigger set at 16 */
#define UART_FCR6_T_TRIGGER_8	0x10 /* Mask for transmit trigger set at 8 */
#define UART_FCR6_T_TRIGGER_24  0x20 /* Mask for transmit trigger set at 24 */
#define UART_FCR6_T_TRIGGER_30	0x30 /* Mask for transmit trigger set at 30 */
/* TI 16750 definitions */
#define UART_FCR7_64BYTE	0x20 /* Go into 64 byte mode */

/*
 * These are the definitions for the Line Control Register
 * 
 * Note: if the word length is 5 bits (UART_LCR_WLEN5), then setting 
 * UART_LCR_STOP will select 1.5 stop bits, not 2 stop bits.
 */
#define UART_LCR_DLAB	0x80	/* Divisor latch access bit */
#define UART_LCR_SBC	0x40	/* Set break control */
#define UART_LCR_SPAR	0x20	/* Stick parity (?) */
#define UART_LCR_EPAR	0x10	/* Even parity select */
#define UART_LCR_PARITY	0x08	/* Parity Enable */
#define UART_LCR_STOP	0x04	/* Stop bits: 0=1 stop bit, 1= 2 stop bits */
#define UART_LCR_WLEN5  0x00	/* Wordlength: 5 bits */
#define UART_LCR_WLEN6  0x01	/* Wordlength: 6 bits */
#define UART_LCR_WLEN7  0x02	/* Wordlength: 7 bits */
#define UART_LCR_WLEN8  0x03	/* Wordlength: 8 bits */

/*
 * These are the definitions for the Line Status Register
 */
#define UART_LSR_TEMT	0x40	/* Transmitter empty */
#define UART_LSR_THRE	0x20	/* Transmit-hold-register empty */
#define UART_LSR_BI	0x10	/* Break interrupt indicator */
#define UART_LSR_FE	0x08	/* Frame error indicator */
#define UART_LSR_PE	0x04	/* Parity error indicator */
#define UART_LSR_OE	0x02	/* Overrun error indicator */
#define UART_LSR_DR	0x01	/* Receiver data ready */

/*
 * These are the definitions for the Interrupt Identification Register
 */
#define UART_IIR_NO_INT	0x01	/* No interrupts pending */
#define UART_IIR_ID	0x06	/* Mask for the interrupt ID */

#define UART_IIR_MSI	0x00	/* Modem status interrupt */
#define UART_IIR_THRI	0x02	/* Transmitter holding register empty */
#define UART_IIR_RDI	0x04	/* Receiver data interrupt */
#define UART_IIR_RLSI	0x06	/* Receiver line status interrupt */

/*
 * These are the definitions for the Interrupt Enable Register
 */
#define UART_IER_MSI	0x08	/* Enable Modem status interrupt */
#define UART_IER_RLSI	0x04	/* Enable receiver line status interrupt */
#define UART_IER_THRI	0x02	/* Enable Transmitter holding register int. */
#define UART_IER_RDI	0x01	/* Enable receiver data interrupt */
/*
 * Sleep mode for ST16650 and TI16750.
 * Note that for 16650, EFR-bit 4 must be selected as well.
 */
#define UART_IERX_SLEEP  0x10	/* Enable sleep mode */

/*
 * These are the definitions for the Modem Control Register
 */
#define UART_MCR_LOOP	0x10	/* Enable loopback test mode */
#define UART_MCR_OUT2	0x08	/* Out2 complement */
#define UART_MCR_OUT1	0x04	/* Out1 complement */
#define UART_MCR_RTS	0x02	/* RTS complement */
#define UART_MCR_DTR	0x01	/* DTR complement */

/*
 * These are the definitions for the Modem Status Register
 */
#define UART_MSR_DCD	0x80	/* Data Carrier Detect */
#define UART_MSR_RI	0x40	/* Ring Indicator */
#define UART_MSR_DSR	0x20	/* Data Set Ready */
#define UART_MSR_CTS	0x10	/* Clear to Send */
#define UART_MSR_DDCD	0x08	/* Delta DCD */
#define UART_MSR_TERI	0x04	/* Trailing edge ring indicator */
#define UART_MSR_DDSR	0x02	/* Delta DSR */
#define UART_MSR_DCTS	0x01	/* Delta CTS */
#define UART_MSR_ANY_DELTA 0x0F	/* Any of the delta bits! */

/*
 * These are the definitions for the Extended Features Register
 * (StarTech 16C660 only, when DLAB=1)
 */
#define UART_EFR_CTS	0x80	/* CTS flow control */
#define UART_EFR_RTS	0x40	/* RTS flow control */
#define UART_EFR_SCD	0x20	/* Special character detect */
#define UART_EFR_ECB	0x10	/* Enhanced control bit */
/*
 * the low four bits control software flow control
 */

#define ser_out( reg, val ) isa_writeb( g_nPortBase + (reg), (val) )
#define ser_in( reg )       isa_readb( g_nPortBase + (reg) )


SPIN_LOCK( g_sSPinLock, "dbg_slock" );

struct _DbgCmd
{
    DbgCmd_s* psNext;
    char	    zName[256];
    char	    zDesc[256];
    dbg_fnc*  pFunc;
};

static DbgCmd_s* g_psCommands = NULL;

typedef struct
{
    int8 dp_anMagic1[4];
    int  dp_nCRC;
    int  dp_nPort;
    int  dp_nSize;
    char dp_anBuffer[DB_PACKET_SIZE];
} DBPacketHdr_s;

typedef struct
{
    int		lb_nBufSize;
    int		lb_nCurSize;
    int		lb_nInPos;
    int		lb_nOutPos;
    uint8*	lb_pBuffer;
} LocalBuffer_s;

static uint8 g_anBuf00[32768];	// Remote printk() output.
static uint8 g_anBuf01[1024];	// Debugger input from remote.
static uint8 g_anBuf02[32768];	// Debugger output from remote.
static uint8 g_anBuf03[1024];
static uint8 g_anBuf04[1024];
static uint8 g_anBuf05[1024];
static uint8 g_anBuf06[1024];
static uint8 g_anBuf07[1024];
static uint8 g_anBuf08[65536];	// Local printk() output.
static uint8 g_anBuf09[1024];	// Local input to debugger.
static uint8 g_anBuf10[32768];	// Local debugger output.
static uint8 g_anBuf11[1024];
static uint8 g_anBuf12[1024];
static uint8 g_anBuf13[1024];
static uint8 g_anBuf14[1024];
static uint8 g_anBuf15[1024];

static LocalBuffer_s g_asBuffers[DB_PORT_COUNT] = {
    {sizeof(g_anBuf00),0,0,0,g_anBuf00},
    {sizeof(g_anBuf01),0,0,0,g_anBuf01},
    {sizeof(g_anBuf02),0,0,0,g_anBuf02},
    {sizeof(g_anBuf03),0,0,0,g_anBuf03},
    {sizeof(g_anBuf04),0,0,0,g_anBuf04},
    {sizeof(g_anBuf05),0,0,0,g_anBuf05},
    {sizeof(g_anBuf06),0,0,0,g_anBuf06},
    {sizeof(g_anBuf07),0,0,0,g_anBuf07},
    {sizeof(g_anBuf08),0,0,0,g_anBuf08},
    {sizeof(g_anBuf09),0,0,0,g_anBuf09},
    {sizeof(g_anBuf10),0,0,0,g_anBuf10},
    {sizeof(g_anBuf11),0,0,0,g_anBuf11},
    {sizeof(g_anBuf12),0,0,0,g_anBuf12},
    {sizeof(g_anBuf13),0,0,0,g_anBuf13},
    {sizeof(g_anBuf14),0,0,0,g_anBuf14},
    {sizeof(g_anBuf15),0,0,0,g_anBuf15}
};
    

static void write_buffer( LocalBuffer_s* psBuffer, const void* pData, int nLen )
{
    if ( nLen > psBuffer->lb_nBufSize - psBuffer->lb_nCurSize ) {
	nLen = psBuffer->lb_nBufSize - psBuffer->lb_nCurSize;
    }
    if ( nLen > 0 ) {
	if ( psBuffer->lb_nInPos + nLen > psBuffer->lb_nBufSize ) {
	    int l1 = psBuffer->lb_nBufSize - psBuffer->lb_nInPos;
	    int l2 = nLen - l1;
	
	    memcpy( psBuffer->lb_pBuffer + psBuffer->lb_nInPos, pData, l1 );
	    memcpy( psBuffer->lb_pBuffer, ((const uint8*)pData) + l1, l2 );
	    psBuffer->lb_nInPos = l2;
	} else {
	    memcpy( psBuffer->lb_pBuffer + psBuffer->lb_nInPos, pData, nLen );
	    psBuffer->lb_nInPos += nLen;
	}
	psBuffer->lb_nCurSize += nLen;
    }
}

static int read_buffer( LocalBuffer_s* psBuffer, void* pData, int nLen )
{
    if ( nLen > psBuffer->lb_nCurSize ) {
	nLen = psBuffer->lb_nCurSize;
    }
    if ( nLen > 0 ) {
	if ( psBuffer->lb_nOutPos + nLen > psBuffer->lb_nBufSize ) {
	    int l1 = psBuffer->lb_nBufSize - psBuffer->lb_nOutPos;
	    int l2 = nLen - l1;
	
	    memcpy( pData, psBuffer->lb_pBuffer + psBuffer->lb_nOutPos, l1 );
	    memcpy( ((uint8*)pData) + l1, psBuffer->lb_pBuffer, l2 );
	    psBuffer->lb_nOutPos = l2;
	} else {
	    memcpy( pData, psBuffer->lb_pBuffer + psBuffer->lb_nOutPos, nLen );
	    psBuffer->lb_nOutPos += nLen;
	}
	psBuffer->lb_nCurSize -= nLen;
    }
    return( nLen );
}

static void add_buffer( DBPacketHdr_s* psHdr )
{
    write_buffer( &g_asBuffers[psHdr->dp_nPort], psHdr->dp_anBuffer, psHdr->dp_nSize );
}

static int read_serial( char* pBuffer, int nSize )
{
    int i;

    for ( i = 0 ; i < nSize && (ser_in( UART_LSR ) & 0x01) ; ++i ) {
	pBuffer[i] = ser_in( UART_RX );
    }
    return( i );
}

static void write_serial( const char* pBuffer, int nSize, bool bTransCRLF )
{
    int	i;

    if ( g_nDebugBaudRate == 0 ) {
	return;
    }
    
    for ( i = 0 ; i < nSize ; ++i ) {
	while( (ser_in( UART_LSR ) & 0x40) == 0 )
	      /*** EMPTY ***/;
	ser_out( UART_TX, pBuffer[i] );
	if ( bTransCRLF && pBuffer[i] == '\n' ) {
	    while( (ser_in( UART_LSR ) & 0x40) == 0 )
		  /*** EMPTY ***/;
	    ser_out( UART_TX, '\r' );
	}
    }
}


static int write_packet( int nPort, const char* pBuffer, int nSize )
{
    DBPacketHdr_s  sHdr;
    int		 nCRC = 0;
    int		 i;

    if ( nSize > DB_PACKET_SIZE ) {
	nSize = DB_PACKET_SIZE;
    }
  
    for ( i = 0 ; i < nSize ; ++i ) {
	nCRC +=  pBuffer[i];
    }
  
    sHdr.dp_anMagic1[0] = 0xde;
    sHdr.dp_anMagic1[1] = 0xad;
    sHdr.dp_anMagic1[2] = 0xba;
    sHdr.dp_anMagic1[3] = 0xbe;
  
    sHdr.dp_nCRC  = nCRC;
    sHdr.dp_nPort = nPort;
    sHdr.dp_nSize = nSize;

    memcpy( sHdr.dp_anBuffer, pBuffer, nSize );

    if ( nPort < DB_PORT_COUNT / 2 ) {
	if ( g_nDebugBaudRate > 0 ) {
	    if ( g_bPlainTextDebug ) {
		write_serial( pBuffer, nSize, true );
	    } else {
		write_serial( (char*) &sHdr, sizeof( sHdr ) - DB_PACKET_SIZE + nSize, false );
	    }
	}
	sHdr.dp_nPort = nPort + DB_PORT_COUNT / 2;
    } else {
	sHdr.dp_nPort = nPort - DB_PORT_COUNT / 2;
    }
    write_buffer( &g_asBuffers[sHdr.dp_nPort], pBuffer, nSize );
//    add_buffer( &sHdr );

    return( nSize );
}

void debug_write( const char* pBuffer, int nSize )
{
    int	nFlg = cli();
    spinlock( &g_sSPinLock );

    dbcon_write( pBuffer, nSize );
  
    while( nSize > 0 ) {
	int nCurSize = min( nSize, DB_PACKET_SIZE );
	write_packet( 0, pBuffer, nCurSize );
	nSize -= nCurSize;
	pBuffer += nCurSize;
    }
    spinunlock( &g_sSPinLock );
    put_cpu_flags( nFlg );
}

int sys_debug_write( int nPort, const char* pBuffer, int nSize )
{
    int nError = nSize;
    int	nFlg = cli();
    spinlock( &g_sSPinLock );
  
    while( nSize > 0 ) {
	int nCurSize = min( nSize, DB_PACKET_SIZE );
	write_packet( nPort, pBuffer, nCurSize );
	nSize -= nCurSize;
	pBuffer += nCurSize;
    }
    spinunlock( &g_sSPinLock );
    put_cpu_flags( nFlg );
    return( nError );
}

int sys_debug_read( int nPort, char* pBuffer, int nSize )
{
    int		nBytesRead = 0;
    int		nFlg;
  
    if ( nPort < 0 || nPort >= DB_PORT_COUNT ) {
	return( -EINVAL );
    }

    nFlg = cli();
    spinlock( &g_sSPinLock );

    nBytesRead = read_buffer( &g_asBuffers[nPort], pBuffer, nSize );
    spinunlock( &g_sSPinLock );
    put_cpu_flags( nFlg );
    return( nBytesRead );
}

static void receive_serial_data()
{
    static DBPacketHdr_s psHdr;
    static int	 	nState = 0;
    static char		anBuffer[16];
    static int		nCurPos = 0;
    static int		nCRC = 0;
    uint8		nChar;
    int			nFlg;
    spinlock_cli( &g_sSPinLock, nFlg );

    if ( g_bPlainTextDebug ) {
	while( read_serial( &nChar, 1 ) == 1 ) {
	    write_serial( &nChar, 1, false );
	    write_buffer( &g_asBuffers[1], &nChar, 1 );
	}
    } else {
	while( read_serial( &nChar, 1 ) == 1 ) {
	    switch ( nState )
	    {
		case 0:
		    if ( nChar == 0xde ) nState = 1;
		    break;
		case 1:
		    if ( nChar == 0xad ) {
			nState = 2;
		    } else {
			nState = 0;
		    }
		    break;
		case 2:
		    if ( nChar == 0xba ) {
			nState = 3;
		    } else {
			nState = 0;
		    }
		    break;
		case 3:
		    if ( nChar == 0xbe ) {
			nState = 4;
			nCurPos = 0;
		    } else {
			nState = 0;
		    }
		    break;
		case 4:
		    anBuffer[nCurPos++] = nChar;
		    if ( nCurPos >= 4 ) {
			psHdr.dp_nCRC = *((int*)(anBuffer));
			nCurPos = 0;
			nState = 5;
		    }
		    break;
		case 5:
		    anBuffer[nCurPos++] = nChar;
		    if ( nCurPos >= 4 ) {
			psHdr.dp_nPort = *((int*)(anBuffer));
			nCurPos = 0;
	  
			if ( psHdr.dp_nPort >= 0 && psHdr.dp_nPort <= DB_PORT_COUNT / 2 ) {
			    nState = 6;
			} else {
			    nState = 0; // Invalid port number
			}
		    }
		    break;
		case 6:
		    anBuffer[nCurPos++] = nChar;
		    if ( nCurPos >= 4 ) {
			psHdr.dp_nSize = *((int*)(anBuffer));
			nCurPos = 0;
			nCRC    = 0;
	  
			if ( psHdr.dp_nSize > 0 && psHdr.dp_nSize <= DB_PACKET_SIZE ) {
			    nState = 7;
			} else {
			    nState = 0; // Invalid packet size
			}
		    }
		    break;
		case 7:
		    psHdr.dp_anBuffer[nCurPos++] = nChar;
		    if ( nCurPos <= psHdr.dp_nSize ) {
			nCRC += (char)nChar;
		    }
	
		    if ( nCurPos >= psHdr.dp_nSize )
		    {
			if ( psHdr.dp_nCRC == nCRC ) {
			    nState = 0;
			    add_buffer( &psHdr );
			} else {
			    nState = 0; // Wrong CRC number.
			}
			nCurPos = 0;
		    }
		    break;
		case 8:
		    if ( (nCurPos == 0 && nChar == 0xde) ||
			 (nCurPos == 1 && nChar == 0xad) ||
			 (nCurPos == 2 && nChar == 0xbe) ||
			 (nCurPos == 3 && nChar == 0xef) || 1 )
		    {
			if ( nCurPos == 3 ) {
			    add_buffer( &psHdr );
	    
			    nState = 0;
			}
			nCurPos++;
		    } else {
			nState = 0;
		    }
		    break;
	    }
	}
    }
    spinunlock_restore( &g_sSPinLock, nFlg );
}

static int com2_irq( int nIrqNum, void* pData, SysCallRegs_s* psRegs )
{
    ser_in( UART_LSR );  // Line status (LSR)
    ser_in( UART_IIR );  // Check interrupt type (IIR)
    ser_in( UART_MSR );  // Check modem status (MSR)

    receive_serial_data();
    return( 0 );
}

int register_debug_cmd( const char* pzName, const char* pzDesc, dbg_fnc* pFunc )
{
    DbgCmd_s* psCmd;

    psCmd = kmalloc( sizeof( DbgCmd_s ), MEMF_KERNEL | MEMF_OKTOFAILHACK );

    if ( NULL == psCmd ) {
	printk( "PANIC : register_debug_cmd() out of memory\n" );
	return( -ENOMEM );
    }

    strcpy( psCmd->zName, pzName );
    strcpy( psCmd->zDesc, pzDesc );
    psCmd->pFunc = pFunc;

    psCmd->psNext = g_psCommands;
    g_psCommands = psCmd;
    return( 0 );
}

static dbg_fnc* find_command( const char* pzName )
{
    DbgCmd_s* psCmd;

    for ( psCmd = g_psCommands ; NULL != psCmd ; psCmd = psCmd->psNext ) {
	if ( strcmp( pzName, psCmd->zName ) == 0 ) {
	    return( psCmd->pFunc );
	}
    }
    return( NULL );
}



static void parse_command( char* pzBuffer )
{
    static char* argv[256];
    int	       argc = 0;
    int	       nStart = 0;
    int	       i;
  
    for ( i = 0 ; pzBuffer[i] != '\0' ; ++i )
    {
	if ( isspace( pzBuffer[i] ) ) {
	    pzBuffer[i] = '\0';
	    argv[argc++] = &pzBuffer[nStart];
	    if ( argc >= 255 ) {
		break;
	    }
	    while( isspace( pzBuffer[i] ) ) i++;
	    nStart = i + 1;
	}
    }
    if ( i > 0 ) {
	argv[argc++] = &pzBuffer[nStart];
    }
  
    if ( argc > 0 ) {
	dbg_fnc* pFunc = find_command( argv[0] );
	if ( NULL != pFunc ) {
	    pFunc( argc, argv );
	} else {
	    dbprintf( DBP_DEBUGGER, "Error: unknown command '%s'\n", argv[0] );
	}
    }
}

static void dbg_help( int argc, char** argv )
{
    DbgCmd_s* psCmd;
  
    for ( psCmd = g_psCommands ; NULL != psCmd ; psCmd = psCmd->psNext ) {
	dbprintf( DBP_DEBUGGER, "%-30s - %s\n", psCmd->zName, psCmd->zDesc );
    }
}

static void dbg_echo( int argc, char** argv )
{
    int i;
  
    for ( i = 0 ; i < argc ; ++i ) {
	dbprintf( DBP_DEBUGGER, "%s\n", argv[i] );
    }
}

static void dbg_toggle_plain_text( int argc, char** argv )
{
    g_bPlainTextDebug = !g_bPlainTextDebug;
    dbprintf( DBP_DEBUGGER, "Plain-text debugging is no %s\n", (g_bPlainTextDebug) ? "enabled" : "disabled" );
}

static char* get_suff( float* pnSize )
{
    if ( *pnSize > 1024.0f*1024.0f ) {
	*pnSize /= 1024.0f*1024.0f;
	return( "Mb" );
    } else if ( *pnSize > 1024.0f ) {
	*pnSize /= 1024.0f;
	return( "Kb" );
    } else {
	return( "bytes" );
    }
}

static void dbg_dump_mem( int argc, char** argv )
{
    SwapInfo_s  sSwapInfo;
    float       vFreeRAM   = (float) (g_sSysBase.ex_nFreePageCount * PAGE_SIZE);
    float	vKMem      = (float) g_sSysBase.ex_nKernelMemSize;
    float       vKMemPages = (float) (g_sSysBase.ex_nKernelMemPages * PAGE_SIZE);
    float	vTotSwap;
    float	vFreeSwap;
  
    const char* pzRamSuff;
    const char* pzTSwpSuff;
    const char* pzFSwpSuff;
    const char* pzKMemSuff;
    const char* pzKMemPagesSuff;
  
    get_swap_info( &sSwapInfo );

    vTotSwap = (float)sSwapInfo.si_nTotSize;
    vFreeSwap= (float)sSwapInfo.si_nFreeSize;
  
    pzTSwpSuff = get_suff( &vTotSwap );
    pzFSwpSuff = get_suff( &vFreeSwap );
    pzKMemSuff = get_suff( &vKMem );
    pzKMemPagesSuff = get_suff( &vKMemPages );
  
    pzRamSuff = get_suff( &vFreeRAM );
    dbprintf( DBP_DEBUGGER, "Free RAM: %d%s Tot SWP: %d%s Free SWP: %d%s PGIN: %d PGOUT: %d\n",
	      (int)vFreeRAM, pzRamSuff, (int)vTotSwap, pzTSwpSuff, (int)vFreeSwap, pzFSwpSuff,
	      sSwapInfo.si_nPageIn, sSwapInfo.si_nPageOut );
    dbprintf( DBP_DEBUGGER, "Kernel MEM: %d%s Alloc KMem: %d%s\n", (int) vKMem, pzKMemSuff, (int)vKMemPages, pzKMemPagesSuff );
}

static void dbg_reboot( int argc, char** argv )
{
    dbprintf( DBP_DEBUGGER, "Rebooting...\n" );
    reboot();
}

static void dbg_reset( int argc, char** argv )
{
    dbprintf( DBP_DEBUGGER, "Reset...\n" );
    hard_reset();
}

static int debugger_thread( void* pData )
{
    static char pzBuffer[4096];
    int	nTotSize = 0;

    dbprintf( DBP_DEBUGGER, "dbg>" );
    for (;;)
    {
	int nSize;
    
	nSize = sys_debug_read( 1, &pzBuffer[nTotSize], 4096 - nTotSize );

	if ( nSize > 0 )
	{
	    int i;
      
	    for ( i = nTotSize ; i < nTotSize + nSize ; ++i )
	    {	
		if ( pzBuffer[i] == 0x08 )
		{
		    memmove( pzBuffer + i - 1, pzBuffer + i + 1, nTotSize + nSize - i - 1 );

		    i -= 2;
		    nSize -= 2;
		    continue;
		}
		if ( pzBuffer[i] == '\r' || pzBuffer[i] == '\n' )
		{
		    pzBuffer[i] = '\0';
	  
		    parse_command( pzBuffer );
		    dbprintf( DBP_DEBUGGER, "dbg>" );
	  
		    memmove( pzBuffer, pzBuffer + i + 1, nTotSize + nSize - i - 1 );
		    nTotSize -= i + 1;
		    i = 0;
		}
	    }
	    nTotSize += nSize;
	}
	snooze( 100000 );
    }
}

void set_debug_port_params( int nBaudRate, int nPort, bool bPlainTextDebug )
{
    uint32 nFlg;

    spinlock_cli( &g_sSPinLock, nFlg );

    if ( nBaudRate == 0 || nPort != g_nDebugPort ) {
	if ( g_nIRQHandle != -1 ) {
	    ser_out( UART_IER, 0 ); // receive irq disabled
	    release_irq( g_nPortIRQ, g_nIRQHandle );
	    g_nIRQHandle = -1;
	}
    }
    if ( nPort == 1 ) {
	g_nPortBase = 0x3f8;
	g_nPortIRQ  = 4;
    } else {
	g_nPortBase = 0x2f8;
	g_nPortIRQ  = 3;
    }
    
    if ( nBaudRate > 0 ) {
	uint nDivisor = 115200 / nBaudRate;

	if ( g_bInitialized && g_nIRQHandle == -1 ) {
	    g_nIRQHandle = request_irq( g_nPortIRQ, com2_irq, NULL, 0, "com2_device", NULL );
	    ser_out( UART_IER, UART_IER_RDI ); // receive irq enabled
	}
    
	ser_out( UART_LCR, UART_LCR_DLAB | UART_LCR_WLEN8 ); // Set UART_LCR_DLAB to enable baud rate divisors
  
	ser_out( UART_DLL, nDivisor & 0xff ); // Baud rate divisor LSB
	ser_out( UART_DLM, nDivisor >> 8  ); // Baud rate divisor MSB
 
	ser_out( UART_LCR, UART_LCR_WLEN8 ); // Clr UART_LCR_DLAB to disable baud rate divisors


	  // Enable FIFO, IRQ when 8 bytes received
	ser_out( UART_FCR, UART_FCR_ENABLE_FIFO | UART_FCR6_R_TRIGGER_24 );
	ser_out( UART_IER, 0 ); // disable irq

	ser_out( UART_MCR, UART_MCR_DTR | UART_MCR_RTS | UART_MCR_OUT1 | UART_MCR_OUT2 );

	  // Clear interrupt registers
	ser_in( UART_LSR );  // Line status (LSR)
	ser_in( UART_RX );
	ser_in( UART_IIR );  // Check interrupt type (IIR)
	ser_in( UART_MSR );  // Check modem status (MSR)
    }
    g_nDebugPort      = nPort;
    g_nDebugBaudRate  = nBaudRate;
    g_bPlainTextDebug = bPlainTextDebug;
    
    spinunlock_restore( &g_sSPinLock, nFlg );
}

static void dbg_port( int argc, char** argv )
{
    if ( argc == 1 ) {
	dbprintf( DBP_DEBUGGER, "Current debug serial port: %d\n", g_nDebugPort );
    } else if ( argc == 2 ) {
	int nPort = atol( argv[1] );
	if ( nPort == 1 || nPort == 2) {
	    dbprintf( DBP_DEBUGGER, "Set serial port to: %d\n", nPort );
	    if ( nPort != g_nDebugPort ) {
		set_debug_port_params( g_nDebugBaudRate, nPort, g_bPlainTextDebug );
	    } else {
		dbprintf( DBP_DEBUGGER, "Error: Invalid port number %d\n", nPort );
	    }
	}
    } else {
	dbprintf( DBP_DEBUGGER, "Invalid argument count\n" );
    }
}

static void dbg_speed( int argc, char** argv )
{
    if ( argc == 1 ) {
	dbprintf( DBP_DEBUGGER, "Current debug baudrate: %d\n", g_nDebugBaudRate );
    } else if ( argc == 2 ) {
	int nBaudRate = atol( argv[1] );
	dbprintf( DBP_DEBUGGER, "Set serial baudrate: %d\n", nBaudRate );
	if ( nBaudRate != g_nDebugBaudRate ) {
	    set_debug_port_params( nBaudRate, g_nDebugPort, g_bPlainTextDebug );
	}
    } else {
	dbprintf( DBP_DEBUGGER, "Invalid argument count\n" );
    }
}

void init_debugger( int nBaudRate, int nPort )
{
    thread_id hThread = spawn_kernel_thread( "debugger", debugger_thread, 200, 0, NULL );
    
    g_bInitialized = true;

    if ( g_nDebugBaudRate > 0  ) {
	g_nIRQHandle = request_irq( g_nPortIRQ, com2_irq, NULL, 0, "com2_device", NULL );
	ser_out( UART_IER, UART_IER_RDI ); // receive irq enabled
    }
    
  
    register_debug_cmd( "help", "print this list", dbg_help );
    register_debug_cmd( "dbport", "set/get the debug serial port", dbg_port );
    register_debug_cmd( "dbspeed", "set/get the debug serial baudrate", dbg_speed );
    register_debug_cmd( "mem", "print information about memory usage", dbg_dump_mem );
    register_debug_cmd( "toggle_plain", "Toggle between plain ascii and packet based serial output", dbg_toggle_plain_text );
    register_debug_cmd( "echo", "print all it's arguments to the debug terminal", dbg_echo );
    register_debug_cmd( "reboot", "Shut down the computer", dbg_reboot );
    register_debug_cmd( "reset", "Shut down the computer without flushing the disk-cache", dbg_reset );

    wakeup_thread( hThread, false );
}
