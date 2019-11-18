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

#ifndef __F_POSIX_IOCTL_H__
#define __F_POSIX_IOCTL_H__

#ifdef __cplusplus
extern "C"{
#endif

#define _IOCTL_CMD_MASK		0x000003ff
#define _IOCTL_CMD_SHIFT	0
#define _IOCTL_MOD_MASK		0x0000fc00
#define _IOCTL_MOD_SHIFT	10
#define _IOCTL_SIZE_MASK	0x3fff0000
#define _IOCTL_SIZE_SHIFT	16
#define _IOCTL_DIR_MASK		0xc0000000
#define _IOCTL_DIR_SHIFT	30

#define IOCTL_DIR_NONE	0x00
#define IOCTL_DIR_READ	0x01
#define IOCTL_DIR_WRITE	0x02


#define IOCTL_MOD_MISC		0x01	/* Unspecified	*/
#define IOCTL_MOD_KERNEL	0x02	/* Internal kernel commands */
#define IOCTL_MOD_BLOCK		0x03	/* Block-devices */
#define IOCTL_MOD_NET_SOCK	0x04	/* Generic network commands */
#define IOCTL_MOD_NET_IF	0x05	/* Network interface drivers */
#define IOCTL_MOD_NET_NIC	0x06	/* Network NIC drivers */
#define IOCTL_MOD_NET_PROTO	0x07	/* Network protocol drivers */
#define	IOCTL_MOD_TERM		0x15	/* Terminal control (PTY's, etc) */

#define MK_IOCTL( mod, cmd, direction, size ) ((((mod)<<_IOCTL_MOD_SHIFT)&_IOCTL_MOD_MASK) | \
					       (((cmd)<<_IOCTL_CMD_SHIFT)&_IOCTL_CMD_MASK) | \
					       (((direction)<<_IOCTL_DIR_SHIFT)&_IOCTL_DIR_MASK) | \
					       (((size)<<_IOCTL_SIZE_SHIFT)&_IOCTL_SIZE_MASK))

#define MK_IOCTLN( mod, cmd, size )	MK_IOCTL( mod, cmd, IOCTL_DIR_NONE, size )
#define MK_IOCTLR( mod, cmd, size )	MK_IOCTL( mod, cmd, IOCTL_DIR_READ, size )
#define MK_IOCTLW( mod, cmd, size )	MK_IOCTL( mod, cmd, IOCTL_DIR_WRITE, size )
#define MK_IOCTLRW( mod, cmd, size )	MK_IOCTL( mod, cmd, IOCTL_DIR_READ|IOCTL_DIR_WRITE, size )

#define IOCTL_MOD( num )  (((num)&_IOCTL_MOD_MASK)>>_IOCTL_MOD_SHIFT)
#define IOCTL_CMD( num )  (((num)&_IOCTL_CMD_MASK)>>_IOCTL_CMD_SHIFT)
#define IOCTL_DIR( num )  (((num)&_IOCTL_DIR_MASK)>>_IOCTL_DIR_SHIFT)
#define IOCTL_SIZE( num ) (((num)&_IOCTL_SIZE_MASK)>>_IOCTL_SIZE_SHIFT)



#ifdef __KERNEL__
int ioctl( int fd, int cmd, ...);
#endif
		
#ifdef __cplusplus
}
#endif
		
		
#endif /* __F_POSIX_IOCTL_H__ */
