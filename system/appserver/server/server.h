#ifndef	INTERFACE_SERVER_HPP
#define	INTERFACE_SERVER_HPP

/*
 *  The AtheOS application server
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
#include <atheos/time.h>
#include <atheos/mouse.h>

#include <gui/region.h>
#include <gui/gfxtypes.h>
#include <gui/bitmap.h>
#include <util/array.h>
#include <util/messagequeue.h>

#include "ddriver.h"
#include "windowdecorator.h"

typedef	struct TextFont	TextFont_s;

using namespace os;

class SrvWindow;
class Layer;
class AppServer;
class SFontInstance;
class DisplayDriver;
class SrvBitmap;

namespace os {
    class Locker;
    class Message;
}

extern ::Layer* g_pcTopView;

extern DisplayDriver*	g_pcDispDrv;

extern Array<Layer>*	g_pcLayers;
extern Locker*		g_pcWinListLock;

void config_changed();

class AppServer
{
public:
    AppServer();
    ~AppServer();

    static AppServer*	GetInstance();
    os::WindowDecorator*  CreateWindowDecorator( Layer* pcView, uint32 nFlags );
  
    void		Run( void );
    DisplayDriver*	GetDisplayDriver( int nIndex )	{ return( m_pcDispDriver ); }

    void 		SendKeyCode( int nKeyCode, int nQualifiers );

    FontNode*		GetWindowTitleFont() const { return( m_pcWindowTitleFont ); }
    FontNode*		GetToolWindowTitleFont() const { return( m_pcToolWindowTitleFont ); }
    
    void	R_ClientDied( thread_id hThread );

    void	ResetEventTime();
    bigtime_t	GetIdleTime() const { return( get_system_time() - m_nLastEvenTime ); }
    int  LoadWindowDecorator( const std::string& cPath );
    
    port_id m_hWndReplyPort;
private:
    int	LoadDecorator( const std::string& cPath, op_create_decorator** ppfCreate );

    void	DispatchMessage( Message* pcReq );
    
    static AppServer*	 s_pcInstance;
    port_id		 m_hRequestPort;
    DisplayDriver*	 m_pcDispDriver;
    bigtime_t		 m_nLastEvenTime;
    int			 m_hCurrentDecorator;
    op_create_decorator* m_pfDecoratorCreator;

    FontNode*		m_pcWindowTitleFont;
    FontNode*		m_pcToolWindowTitleFont;
};

#endif	/*	INTERFACE_SERVER_HPP	*/
