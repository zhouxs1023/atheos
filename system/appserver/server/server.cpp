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

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <stdexcept>

#include <atheos/types.h>
#include <atheos/threads.h>
#include <atheos/kernel.h>
#include <atheos/time.h>
#include <atheos/image.h>
#include <atheos/msgport.h>

#include <macros.h>

#include <gui/guidefines.h>
#include <util/locker.h>
#include <appserver/protocol.h>

#include "ddriver.h"
#include "server.h"
#include "swindow.h"
#include "layer.h"
#include "sfont.h"
#include "keyboard.h"
#include "sapplication.h"
#include "winselect.h"
#include "bitmap.h"
#include "clipboard.h"
#include "inputnode.h"
#include "config.h"
#include "sprite.h"
#include "defaultdecorator.h"

void  ScreenShot();


static WinSelect* g_pcWinSelector = NULL;

AppServer* AppServer::s_pcInstance = NULL;

Array<Layer>*	   g_pcLayers;
Array<BitmapNode>* g_pcBitmaps;


SrvApplication* g_pcFirstApp = NULL;
Layer*	  	g_pcTopView = NULL;
DisplayDriver*  g_pcDispDrv = NULL;


int AppServer::LoadDecorator( const std::string& cPath, op_create_decorator** ppfCreate )
{
    int	nPlugin;
    op_create_decorator* pfCreate = NULL;
  
    nPlugin = load_library( cPath.c_str(), 0 );
    if ( nPlugin < 0 ) {
	dbprintf( "Error: Failed to load window decorator %s\n", cPath.c_str() );
	return( -1 );
    }
    op_get_decorator_version* pfGetVersion;

    int nError;
    nError = get_symbol_address( nPlugin, "get_api_version", -1, (void**) &pfGetVersion );
    if ( nError < 0 ) {
	dbprintf( "Error: window decorator '%s' does not export get_api_version()\n", cPath.c_str() );
	unload_library( nPlugin );
	return( -1 );
    }
    int nVersion = pfGetVersion();
    if ( nVersion != WNDDECORATOR_APIVERSION ) {
	if ( nVersion < WNDDECORATOR_APIVERSION ) {
	    dbprintf( "Error: window decorator '%s' to old. VER=%d, CUR VER=%d\n", cPath.c_str(), nVersion, WNDDECORATOR_APIVERSION );
	} else {
	    dbprintf( "Error: window decorator '%s' to new. VER=%d, CUR VER=%d\n", cPath.c_str(), nVersion, WNDDECORATOR_APIVERSION );
	}
	unload_library( nPlugin );
	return( -1 );
    }
  
    nError = get_symbol_address( nPlugin, "create_decorator", -1, (void**) &pfCreate );
    if ( nError < 0 ) {
	dbprintf( "Error: window decorator '%s' does not export create_decorator()\n", cPath.c_str() );
	unload_library( nPlugin );
	return( -1 );
    }
    *ppfCreate = pfCreate;
    return( nPlugin );
}

int AppServer::LoadWindowDecorator( const std::string& cPath )
{
    op_create_decorator* pfCreate = NULL;
    int nLib;
    nLib = LoadDecorator( cPath, &pfCreate );
    if ( nLib >= 0 ) {
	m_pfDecoratorCreator = pfCreate;
	int nOldPlugin = m_hCurrentDecorator;
	m_hCurrentDecorator = nLib;
	SrvApplication::ReplaceDecorators();
	unload_library( nOldPlugin );
	return( 0 );
    } else {
	return( -1 );
    }
}

WindowDecorator* AppServer::CreateWindowDecorator( Layer* pcView, uint32 nFlags )
{
    if ( m_pfDecoratorCreator != NULL ) {
	return( m_pfDecoratorCreator( pcView, nFlags ) );
    } else {
	return( new DefaultDecorator( pcView, nFlags ) );
    }
}

//----------------------------------------------------------------------------
// NAME:
// DESC:
// NOTE:
// SEE ALSO:
//----------------------------------------------------------------------------

