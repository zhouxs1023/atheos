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

#ifndef __F_KDEBUG_H__
#define __F_KDEBUG_H__

#include <atheos/types.h>
#include <atheos/threads.h>

#ifdef __cplusplus
extern "C" {
#if 0  
} /*make emacs indention work */
#endif
#endif

#define DB_PACKET_SIZE 128
#define DB_PORT_COUNT  16

#define DBP_PRINTK   0
#define DBP_DEBUGGER 2

typedef void dbg_fnc( int argc, char** argv );

#ifndef __KERNEL__

int debug_write( int nPort, const char* pBuffer, int nSize );
int debug_read( int nPort, char* pBuffer, int nSize );
int  dbprintf( const char* pzFmt, ... ) __attribute__ ((format (printf, 1, 2)));

#else /* __KERNEL__ */

void trace_stack( uint32 nEIP, uint32* pStack );

int  sys_debug_write( int nPort, const char* pBuffer, int nSize );
int  sys_debug_read( int nPort, char* pBuffer, int nSize );
void debug_write( const char* pBuffer, int nSize );
int  dbprintf( int nPort, const char* pzFmt, ... ) __attribute__ ((format (printf, 2, 3)));
int  printk( const char* pzFmt, ... ) __attribute__ ((format (printf, 1, 2)));
void panic( const char* pzFmt, ... ) __attribute__ ((format (printf, 1, 2)));
void set_debug_port_params( int nBaudRate, int nPort, bool bPlainTextDebug );
void init_debugger( int nBaudRate, int nSerialPort );
int  register_debug_cmd( const char* pzName, const char* pzDesc, dbg_fnc* pFunc );

void dbcon_write( const char* pData, int nSize );
void dbcon_clear();


#endif /* __KERNEL__ */


#ifdef __cplusplus
}
#endif

#endif /* __F_KDEBUG_H__ */
