#include "cbase.h"
#include "c_portal_playerlocaldata.h"


BEGIN_RECV_TABLE_NOBASE( CPortalPlayerLocalData, DT_PortalLocal ) // Purpose: Not done (Line 16)

RecvPropFloat( RECVINFO( m_flAirControlSupressionTime ) ),
RecvPropVector( RECVINFO( m_vPreUpdateVelocity ) ),

RecvPropInt( RECVINFO( m_PaintedPowerType ) ),

// RecvPropDataTable( RECVINFO( m_PaintedPowerTimer ) ), // FIXME!

RecvPropFloat( RECVINFO( m_flCurrentStickTime ) ),
RecvPropFloat( RECVINFO( m_flAirInputScale ) ),

RecvPropInt( RECVINFO( m_InAirState ) ),

RecvPropBool( RECVINFO( m_bJumpedThisFrame ) ),
RecvPropBool( RECVINFO( m_bBouncedThisFrame ) ),
RecvPropBool( RECVINFO( m_bDuckedInAir ) ),


RecvPropBool( RECVINFO( m_bZoomedIn ) ),

RecvPropFloat( RECVINFO( m_fBouncedTime ) ),

RecvPropBool( RECVINFO( m_bPreventedCrouchJumpThisFrame ) ),

END_RECV_TABLE()


BEGIN_PREDICTION_DATA_NO_BASE( C_PortalPlayerLocalData )

END_PREDICTION_DATA()

const Vector DEFAULT_SURFACE_NORMAL( 0.0, 0.0, 1.0 );

C_PortalPlayerLocalData::C_PortalPlayerLocalData()
{
	m_bJumpedThisFrame = false;
	m_bBouncedThisFrame = false;
	m_bDuckedInAir = false;
	m_bZoomedIn = false;
	m_bPreventedCrouchJumpThisFrame = false;

	m_vPreUpdateVelocity = vec3_origin;

	m_PaintedPowerType = NO_POWER;
	m_InAirState = ON_GROUND;

	m_flAirControlSupressionTime = 0.0;
	m_flAirInputScale = 1.0;
	m_flCurrentStickTime = 0.5;
	m_fBouncedTime = 0.0;
}