AppServer::AppServer()
{
    s_pcInstance = this;
    m_pfDecoratorCreator = NULL;
    printf( "Load default fonts\n" );

    m_pcWindowTitleFont = new FontNode;
    m_pcToolWindowTitleFont = new FontNode;
    
    const font_properties* psProp;
    
    psProp = AppserverConfig::GetInstance()->GetFontConfig( DEFAULT_FONT_WINDOW );
    m_pcWindowTitleFont->SetProperties( *psProp );
    
    psProp = AppserverConfig::GetInstance()->GetFontConfig( DEFAULT_FONT_TOOL_WINDOW );
    m_pcToolWindowTitleFont->SetProperties( *psProp );
    
    m_hCurrentDecorator = LoadDecorator( AppserverConfig::GetInstance()->GetWindowDecoratorPath().c_str(), &m_pfDecoratorCreator );
}

AppServer* AppServer::GetInstance()
{
    return( s_pcInstance );
}

void AppServer::ResetEventTime()
{
    m_nLastEvenTime = get_system_time();
}

//----------------------------------------------------------------------------
// NAME:
// DESC:
// NOTE:
// SEE ALSO:
//----------------------------------------------------------------------------

void AppServer::R_ClientDied( thread_id hClient )
{
    SrvApplication* pcApp = SrvApplication::FindApp( hClient );

    if ( NULL != pcApp ) {
	send_msg( pcApp->GetReqPort(), M_QUIT, NULL, 0 );
    }
}

//----------------------------------------------------------------------------
// NAME:
// DESC:
// NOTE:
// SEE ALSO:
//----------------------------------------------------------------------------

