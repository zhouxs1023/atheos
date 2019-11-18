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

#ifndef	__F_MSGPORTS_H__
#define	__F_MSGPORTS_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <atheos/types.h>
#include <atheos/typedefs.h>




typedef	struct _MessageNode MessageNode_s;

struct _MessageNode
{
  MessageNode_s* mn_psNext;
  int		 mn_nSize;
  uint32	 mn_nCode;
};

	
typedef struct _MsgPort MsgPort_s;
struct _MsgPort
{
    char           mp_zName[OS_NAME_LENGTH];
    port_id	   mp_hPortID;	/* Index into the global port array	*/
    uint32	   mp_nFlags;	/* see definition below			*/
    int		   mp_nMaxCount;
    int		   mp_nCount;
    MessageNode_s* mp_psFirstMsg;
    MessageNode_s* mp_psLastMsg;
    sem_id	   mp_hSyncSema;
    MsgPort_s*	   mp_psNext;
    MsgPort_s*	   mp_psPrev;
    proc_id	   mp_hOwner;
};

  
MsgPort_s*	get_port_from_handle( port_id hPort );
void 		exit_free_ports( Process_s* psProc );
    


#ifdef __cplusplus
}
#endif


#endif	/* __F_MSGPORTS_H__ */
    
