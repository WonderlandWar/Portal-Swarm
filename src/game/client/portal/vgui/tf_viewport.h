//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef TF_VIEWPORT_H
#define TF_VIEWPORT_H


#include "tf_shareddefs.h"
#include "baseviewport.h"
#include "clientmode_tf.h"


using namespace vgui;

namespace vgui 
{
	class Panel;
	class Label;
	class CBitmapImagePanel;
}

//-----------------------------------------------------------------------------
// Purpose: this is the viewport that contains all the hud elements
//-----------------------------------------------------------------------------
class CHudViewport : public CBaseViewport
{
private:
	DECLARE_CLASS_SIMPLE( CHudViewport, CBaseViewport );

protected:
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme )
	{
		BaseClass::ApplySchemeSettings( pScheme );

		GetHud().InitColors( pScheme );

		SetPaintBackgroundEnabled( false );
	}

	virtual void CreateDefaultPanels( void ) { /* don't create any panels yet*/ };
};

//==============================================================================
class TFViewport : public CHudViewport
{

public:
	DECLARE_CLASS_SIMPLE( TFViewport, CHudViewport );

public:

	IViewPortPanel* CreatePanelByName(const char *szPanelName);
	void CreateDefaultPanels( void );

	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void Start( IGameUIFuncs *pGameUIFuncs, IGameEventManager2 * pGameEventManager );
		
	int GetDeathMessageStartHeight( void );

	virtual void OnScreenSizeChanged( int iOldWide, int iOldTall );

private:
	void CenterWindow( vgui::Frame *win );

};


#endif // TF_Viewport_H