void AppServer::SendKeyCode( int nKeyCode, int nQual )
{
    ResetEventTime();
    if ( 0x34 == nKeyCode ) // DEL
    {
	if ( (nQual & QUAL_ALT) && (nQual & QUAL_CTRL) ) {
	    reboot();
	    return;
	}
    }
#ifndef __NO_SCREENSHOT_SUPPORT__    
    if ( nKeyCode == 0x0e ) { // Print-screen
	ScreenShot();
	return;
    }
#endif    
    if ( (nQual & QUAL_LALT) && nKeyCode == 0x28 ) { // ALT-W
	g_cLayerGate.Close();
	SrvWindow* pcWnd = get_active_window(false);
	if ( pcWnd != NULL ) {
	    Message cMsg( M_QUIT );
	    pcWnd->PostUsrMessage( &cMsg );
	}
	g_cLayerGate.Open();
	return;
    }
    if ( (nQual & QUAL_LALT) && nKeyCode == 0x27 ) { // ALT-Q
	g_cLayerGate.Close();
	SrvApplication* pcApp = get_active_app();
	if ( pcApp != NULL ) {
	    Message cMsg( M_QUIT );
	    pcApp->PostUsrMessage( &cMsg );
	}
	g_cLayerGate.Open();
	return;
    }

    if ( 0x1e == nKeyCode ) // BACKSPACE
    {
	if ( (nQual & QUAL_ALT) && (nQual & QUAL_CTRL) ) {
	    FILE* hFile;
	    dbprintf( "Load configuration\n" );
	    hFile = fopen( "/system/config/appserver", "r" );

	    if ( hFile != NULL ) {
		AppserverConfig::GetInstance()->LoadConfig( hFile, true );
		fclose( hFile );
	    } else {
		dbprintf( "Error: failed to open appserver configuration file: /system/config/appserver" );
	    }
	    return;
	}
    }

    if ( g_pcWinSelector != NULL )
    {
	g_cLayerGate.Close();
	if ( (nQual & QUAL_LALT) == 0 || nKeyCode == 0x01 || nKeyCode == 17 ) {
	    if ( (nKeyCode & 0x7f) == 0x5d ) { // L-ALT up
		g_pcWinSelector->UpdateWinList( true, true );
	    } else if ( nKeyCode == 17 ) {
		g_pcWinSelector->UpdateWinList( false, true );
	    } else {
		g_pcWinSelector->UpdateWinList( false, false );
	    }
	    delete g_pcWinSelector;
	    g_pcWinSelector = NULL;
	    g_pcTopView->UpdateRegions();
	    SrvWindow::HandleMouseTransaction();
	} else if ( nKeyCode == 0x26 ) {
	    g_pcWinSelector->Step( (nQual & QUAL_SHIFT) == 0 );
	}
	g_cLayerGate.Open();
	return;
    }
    if ( nQual & QUAL_LALT )
    {
	if ( nKeyCode >= 0x02 && nKeyCode <= 0x0d ) // F1-F12
	{
	    g_cLayerGate.Close();

	    int nDesktop = nKeyCode - 0x02;
	    SrvWindow* pcActiveWnd = get_active_window(false);
	    SrvSprite::Hide();
	    set_desktop( nDesktop );
	    if ( pcActiveWnd != NULL && (InputNode::GetMouseButtons() & 0x01 ) )
	    {
		uint32 nNewMask = 1 << nDesktop;

		if ( InputNode::GetMouseButtons() & 0x02 ) {
		    nNewMask |= pcActiveWnd->GetDesktopMask();
		}
		pcActiveWnd->SetDesktopMask( nNewMask );
		set_active_window( pcActiveWnd );
	    }
	    g_pcTopView->UpdateRegions();
	    SrvSprite::Unhide();
	    g_cLayerGate.Open();
	    return;
	}
	if ( nKeyCode == 17 ) {
	    g_cLayerGate.Close();
	    SrvWindow* pcActiveWnd = get_active_window(false);
	    SrvSprite::Hide();
	    if ( toggle_desktops() ) {
		if ( pcActiveWnd != NULL && (InputNode::GetMouseButtons() & 0x01 ) )
		{
		    uint32 nNewMask = 1 << get_active_desktop();

		    if ( InputNode::GetMouseButtons() & 0x02 ) {
			nNewMask |= pcActiveWnd->GetDesktopMask();
		    }
		    pcActiveWnd->SetDesktopMask( nNewMask );
		    set_active_window( pcActiveWnd );
		}
		g_pcTopView->UpdateRegions();
	    }
	    SrvSprite::Unhide();
	    g_cLayerGate.Open();
	    return;
	}
	if ( nKeyCode == 0x26 ) // ALT
	{
	    g_cLayerGate.Close();
      
	    if ( g_pcTopView->GetTopChild() != NULL ) {
		g_pcWinSelector = new WinSelect();
		g_pcTopView->UpdateRegions();
		SrvWindow::HandleMouseTransaction();
	    }
	    g_cLayerGate.Open();
	    return;
	}
    }
  
    if ( !( (nQual & QUAL_ALT)  && (nQual & QUAL_CTRL) && (0x1e == nKeyCode || 0x9e == nKeyCode)) ) {  // BACKSPACE
	char zConvString[8];
	char zRawString[8];

	convert_key_code( zConvString, nKeyCode & 0x7f, nQual );
	convert_key_code( zRawString, nKeyCode & 0x7f, 0 );
      
	SrvWindow::SendKbdEvent( nKeyCode, nQual, zConvString, zRawString );
    }
}

