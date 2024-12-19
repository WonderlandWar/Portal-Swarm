#ifndef PORTAL_PLAYER_LOCAL_DATA_H
#define PORTAL_PLAYER_LOCAL_DATA_H

#include "cbase.h"
#include "portal_player_shared.h"

#define CPortalPlayerLocalData C_PortalPlayerLocalData

class C_Trigger_TractorBeam;

class C_PortalPlayerLocalData
{
public:
	
	DECLARE_CLASS_NOBASE( C_PortalPlayerLocalData );
	DECLARE_EMBEDDED_NETWORKVAR();
	DECLARE_SIMPLE_DATADESC();
	DECLARE_PREDICTABLE();

	C_PortalPlayerLocalData();

	InAirState m_InAirState;
	PaintPowerType m_PaintedPowerType;

	bool m_bJumpedThisFrame;
	bool m_bBouncedThisFrame;
	bool m_bDuckedInAir;
	bool m_bZoomedIn;
	bool m_bPreventedCrouchJumpThisFrame;

	float m_flAirControlSupressionTime;
	float m_flAirInputScale;
	float m_flCurrentStickTime;
	float m_fBouncedTime;

	CountdownTimer m_PaintedPowerTimer;
	CachedPaintPowerChoiceResult m_CachedPaintPowerChoiceResults[PAINT_POWER_TYPE_COUNT];

	Vector m_vPreUpdateVelocity;
};

EXTERN_RECV_TABLE(DT_PortalLocal);

#endif //PORTAL_PLAYER_LOCAL_DATA_H