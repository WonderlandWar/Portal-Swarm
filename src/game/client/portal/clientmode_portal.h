//========= Copyright � 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef PORTAL_CLIENTMODE_H
#define PORTAL_CLIENTMODE_H
#ifdef _WIN32
#pragma once
#endif

#include "clientmode_shared.h"
#include <vgui_controls/EditablePanel.h>
#include <vgui/Cursor.h>

class CHudViewport;

namespace vgui
{
	typedef unsigned long HScheme;
}

class ClientModePortalNormal : public ClientModeShared 
{
DECLARE_CLASS( ClientModePortalNormal, ClientModeShared );

private:

// IClientMode overrides.
public:

					ClientModePortalNormal();
	virtual			~ClientModePortalNormal();

	virtual void	Init();
	virtual void	InitViewport();
	virtual void	LevelInit( const char *newmap );
	virtual void	DoPostScreenSpaceEffects( const CViewSetup *pSetup );
	
	virtual void	SetBlurFade( float scale );
	virtual float	GetBlurFade( void ) { return m_BlurFadeScale; }
	
private:

	float m_BlurFadeScale;
	//	void	UpdateSpectatorMode( void );

};


extern IClientMode *GetClientModeNormal();
extern ClientModePortalNormal* GetClientModePortalNormal();


#endif // PORTAL_CLIENTMODE_H