void AppServer::DispatchMessage( Message* pcReq )
{
    switch( pcReq->GetCode() )
    {
	case DR_CREATE_APP:
	{
	    const char* pzName;
	    proc_id 	hOwner;
	    port_id	hEventPort;

	    pcReq->FindInt( "process_id", &hOwner );
	    pcReq->FindInt( "event_port", &hEventPort );
	    pcReq->FindString( "app_name", &pzName );

	    SrvApplication* pcApp;
	    try {
		pcApp = new SrvApplication( pzName, hOwner, hEventPort );
	    } catch(...) {
		Message cReply;
		cReply.AddInt32( "app_cmd_port", -1 );
		pcReq->SendReply( &cReply );
		break;
	    }
	    
	    Message cReply;
	    cReply.AddInt32( "app_cmd_port", pcApp->GetReqPort() );

	    cReply.AddFloat( "cfg_shine_tint", 0.9f );
	    cReply.AddFloat( "cfg_shadow_tint", 0.9f );

	    for ( int i = 0 ; i < COL_COUNT ; ++i ) {
		cReply.AddColor32( "cfg_colors", get_default_color( static_cast<default_color_t>(i) ) );
	    }
	    pcReq->SendReply( &cReply );
	    break;
	}
	case DR_SET_COLOR_CONFIG:
	{
	    for ( int i = 0 ; i < COL_COUNT ; ++i ) {
		Color32_s sColor;
		if ( pcReq->FindColor32( "_colors", &sColor, i ) != 0 ) {
		    break;
		}
		AppserverConfig::GetInstance()->SetDefaultColor( static_cast<default_color_t>(i), sColor );
	    }
	    SrvApplication::NotifyColorCfgChanged();
	    break;
	}
	case DR_GET_KEYBOARD_CFG:
	{
	    Message cReply;
	    cReply.AddInt32( "delay", AppserverConfig::GetInstance()->GetKeyDelay() );
	    cReply.AddInt32( "repeat", AppserverConfig::GetInstance()->GetKeyRepeat() );
	    const char* pzKeymap = strrchr( AppserverConfig::GetInstance()->GetKeymapPath().c_str(), '/' );
	    if ( pzKeymap == NULL ) {
		pzKeymap = AppserverConfig::GetInstance()->GetKeymapPath().c_str();
	    } else {
		pzKeymap++;
	    }
	    cReply.AddString( "keymap", pzKeymap );
	    pcReq->SendReply( &cReply );
	    break;
	}
	case DR_SET_KEYBOARD_CFG:
	{
	    const char* pzKeyMap;
	    char zKeyMapPath[1024];
	    int32	nKeyDelay;
	    int32	nKeyRepeat;
	    
	    if ( pcReq->FindInt( "delay", &nKeyDelay ) == 0 ) {
		AppserverConfig::GetInstance()->SetKeyDelay( nKeyDelay );
	    }
	    if ( pcReq->FindInt( "repeat", &nKeyRepeat ) == 0 ) {
		AppserverConfig::GetInstance()->SetKeyRepeat( nKeyRepeat );
	    }

	    if ( pcReq->FindString( "keymap", &pzKeyMap ) == 0 ) {
		strcpy( zKeyMapPath, "/system/keymaps/" );
		strncat( zKeyMapPath, pzKeyMap, 1000 );

		AppserverConfig::GetInstance()->SetKeymapPath( zKeyMapPath );
	    }
	    break;
	}
	case DR_RESCAN_FONTS:
	{
	    Message cReply;
	    FontServer::GetInstance()->ScanDirectory( "/system/fonts/" );
	    cReply.AddBool( "changed", true );
	    pcReq->SendReply( &cReply );
	    break;
	}
	case DR_GET_DEFAULT_FONT_NAMES:
	{
	    Message cReply;
	    AppserverConfig::GetInstance()->GetDefaultFontNames( &cReply );
	    pcReq->SendReply( &cReply );
	    break;
	}
	case DR_GET_DEFAULT_FONT:
	{
	    const char* pzConfigName;
	    Message cReply;
	    
	    if ( pcReq->FindString( "config_name", &pzConfigName ) != 0 ) {
		cReply.AddInt32( "error", -EINVAL );
		pcReq->SendReply( &cReply );
		break;
	    }
	    const font_properties* psProps = AppserverConfig::GetInstance()->GetFontConfig( pzConfigName );
	    if ( psProps == NULL ) {
		cReply.AddInt32( "error", -EINVAL );
		pcReq->SendReply( &cReply );
		break;
	    }
	    cReply.AddString( "family", psProps->m_cFamily );
	    cReply.AddString( "style", psProps->m_cStyle );
	    cReply.AddFloat( "size", psProps->m_vSize );
	    cReply.AddFloat( "shear", psProps->m_vShear );
	    cReply.AddFloat( "rotation", psProps->m_vRotation );
	    cReply.AddInt32( "error", 0 );
	    pcReq->SendReply( &cReply );
	    break;
	}
	case DR_SET_DEFAULT_FONT:
	case DR_ADD_DEFAULT_FONT:
	{
	    const char* pzConfigName;
	    Message cReply;
	    
	    if ( pcReq->FindString( "config_name", &pzConfigName ) != 0 ) {
		cReply.AddInt32( "error", -EINVAL );
		pcReq->SendReply( &cReply );
		break;
	    }
	    font_properties sProps;
	    pcReq->FindString( "family", &sProps.m_cFamily );
	    pcReq->FindString( "style", &sProps.m_cStyle );
	    pcReq->FindFloat( "size", &sProps.m_vSize );
	    pcReq->FindFloat( "shear", &sProps.m_vShear );
	    pcReq->FindFloat( "rotation", &sProps.m_vRotation );

	    if ( pcReq->GetCode() == DR_SET_DEFAULT_FONT ) {
		cReply.AddInt32( "error", AppserverConfig::GetInstance()->SetFontConfig( pzConfigName, sProps ) );
	    } else {
		cReply.AddInt32( "error", AppserverConfig::GetInstance()->AddFontConfig( pzConfigName, sProps ) );
	    }
	    if ( strcmp( pzConfigName, DEFAULT_FONT_WINDOW ) == 0 ) {
		m_pcWindowTitleFont->SetProperties( sProps );
		SrvApplication::NotifyWindowFontChanged( false );
	    }
    
	    if ( strcmp( pzConfigName, DEFAULT_FONT_TOOL_WINDOW ) == 0 ) {
		m_pcToolWindowTitleFont->SetProperties( sProps );
		SrvApplication::NotifyWindowFontChanged( true );
	    }
	    
	    pcReq->SendReply( &cReply );
	    break;
	}
	case DR_SET_APPSERVER_CONFIG:
	{
	    AppserverConfig::GetInstance()->SetConfig( pcReq );
	    break;
	}
	case DR_GET_APPSERVER_CONFIG:
	{
	    Message cReply;
	    AppserverConfig::GetInstance()->GetConfig( &cReply );
	    pcReq->SendReply( &cReply );
	    break;
	}
	
    }
}

