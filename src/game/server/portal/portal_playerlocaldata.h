//========= Copyright  1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef PORTAL_PLAYERLOCALDATA_H
#define PORTAL_PLAYERLOCALDATA_H
#ifdef _WIN32
#pragma once
#endif

#include "networkvar.h"

#include "portal_player_shared.h"   // PortalPlayerStatistics_t define
#include "portal_shareddefs.h"      // USE_SLOWTIME define

//-----------------------------------------------------------------------------
// Purpose: Player specific data for Portal ( sent only to local player, too )
//-----------------------------------------------------------------------------
class CPortalPlayerLocalData
{
public:
    // Save/restore
    DECLARE_SIMPLE_DATADESC();
	DECLARE_CLASS_NOBASE( CPortalPlayerLocalData );
	DECLARE_EMBEDDED_NETWORKVAR();

    CPortalPlayerLocalData();
	
	CNetworkVar( InAirState, m_InAirState );
	CNetworkVar( PaintPowerType, m_PaintedPowerType );

	CNetworkVar( bool, m_bJumpedThisFrame );
	CNetworkVar( bool, m_bBouncedThisFrame );
	CNetworkVar( bool, m_bDuckedInAir );
	CNetworkVar( bool, m_bPreventedCrouchJumpThisFrame );

	bool m_bAttemptHullResize;

	CNetworkVar( float, m_flAirControlSupressionTime );
	CNetworkVar( float, m_flAirInputScale );
	CNetworkVar( float, m_flCurrentStickTime );
	CNetworkVar( float, m_fBouncedTime );

	CountdownTimer m_PaintedPowerTimer;

	CachedPaintPowerChoiceResult m_CachedPaintPowerChoiceResults[PAINT_POWER_TYPE_COUNT];
	
	CNetworkVector( m_vPreUpdateVelocity );
};

EXTERN_SEND_TABLE(DT_PortalLocal);


#endif // PORTAL_PLAYERLOCALDATA_H
