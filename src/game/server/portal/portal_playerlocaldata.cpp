//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: (IDA Pro + 2014 dev leak PDB = this file made from scratch)
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "portal_playerlocaldata.h"
#include "portal_player.h"
#include "mathlib/mathlib.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

BEGIN_SEND_TABLE_NOBASE( CPortalPlayerLocalData, DT_PortalLocal ) // Purpose: Not done (Line 16)

SendPropFloat( SENDINFO( m_flAirControlSupressionTime ) ),
SendPropVector( SENDINFO( m_vPreUpdateVelocity ) ),

SendPropInt( SENDINFO( m_PaintedPowerType ) ),

// SendPropDataTable( SENDINFO( m_PaintedPowerTimer ) ), // FIXME!

SendPropFloat( SENDINFO( m_flCurrentStickTime ) ),
SendPropFloat( SENDINFO( m_flAirInputScale ) ),

SendPropInt( SENDINFO( m_InAirState ) ),

SendPropBool( SENDINFO( m_bJumpedThisFrame ) ),
SendPropBool( SENDINFO( m_bBouncedThisFrame ) ),
SendPropBool( SENDINFO( m_bDuckedInAir ) ),

SendPropFloat( SENDINFO( m_fBouncedTime ) ),

SendPropBool( SENDINFO( m_bPreventedCrouchJumpThisFrame ) ),

END_SEND_TABLE()

BEGIN_SIMPLE_DATADESC( CPortalPlayerLocalData )

DEFINE_FIELD( m_InAirState, FIELD_INTEGER ),
DEFINE_FIELD( m_PaintedPowerType, FIELD_INTEGER ),
DEFINE_FIELD( m_bAttemptHullResize, FIELD_BOOLEAN ),
DEFINE_FIELD( m_bJumpedThisFrame, FIELD_BOOLEAN ),
DEFINE_FIELD( m_bBouncedThisFrame, FIELD_BOOLEAN ),
DEFINE_FIELD( m_bDuckedInAir, FIELD_BOOLEAN ),
DEFINE_FIELD( m_bPreventedCrouchJumpThisFrame, FIELD_BOOLEAN ),

DEFINE_FIELD( m_flAirControlSupressionTime, FIELD_FLOAT ),
DEFINE_FIELD( m_flAirInputScale, FIELD_FLOAT ),
DEFINE_FIELD( m_flCurrentStickTime, FIELD_FLOAT ),
DEFINE_FIELD( m_fBouncedTime, FIELD_FLOAT ),

DEFINE_FIELD( m_vPreUpdateVelocity, FIELD_VECTOR ),
END_DATADESC()

const Vector DEFAULT_SURFACE_NORMAL( 0.0, 0.0, 1.0 );

CPortalPlayerLocalData::CPortalPlayerLocalData()
{
	m_bJumpedThisFrame = false;
	m_bBouncedThisFrame = false;
	m_bDuckedInAir = false;
	m_bPreventedCrouchJumpThisFrame = false;

	m_vPreUpdateVelocity = vec3_origin;

	m_PaintedPowerType = NO_POWER;
	m_InAirState = ON_GROUND;

	m_bAttemptHullResize = false;

	m_flAirControlSupressionTime = 0.0;
	m_flAirInputScale = 1.0;
	m_flCurrentStickTime = 0.5;
	m_fBouncedTime = 0.0;
}