//----------------------------------------------------------------------------
// NAME:
// DESC:
// NOTE:
// SEE ALSO:
//----------------------------------------------------------------------------

void AppServer::Run( void )
{
    enum	{ e_MessageSize = 1024*64 };
    uint8* pBuffer = new uint8[ e_MessageSize ];

    set_thread_priority( get_thread_id( NULL ), DISPLAY_PRIORITY );
	
    m_hRequestPort = create_port( "gui_server_cmd", DEFAULT_PORT_SIZE );


    init_desktops();
  
    InitInputSystem();

    set_app_server_port( m_hRequestPort );
  
    for(;;)
    {
	uint32 nCode;

	if ( get_msg_x( m_hRequestPort, &nCode, pBuffer, e_MessageSize, 1000000 ) >= 0 )
	{
	    if ( AppserverConfig::GetInstance()->IsDirty() ) {
		static bigtime_t nLastSaved = 0;
		bigtime_t nCurTime = get_system_time();
		if ( nCurTime > nLastSaved + 1000000 ) {
		    AppserverConfig::GetInstance()->SaveConfig();
		    nLastSaved = nCurTime;
		}
	    }
	    switch( nCode )
	    {
		case -1:
		    R_ClientDied( ((DR_ThreadDied_s*)pBuffer)->hThread );
		    break;
		case DR_CREATE_APP:
		case DR_SET_COLOR_CONFIG:
		case DR_GET_KEYBOARD_CFG:
		case DR_SET_KEYBOARD_CFG:
		case DR_RESCAN_FONTS:
		case DR_GET_DEFAULT_FONT_NAMES:
		case DR_GET_DEFAULT_FONT:
		case DR_SET_DEFAULT_FONT:
		case DR_ADD_DEFAULT_FONT:
		case DR_SET_APPSERVER_CONFIG:
		case DR_GET_APPSERVER_CONFIG:
		{
		    try {
			Message cReq( pBuffer );
			DispatchMessage( &cReq );
		    } catch(...) {
			dbprintf( "Error: Catched exception while handling request %ld\n", nCode );
		    }
		    break;
		}
		case DR_SET_CLIPBOARD_DATA:
		{
		    DR_SetClipboardData_s* psReq = (DR_SetClipboardData_s*) pBuffer;
		    SrvClipboard::SetData( psReq->m_zName, psReq->m_anBuffer, psReq->m_nTotalSize );
		    break;
		}
		case DR_GET_CLIPBOARD_DATA:
		{
		    DR_GetClipboardData_s* psReq = (DR_GetClipboardData_s*) pBuffer;
		    DR_GetClipboardDataReply_s sReply;
		    int	 nSize;
		    uint8* pData = SrvClipboard::GetData( psReq->m_zName, &nSize );


		    if ( pData == NULL || nSize == 0 ) {
			sReply.m_nTotalSize = 0;
			sReply.m_nFragmentSize = 0;
			send_msg( psReq->m_hReply, 0, &sReply, sizeof(sReply) - CLIPBOARD_FRAGMENT_SIZE );
		    } else {
			sReply.m_nTotalSize = nSize;
			int nOffset = 0;
	    
			while( nSize > 0 ) {
			    int nCurSize = min( CLIPBOARD_FRAGMENT_SIZE, nSize );
			    memcpy( sReply.m_anBuffer, pData + nOffset, nCurSize );
			    sReply.m_nFragmentSize = nCurSize;
			    send_msg( psReq->m_hReply, 0, &sReply, sizeof(sReply) - CLIPBOARD_FRAGMENT_SIZE + nCurSize );
			    nSize   -= nCurSize;
			    nOffset += nCurSize;
			}
		    }
		    break;
		}
		case DR_SET_WINDOW_DECORATOR:
		{
		    const DR_SetWindowDecorator_s* psReq = reinterpret_cast<const DR_SetWindowDecorator_s*>(pBuffer);
		    AppserverConfig::GetInstance()->SetWindowDecoratorPath( psReq->m_zDecoratorPath );
		    break;
		}
		default:
		    dbprintf( "WARNING : AppServer::Run() Unknown command %ld\n", nCode );
		    break;
	    }
	}
    }
}

