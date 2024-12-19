//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#ifndef PORTAL_PLAYER_SHARED_H
#define PORTAL_PLAYER_SHARED_H
#pragma once

#define PORTAL_PUSHAWAY_THINK_INTERVAL		(1.0f / 20.0f)
#include "studio.h"
#include "paint_color_manager.h"
#include "mathlib/ssemath.h"

enum
{
	PLAYER_SOUNDS_CITIZEN = 0,
	PLAYER_SOUNDS_COMBINESOLDIER,
	PLAYER_SOUNDS_METROPOLICE,
	PLAYER_SOUNDS_MAX,
};

enum 
{
	CONCEPT_CHELL_IDLE,
	CONCEPT_CHELL_DEAD,
};

extern const char *g_pszChellConcepts[];
int GetChellConceptIndexFromString( const char *pszConcept );

struct PaintPowerInfo_t;

const float STEEP_SLOPE = 0.7;
char const* const PORTAL_PREDICTED_CONTEXT = "Portal Predicted Powers";

enum JumpButtonPress
{
	JUMP_ON_TOUCH = 0,
	PRESS_JUMP_TO_BOUNCE,
	HOLD_JUMP_TO_BOUNCE,
	TRAMPOLINE_BOUNCE
};

enum InAirState
{
	ON_GROUND,
	IN_AIR_JUMPED,
	IN_AIR_BOUNCED,
	IN_AIR_FELL
};

enum PaintSurfaceType
{
	FLOOR_SURFACE = 0,
	WALL_SURFACE,
	CEILING_SURFACE
};

void ExpandAABB( Vector& boxMin, Vector& boxMax, const Vector& sweepVector );

//=============================================================================
// Paint Power Choice
//=============================================================================
struct PaintPowerChoiceCriteria_t
{
	Vector vNormInputDir;
	Vector vNormVelocity;
	bool bInPortal;
};

struct PaintPowerChoiceResult_t
{
	const PaintPowerInfo_t* pPaintPower;
	float flInputCos;
	float flVelocityCos;
	bool bWasIgnored;

	inline void Initialize()
	{
		pPaintPower = NULL;
		flInputCos = 1.0f;
		flVelocityCos = 2.0f;
		bWasIgnored = false;
	}
};

typedef CUtlVectorFixed< PaintPowerChoiceResult_t, PAINT_POWER_TYPE_COUNT_PLUS_NO_POWER > PaintPowerChoiceResultArray;

struct CachedPaintPowerChoiceResult
{
	Vector surfaceNormal;
	CBaseHandle surfaceEntity;
	bool wasValid;
	bool wasIgnored;

	inline void Initialize()
	{
		surfaceNormal = Vector( 0, 0, 0 );
		surfaceEntity = NULL;
		wasValid = false;
		wasIgnored = false;
	}
};

//=============================================================================
// Contact Determination (used for determining available paint powers)
//=============================================================================
const int ALL_CONTENT = 0xFFFFFFFF;

struct BrushContact
{
	Vector point;
	Vector normal;
	CBaseEntity* pBrushEntity;
	bool isOnThinSurface;

	void Initialize( const Vector& contactPt,
					 const Vector& normal,
					 CBaseEntity* pBrushEntity,
					 bool onThinSurface );
	void Initialize( const fltx4& contactPt,
		const fltx4& normal,
		CBaseEntity* pBrushEntity,
		bool onThinSurface );
};

typedef CUtlVector<BrushContact> ContactVector;
typedef CUtlVector<cplane_t> CollisionPlaneVector;

#define CONTENTS_BRUSH_PAINT MASK_SOLID_BRUSHONLY
void ComputeAABBContactsWithBrushEntity( ContactVector& contacts, const Vector& boxOrigin, const Vector& boxMin, const Vector& boxMax, CBaseEntity* pBrushEntity, int contentsMask = CONTENTS_BRUSH_PAINT );
void ComputeAABBContactsWithBrushEntity( ContactVector& contacts, const cplane_t *pClipPlanes, int iClipPlaneCount, const Vector& boxOrigin, const Vector& boxMin, const Vector& boxMax, CBaseEntity* pBrushEntity, int contentsMask = CONTENTS_BRUSH_PAINT );

#ifndef CLIENT_DLL
class CPortal_Player;
#else
class C_Portal_Player;
#define CPortal_Player C_Portal_Player
#endif

void TracePlayerBoxAgainstCollidables( trace_t& trace,
									   const CPortal_Player* player,
									   const Vector& startPos,
									   const Vector& endPos,
									   const Vector& boxLocalMin,
									   const Vector& boxLocalMax );

struct StringCompare_t
{
	StringCompare_t( char const* str ) : m_str( str ) {}

	char const* const m_str;

	inline bool operator()( char const* str ) const
	{
		return V_strcmp( m_str, str ) == 0;
	}
};

#endif //PORTAL_PLAYER_SHARED_h