//----------------------------------------------------------------------------
// NAME:
// DESC:
// NOTE:
// SEE ALSO:
//----------------------------------------------------------------------------

int main( int argc, char** argv )
{
    dbprintf( "Appserver Alive %d\n", get_thread_id( NULL ) );

    signal( SIGINT, SIG_IGN );
    signal( SIGQUIT, SIG_IGN );
    signal( SIGTERM, SIG_IGN );
	
    g_pcBitmaps	= new Array<BitmapNode>;
    g_pcLayers	= new Array<Layer>;

    AppserverConfig* pcConfig = new AppserverConfig();
    screen_mode sMode;
    

    sMode.m_nWidth	  = 640;
    sMode.m_nHeight	  = 480;
    sMode.m_nBytesPerLine = 1280;
    sMode.m_eColorSpace   = CS_RGB16;
    sMode.m_vRefreshRate  = 60.0f;
    sMode.m_vHPos	  = 80;
    sMode.m_vVPos	  = 50;
    sMode.m_vHSize	  = 70;
    sMode.m_vVSize	  = 80;
	
    for ( int i = 0 ; i < 32 ; ++i ) {
	set_desktop_config( i, sMode, "" );
    }
  
    FILE* hFile;
    dbprintf( "Load configuration\n" );
    hFile = fopen( "/system/config/appserver", "r" );

    g_sKeymap = g_sDefaultKeyMap;
    if ( hFile != NULL ) {
	pcConfig->LoadConfig( hFile, false );
	fclose( hFile );
    } else {
	dbprintf( "Error: failed to open appserver configuration file: /system/config/appserver" );
    }
    dbprintf( "Start keybord thread\n" );
  
    InitKeyboard();

    AppServer* pcDevice = new AppServer();
    pcDevice->Run();
    dbprintf( "WARNING : layers.device failed to initiate itself!!!\n" );
    return( 0 );
}

