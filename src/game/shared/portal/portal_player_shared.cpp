//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#include "cbase.h"
#include "portal_player_shared.h"
//#include "portal_playeranimstate.h"
#include "meshutils/mesh.h"

#ifdef CLIENT_DLL
#include "c_portal_player.h"
#include "prediction.h"
#define CRecipientFilter C_RecipientFilter
#else
#include "portal_player.h"
#include "ai_basenpc.h"
#include "portal_gamestats.h"
#include "util.h"
#include "physicsshadowclone.h"
#include "world.h"
#endif

#include "engine/IEngineSound.h"
#include "SoundEmitterSystem/isoundemittersystembase.h"
#include "movevars_shared.h"
#include "in_buttons.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

acttable_t	unarmedActtable[] = 
{
	{ ACT_HL2MP_IDLE,					ACT_HL2MP_IDLE_MELEE,					false },
	{ ACT_HL2MP_RUN,					ACT_HL2MP_RUN_MELEE,					false },
	{ ACT_HL2MP_IDLE_CROUCH,			ACT_HL2MP_IDLE_CROUCH_MELEE,			false },
	{ ACT_HL2MP_WALK_CROUCH,			ACT_HL2MP_WALK_CROUCH_MELEE,			false },
	{ ACT_HL2MP_GESTURE_RANGE_ATTACK,	ACT_HL2MP_GESTURE_RANGE_ATTACK_MELEE,	false },
	{ ACT_HL2MP_GESTURE_RELOAD,			ACT_HL2MP_GESTURE_RELOAD_MELEE,			false },
	{ ACT_HL2MP_JUMP,					ACT_HL2MP_JUMP_MELEE,					false },
};

const char *g_pszChellConcepts[] =
{
	"CONCEPT_CHELL_IDLE",
	"CONCEPT_CHELL_DEAD",
};

// The depth of the OBB on the portal surface within which we reject contacts
// HACK: Half width of player box + 1 for now. We'll have to do a projection of
//		 the collision shape onto the portal normal when we have oriented bounding data
const float PORTAL_PLANE_IGNORE_EPSILON = 17.0f;

ConVar sv_speed_paint_max("sv_speed_paint_max", "800.0f", FCVAR_REPLICATED | FCVAR_CHEAT, "For tweaking the max speed for speed paint.");

ConVar sv_enableholdrotation( "sv_enableholdrotation", "0", FCVAR_REPLICATED, "When enabled, hold attack2 to rotate held objects" );
ConVar sv_player_use_cone_size( "sv_player_use_cone_size", "0.6", FCVAR_REPLICATED | FCVAR_CHEAT );
ConVar sv_use_trace_duration( "sv_use_trace_duration", "0.5", FCVAR_REPLICATED | FCVAR_CHEAT );
#ifdef GAME_DLL
extern ConVar hl2_walkspeed;
extern ConVar hl2_normspeed;
extern ConVar hl2_sprintspeed;
#else
ConVar hl2_walkspeed( "hl2_walkspeed", "150" );
ConVar hl2_normspeed( "hl2_normspeed", "190" );
ConVar hl2_sprintspeed( "hl2_sprintspeed", "320" );
#endif
char const* const JUMP_HELPER_CONTEXT = "Jump Helper Powers";
char const* const AIR_LOOK_AHEAD_CONTEXT = "Air Look Ahead Powers";
char const* const STICK_PREDICTION_CONTEXT = "Stick Prediction Powers";
char const* const PAINT_SCREEN_EFFECT = "boomer_vomit_screeneffect";//"paint_screen_effect";
char const* const PAINT_DRIP_EFFECT = "boomer_vomit_survivor";//"paint_drip_effect";

const float BOUNCE_PAINT_INPUT_DAMPING = 0.1f;
const float BOUNCE_PAINT_INPUT_DAMP_TIME = 2.0;
const float MIN_SPEED_PAINT_AIR_INPUT_START_DAMPING = 0.05f;
const float SPEED_PAINT_AIR_INPUT_DAMP_TIME = 5.0f;
const float RELATIVE_ERROR = 1.0e-6;
const float DEAD_INPUT_COS = 0.70710678118654752440084436210485f;

//bounce convars
ConVar sv_bounce_paint_forward_velocity_bonus("sv_bounce_paint_forward_velocity_bonus", "0.375f", FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY, "What percentage of forward velocity to add onto a ground bounce");
ConVar bounce_paint_wall_jump_upward_speed("bounce_paint_wall_jump_upward_speed", "275", FCVAR_REPLICATED | FCVAR_CHEAT, "The upward velocity added when bouncing off a wall");
ConVar bounce_paint_min_speed("bounce_paint_min_speed", "500.0f", FCVAR_REPLICATED | FCVAR_CHEAT, "For tweaking how high bounce paint launches the player.");
ConVar sv_wall_bounce_trade("sv_wall_bounce_trade", "0.73", FCVAR_REPLICATED | FCVAR_CHEAT, "How much outward velocity is traded for upward velocity on wall bounces");
ConVar jump_helper_enabled("jump_helper_enabled", "0", FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY, "Use long jump helper code?");
ConVar jump_helper_look_ahead_time("jump_helper_look_ahead_time", "0.2f", FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY, "Look ahead time for long jump helper. This assumes constant velocity and samples ahead in discrete chunks of time. Use paint_power_look_ahead_sample_density to adjust how many samples are taken over a given distance.");
ConVar paint_power_look_ahead_sample_density("paint_power_look_ahead_sample_density", "0.07f", FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY, "Number of samples per game unit along the extent of look ahead vector.");
ConVar jump_helper_late_jump_max_time("jump_helper_late_jump_max_time", "0.2f", FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY, "How late a player can try to jump and still get a super jump she's no longer touching.");
ConVar jump_helper_debug_enabled("jump_helper_debug_enabled", "0", FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY, "Toggle debug draw and and messages for jump helper.");
ConVar bounce_reflect_restitution("bounce_reflect_restitution", "1.0", FCVAR_REPLICATED | FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY, "The elasticity of a collision with a bounce paint surface. Values should be in the range [0, 1].");
ConVar trampoline_bounce_off_walls_while_on_ground("trampoline_bounce_off_walls_while_on_ground", "0", FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY, "Trampoline bounce activates a wall jump if the player walks into a wall.");
ConVar jump_button_can_activate_trampoline_bounce("jump_button_can_activate_trampoline_bounce", "1", FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY, "The space bar can activate the initial bounce in trampoline bounce mode.");
ConVar bounce_reflect_wall_jumps_enabled("bounce_reflect_wall_jumps_enabled", "1", FCVAR_REPLICATED | FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY, "Enable/Disable wall jumps for reflection bounce.");
ConVar bounce_reflect_wall_jump_min_up_speed("bounce_reflect_wall_jump_min_up_speed", "10.0", FCVAR_REPLICATED | FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY, "The minimum upward speed the player will jump off the wall with when reflection bounce wall jumps are enabled.");
ConVar trampoline_bounce_min_impact_speed("trampoline_bounce_min_impact_speed", "4.0", FCVAR_REPLICATED | FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY, "The minimum speed into the surface to activate a trampoline bounce.");
ConVar bounce_ledge_fall_height_boost("bounce_ledge_fall_height_boost", "45", FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY, "Toggle whether bounce reflections after falling off a ledge without jumping add upward hop velocity on the first bounce.");
ConVar bounce_auto_trigger_min_speed("bounce_auto_trigger_min_speed", "500", FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY, "At what speed the player will auto-bounce when running over bounce paint.");
ConVar look_dependent_auto_long_jump_enabled("look_dependent_auto_long_jump_enabled", "1", FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY, "The player must be looking roughly in the direction she's travelling to get activate an auto long jump.");
ConVar look_dependent_auto_long_jump_min_cos_angle("look_dependent_auto_long_jump_min_cos_angle", "0.7", FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY, "The player must be looking roughly in the direction she's travelling to get activate an auto long jump.");

//speed convars
ConVar sv_speed_paint_acceleration("sv_speed_paint_acceleration", "500.0f", FCVAR_REPLICATED | FCVAR_CHEAT, "How fast the player accelerates on speed paint.");
ConVar sv_speed_paint_ramp_acceleration("sv_speed_paint_ramp_acceleration", "1000.0f", FCVAR_REPLICATED | FCVAR_CHEAT, "How fast the player accelerates on speed paint when on a ramp.");
ConVar sv_speed_paint_on_bounce_deceleration_delay("sv_speed_paint_on_bounce_deceleration_delay", "0.2f", FCVAR_REPLICATED | FCVAR_CHEAT, "How long before starting to decelerate if going from speed to bounce.");
ConVar sv_speed_paint_straf_accel_scale("sv_speed_paint_straf_accel_scale", "0.7f", FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY, "Scale applied to acceleration when the player is strafing on speed paint.");

ConVar sv_contact_region_thickness( "sv_contact_region_thickness", "0.2f", FCVAR_REPLICATED | FCVAR_CHEAT, "The thickness of a contact region (how much the box expands)." );
ConVar sv_clip_contacts_to_portals( "sv_clip_contacts_to_portals", "0", FCVAR_REPLICATED | FCVAR_CHEAT, "Enable/Disable clipping contact regions to portal planes." );
ConVar sv_debug_draw_contacts( "sv_debug_draw_contacts", "0", FCVAR_REPLICATED | FCVAR_CHEAT, "0: Dont draw anything.  1: Draw contacts.  2: Draw colored contacts" );
ConVar sv_post_teleportation_box_time( "sv_post_teleportation_box_time", ".0333333f", FCVAR_REPLICATED | FCVAR_CHEAT, "Time to use a slightly expanded box for contacts right after teleportation." );

// Paintable player ConVars
ConVar player_can_use_painted_power("player_can_use_painted_power", "0", FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY, "Allow/disallow players to be painted.");
ConVar player_loses_painted_power_over_time("player_loses_painted_power_over_time", "1", FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY, "The player should lose the power after having it for player_paint_effects_duration.");
ConVar player_paint_effects_duration("player_paint_effects_duration", "4.0f", FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY, "Amount of time player maintains her power after being painted.");
ConVar player_paint_effects_enabled("player_paint_effects_enabled", "1", FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY, "Display the visual effects of being painted.");
ConVar player_paint_screen_effect_restart_delay("player_paint_screen_effect_restart_delay", "0.5f", FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY, "Time to wait between particle system restarts when the player gets painted multiple times in rapid succession.");

//Playtesting convars
ConVar sv_press_jump_to_bounce("sv_press_jump_to_bounce", "3", FCVAR_REPLICATED, "0: Bounce on touch, 1: Bounce on press, 2: Bounce on hold");
ConVar sv_bounce_reflect_enabled("sv_bounce_reflect_enabled", "0", FCVAR_REPLICATED | FCVAR_CHEAT, "Enable/Disable reflection on bounce.");

//Wall jump convars
ConVar sv_wall_jump_help("sv_wall_jump_help", "1", FCVAR_REPLICATED, "Enable the wall jump helper to help keep players bouncing between two opposing walls");
ConVar sv_wall_jump_help_threshold("sv_wall_jump_help_threshold", "9.0f", FCVAR_REPLICATED, "Threshold at which the wall jump helper will bring the player's velocity in line with the surface normal");
ConVar sv_wall_jump_help_amount("sv_wall_jump_help_amount", "5.0f", FCVAR_REPLICATED, "Maximum correction amount per wall bounce");
ConVar sv_wall_jump_help_debug("sv_wall_jump_help_debug", "0", FCVAR_REPLICATED);

//Debug convars
ConVar show_player_paint_power_debug( "show_player_paint_power_debug", "0", FCVAR_REPLICATED );
ConVar mp_should_gib_bots("mp_should_gib_bots", "1", FCVAR_REPLICATED | FCVAR_CHEAT);
ConVar sv_debug_bounce_reflection("sv_debug_bounce_reflection", "0", FCVAR_REPLICATED );
ConVar sv_debug_bounce_reflection_time("sv_debug_bounce_reflection_time", "15.f", FCVAR_REPLICATED );
ConVar paint_compute_contacts_simd("paint_compute_contacts_simd", "0", FCVAR_REPLICATED, "Compute the contacts with paint in fast SIMD (1) or with slower FPU (0)." );

ConVar prevent_crouch_jump("prevent_crouch_jump", "1", FCVAR_REPLICATED | FCVAR_CHEAT, "Enable/Disable crouch jump prevention.");

extern ConVar sv_footsteps;
extern ConVar sv_debug_player_use;

extern float IntervalDistance( float x, float x0, float x1 );



const Vector ComputeJumpVelocity( const Vector& normal,
								 const Vector& localUp,
								 float desiredNormalSpeed )
{
	Vector outBounceVelocity = vec3_origin;
	const float flNormDot = DotProduct( localUp, normal );
	const float flTrade = sv_wall_bounce_trade.GetFloat();	// We trade some outward velocity for upward velocity

	// Add upward velocity if surface normal is facing up, relative to the player.
	float flBounceScale = 0.f;

	if( sv_press_jump_to_bounce.GetInt() != TRAMPOLINE_BOUNCE || bounce_reflect_wall_jumps_enabled.GetBool() )
	{
		if( flNormDot > -0.1f )
		{
			// Extra upward wall bounce velocity
			flBounceScale = 1.f - flNormDot;
			outBounceVelocity += localUp * bounce_paint_wall_jump_upward_speed.GetFloat() * flBounceScale;
		}
		// Downward facing wall. Add velocity in the XY plane.
		else
		{
			// Vector pointing out of the surface in the XY plane
			Vector vOut = ( normal - (localUp * flNormDot) ).Normalized();

			// Extra lateral velocity off the wall
			flBounceScale = DotProduct( vOut, normal );
			outBounceVelocity += vOut * bounce_paint_wall_jump_upward_speed.GetFloat() * flBounceScale;
		}
	}

	// Calculate how much bounce velocity is left to spend after the lateral
	float fWallBounceScale = flTrade + ( (1.f - flBounceScale) * (1.0 - flTrade) );

	// Velocity off of the surface
	outBounceVelocity += normal * desiredNormalSpeed * fWallBounceScale;

	return outBounceVelocity;
}


const Vector ComputeWallJumpHelperVelocity( const Vector& finalBounceVelocity,
										   const Vector& normal )
{
	Vector velocity = finalBounceVelocity;
	if( sv_wall_jump_help.GetBool() )
	{
		Vector vecOutVelocity = velocity;
		vecOutVelocity.z = 0.0f;
		VectorNormalize( vecOutVelocity );
		float flOutAngle = RAD2DEG( acos( clamp( DotProduct( normal, vecOutVelocity ), -1.0f, 1.0f ) ) );
		float flCorrection = 0.0f;

		//Do not correct if the out angle is the same as the normal
		if( flOutAngle == 0.0f )
		{
			flCorrection = 0.0f;
			return velocity;
		}

		float flSign = vecOutVelocity.x * normal.y - vecOutVelocity.y * normal.x;

		//The out angle is small enough to snap to the normal
		if( flOutAngle <= sv_wall_jump_help_amount.GetFloat() )
		{
			flCorrection = flOutAngle;
		}
		//The out angle needs to be corrected to the normal dir
		else if( flOutAngle <= sv_wall_jump_help_threshold.GetFloat() )
		{
			flCorrection = sv_wall_jump_help_amount.GetFloat();
		}

		//Rotate the out velocity towards the normal
		VMatrix matRotate;
		if( flSign > 0.0f )
		{
			MatrixBuildRotateZ( matRotate, flCorrection );
		}
		else
		{
			MatrixBuildRotateZ( matRotate, -flCorrection );
		}

		Vector rotatedVelocity;
		VectorRotate( velocity, matRotate.As3x4(), rotatedVelocity );
		velocity = rotatedVelocity;

		//#ifndef CLIENT_DLL
		//if( sv_wall_jump_help_debug.GetBool() )
		//{
		//	Vector vecInVelocity = velocity;
		//	vecInVelocity.z = 0.0f;
		//	VectorNormalize( vecInVelocity );
		//	float flInAngle = 180.0f - RAD2DEG( acos( DotProduct( normal, vecInVelocity ) ) );
		//	DevMsg( "InAngle: %f, OutAngle: %f, Correction: %f\n", flInAngle, flOutAngle, flCorrection );
		//}
		//#endif //!CLIENT_DLL
	}

	return velocity;
}


const Vector ComputeBouncePostVelocityReflection( const Vector& preVelocity,
												 const Vector& normal,
												 const Vector& localUp )
{
	// Cancel out velocity in the normal direction
	const float normalSpeed = DotProduct( preVelocity, normal );
	const Vector normalVelocity = normalSpeed * normal;
	Vector velocity = preVelocity;
	velocity -= normalVelocity;

	// The base bounce velocity is the remaining velocity required to reflect the
	// velocity when the player hit the surface.
	Vector bounceVelocity = -bounce_reflect_restitution.GetFloat() * normalVelocity;

	// Optionally trade some outward velocity for upward velocity on wall jumps
	const bool shouldWallJump = bounce_reflect_wall_jumps_enabled.GetBool() && CloseEnough( normal.z, 0.0f );
	if( shouldWallJump )
	{
		bounceVelocity = ComputeJumpVelocity( normal, localUp, fabs( normalSpeed ) );

		// Don't let the velocity tangent to the surface cancel out the wall jump
		const float minWallJumpUpSpeed = bounce_reflect_wall_jump_min_up_speed.GetFloat();
		if( bounceVelocity.z + velocity.z < minWallJumpUpSpeed )
		{
			bounceVelocity.z += minWallJumpUpSpeed - velocity.z;
		}
	}

	velocity += bounceVelocity;

	// If wall jumps are allowed, use the wall jump helper to bring
	// the post impact velocity closer to the normal direction
	if( shouldWallJump )
		velocity = ComputeWallJumpHelperVelocity( velocity, normal );

	return velocity;
}


const Vector ComputeBouncePostVelocityNoReflect( const Vector& preVelocity,
												const Vector& normal,
												const Vector& localUp )
{
	Vector vBounceVel(0,0,0);

	Vector velocity = preVelocity;
	velocity -= normal * DotProduct( velocity, normal );

	// Cancel out downward velocity (allows for going up parallel walls)
	velocity -= localUp * DotProduct( velocity, localUp );

	// Store this for later
	Vector velNorm = velocity;
	velNorm.NormalizeInPlace();

	// Add upward velocity if surface normal is facing perpendicular to up, relative to the player
	vBounceVel = ComputeJumpVelocity( normal, localUp, bounce_paint_min_speed.GetFloat() );

	// Don't let our velocity fight the new bounce velocity
	velocity -= velocity.Normalized() * DotProduct( velocity, vBounceVel.Normalized() );
	velocity += vBounceVel;

	// Use the wall jump helper to bring the post jump velocity closer to the normal direction
	velocity = ComputeWallJumpHelperVelocity( velocity, normal );

	return velocity;
}


void ExpandAABB( Vector& boxMin, Vector& boxMax, const Vector& sweepVector )
{
	for( unsigned i = 0; i < 3; ++i )
	{
		boxMin[i] += sweepVector[i] < 0 ? sweepVector[i] : 0;
		boxMax[i] += sweepVector[i] > 0 ? sweepVector[i] : 0;
	}
}

void BrushContact::Initialize( const Vector& contactPt,
							   const Vector& planeNormal,
							   CBaseEntity* pEntity,
							   bool onThinSurface )
{
	point = contactPt;
	normal = planeNormal;
	pBrushEntity = pEntity;
	isOnThinSurface = onThinSurface;
}

void BrushContact::Initialize( const fltx4& contactPt,
							  const fltx4& planeNormal,
							  CBaseEntity* pEntity,
							  bool onThinSurface )
{
	StoreUnaligned3SIMD( &point.x, contactPt );
	StoreUnaligned3SIMD( &normal.x, planeNormal );
	pBrushEntity = pEntity;
	isOnThinSurface = onThinSurface;
}

typedef CUtlVector<Vector4D> PlaneVector;

void AddBboxToPlaneList( Vector4D *pListInsertPosition, const Vector &vMins, const Vector &vMaxs )
{
	pListInsertPosition[0] = Vector4D(1, 0, 0, vMaxs.x);
	pListInsertPosition[1] = Vector4D(-1, 0, 0, -vMins.x);
	pListInsertPosition[2] = Vector4D(0, 1, 0, vMaxs.y);
	pListInsertPosition[3] = Vector4D(0, -1, 0, -vMins.y);
	pListInsertPosition[4] = Vector4D(0, 0, 1, vMaxs.z);
	pListInsertPosition[5] = Vector4D(0, 0, -1, -vMins.z);
}

void AddBboxToPlaneList_SIMD( fltx4 *pListInsertPosition, const fltx4 &vMins, const fltx4 &vMaxs )
{
	pListInsertPosition[0] = SetWFromXSIMD( g_SIMD_Identity[0], vMaxs );	//	Vector4D(1, 0, 0, vMaxs.x);
	pListInsertPosition[1] = -SetWFromXSIMD( g_SIMD_Identity[0], vMins );	//	Vector4D(-1, 0, 0, -vMins.x);
	pListInsertPosition[2] = SetWFromYSIMD( g_SIMD_Identity[1], vMaxs );	//  Vector4D(0, 1, 0, vMaxs.y);
	pListInsertPosition[3] = -SetWFromYSIMD( g_SIMD_Identity[1], vMins );	//	Vector4D(0, -1, 0, -vMins.y);
	pListInsertPosition[4] = SetWFromZSIMD( g_SIMD_Identity[2], vMaxs );	//	Vector4D(0, 0, 1, vMaxs.z);
	pListInsertPosition[5] = -SetWFromZSIMD( g_SIMD_Identity[2], vMins );	//	Vector4D(0, 0, -1, -vMins.z);
}

inline const Vector ToVector( const float* vec )
{
	return Vector( vec[0], vec[1], vec[2] );
}


void DebugDrawMesh( const CMesh& mesh )
{
	for ( int i = 0; i < mesh.TriangleCount(); ++i )
	{
		int i0 = mesh.m_pIndices[i*3 + 0];
		int i1 = mesh.m_pIndices[i*3 + 1];
		int i2 = mesh.m_pIndices[i*3 + 2];
		NDebugOverlay::Triangle( ToVector( mesh.GetVertex(i0) ),
			ToVector( mesh.GetVertex(i1) ),
			ToVector( mesh.GetVertex(i2) ),
			255, 0, 0, 128, true, 0 );

	}
}


const Vector ComputeCentroid( const CMesh& volume )
{
	Vector centroid( 0, 0, 0 );
	for ( int i = 0; i < volume.m_nVertexCount; ++i )
	{
		//NDebugOverlay::Sphere( ToVector( volume.GetVertex(i) ), 5.0f, 0, 255, 255, false, 0 );
		centroid += ToVector( volume.GetVertex(i) );
	}

	centroid *= (1.0f / volume.m_nVertexCount);
	return centroid;
}

const fltx4 ComputeCentroid_SIMD( const CMesh& volume )
{
	fltx4 centroid = Four_Zeros;
	int nCount = volume.m_nVertexCount;
	fltx4 f4NumberOfVertices = ReplicateX4( (float)nCount );
	int nIndex = 0;
	while ( nCount >= 4 )
	{
		fltx4 p0 = *(const fltx4 *)volume.GetVertex( nIndex );
		fltx4 p1 = *(const fltx4 *)volume.GetVertex( nIndex + 1 );
		fltx4 p2 = *(const fltx4 *)volume.GetVertex( nIndex + 2 );
		fltx4 p3 = *(const fltx4 *)volume.GetVertex( nIndex + 3 );
		// Try to reduce register dependency a bit
		fltx4 first2 = AddSIMD( p0, p1 );
		fltx4 second2 = AddSIMD( p2, p3 );
		centroid = AddSIMD( centroid, first2 );
		centroid = AddSIMD( centroid, second2 );
		nCount -= 4;
		nIndex += 4;
	}
	while ( nCount > 0 )
	{
		fltx4 p0 = *(const fltx4 *)volume.GetVertex( nIndex );
		centroid = AddSIMD( centroid, p0 );
		--nCount;
		++nIndex;
	}

	centroid = MulSIMD( centroid, ReciprocalSIMD( f4NumberOfVertices ) );
	return centroid;
}

// outsidePt is a point that should be on the positive side of the plane defined by the triangle
bool ComputeContactPlane( cplane_t& plane, int& contactTriangleIndex, const CMesh& contactRegion, const Vector& outsidePt )
{
	Vector contactNormal( 0, 0, 1 );
	float maxAreaSq = -FLT_MAX;
	int p0VertexIndex = 0;
	const int triangleCount = contactRegion.TriangleCount();
	contactTriangleIndex = triangleCount;

	for( int i = 0; i < triangleCount; ++i )
	{
		// Get the vertices of the triangle
		const int triangleIndex = i * 3;
		const int i0 = contactRegion.m_pIndices[triangleIndex];
		const int i1 = contactRegion.m_pIndices[triangleIndex + 1];
		const int i2 = contactRegion.m_pIndices[triangleIndex + 2];

		const Vector p0 = ToVector( contactRegion.GetVertex(i0) );
		const Vector p1 = ToVector( contactRegion.GetVertex(i1) );
		const Vector p2 = ToVector( contactRegion.GetVertex(i2) );

		// Compute the normal to the plane defined by the triangle
		const Vector planeNormal = CrossProduct( p1 - p0, p2 - p0 );

		// Choose the normal corresponding to the triangle with the largest area
		// (since this is the intersection volume of a slightly expanded AABB, which
		// is only penetrating the brush by the expansion amount). Also, make sure it
		// points in the general direction of the point outside the plane.
		const Vector outVector = outsidePt - p0;
		const float areaSq = planeNormal.LengthSqr();
		if( areaSq > maxAreaSq && DotProduct( planeNormal, outVector ) > 0 )
		{
			maxAreaSq = areaSq;
			contactNormal = planeNormal;
			p0VertexIndex = i0;
			contactTriangleIndex = i;
		}

		//NDebugOverlay::Triangle( p0, p1, p2, 255, 0, 0, 128, true, 0 );
		//NDebugOverlay::HorzArrow( p0, p0 + 20.0f * planeNormal.Normalized(), 1, 255, 0, 255, 128, true, 0 );
		//NDebugOverlay::HorzArrow( p1, p1 + 20.0f * planeNormal.Normalized(), 1, 255, 0, 255, 128, true, 0 );
		//NDebugOverlay::HorzArrow( p2, p2 + 20.0f * planeNormal.Normalized(), 1, 255, 0, 255, 128, true, 0 );
	}

	// Normalize the normal
	const bool isValid = maxAreaSq > 0;
	contactNormal *= isValid ? 1.0f / sqrt( maxAreaSq ) : 1.0f;

	// Fill out the plane
	plane.normal = contactNormal;
	plane.dist = DotProduct( contactNormal, ToVector( contactRegion.GetVertex(p0VertexIndex) ) );

	return isValid;
}

int ComputeContactPlane_SIMD( fltx4& plane, const CMesh& contactRegion, const fltx4& outsidePt )
{
	fltx4 contactNormal = g_SIMD_Identity[2];		//	( 0, 0, 1 );
	fltx4 maxAreaSq = Four_Zeros;
	const int nTriangleCount = contactRegion.TriangleCount();
	int contactTriangleIndex = -1;
	fltx4 pReferenceVertex = Four_Zeros;

	for( int i = 0; i < nTriangleCount; ++i )
	{
		// Get the vertices of the triangle
		const int triangleIndex = i * 3;
		const int i0 = contactRegion.m_pIndices[triangleIndex];
		const int i1 = contactRegion.m_pIndices[triangleIndex + 1];
		const int i2 = contactRegion.m_pIndices[triangleIndex + 2];

		// The mesh has been created from fltx4 (aligned with a stride of 4).
		const fltx4 p0 = *(const fltx4 *)( contactRegion.GetVertex(i0) );
		const fltx4 p1 = *(const fltx4 *)( contactRegion.GetVertex(i1) );
		const fltx4 p2 = *(const fltx4 *)( contactRegion.GetVertex(i2) );

		// Compute the normal to the plane defined by the triangle
		const fltx4 planeNormal = CrossProductSIMD( p1 - p0, p2 - p0 );

		// Choose the normal corresponding to the triangle with the largest area
		// (since this is the intersection volume of a slightly expanded AABB, which
		// is only penetrating the brush by the expansion amount). Also, make sure it
		// points in the general direction of the point outside the plane.
		const fltx4 outVector = outsidePt - p0;
		const fltx4 areaSq = Dot3SIMD( planeNormal, planeNormal );
		const fltx4 dotProd = Dot3SIMD( planeNormal, outVector );
		const fltx4 f4IsMax = (fltx4)CmpLeSIMD( areaSq, maxAreaSq );			//	if ( areaSq <= maxAreaSq )
		const fltx4 f4IsDotPos = (fltx4)CmpLeSIMD( dotProd, Four_Zeros );		//	if ( DotProduct( planeNormal, outVector ) <= 0 )
		//if ( areaSq > maxAreaSq && DotProduct( planeNormal, outVector ) > 0 )	- Except we do the opposite tests and look if the result is false
		if ( IsAllEqual( OrSIMD( f4IsMax, f4IsDotPos), Four_Zeros ) )
		{
			maxAreaSq = areaSq;
			contactNormal = planeNormal;
			pReferenceVertex = p0;
			contactTriangleIndex = i;
		}

		//NDebugOverlay::Triangle( p0, p1, p2, 255, 0, 0, 128, true, 0 );
		//NDebugOverlay::HorzArrow( p0, p0 + 20.0f * planeNormal.Normalized(), 1, 255, 0, 255, 128, true, 0 );
		//NDebugOverlay::HorzArrow( p1, p1 + 20.0f * planeNormal.Normalized(), 1, 255, 0, 255, 128, true, 0 );
		//NDebugOverlay::HorzArrow( p2, p2 + 20.0f * planeNormal.Normalized(), 1, 255, 0, 255, 128, true, 0 );
	}

	if (contactTriangleIndex < 0)
	{
		return -1;
	}

	// At that point the area is greater than zero (i.e. valid)
	// Normalize the normal
	// const bool isValid = maxAreaSq > 0;
	// contactNormal *= isValid ? 1.0f / sqrt( maxAreaSq ) : 1.0f;
	contactNormal = MulSIMD( contactNormal, ReciprocalSqrtSIMD( maxAreaSq ) );

	// Fill out the plane
	// plane.normal = contactNormal;
	// plane.dist = DotProduct( contactNormal, ToVector( contactRegion.GetVertex(p0VertexIndex) ) );
	fltx4 f4Dist = Dot3SIMD( contactNormal, pReferenceVertex );
	plane = SetWSIMD( contactNormal, f4Dist );

	return contactTriangleIndex;
}

void ComputeAABBContactsWithBrushEntity( ContactVector& contacts, const Vector& boxOrigin, const Vector& boxMin, const Vector& boxMax, CBaseEntity* pBrushEntity, int contentsMask )
{
	ComputeAABBContactsWithBrushEntity( contacts, NULL, 0, boxOrigin, boxMin, boxMax, pBrushEntity, contentsMask );
}

void ComputeAABBContactsWithBrushEntity_Old( ContactVector& contacts, const cplane_t *pClipPlanes, int iClipPlaneCount, const Vector& boxOrigin, const Vector& boxMin, const Vector& boxMax, CBaseEntity* pBrushEntity, int contentsMask)
{
	//typedef CUtlVector<int>	BrushIndexVector;
	typedef CUtlVector<uint32> PlaneIndexVector;
	//typedef CUtlVector<BrushSideInfo_t> BrushSideInfoVector;
	//typedef BrushSideInfo_t* BrushSideInfoIterator;
	// Get the collision model index of the brush entity
	AssertMsg( pBrushEntity->IsBSPModel(), "Your brush entity is not a brush entity." );
#ifdef DEBUG
	ICollideable* pCollideable = enginetrace->GetCollideable( pBrushEntity );
	const int cmodelIndex = pCollideable->GetCollisionModelIndex() - 1;
	AssertMsg( !pBrushEntity->IsWorld() || cmodelIndex == 0, "World collision model index should be 0." );
#endif
	// The query box must be in local space for non-world brush entities

	Vector queryBoxMin = boxMin;
	Vector queryBoxMax = boxMax;
	if( !pBrushEntity->IsWorld() )
	{
		const matrix3x4_t& entityToWorld = pBrushEntity->EntityToWorldTransform();
		ITransformAABB( entityToWorld, boxMin, boxMax, queryBoxMin, queryBoxMax );
	}

	// Draw bbox lines if necessary
	//DebugDrawLine( queryBoxMin, queryBoxMax, 0, 255, 0, false, 0.25 );

	// Get the indices of all the colliding brushes
	//BrushIndexVector brushIndices;
	//CBrushQuery brushQuery;
	CUtlVector<int> WorldBrushes;
	enginetrace->GetBrushesInAABB( queryBoxMin, queryBoxMax, &WorldBrushes, contentsMask );

	// Find the contact regions
	CUtlVector<BrushSideInfo_t> brushSides;
	//BrushSideInfo_t *brushSides = (BrushSideInfo_t *)stackalloc( sizeof( BrushSideInfo_t ) * COMMON_BRUSH_SIDES );
	//PlaneVector planes;
	
	//if ( !pBrushEntity->IsWorld() )
	//	Msg("Not the world..?\n");

	for( int i = 0; i < WorldBrushes.Count(); ++i )
	{
		// Get the brush side info
		int iBrushContents;
#ifdef DEBUG
		bool bHasBrushInfo = enginetrace->GetBrushInfo( WorldBrushes[i], &brushSides, &iBrushContents );
#else // Bleh, unreferenced local variable
		enginetrace->GetBrushInfo( WorldBrushes[i], &brushSides, &iBrushContents );
#endif
		int iNumBrushSides = brushSides.Count();
		Assert( iNumBrushSides > 0 );

#ifdef DEBUG // If this triggers, we need to rewrite the condition
		if ( iNumBrushSides <= 0 )
			Assert( !bHasBrushInfo );

		//if ( !pBrushEntity->IsWorld() )
		//	Msg("This entity is not the world!! Whoohoo!!\n");
#endif

		if( iNumBrushSides <= 0 )
			continue;
		
		Vector4D *planes = (Vector4D *)stackalloc( sizeof( Vector4D ) * (brushSides.Count() + 6 /*bbox*/ + iClipPlaneCount) );
		
		//remove bevel planes
		{
			int iWriteIndex = 0;
			for( int sideIndex = 0; sideIndex < iNumBrushSides; ++sideIndex )
			{
				if( brushSides[sideIndex].bevel != 0 )
					continue;

				brushSides[iWriteIndex] = brushSides[sideIndex];
				++iWriteIndex;
			}
			iNumBrushSides = iWriteIndex;
		}

		// Transform the planes to world space
		for( int sideIndex = 0; sideIndex < iNumBrushSides; ++sideIndex )
		{
			//cplane_t cur = brushSides[sideIndex].plane;
			//cplane_t temp;
			//MatrixTransformPlane( entityToWorld, cur, temp );
			planes[sideIndex] = brushSides[sideIndex].plane;//Vector4D( temp.normal.x, temp.normal.y, temp.normal.z, temp.dist );
		}

		int iPlaneCount = iNumBrushSides;

		// Add the box planes
		AddBboxToPlaneList( &planes[iNumBrushSides], boxMin, boxMax );
		iPlaneCount += 6;

		// Add the clip planes
		for( int j = 0; j < iClipPlaneCount; ++j )
		{
			const cplane_t& plane = pClipPlanes[j];
			planes[iPlaneCount] = Vector4D( -plane.normal.x, -plane.normal.y, -plane.normal.z, -plane.dist );
			++iPlaneCount;
		}

		Assert( iPlaneCount <= (brushSides.Count() + 6 + iClipPlaneCount) );

		// Compute the contact region
		CMesh contactRegion;
		PlaneIndexVector trianglePlaneIndices;
		HullFromPlanes( &contactRegion, &trianglePlaneIndices, reinterpret_cast<const float*>( planes ), iPlaneCount, 4 );
		//DebugDrawMesh( contactRegion );

		// If the contact region exists
		if( contactRegion.m_nVertexCount > 0 )
		{
			// Compute the contact point and normal
			cplane_t plane;
			int triangleIndex;
			if( ComputeContactPlane( plane, triangleIndex, contactRegion, boxOrigin ) )
			{
				const Vector centroid = ComputeCentroid( contactRegion );
				const float dist = DotProduct( centroid, plane.normal );
				const float diff = dist - plane.dist;
				const Vector contactPt = centroid - diff * plane.normal;

				// Figure out if the contact is on a thin surface
				const int planeIndex = trianglePlaneIndices[triangleIndex];
				const bool isOnThinSurface = planeIndex < iNumBrushSides ? brushSides[planeIndex].thin != 0 : false;

				BrushContact contact;
				contact.Initialize( contactPt, plane.normal, pBrushEntity, isOnThinSurface );
				contacts.AddToTail( contact );

				if( sv_debug_draw_contacts.GetInt() == 1 )
				{
					Color color( contact.isOnThinSurface ? 0 : 255, 255, 255 );
					NDebugOverlay::Sphere( contact.point, 5.0f, color.r(), color.g(), color.b(), true, 0 );
					NDebugOverlay::Line( contact.point, contact.point + 20.0f * contact.normal, color.r(), color.g(), color.b(), true, 0 );
				}
				//DevMsg( "Contact: (%f, %f, %f)\n", XYZ( contact.point ) );
				//DevMsg( "Normal: (%f, %f, %f)\n", XYZ( contact.normal ) );
			}
		}
	}
}

void ComputeAABBContactsWithBrushEntity_SIMD( ContactVector& contacts, const cplane_t *pClipPlanes, int iClipPlaneCount, const Vector& boxOrigin, const Vector& boxMin, const Vector& boxMax, CBaseEntity* pBrushEntity, int contentsMask)
{
	//typedef CUtlVector<int>	BrushIndexVector;
	typedef CUtlVector<uint16> PlaneIndexVector;
	//typedef CUtlVector<BrushSideInfo_t> BrushSideInfoVector;
	//typedef BrushSideInfo_t* BrushSideInfoIterator;
	// Get the collision model index of the brush entity
	AssertMsg( pBrushEntity->IsBSPModel(), "Your brush entity is not a brush entity." );
	ICollideable* pCollideable = enginetrace->GetCollideable( pBrushEntity );
#pragma warning( disable : 4189 )
	const int cmodelIndex = pCollideable->GetCollisionModelIndex() - 1;
	AssertMsg( !pBrushEntity->IsWorld() || cmodelIndex == 0, "World collision model index should be 0." );
	const matrix3x4_t& entityToWorld = pBrushEntity->EntityToWorldTransform();

	// The query box must be in local space for non-world brush entities
	Vector queryBoxMin = boxMin;
	Vector queryBoxMax = boxMax;
	if( !pBrushEntity->IsWorld() )
	{
		ITransformAABB( entityToWorld, boxMin, boxMax, queryBoxMin, queryBoxMax );
	}

	// Get the indices of all the colliding brushes
	//BrushIndexVector brushIndices;
	//CBrushQuery brushQuery;
	CUtlVector<int> WorldBrushes;
	enginetrace->GetBrushesInAABB( queryBoxMin, queryBoxMax, &WorldBrushes, contentsMask );

	// Find the contact regions
	//BrushSideInfoVector brushSides;
	//BrushSideInfo_t *brushSides = (BrushSideInfo_t *)stackalloc( sizeof( BrushSideInfo_t ) * brushQuery.MaxBrushSides() );
	CUtlVector<BrushSideInfo_t> brushSides;
	//PlaneVector planes;
	fltx4 f4BoxMin = LoadUnalignedSIMD( &boxMin.x );
	fltx4 f4BoxMax = LoadUnalignedSIMD( &boxMax.x );
	fltx4 f4BoxOrigin = LoadUnalignedSIMD( &boxOrigin.x );

	for( int i = 0; i < WorldBrushes.Count(); ++i )
	{
		// Get the brush side info
		int iBrushContents;
#ifdef DEBUG
		bool bHasBrushInfo = enginetrace->GetBrushInfo( WorldBrushes[i], &brushSides, &iBrushContents );
#else // Bleh, unreferenced local variable
		enginetrace->GetBrushInfo( WorldBrushes[i], &brushSides, &iBrushContents );
#endif
		int iNumBrushSides = brushSides.Count();
		Assert( iNumBrushSides > 0 );

#ifdef DEBUG // If this triggers, we need to rewrite the condition
		if ( iNumBrushSides <= 0 )
			Assert( !bHasBrushInfo );

		//if ( !pBrushEntity->IsWorld() )
		//	Msg("This entity is not the world!! Whoohoo!!\n");
#endif
		
#if 0
		const int NUMBER_OF_FLTX4 = brushSides.Count() + 6 /*bbox*/ + iClipPlaneCount;

		Vector4D *planes = (Vector4D *)stackalloc( sizeof( Vector4D ) * ( NUMBER_OF_FLTX4 + 1 ) );
#else
		const int NUMBER_OF_FLTX4 = brushSides.Count() + 6 /*bbox*/ + iClipPlaneCount;

		fltx4 *planes = (fltx4 *)stackalloc( sizeof( fltx4 ) * ( NUMBER_OF_FLTX4 + 1 ) );		// +1 for VMX alignment
		planes = (fltx4*)ALIGN_VALUE( (int)planes, sizeof(fltx4) );
#endif

		//remove bevel planes
		{
			int iWriteIndex = 0;
			for( int sideIndex = 0; sideIndex < iNumBrushSides; ++sideIndex )
			{
				if( brushSides[sideIndex].bevel != 0 )
					continue;

				brushSides[iWriteIndex] = brushSides[sideIndex];
				++iWriteIndex;
			}
			iNumBrushSides = iWriteIndex;
		}

		// Transform the planes to world space
		for( int sideIndex = 0; sideIndex < iNumBrushSides; ++sideIndex )
		{
#if 1
			cplane_t planeConverted;
			Vector4DPlane( brushSides[sideIndex].plane, &planeConverted );

			cplane_t temp;
			MatrixTransformPlane( entityToWorld, planeConverted, temp );		// Could be optimized further here...
			planes[sideIndex] = LoadUnalignedSIMD(&temp.normal);		// Read XYZ and dist of the plane
#else
			planes[sideIndex] = brushSides[sideIndex].plane;
#endif
		}

		int iPlaneCount = iNumBrushSides;

		// Add the box planes
		AddBboxToPlaneList_SIMD( &planes[iNumBrushSides], f4BoxMin, f4BoxMax );
		iPlaneCount += 6;

		// Add the clip planes
		for( int j = 0; j < iClipPlaneCount; ++j )
		{
			const cplane_t& plane = pClipPlanes[j];
			planes[iPlaneCount] = -LoadUnalignedSIMD(&plane.normal);
			++iPlaneCount;
		}

		Assert( iPlaneCount <= NUMBER_OF_FLTX4 );

		// Compute the contact region
		CMesh contactRegion;
		PlaneIndexVector trianglePlaneIndices;
		HullFromPlanes_SIMD( &contactRegion, &trianglePlaneIndices, planes, iPlaneCount );
		//DebugDrawMesh( contactRegion );

		// If the contact region exists
		if( contactRegion.m_nVertexCount > 0 )
		{
			// Compute the contact point and normal
			fltx4 plane;
			int nTriangleIndex = ComputeContactPlane_SIMD( plane, contactRegion, f4BoxOrigin );
#if _DEBUG
			cplane_t slowPlane;
			int slowTriangleIndex;
			bool bSlowValid = ComputeContactPlane( slowPlane, slowTriangleIndex, contactRegion, boxOrigin );
			Assert( (nTriangleIndex >= 0) == bSlowValid );
			// For optimization reasons, we changed the API a bit.
			if (nTriangleIndex >= 0)
			{
				Assert( nTriangleIndex == slowTriangleIndex );

				// Because ComputeContactPlane_SIMD() does a square root reciprocal, there is some error involved that will slightly create divergence in the results.
				Assert( fabs( SubFloat( plane, 0 ) - slowPlane.normal.x ) < 0.001f );
				Assert( fabs( SubFloat( plane, 1 ) - slowPlane.normal.y ) < 0.001f );
				Assert( fabs( SubFloat( plane, 2 ) - slowPlane.normal.z ) < 0.001f );
				Assert( fabs( SubFloat( plane, 3 ) - slowPlane.dist ) < 0.001f );
			}
			else
			{
				Assert( slowTriangleIndex == contactRegion.TriangleCount() );
			}
#endif
			if ( nTriangleIndex >= 0 )
			{
				const fltx4 centroid = ComputeCentroid_SIMD( contactRegion );
				const fltx4 dist = Dot3SIMD( centroid, plane );
				const fltx4 diff = dist - SplatWSIMD( plane );
				const fltx4 contactPt = MsubSIMD(diff, plane, centroid);		//	centroid - diff * plane.normal;

				// Figure out if the contact is on a thin surface
				const int planeIndex = trianglePlaneIndices[nTriangleIndex];
				const bool isOnThinSurface = planeIndex < iNumBrushSides ? brushSides[planeIndex].thin != 0 : false;

				BrushContact contact;
				contact.Initialize( contactPt, plane, pBrushEntity, isOnThinSurface );
				contacts.AddToTail( contact );

				//DevMsg( "Contact: (%f, %f, %f)\n", XYZ( contact.point ) );
				//DevMsg( "Normal: (%f, %f, %f)\n", XYZ( contact.normal ) );
			}
		}
	}

	// Do the test only once instead of for every contacts. This is debug code after all.
	if( sv_debug_draw_contacts.GetInt() == 1 )
	{
		for ( int i  = 0 ; i < contacts.Count() ; ++i )
		{
			const BrushContact & contact = contacts[i];
			Color color( contact.isOnThinSurface ? 0 : 255, 255, 255 );
			NDebugOverlay::Sphere( contact.point, 5.0f, color.r(), color.g(), color.b(), true, 0 );
			NDebugOverlay::Line( contact.point, contact.point + 20.0f * contact.normal, color.r(), color.g(), color.b(), true, 0 );
		}
	}
}

void ComputeAABBContactsWithBrushEntity( ContactVector& contacts, const cplane_t *pClipPlanes, int iClipPlaneCount, const Vector& boxOrigin, const Vector& boxMin, const Vector& boxMax, CBaseEntity* pBrushEntity, int contentsMask)
{
	if ( paint_compute_contacts_simd.GetBool() )
	{
		ComputeAABBContactsWithBrushEntity_SIMD( contacts, pClipPlanes, iClipPlaneCount, boxOrigin, boxMin, boxMax, pBrushEntity, contentsMask );
#if _DEBUG && 1
		ContactVector fpuContacts;
		ComputeAABBContactsWithBrushEntity_Old( fpuContacts, pClipPlanes, iClipPlaneCount, boxOrigin, boxMin, boxMax, pBrushEntity, contentsMask );

		// Now check that the official version and the SIMD version give the same results.
		Assert( contacts.Count() == fpuContacts.Count() );
		for (int i = 0 ; i < contacts.Count() ; ++i )
		{
			// Because there are some reciprocal called in the SIMD version, there is a bit of inaccuracy in the results.
			Assert( contacts[i].point.DistTo(fpuContacts[i].point) < 0.01 );
			Assert( contacts[i].normal.DistTo(fpuContacts[i].normal) < 0.01 );
			Assert( contacts[i].pBrushEntity == fpuContacts[i].pBrushEntity );
			Assert( contacts[i].isOnThinSurface == fpuContacts[i].isOnThinSurface );
		}
#endif
	}
	else
	{
		ComputeAABBContactsWithBrushEntity_Old( contacts, pClipPlanes, iClipPlaneCount, boxOrigin, boxMin, boxMax, pBrushEntity, contentsMask );
	}
}

void TracePlayerBoxAgainstCollidables( trace_t& trace,
									  const CPortal_Player* player,
									  const Vector& startPos,
									  const Vector& endPos,
									  const Vector& boxLocalMin,
									  const Vector& boxLocalMax )
{
#ifdef CLIENT_DLL
	CTraceFilterSimpleClassnameList traceFilter( player, COLLISION_GROUP_PLAYER_MOVEMENT );
	traceFilter.AddClassnameToIgnore("prop_weighted_cube");
#else
	CTraceFilterSimpleClassnameList baseFilter( player, COLLISION_GROUP_PLAYER_MOVEMENT );
	baseFilter.AddClassnameToIgnore( "prop_weighted_cube" );
	baseFilter.AddClassnameToIgnore( "prop_physics_paintable" );
	CTraceFilterTranslateClones traceFilter( &baseFilter );
#endif

	Ray_t ray;
	ray.Init( startPos, endPos, boxLocalMin, boxLocalMax );
	UTIL_ClearTrace( trace );
	UTIL_Portal_TraceRay_With( player->m_hPortalEnvironment, ray, MASK_PLAYERSOLID, &traceFilter, &trace, true );
}


//The weapon shoot position of the player
ConVar player_paint_shoot_pos_forward_scale( "player_paint_shoot_pos_forward_scale", "55.0f", FCVAR_REPLICATED );
ConVar player_paint_shoot_pos_right_scale( "player_paint_shoot_pos_right_scale", "12.0f", FCVAR_REPLICATED );
ConVar player_paint_shoot_pos_up_scale( "player_paint_shoot_pos_up_scale", "25.0f", FCVAR_REPLICATED );

#ifndef CLIENT_DLL
ConVar debug_player_paint_shoot_pos( "debug_player_paint_shoot_pos", "0" );
#endif

float CPortal_Player::GetNormalSpeed() const
{
	return m_Local.m_bWearingSuit ? hl2_normspeed.GetFloat() : hl2_walkspeed.GetFloat();
}

Vector CPortal_Player::GetPaintGunShootPosition()
{
	Vector vecEyePosition = EyePosition();

	Vector vecForward, vecRight, vecUp;

	EyeVectors( &vecForward, &vecRight, &vecUp );

	vecForward.NormalizeInPlace();
	vecUp.NormalizeInPlace();
	vecRight.NormalizeInPlace();

	Vector vecMuzzlePos = vecEyePosition +
		( player_paint_shoot_pos_forward_scale.GetFloat() * vecForward ) +
		( player_paint_shoot_pos_right_scale.GetFloat() * vecRight ) -
		( player_paint_shoot_pos_up_scale.GetFloat() * vecUp );

#ifndef CLIENT_DLL
	if( debug_player_paint_shoot_pos.GetBool() )
	{
		NDebugOverlay::Sphere( vecMuzzlePos, 4.0f, 255, 255, 255, false, 0 );
	}
#endif

	return vecMuzzlePos;
}

//-----------------------------------------------------------------------------
// Consider the weapon's built-in accuracy, this character's proficiency with
// the weapon, and the status of the target. Use this information to determine
// how accurately to shoot at the target.
//-----------------------------------------------------------------------------
Vector CPortal_Player::GetAttackSpread( CBaseCombatWeapon *pWeapon, CBaseEntity *pTarget )
{
	if ( pWeapon )
		return pWeapon->GetBulletSpread( WEAPON_PROFICIENCY_PERFECT );
	
	return VECTOR_CONE_15DEGREES;
}

void CPortal_Player::GetStepSoundVelocities( float *velwalk, float *velrun )
{
	// UNDONE: need defined numbers for run, walk, crouch, crouch run velocities!!!!	
	if ( ( GetFlags() & FL_DUCKING ) || ( GetMoveType() == MOVETYPE_LADDER ) )
	{
		*velwalk = 10;		// These constants should be based on cl_movespeedkey * cl_forwardspeed somehow
		*velrun = 60;		
	}
	else
	{
		*velwalk = 90;
		*velrun = 220;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : step - 
//			fvol - 
//			force - force sound to play
//-----------------------------------------------------------------------------
void CPortal_Player::PlayStepSound( Vector &vecOrigin, surfacedata_t *psurface, float fvol, bool force )
{
#ifndef CLIENT_DLL
	IncrementStepsTaken();
#endif

	BaseClass::PlayStepSound( vecOrigin, psurface, fvol, force );
}

Activity CPortal_Player::TranslateActivity( Activity baseAct, bool *pRequired /* = NULL */ )
{
	Activity translated = baseAct;

	if ( GetActiveWeapon() )
	{
		translated = GetActiveWeapon()->ActivityOverride( baseAct, pRequired );
	}
	else if ( unarmedActtable )
	{
		acttable_t *pTable = unarmedActtable;
		int actCount = ARRAYSIZE(unarmedActtable);

		for ( int i = 0; i < actCount; i++, pTable++ )
		{
			if ( baseAct == pTable->baseAct )
			{
				translated = (Activity)pTable->weaponAct;
			}
		}
	}
	else if (pRequired)
	{
		*pRequired = false;
	}

	return translated;
}

CWeaponPortalBase* CPortal_Player::GetActivePortalWeapon() const
{
	CBaseCombatWeapon *pWeapon = GetActiveWeapon();
	if ( pWeapon )
	{
		return dynamic_cast< CWeaponPortalBase* >( pWeapon );
	}
	else
	{
		return NULL;
	}
}

CBaseEntity *CPortal_Player::FindUseEntity()
{
	Vector forward, up;
	EyeVectors( &forward, NULL, &up );

	trace_t tr;
	// Search for objects in a sphere (tests for entities that are not solid, yet still useable)
	Vector searchCenter = EyePosition();

	// NOTE: Some debris objects are useable too, so hit those as well
	// A button, etc. can be made out of clip brushes, make sure it's +useable via a traceline, too.
	int useableContents = MASK_SOLID | CONTENTS_DEBRIS | CONTENTS_PLAYERCLIP;

	UTIL_TraceLine( searchCenter, searchCenter + forward * 1024, useableContents, this, COLLISION_GROUP_NONE, &tr );
	// try the hit entity if there is one, or the ground entity if there isn't.
	CBaseEntity *pNearest = NULL;
	CBaseEntity *pObject = tr.m_pEnt;

	// TODO: Removed because we no longer have ghost animatings. We may need similar code that clips rays against transformed objects.
//#ifndef CLIENT_DLL
//	// Check for ghost animatings (these aren't hit in the normal trace because they aren't solid)
//	if ( !IsUseableEntity(pObject, 0) )
//	{
//		Ray_t rayGhostAnimating;
//		rayGhostAnimating.Init( searchCenter, searchCenter + forward * 1024 );
//
//		CBaseEntity *list[1024];
//		int nCount = UTIL_EntitiesAlongRay( list, 1024, rayGhostAnimating, 0 );
//
//		// Loop through all entities along the pick up ray
//		for ( int i = 0; i < nCount; i++ )
//		{
//			CGhostAnimating *pGhostAnimating = dynamic_cast<CGhostAnimating*>( list[i] );
//
//			// If the entity is a ghost animating
//			if( pGhostAnimating )
//			{
//				trace_t trGhostAnimating;
//				enginetrace->ClipRayToEntity( rayGhostAnimating, MASK_ALL, pGhostAnimating, &trGhostAnimating );
//
//				if ( trGhostAnimating.fraction < tr.fraction )
//				{
//					// If we're not grabbing the clipped ghost
//					VPlane plane = pGhostAnimating->GetLocalClipPlane();
//					UTIL_Portal_PlaneTransform( pGhostAnimating->GetCloneTransform(), plane, plane );
//					if ( plane.GetPointSide( trGhostAnimating.endpos ) != SIDE_FRONT )
//					{
//						tr = trGhostAnimating;
//						pObject = tr.m_pEnt;
//					}
//				}
//			}
//		}
//	}
//#endif

	int count = 0;
	// UNDONE: Might be faster to just fold this range into the sphere query
	const int NUM_TANGENTS = 7;
	while ( !IsUseableEntity(pObject, 0) && count < NUM_TANGENTS)
	{
		// trace a box at successive angles down
		//							45 deg, 30 deg, 20 deg, 15 deg, 10 deg, -10, -15
		const float tangents[NUM_TANGENTS] = { 1, 0.57735026919f, 0.3639702342f, 0.267949192431f, 0.1763269807f, -0.1763269807f, -0.267949192431f };
		Vector down = forward - tangents[count]*up;
		VectorNormalize(down);
		UTIL_TraceHull( searchCenter, searchCenter + down * 72, -Vector(16,16,16), Vector(16,16,16), useableContents, this, COLLISION_GROUP_NONE, &tr );
		pObject = tr.m_pEnt;
		count++;
	}
	float nearestDot = CONE_90_DEGREES;
	if ( IsUseableEntity(pObject, 0) )
	{
		Vector delta = tr.endpos - tr.startpos;
		float centerZ = CollisionProp()->WorldSpaceCenter().z;
		delta.z = IntervalDistance( tr.endpos.z, centerZ + CollisionProp()->OBBMins().z, centerZ + CollisionProp()->OBBMaxs().z );
		float dist = delta.Length();
		if ( dist < PLAYER_USE_RADIUS )
		{
#ifndef CLIENT_DLL

			if ( sv_debug_player_use.GetBool() )
			{
				NDebugOverlay::Line( searchCenter, tr.endpos, 0, 255, 0, true, 30 );
				NDebugOverlay::Cross3D( tr.endpos, 16, 0, 255, 0, true, 30 );
			}

			if ( pObject->MyNPCPointer() && pObject->MyNPCPointer()->IsPlayerAlly( this ) )
			{
				// If about to select an NPC, do a more thorough check to ensure
				// that we're selecting the right one from a group.
				pObject = DoubleCheckUseNPC( pObject, searchCenter, forward );
			}

			g_PortalGameStats.Event_PlayerUsed( searchCenter, forward, pObject );
#endif

			return pObject;
		}
	}

#ifndef CLIENT_DLL
	CBaseEntity *pFoundByTrace = pObject;
#endif

	// check ground entity first
	// if you've got a useable ground entity, then shrink the cone of this search to 45 degrees
	// otherwise, search out in a 90 degree cone (hemisphere)
	if ( GetGroundEntity() && IsUseableEntity(GetGroundEntity(), FCAP_USE_ONGROUND) )
	{
		pNearest = GetGroundEntity();
		nearestDot = CONE_45_DEGREES;
	}

	for ( CEntitySphereQuery sphere( searchCenter, PLAYER_USE_RADIUS ); ( pObject = sphere.GetCurrentEntity() ) != NULL; sphere.NextEntity() )
	{
		if ( !pObject )
			continue;

		if ( !IsUseableEntity( pObject, FCAP_USE_IN_RADIUS ) )
			continue;

		// see if it's more roughly in front of the player than previous guess
		Vector point;
		pObject->CollisionProp()->CalcNearestPoint( searchCenter, &point );

		Vector dir = point - searchCenter;
		VectorNormalize(dir);
		float dot = DotProduct( dir, forward );

		// Need to be looking at the object more or less
		if ( dot < 0.8 )
			continue;

		if ( dot > nearestDot )
		{
			// Since this has purely been a radius search to this point, we now
			// make sure the object isn't behind glass or a grate.
			trace_t trCheckOccluded;
			UTIL_TraceLine( searchCenter, point, useableContents, this, COLLISION_GROUP_NONE, &trCheckOccluded );

			if ( trCheckOccluded.fraction == 1.0 || trCheckOccluded.m_pEnt == pObject )
			{
				pNearest = pObject;
				nearestDot = dot;
			}
		}
	}

#ifndef CLIENT_DLL
	if ( !pNearest )
	{
		// Haven't found anything near the player to use, nor any NPC's at distance.
		// Check to see if the player is trying to select an NPC through a rail, fence, or other 'see-though' volume.
		trace_t trAllies;
		UTIL_TraceLine( searchCenter, searchCenter + forward * PLAYER_USE_RADIUS, MASK_OPAQUE_AND_NPCS, this, COLLISION_GROUP_NONE, &trAllies );

		if ( trAllies.m_pEnt && IsUseableEntity( trAllies.m_pEnt, 0 ) && trAllies.m_pEnt->MyNPCPointer() && trAllies.m_pEnt->MyNPCPointer()->IsPlayerAlly( this ) )
		{
			// This is an NPC, take it!
			pNearest = trAllies.m_pEnt;
		}
	}

	if ( pNearest && pNearest->MyNPCPointer() && pNearest->MyNPCPointer()->IsPlayerAlly( this ) )
	{
		pNearest = DoubleCheckUseNPC( pNearest, searchCenter, forward );
	}

	if ( sv_debug_player_use.GetBool() )
	{
		if ( !pNearest )
		{
			NDebugOverlay::Line( searchCenter, tr.endpos, 255, 0, 0, true, 30 );
			NDebugOverlay::Cross3D( tr.endpos, 16, 255, 0, 0, true, 30 );
		}
		else if ( pNearest == pFoundByTrace )
		{
			NDebugOverlay::Line( searchCenter, tr.endpos, 0, 255, 0, true, 30 );
			NDebugOverlay::Cross3D( tr.endpos, 16, 0, 255, 0, true, 30 );
		}
		else
		{
			NDebugOverlay::Box( pNearest->WorldSpaceCenter(), Vector(-8, -8, -8), Vector(8, 8, 8), 0, 255, 0, true, 30 );
		}
	}

	g_PortalGameStats.Event_PlayerUsed( searchCenter, forward, pNearest );
#endif

	return pNearest;
}

CBaseEntity* CPortal_Player::FindUseEntityThroughPortal( void )
{
	Vector forward, up;
	EyeVectors( &forward, NULL, &up );

	CProp_Portal *pPortal = GetHeldObjectPortal();

	trace_t tr;
	// Search for objects in a sphere (tests for entities that are not solid, yet still useable)
	Vector searchCenter = EyePosition();

	Vector vTransformedForward, vTransformedUp, vTransformedSearchCenter;

	VMatrix matThisToLinked = pPortal->MatrixThisToLinked();
	UTIL_Portal_PointTransform( matThisToLinked, searchCenter, vTransformedSearchCenter );
	UTIL_Portal_VectorTransform( matThisToLinked, forward, vTransformedForward );
	UTIL_Portal_VectorTransform( matThisToLinked, up, vTransformedUp );


	// NOTE: Some debris objects are useable too, so hit those as well
	// A button, etc. can be made out of clip brushes, make sure it's +useable via a traceline, too.
	int useableContents = MASK_SOLID | CONTENTS_DEBRIS | CONTENTS_PLAYERCLIP;

	//UTIL_TraceLine( vTransformedSearchCenter, vTransformedSearchCenter + vTransformedForward * 1024, useableContents, this, COLLISION_GROUP_NONE, &tr );
	Ray_t rayLinked;
	rayLinked.Init( searchCenter, searchCenter + forward * 1024 );
	UTIL_PortalLinked_TraceRay( pPortal, rayLinked, useableContents, this, COLLISION_GROUP_NONE, &tr );

	// try the hit entity if there is one, or the ground entity if there isn't.
	CBaseEntity *pNearest = NULL;
	CBaseEntity *pObject = tr.m_pEnt;
	int count = 0;
	// UNDONE: Might be faster to just fold this range into the sphere query
	const int NUM_TANGENTS = 7;
	while ( !IsUseableEntity(pObject, 0) && count < NUM_TANGENTS)
	{
		// trace a box at successive angles down
		//							45 deg, 30 deg, 20 deg, 15 deg, 10 deg, -10, -15
		const float tangents[NUM_TANGENTS] = { 1, 0.57735026919f, 0.3639702342f, 0.267949192431f, 0.1763269807f, -0.1763269807f, -0.267949192431f };
		Vector down = vTransformedForward - tangents[count]*vTransformedUp;
		VectorNormalize(down);
		UTIL_TraceHull( vTransformedSearchCenter, vTransformedSearchCenter + down * 72, -Vector(16,16,16), Vector(16,16,16), useableContents, this, COLLISION_GROUP_NONE, &tr );
		pObject = tr.m_pEnt;
		count++;
	}
	float nearestDot = CONE_90_DEGREES;
	if ( IsUseableEntity(pObject, 0) )
	{
		Vector delta = tr.endpos - tr.startpos;
		float centerZ = CollisionProp()->WorldSpaceCenter().z;
		delta.z = IntervalDistance( tr.endpos.z, centerZ + CollisionProp()->OBBMins().z, centerZ + CollisionProp()->OBBMaxs().z );
		float dist = delta.Length();
		if ( dist < PLAYER_USE_RADIUS )
		{
#ifndef CLIENT_DLL

			if ( pObject->MyNPCPointer() && pObject->MyNPCPointer()->IsPlayerAlly( this ) )
			{
				// If about to select an NPC, do a more thorough check to ensure
				// that we're selecting the right one from a group.
				pObject = DoubleCheckUseNPC( pObject, vTransformedSearchCenter, vTransformedForward );
			}
#endif

			return pObject;
		}
	}

	// check ground entity first
	// if you've got a useable ground entity, then shrink the cone of this search to 45 degrees
	// otherwise, search out in a 90 degree cone (hemisphere)
	if ( GetGroundEntity() && IsUseableEntity(GetGroundEntity(), FCAP_USE_ONGROUND) )
	{
		pNearest = GetGroundEntity();
		nearestDot = CONE_45_DEGREES;
	}

	for ( CEntitySphereQuery sphere( vTransformedSearchCenter, PLAYER_USE_RADIUS ); ( pObject = sphere.GetCurrentEntity() ) != NULL; sphere.NextEntity() )
	{
		if ( !pObject )
			continue;

		if ( !IsUseableEntity( pObject, FCAP_USE_IN_RADIUS ) )
			continue;

		// see if it's more roughly in front of the player than previous guess
		Vector point;
		pObject->CollisionProp()->CalcNearestPoint( vTransformedSearchCenter, &point );

		Vector dir = point - vTransformedSearchCenter;
		VectorNormalize(dir);
		float dot = DotProduct( dir, vTransformedForward );

		// Need to be looking at the object more or less
		if ( dot < 0.8 )
			continue;

		if ( dot > nearestDot )
		{
			// Since this has purely been a radius search to this point, we now
			// make sure the object isn't behind glass or a grate.
			trace_t trCheckOccluded;
			UTIL_TraceLine( vTransformedSearchCenter, point, useableContents, this, COLLISION_GROUP_NONE, &trCheckOccluded );

			if ( trCheckOccluded.fraction == 1.0 || trCheckOccluded.m_pEnt == pObject )
			{
				pNearest = pObject;
				nearestDot = dot;
			}
		}
	}

#ifndef CLIENT_DLL
	if ( !pNearest )
	{
		// Haven't found anything near the player to use, nor any NPC's at distance.
		// Check to see if the player is trying to select an NPC through a rail, fence, or other 'see-though' volume.
		trace_t trAllies;
		UTIL_TraceLine( vTransformedSearchCenter, vTransformedSearchCenter + vTransformedForward * PLAYER_USE_RADIUS, MASK_OPAQUE_AND_NPCS, this, COLLISION_GROUP_NONE, &trAllies );

		if ( trAllies.m_pEnt && IsUseableEntity( trAllies.m_pEnt, 0 ) && trAllies.m_pEnt->MyNPCPointer() && trAllies.m_pEnt->MyNPCPointer()->IsPlayerAlly( this ) )
		{
			// This is an NPC, take it!
			pNearest = trAllies.m_pEnt;
		}
	}

	if ( pNearest && pNearest->MyNPCPointer() && pNearest->MyNPCPointer()->IsPlayerAlly( this ) )
	{
		pNearest = DoubleCheckUseNPC( pNearest, vTransformedSearchCenter, vTransformedForward );
	}

#endif

	return pNearest;
}


void CPortal_Player::AddSurfacePaintPowerInfo( const BrushContact& contact, char const* context )
{
	AddSurfacePaintPowerInfo( PaintPowerInfo_t( contact.normal,
												contact.point,
												contact.pBrushEntity,
												NO_POWER,
												contact.isOnThinSurface ),
												context );
}


// In Swarm, we're limited by some engine functions that Portal 2 used, but aren't available in Swarm.

// Normal Portal 2 detection, can't detect brush *entities* at the moment.
#define PAINT_DETECTION_BRUSHCONTACTS 3

void CPortal_Player::AddSurfacePaintPowerInfo( const trace_t& trace, char const* context )
{
	if( trace.m_pEnt )
	{
		if( trace.m_pEnt->IsBSPModel() )
		{
			const float DIM_OFFSET = sv_contact_region_thickness.GetFloat();
			const Vector AABB_OFFSET( DIM_OFFSET, DIM_OFFSET, DIM_OFFSET );

			// Compute the contacts with a slightly expanded swept AABB
			const Vector traceDelta = trace.endpos - trace.startpos;
			const Vector contactBoxMin = GetAbsOrigin() + traceDelta + WorldAlignMins() - AABB_OFFSET,
						 contactBoxMax = GetAbsOrigin() + traceDelta + WorldAlignMaxs() + AABB_OFFSET;

			const cplane_t *pClip = (m_hPortalEnvironment != 0 && sv_clip_contacts_to_portals.GetBool()) ? &m_hPortalEnvironment.Get()->m_plane_Origin : NULL;
			const int iClipCount = pClip ? 1 : 0;
			ContactVector contacts;
			ComputeAABBContactsWithBrushEntity( contacts, pClip, iClipCount, WorldSpaceCenter(), contactBoxMin, contactBoxMax, trace.m_pEnt );

			// Fill in the surface power info vector
			const int contactCount = contacts.Count();
			for( int i = 0; i < contactCount; ++i )
			{
				AddSurfacePaintPowerInfo( contacts[i], context );
			}
		}
		else if( trace.plane.normal != vec3_origin )
		{
			AddSurfacePaintPowerInfo( PaintPowerInfo_t( trace.plane.normal,
														UTIL_ProjectPointOntoPlane( trace.endpos, trace.plane ),
														trace.m_pEnt ),
														context );	
		}
	}
}


void CPortal_Player::DeterminePaintContacts()
{
	if( GetMoveType() == MOVETYPE_NOCLIP )
	{
		return;
	}

	const float DIM_OFFSET = sv_contact_region_thickness.GetFloat();
	const Vector AABB_OFFSET( DIM_OFFSET, DIM_OFFSET, DIM_OFFSET );

	// Compute the contacts with a slightly expanded swept AABB
	Vector contactBoxMin = GetAbsOrigin() + WorldAlignMins() - AABB_OFFSET,
		contactBoxMax = GetAbsOrigin() + WorldAlignMaxs() + AABB_OFFSET;
	const Vector traceBoxMin = contactBoxMin, traceBoxMax = contactBoxMax;

	// Compute the displacement and use it to sweep the box
	const Vector displacement = gpGlobals->frametime * m_PortalLocal.m_vPreUpdateVelocity;
	//ExpandAABB( contactBoxMin, contactBoxMax, displacement );

	// HACK: Sweep the box ahead an extra frame while in the air to catch the ground early for
	// trampoline/reflect bounce, so that the velocity doesn't get zeroed out before we can use it.
	float traceLookAheadFactor = 1.0f;
	if( ( sv_press_jump_to_bounce.GetInt() == TRAMPOLINE_BOUNCE || sv_bounce_reflect_enabled.GetBool() ) &&
		GetGroundEntity() == NULL )
	{
		traceLookAheadFactor = 2.0f;
		ExpandAABB( contactBoxMin, contactBoxMax, 2.0f * displacement );
	}

	//NDebugOverlay::Box( Vector(0, 0, 0), contactBoxMin, contactBoxMax, 0, 0, 255, 1, 0 );
	//NDebugOverlay::Box( GetAbsOrigin(), GetHullMins(), GetHullMaxs(), 0, 255, 0, 1, 0 );

	// Find contacts with the world
#ifdef CLIENT_DLL
	CWorld* pWorld = GetClientWorldEntity();
#else
	CWorld* pWorld = GetWorldEntity();
#endif	

	cplane_t *pClip = (m_hPortalEnvironment != 0 && sv_clip_contacts_to_portals.GetBool()) ? &m_hPortalEnvironment.Get()->m_plane_Origin : NULL;
	int iClipCount = pClip ? 1 : 0;
	ContactVector contacts;

	ComputeAABBContactsWithBrushEntity( contacts, pClip, iClipCount, WorldSpaceCenter(), contactBoxMin, contactBoxMax, pWorld );

	// Fill in the surface power info vector
	int contactCount = contacts.Count();
	for( int i = 0; i < contactCount; ++i )
	{
		AddSurfacePaintPowerInfo( contacts[i] );
	}

	// Find contacts with non-world entities
	{
		const Vector& center = WorldSpaceCenter();
		trace_t trace;
		TracePlayerBoxAgainstCollidables( trace, this, center, center + traceLookAheadFactor * displacement, traceBoxMin - center, traceBoxMax - center );

		if( trace.DidHitNonWorldEntity() )
		{
			AddSurfacePaintPowerInfo( trace );		
		}
	}

	// Look ahead for powers the player may want to use
	if( jump_helper_enabled.GetBool() || jump_helper_debug_enabled.GetBool() )
	{
		PredictPaintContacts( contactBoxMin, contactBoxMax, traceBoxMin, traceBoxMax, jump_helper_look_ahead_time.GetFloat(), JUMP_HELPER_CONTEXT );
	}
}


void CPortal_Player::PredictPaintContacts( const Vector& contactBoxMin,
										 const Vector& contactBoxMax,
										 const Vector& traceBoxMin,
										 const Vector& traceBoxMax,
										 float lookAheadTime,
										 char const* context )
{
	const Vector& worldSpaceCenter = WorldSpaceCenter();
	const Vector lookAheadVector = lookAheadTime * m_PortalLocal.m_vPreUpdateVelocity;
	Vector lookAheadDirection = lookAheadVector;
	const float totalDistance = lookAheadDirection.NormalizeInPlace();
	const int numSamples = static_cast<int>( fpmax( totalDistance * paint_power_look_ahead_sample_density.GetFloat(), 1.0f ) );
	const float stepSize = totalDistance / numSamples;
	ContactVector contacts;
	
#ifdef CLIENT_DLL
	CWorld* pWorld = GetClientWorldEntity();
#else
	CWorld* pWorld = GetWorldEntity();
#endif	
	cplane_t *pClip = (m_hPortalEnvironment != 0 && sv_clip_contacts_to_portals.GetBool()) ? &m_hPortalEnvironment.Get()->m_plane_Origin : NULL;
	int iClipCount = pClip ? 1 : 0;

	Vector prevCenter = worldSpaceCenter;
	float distance = stepSize;
	for( int sample = 0; sample < numSamples; distance += stepSize, ++sample )
	{
		// Find precise contacts with the world
		const Vector offset = distance * lookAheadDirection;
		const Vector center = worldSpaceCenter + offset;
		ComputeAABBContactsWithBrushEntity( contacts, pClip, iClipCount, center, contactBoxMin + offset, contactBoxMax + offset, pWorld );

		int contactCount = contacts.Count();
		for( int i = 0; i < contactCount; ++i )
		{
			AddSurfacePaintPowerInfo( contacts[i], context );
		}
		contacts.RemoveAll();

		// Trace for non-world entities
		trace_t trace;
		TracePlayerBoxAgainstCollidables( trace, this, prevCenter, center, traceBoxMin - worldSpaceCenter, traceBoxMax - worldSpaceCenter );

		if( trace.DidHitNonWorldEntity() )
		{
			//DevMsg( "Collided with %s\n", trace.m_pEnt->GetClassname() );
			AddSurfacePaintPowerInfo( trace, context );
		}

		prevCenter = center;
	}
}


PaintPowerState CPortal_Player::ActivateSpeedPower( PaintPowerInfo_t& speedInfo )
{
	// Speed is initially the walking speed or the current tangential speed, whichever is greater
	const Vector& localVelocity = GetLocalVelocity();
	const Vector tangentialVelocity = localVelocity - DotProduct( localVelocity, speedInfo.m_SurfaceNormal ) * speedInfo.m_SurfaceNormal;
	SetMaxSpeed( clamp( tangentialVelocity.Length(), GetNormalSpeed(), sv_speed_paint_max.GetFloat() ) );

	m_flSpeedDecelerationTime = 0.0f;

	return ACTIVE_PAINT_POWER;
}

PaintPowerState CPortal_Player::UseSpeedPower( PaintPowerInfo_t& speedInfo )
{
	// Compute normalized forward direction in tangent plane
	const Vector& vInputVec = GetInputVector();
	const Vector vWishDirection = vInputVec.Normalized();
	const Vector vTangentRight = CrossProduct( vWishDirection, speedInfo.m_SurfaceNormal );
	const Vector vNormTangentForward = CrossProduct( speedInfo.m_SurfaceNormal, vTangentRight ).Normalized();
	const bool bStrafing = fabs( DotProduct( vWishDirection, Left() ) ) > 0.9f;

	// Figure out if the player is moving up a ramp
	const float flWorldUpAngle = vNormTangentForward.z;
	const bool bMovingUpRamp = flWorldUpAngle > 0 && flWorldUpAngle < 1.0f - RELATIVE_ERROR;
	const bool bInAir = GetGroundEntity() == NULL;

	// Compute the change in speed this frame
	float flAcceleration = bInAir ? 0.0f : ( bMovingUpRamp ? sv_speed_paint_ramp_acceleration.GetFloat() : sv_speed_paint_acceleration.GetFloat() );
	flAcceleration *= bStrafing ? sv_speed_paint_straf_accel_scale.GetFloat() : 1.f;
	float fldv = gpGlobals->frametime * flAcceleration;

	// If the player is drastically changing direction, knock the speed back down.
	// If the player is in the air his speed doesn't change
	// Otherwise, scale the change in speed by how consistent it is with the current direction
	Vector vLocalDirection = GetLocalVelocity();
	const float flWishDirSpeed = DotProduct( vLocalDirection, vWishDirection );
	const float flSpeed = vLocalDirection.NormalizeInPlace();
	const float flWishCos = DotProduct( vWishDirection, vLocalDirection );
	const float flSpeedFactor = vInputVec.IsZero() ? -1.0f : flWishCos;

	fldv = (flWishCos < 0.0f) ? ( flWishCos * fabs( flSpeed - GetNormalSpeed() ) ) : (flSpeedFactor * fldv);

	float flDefaultNewMaxSpeed = clamp( MaxSpeed() + fldv,
		GetNormalSpeed(),
		sv_speed_paint_max.GetFloat() );

	// Compute the new max speed
	float flNewMaxSpeed = SpeedPaintAcceleration( flDefaultNewMaxSpeed,
		flSpeed,
		flWishCos,
		flWishDirSpeed );

	// Use the new max speed or the lower bound if the player has stopped moving completely
	SetMaxSpeed( GetLocalVelocity().IsZero() ? GetNormalSpeed() : flNewMaxSpeed );

	return ACTIVE_PAINT_POWER;
}


PaintPowerState CPortal_Player::DeactivateSpeedPower( PaintPowerInfo_t& speedInfo )
{
	const float walkSpeed = GetNormalSpeed();
	const bool onGround = GetGroundEntity() != NULL;
	float newMaxSpeed = 0.f;

	// Deactivate in the air (and touching something besides speed paint)
	if( !onGround && !IsEmptyRange( GetSurfacePaintPowerInfo() ) )
	{
		const Vector& velocity = GetAbsVelocity();
		const Vector tangentialVelocity = velocity - DotProduct( velocity, speedInfo.m_SurfaceNormal ) * speedInfo.m_SurfaceNormal;
		newMaxSpeed = tangentialVelocity.Length();
	}
	// On the ground and not on speed paint or in the air and not touching anything
	else
	{
		if( IsActivePower( GetPaintPower( BOUNCE_POWER ) ) && m_flSpeedDecelerationTime < sv_speed_paint_on_bounce_deceleration_delay.GetFloat() )
		{
			m_flSpeedDecelerationTime += gpGlobals->frametime;
			newMaxSpeed = MaxSpeed();
		}
		else
		{
			// Compute new max speed
			float flDefaultNewMaxSpeed = MaxSpeed() - gpGlobals->frametime * sv_speed_paint_acceleration.GetFloat();
			Vector vLocalDirection = GetLocalVelocity();
			const Vector vWishDirection = GetInputVector().Normalized();
			const float flWishDirSpeed = DotProduct( vLocalDirection, vWishDirection );
			const float flSpeed = vLocalDirection.NormalizeInPlace();
			const float flWishCos = DotProduct( vWishDirection, vLocalDirection );

			newMaxSpeed = SpeedPaintAcceleration( flDefaultNewMaxSpeed,
				flSpeed,
				flWishCos,
				flWishDirSpeed );
		}
	}


	// Set the max speed. Done deactivating if it's normal walking speed
	newMaxSpeed = clamp( newMaxSpeed, walkSpeed, sv_speed_paint_max.GetFloat() );
	SetMaxSpeed( newMaxSpeed );
	return newMaxSpeed == walkSpeed && onGround ? INACTIVE_PAINT_POWER : DEACTIVATING_PAINT_POWER;
}

float CPortal_Player::SpeedPaintAcceleration( float flDefaultMaxSpeed,
											  float flSpeed,
											  float flWishCos,
											  float flWishDirSpeed ) const
{
	// Compute the new max speed and clamp it
	float flNewMaxSpeed = 0.f;

	// If they're not pushing any movement buttons, then cap their maxspeed at their current speed
	if( GetInputVector().IsZero() )
	{
		flNewMaxSpeed = clamp( flNewMaxSpeed, GetNormalSpeed(), flSpeed );
	}
	// If they're trying to move mostly orthogonal to their velocity then penalize their maxspeed
	else if( flWishCos > 0.f && flWishCos < 0.3f )
	{
		// Cap their speed to a point between the speed they're moving in the direction they're pressing and
		// their total speed.
		flNewMaxSpeed = MAX( GetNormalSpeed(), flWishDirSpeed + 0.3f * (flSpeed - flWishDirSpeed) );
	}
	else
	{
		flNewMaxSpeed = flDefaultMaxSpeed;
	}

	return flNewMaxSpeed;
}

bool CPortal_Player::LateSuperJumpIsValid() const
{
	return jump_helper_enabled.GetBool() &&
		IsBouncePower( m_CachedJumpPower ) &&
		gpGlobals->curtime - m_flCachedJumpPowerTime < jump_helper_late_jump_max_time.GetFloat();
}

void CPortal_Player::CachePaintPowerChoiceResults( const PaintPowerChoiceResultArray& choiceInfo )
{
	int const count = MIN( PAINT_POWER_TYPE_COUNT, choiceInfo.Count() );
	if( count == 0 )
	{
		for( int i = 0; i < PAINT_POWER_TYPE_COUNT; ++i )
		{
			m_PortalLocal.m_CachedPaintPowerChoiceResults[i].Initialize();
		}
	}
	else
	{
		for( int i = 0; i < count; ++i )
		{
			bool const wasValid = choiceInfo[i].pPaintPower != NULL;
			m_PortalLocal.m_CachedPaintPowerChoiceResults[i].wasValid = wasValid;
			m_PortalLocal.m_CachedPaintPowerChoiceResults[i].wasIgnored = choiceInfo[i].bWasIgnored;
			if( wasValid )
			{
				m_PortalLocal.m_CachedPaintPowerChoiceResults[i].surfaceEntity = choiceInfo[i].pPaintPower->m_HandleToOther;
				m_PortalLocal.m_CachedPaintPowerChoiceResults[i].surfaceNormal = choiceInfo[i].pPaintPower->m_SurfaceNormal;
			}
		}
	}
}

void CPortal_Player::SetJumpedThisFrame( bool jumped )
{
	if( jumped != m_PortalLocal.m_bJumpedThisFrame )
	{
		//EASY_DIFFPRINT( this, "CPortal_Player::SetJumpedThisFrame() %s", jumped ? "true" : "false" );
	}
	m_PortalLocal.m_bJumpedThisFrame = jumped;
}


bool CPortal_Player::JumpedThisFrame() const
{
	return m_PortalLocal.m_bJumpedThisFrame;
}


void CPortal_Player::SetBouncedThisFrame( bool bounced )
{
	m_PortalLocal.m_bBouncedThisFrame = bounced;
}


bool CPortal_Player::BouncedThisFrame() const
{
	return m_PortalLocal.m_bBouncedThisFrame;
}


InAirState CPortal_Player::GetInAirState() const
{
	return m_PortalLocal.m_InAirState;
}

// Extracted and noinlined to give us a standalone callsite to protect
void UpdatePaintZ( Vector &velocity, float inGravity )
{
	// Find the height the player will go at the current velocity
	// 0 = v0 + g * t
	// t = -v0 / g					- time for gravity to zero velocity, assuming g < 0
	// h = v * t + 0.5 * g * t^2	- height reached
	const float gravityScale = inGravity != 0 ? inGravity : 1.0f;
	const float gravity = -gravityScale * sv_gravity.GetFloat();
	const float time = -velocity.z / gravity;
	const float baseHeight = velocity.z * time + 0.5f * gravity * time * time;

	// Compute how fast to go to reach the same height plus the height boost
	// h = 0.5 * g * t^2			- distance traveled with linear accel (target height)
	// t = sqrt(2.0 * h / g)		- how long to fall h units
	// v = g * t					- velocity at the end (just invert it to jump up that high)
	// v = g * sqrt(2.0 * h / g)
	// v^2 = g * g * 2.0 * h / g
	// v = sqrt(g * 2.0 * h)
	const float targetHeight = baseHeight + bounce_ledge_fall_height_boost.GetFloat();
	velocity.z = sqrt( 2 * -gravity * targetHeight ); // Negate gravity so the value is valid
}

bool CPortal_Player::CheckToUseBouncePower( PaintPowerInfo_t& info )
{	
	// Crouching opts out of using bounce powers
	const bool bIsCrouching = m_Local.m_bDucking || GetFlags() & FL_DUCKING || m_PortalLocal.m_bPreventedCrouchJumpThisFrame;
	const bool bShouldJump = IsTryingToSuperJump( &info );
#ifndef CLIENT_DLL
	// They're going to suppress an auto-bounce
	if( bIsCrouching && bShouldJump && gpGlobals->curtime - m_flLastSuppressedBounceTime > 1.f )
	{
		m_flLastSuppressedBounceTime = gpGlobals->curtime;

		IGameEvent *event = gameeventmanager->CreateEvent("player_suppressed_bounce");
		if( event )
		{
			event->SetInt("userid", GetUserID() );
			gameeventmanager->FireEvent( event );
		}
	}
#endif


	// Trying to use super jump 
	if( bShouldJump && !bIsCrouching )
	{
#ifndef CLIENT_DLL
		//IGameEvent *event = gameeventmanager->CreateEvent( "player_paint_jumped" );
		//if ( event )
		//{
		//	event->SetInt("userid", GetUserID() );
		//	gameeventmanager->FireEvent( event );
		//}

		if( info.m_SurfaceNormal.z > 0.9f )
		{
			// Reset the bounce count if this is a different surface than last time
			const float planeDistance = info.m_ContactPoint.z;
			if( fabs( planeDistance - m_LastGroundBouncePlaneDistance ) > sv_stepsize.GetFloat() )
				m_nBounceCount = 0;

			m_LastGroundBouncePlaneDistance = planeDistance;

			++m_nBounceCount;

			IGameEvent *event = gameeventmanager->CreateEvent("bounce_count");
			if( event )
			{
				event->SetInt("userid", GetUserID() );
				event->SetInt("bouncecount", m_nBounceCount );
				gameeventmanager->FireEvent( event );
			}
		}

		EmitSound( "Player.JumpPowerUse" );
#endif //!CLIENT_DLL

		m_PortalLocal.m_flAirInputScale = BOUNCE_PAINT_INPUT_DAMPING; 

		// In the air now.
		SetBouncedThisFrame( true );
		SetGroundEntity( NULL );

		// Compute velocity in the normal direction
		Vector velocity = m_PortalLocal.m_vPreUpdateVelocity;
		const float normalSpeed = DotProduct( velocity, info.m_SurfaceNormal );

		if ( sv_debug_bounce_reflection.GetBool() )
		{
			NDebugOverlay::VertArrow( info.m_ContactPoint - 50.f * velocity.Normalized(), info.m_ContactPoint, 2, 255, 0, 0, 64, true, sv_debug_bounce_reflection_time.GetFloat() );
		}
		// If the reflected normal velocity will be larger than the minimum bounce speed
		if ( ( sv_bounce_reflect_enabled.GetBool() || sv_press_jump_to_bounce.GetInt() == TRAMPOLINE_BOUNCE ) &&
			  bounce_reflect_restitution.GetFloat() * fabs( normalSpeed ) >= bounce_paint_min_speed.GetFloat() )
		{
			// Reflect the velocity
			velocity = ComputeBouncePostVelocityReflection( velocity, info.m_SurfaceNormal, Vector(0,0,1) );
		}
		else
		{
			// Otherwise, perform a jump
			velocity = ComputeBouncePostVelocityNoReflect( velocity, info.m_SurfaceNormal, Vector(0,0,1) );
		}

		if ( sv_debug_bounce_reflection.GetBool() )
		{
			NDebugOverlay::VertArrow( info.m_ContactPoint, info.m_ContactPoint + 50.f * info.m_SurfaceNormal, 2, 255, 255, 0, 64, true, sv_debug_bounce_reflection_time.GetFloat() );
			NDebugOverlay::VertArrow( info.m_ContactPoint, info.m_ContactPoint + 50.f * Vector(0,0,1), 2, 0, 0, 255, 64, true, sv_debug_bounce_reflection_time.GetFloat() );
			NDebugOverlay::VertArrow( info.m_ContactPoint, info.m_ContactPoint + 50.f * velocity.Normalized(), 2, 0, 255, 0, 64, true, sv_debug_bounce_reflection_time.GetFloat() );
		}

		const bool bounceSurfaceIsGround = AlmostEqual( info.m_SurfaceNormal.z, 1.0f );

		// If the player jumped from the ground
		if( GetInAirState() == ON_GROUND && bounceSurfaceIsGround ) 
		{
			// Add in some bonus lateral velocity for a jump off the ground
			Vector vLateralVelocity = velocity;
			vLateralVelocity.z = 0;
			Vector vMaxLateralVel = vLateralVelocity.Normalized() * (GetNormalSpeed() + GetNormalSpeed() * sv_bounce_paint_forward_velocity_bonus.GetFloat());

			// If our velocity is lower than is allowed with the lateral bonus
			if( velocity.Length2DSqr() < vMaxLateralVel.Length2DSqr() )
			{
				Vector vBonusLateral = vLateralVelocity * sv_bounce_paint_forward_velocity_bonus.GetFloat();

				// Clip extra velocity that would make us go too fast
				if( (vBonusLateral + velocity).Length2DSqr() > vMaxLateralVel.Length2DSqr() )
				{
					vBonusLateral.x = vMaxLateralVel.x - velocity.x;
					vBonusLateral.y = vMaxLateralVel.y - velocity.y;
				}

				velocity += vBonusLateral;
			}
		}

		// Add some height if the player fell off a ledge onto the ground
		if( GetInAirState() == IN_AIR_FELL && bounceSurfaceIsGround )
		{
			UpdatePaintZ( velocity, GetGravity() );
		}

		SetAbsVelocity( velocity );

		if ( info.m_SurfaceNormal.z > 0.75f )
		{
			OnBounced();
		}

		return true;
	}

	return false;
}

float CPortal_Player::PredictedAirTimeEnd( void )
{
	float fSeconds = 0.0f;

	if ( GetGroundEntity() == NULL )
	{
#ifdef CLIENT_DLL
		Vector vPos = GetNetworkOrigin();
#else
		Vector vPos = GetAbsOrigin();
#endif

		trace_t tr;
		tr.fraction = 1.0f;

		const float fStep = 0.4f;

		for ( float i = 0.0f; i < 4.0f && tr.fraction >= 1.0f; i += fStep )
		{
			Vector vEndPos = vPos + ( GetAbsVelocity() * fStep - Vector( 0.0f, 0.0f, sv_gravity.GetFloat() * i ) );
			UTIL_TraceEntity( this, vPos, vEndPos, MASK_PLAYERSOLID, &tr );

			fSeconds += tr.fraction * fStep;
			vPos = vEndPos;
		}

		return fSeconds;
	}
	else
	{
		return 0.0f;
	}
}


PaintPowerState CPortal_Player::ActivateBouncePower( PaintPowerInfo_t& info )
{
	// Not dead
	bool bUsedPower = false;
	if ( !pl.deadflag )
	{
		bUsedPower = CheckToUseBouncePower( info );
	}

	return bUsedPower ? DEACTIVATING_PAINT_POWER : ACTIVE_PAINT_POWER;
}


PaintPowerState CPortal_Player::UseBouncePower( PaintPowerInfo_t& info )
{
	bool bUsedPower = false;

	if( !pl.deadflag )
	{
		bUsedPower = CheckToUseBouncePower( info );
	}

	return bUsedPower ? DEACTIVATING_PAINT_POWER : ACTIVE_PAINT_POWER;
}


PaintPowerState CPortal_Player::DeactivateBouncePower( PaintPowerInfo_t& info )
{
	// TODO: Fix late jumps. This needs to happen on the first frame of deactivation
	//if( jump_helper_enabled.GetBool() )
	//{
	//	// Cache off this jump in case of a late hit
	//	m_flCachedJumpPowerTime = gpGlobals->curtime;
	//	m_CachedJumpPower = info;
	//}

	// Bring input scale back to 1
	return IsEmptyRange( GetSurfacePaintPowerInfo() ) ? INACTIVE_PAINT_POWER : DEACTIVATING_PAINT_POWER;
}

void CPortal_Player::OnBounced( float fTimeOffset )
{
	m_PortalLocal.m_fBouncedTime = gpGlobals->curtime + fTimeOffset;
}

bool CPortal_Player::IsPressingJumpKey() const
{
	return ( m_afButtonForced & IN_JUMP ) ? false : ( m_afButtonPressed & IN_JUMP ) != 0;
}


bool CPortal_Player::IsHoldingJumpKey() const
{
	return ( m_afButtonForced & IN_JUMP ) ? false : ( m_nButtons & IN_JUMP ) != 0;
}


bool CPortal_Player::IsTryingToSuperJump( const PaintPowerInfo_t* pInfo ) const
{
	if( !pInfo )
		return false;

	const int superJumpMode = sv_press_jump_to_bounce.GetInt();

	// For the trampoline bounce mode, the player can bounce if the velocity is significantly toward the surface.
	// Note that this condition requires the player be in the air. That is because we must always detect this case
	// for ground bounces before she gets a ground entity and her z velocity is zeroed out. Requiring this
	// necessary condition prevents misfires while walking on steep slopes, which occur because step code handles
	// movement along z, so the velocity is always in the xy-plane, making the velocity appear to point more toward
	// the surface than it actually does.
	// Long jumps are automatic at sufficient speed, optionally if the player is looking roughly in the direction of movement.
	// Optionally, the player can also super jump if the jump key is pressed or won't jump off a wall while simply walking into it.
	const Vector& velocity = m_PortalLocal.m_vPreUpdateVelocity;
	const float normalVelocity = DotProduct( pInfo->m_SurfaceNormal, velocity );
	const bool isWallJumpFromGround = AlmostEqual(pInfo->m_SurfaceNormal.z, 0.0f );
	const bool isInAir = GetGroundEntity() == NULL;
	const bool canTrampolineBounceOffWall = trampoline_bounce_off_walls_while_on_ground.GetBool() || isInAir;
	const bool spaceBarActivatedTrampolineJump = IsPressingJumpKey() && jump_button_can_activate_trampoline_bounce.GetBool();
	const bool canAutoLongJump = MaxSpeed() > bounce_auto_trigger_min_speed.GetFloat() &&
								 !IsActivatingPower( GetPaintPower( SPEED_POWER ) ) &&
								 !isInAir &&
								 ( !look_dependent_auto_long_jump_enabled.GetBool() || DotProduct( Forward(), velocity.Normalized() ) >= look_dependent_auto_long_jump_min_cos_angle.GetFloat() );
	const bool canTrampolineBounce = ( superJumpMode == TRAMPOLINE_BOUNCE ) &&
									 ( ( normalVelocity < -trampoline_bounce_min_impact_speed.GetFloat() && ( isInAir || pInfo->m_SurfaceNormal.z <= 0.0f ) ) ||
									   ( spaceBarActivatedTrampolineJump ) ||
									   ( canAutoLongJump ) ) &&
									 ( !isWallJumpFromGround || canTrampolineBounceOffWall || spaceBarActivatedTrampolineJump );

	// The conditions for the jump mode are satisfied
	const bool jumpConditionsSatisfied = ( superJumpMode == JUMP_ON_TOUCH ) ||
										 ( superJumpMode == PRESS_JUMP_TO_BOUNCE && IsPressingJumpKey() ) ||
										 ( superJumpMode == HOLD_JUMP_TO_BOUNCE && IsHoldingJumpKey() ) ||
										 ( canTrampolineBounce );

	// Should perform a late super jump
	const bool jumpHelperShouldLateJump = jumpConditionsSatisfied && LateSuperJumpIsValid();

	// If they're legitimately jumping, or performing a late jump but not crouching.
	return (jumpConditionsSatisfied || jumpHelperShouldLateJump);	   
}

inline bool IsBetterPaintPowerVelocityInputScore( float flCurrentBestInputCos,
												 float flCurrentBestVelocityCos,
												 float flQueryInputCos,
												 float flQueryVelocityCos )
{
	return (flCurrentBestInputCos - flQueryInputCos > EQUAL_EPSILON && fabs(flQueryInputCos) > DEAD_INPUT_COS) ||	// Pushing most towards this one and pushing toward it significantly
		(flCurrentBestInputCos >= 0 && flCurrentBestVelocityCos - flQueryInputCos > EQUAL_EPSILON );				// Not pushing toward anything yet, but moving most toward this one
}

float ComputeHullWidth( const Vector& hullMins, const Vector& hullMaxs )
{
	Vector diffInPlane = hullMaxs - hullMins;
	return fabs( diffInPlane[ diffInPlane.LargestComponent() ] );
}

void CPortal_Player::ChooseBestPaintPowersInRange( PaintPowerChoiceResultArray& bestPowers,
												 PaintPowerConstIter begin,
												 PaintPowerConstIter end,
												 const PaintPowerChoiceCriteria_t& choiceCriteria ) const
{
	// Note: Valid input and velocity cosines should all be in the range [-1, 1), but we don't really want anything close to 1.
	// Theoretically, 1.0f would be an appropriate default value. However, in the case where the player jumps from a standstill
	// (no WASD input), she will not get a power on the bottom of a projected wall because we don't know about the contact until
	// after she starts falling, making the velocity cosine precisely 1.0f. Cases like this are the only reason I'm making the
	// default value 2.0f here. Feel free to change it if this is no longer the case. The input cosine default remains unchanged
	// because it should always reflect the actual input intentions. If its value is undesirable, velocity takes over.
	// -Ted
	bestPowers.EnsureCount( PAINT_POWER_TYPE_COUNT_PLUS_NO_POWER );
	PaintPowerChoiceResult_t bestIgnoredPowers[PAINT_POWER_TYPE_COUNT_PLUS_NO_POWER];
	for( unsigned i = 0; i < PAINT_POWER_TYPE_COUNT_PLUS_NO_POWER; ++i )
	{
		bestPowers[i].Initialize();
		bestIgnoredPowers[i].Initialize();
	}
	
	// Iterate through all the surfaces to see if there's a more appropriate power to use
	for( PaintPowerConstIter iter = begin; iter != end; ++iter )
	{
		const PaintPowerInfo_t& powerInfo = *iter;
		const unsigned powerIndex = powerInfo.m_PaintPowerType;

		AssertMsg( powerIndex < PAINT_POWER_TYPE_COUNT_PLUS_NO_POWER, "Invalid paint power may cause out-of-bounds array access!" );

		// How much the player is pushing toward this surface and how different the normal is, respectively
		const float flInputCos = DotProduct( choiceCriteria.vNormInputDir, powerInfo.m_SurfaceNormal );
		const float flVelocityCos = DotProduct( choiceCriteria.vNormVelocity, powerInfo.m_SurfaceNormal );

		if( show_player_paint_power_debug.GetBool() )
		{
			DevMsg( "Paint Power #%d\n", iter - begin );
			PrintPowerInfoDebugMsg( powerInfo );
			DevMsg( "\n" );
			DevMsg( "Input Cosine: %f\n", flInputCos );
			DevMsg( "Velocity Cosine: %f\n", flVelocityCos );
			DevMsg("------------------------------\n");
		}

		// If this is the best choice so far
		if( powerIndex < PAINT_POWER_TYPE_COUNT_PLUS_NO_POWER &&
			IsBetterPaintPowerVelocityInputScore( bestPowers[powerIndex].flInputCos,
												  bestPowers[powerIndex].flVelocityCos,
												  flInputCos,
												  flVelocityCos ) )
		{
			bool ignorePower = false;

			// Special handling if you're in a portal environment
			if( choiceCriteria.bInPortal )
			{
				const Vector& portalNormal = m_hPortalEnvironment.Get()->m_plane_Origin.normal;
				const Vector& powerNormal = powerInfo.m_SurfaceNormal;

				const float normCosAngle = DotProduct( portalNormal, powerNormal );
				const float inputCosAngle = DotProduct( portalNormal, choiceCriteria.vNormInputDir);

				const bool nearPortal = UTIL_PointIsNearPortal( powerInfo.m_ContactPoint, m_hPortalEnvironment, PORTAL_PLANE_IGNORE_EPSILON, 0.5f * ComputeHullWidth( VEC_HULL_MIN, VEC_HULL_MAX ) - 5.0f );
				const bool aboutParallel = CloseEnough( normCosAngle, 1.0f );
				const bool pushingTowardPortal = inputCosAngle + RELATIVE_ERROR < 0;

				// If it's no power on a portal surface, it doesn't matter
				ignorePower |= nearPortal && IsNoPower( powerInfo ) && aboutParallel;

				// If it's bounce or speed, the contact point is near the portal, and the normals are parallel or player is moving toward the portal.
				ignorePower |= nearPortal &&
							   ( IsBouncePower( powerInfo ) || IsSpeedPower( powerInfo ) ) &&
							   ( aboutParallel || pushingTowardPortal );
			}

			// If the the player already has a power which is neither stick nor is on the ground that she's moving (but not pushing) toward and this
			// is no power, just keep the power she has already.
			//ignorePower |= IsNoPower( powerInfo ) && pBestPower != 0 &&
			//			   (!IsStickPower( *pBestPower ) || pBestPower->m_SurfaceNormal.z >= STEEP_SLOPE) &&
			//			   fBestInputCos >= 0.0f && fBestVelocityCos < 0.0f;

			// Don't let speed paint on surfaces the player isn't standing on affect her, unless she's trying to leave stick paint and go
			// to a speed surface.
			const bool walkingOffStick = 1 < STEEP_SLOPE && powerInfo.m_SurfaceNormal.z > STEEP_SLOPE;
			ignorePower |= !walkingOffStick && IsSpeedPower(powerInfo) && powerInfo.m_SurfaceNormal.z < STEEP_SLOPE;

			// For trampoline bounce, ignore surfaces the player isn't trying to use, so if the player pushes into a wall
			// she can't use, it doesn't take priority over the ground she's falling onto. This doesn't change anything
			// in any other bounce mode, since the power will never activate if the player isn't trying to use it.
			ignorePower |= !walkingOffStick && IsBouncePower( powerInfo ) && !IsTryingToSuperJump( &powerInfo );

			// Update the current best power
			PaintPowerChoiceResult_t& result = ignorePower ? bestIgnoredPowers[powerIndex] : bestPowers[powerIndex];

			// Make sure that this is actually better than the power to overwrite
			// Note: This is to ensure the ignored powers are consistent but have no effect on the actual best powers
			if( !ignorePower || IsBetterPaintPowerVelocityInputScore( result.flInputCos, result.flVelocityCos, flInputCos, flVelocityCos ) )
			{
				result.pPaintPower = iter;
				result.flInputCos = flInputCos;
				result.flVelocityCos = flVelocityCos;
				result.bWasIgnored = ignorePower;
			}
		}
	}

	// If any of the best powers were never set, replace it with the best ignored power of the same type
	for( unsigned i = 0; i < PAINT_POWER_TYPE_COUNT_PLUS_NO_POWER; ++i )
	{
		if( bestPowers[i].pPaintPower == NULL )
			bestPowers[i] = bestIgnoredPowers[i];
	}
}

//-----------------------------------------------------------------------------
// Paint Power User Implementation
//-----------------------------------------------------------------------------
void CPortal_Player::ChooseActivePaintPowers( PaintPowerInfoVector& activePowers )
{
	// Get all world contacts
	DeterminePaintContacts();

	// Cached stick power and new power
	PaintPowerChoiceResultArray bestTouchedPowers;
	PaintPowerConstRange touchedPowerRange = GetSurfacePaintPowerInfo();

	// Criteria to choose the best power
	PaintPowerChoiceCriteria_t choiceCriteria;
	choiceCriteria.bInPortal = m_hPortalEnvironment != 0;

	// If the player is touching anything
	if( HasAnySurfacePaintPowerInfo() || LateSuperJumpIsValid() )
	{
		// Figure out colors/powers
		MapSurfacesToPowers();

		// Sort the surfaces by priority
		PrioritySortSurfacePaintPowerInfo( &DescendingPaintPriorityCompare );

		// Compute the absolute velocity with the change due to gravity
		const float gravityScale = (GetGravity() != 0) ? GetGravity() : 1.0f;
		const float gravityMagnitude = -(gravityScale * sv_gravity.GetFloat() * gpGlobals->frametime);
		const Vector dvStickGravity(0,0, gravityMagnitude);
		choiceCriteria.vNormVelocity = (GetAbsVelocity() + dvStickGravity).Normalized();

		// The normalized input direction
		choiceCriteria.vNormInputDir = GetInputVector().Normalized();

		const char *dllString;
#ifdef CLIENT_DLL
		dllString = "Client";
#else
		dllString = "Server";
#endif

		if( show_player_paint_power_debug.GetBool() )
		{
			DevMsg( "DLL: %s\n", dllString );
			DevMsg( "Norm Input: (%f, %f, %f)\n", XYZ( choiceCriteria.vNormInputDir ) );
			DevMsg( "Abs Velocity: (%f, %f, %f)\n", XYZ( GetAbsVelocity() ) );
			//DevMsg( "Stick Gravity: (%f, %f, %f)\n", XYZ( dvStickGravity ) );
			DevMsg( "Norm Velocity: (%f, %f, %f)\n", XYZ( choiceCriteria.vNormVelocity ) );
			DevMsg("------------------------------\n");
		}

		// Get all the paint power info of the surfaces the player is touching
		ChooseBestPaintPowersInRange( bestTouchedPowers, touchedPowerRange.first, touchedPowerRange.second, choiceCriteria );
		if( show_player_paint_power_debug.GetBool() )
		{
			DevMsg( "Context: Touched\n" );
			DevMsg( "Paint Power Count: %d\n", touchedPowerRange.second - touchedPowerRange.first );
			DevMsg("------------------------------\n");
		}

		// Trying to jump and jump helper is enabled
		const bool bJumpHelperEnabled = jump_helper_enabled.GetBool();
		const bool bIsPressingJumpKey = IsPressingJumpKey();
		const PaintPowerInfo_t* pBestLookAheadSuperJumpPower = 0;
		PaintPowerConstRange jumpHelperLookAheadPowerRange = GetSurfacePaintPowerInfo( JUMP_HELPER_CONTEXT );
		if( bJumpHelperEnabled )
		{
			// Check the look ahead list for the super jump power
			PaintPowerChoiceResultArray bestJumpHelperLookAheadPowers;
			ChooseBestPaintPowersInRange( bestJumpHelperLookAheadPowers, jumpHelperLookAheadPowerRange.first, jumpHelperLookAheadPowerRange.second, choiceCriteria );
			const PaintPowerInfo_t* pBestTouchedSuperJumpPower = bestTouchedPowers[BOUNCE_POWER].pPaintPower;
			pBestLookAheadSuperJumpPower = bestJumpHelperLookAheadPowers[BOUNCE_POWER].pPaintPower;

			if( bIsPressingJumpKey )
			{
				if( show_player_paint_power_debug.GetBool() )
				{
					DevMsg( "Context: %s\n", JUMP_HELPER_CONTEXT );
					DevMsg( "Paint Power Count: %d\n", touchedPowerRange.second - touchedPowerRange.first );
					DevMsg("------------------------------\n");
				}

				// Didn't get a touched super jump
				if( !pBestTouchedSuperJumpPower )
				{
					// Got a predicted super jump
					if( pBestLookAheadSuperJumpPower && IsBouncePower( *pBestLookAheadSuperJumpPower ) )
					{
						if( GetGroundEntity() )
						{
							// Early jump from the ground
							bestTouchedPowers[BOUNCE_POWER].pPaintPower = pBestLookAheadSuperJumpPower;
							bestTouchedPowers[BOUNCE_POWER].bWasIgnored = false;
						}
					}
					// Got a late super jump
					else if( LateSuperJumpIsValid() )
					{
						// Use the late super jump
						bestTouchedPowers[BOUNCE_POWER].pPaintPower = &m_CachedJumpPower;
						bestTouchedPowers[BOUNCE_POWER].bWasIgnored = false;
						m_flCachedJumpPowerTime = -FLT_MAX;
					}
				}
			}
		}

		// Jump helper debug draw
		if( jump_helper_debug_enabled.GetBool() )
		{
			const float duration = bIsPressingJumpKey ? 5.0f : 0.0f;
			//DrawJumpHelperDebug( jumpHelperLookAheadPowerRange.first, jumpHelperLookAheadPowerRange.second, duration, false, bestTouchedPowers[BOUNCE_POWER].pPaintPower );

			// Late hit was available
			if( LateSuperJumpIsValid() )
			{
				if( bestTouchedPowers[BOUNCE_POWER].pPaintPower == &m_CachedJumpPower )
					DrawPaintPowerContactInfo( m_CachedJumpPower, Color( 255, 0, 255 ), duration, false );
				else if( IsBouncePower( m_CachedJumpPower ) )
					DrawPaintPowerContactInfo( m_CachedJumpPower, Color( 255, 0, 0 ), duration, false );
			}

			// If we got a new bounce power, draw it in case it was a surface we actually touched.
			if( bestTouchedPowers[BOUNCE_POWER].pPaintPower )
				DrawPaintPowerContactInfo( *bestTouchedPowers[BOUNCE_POWER].pPaintPower, Color( 255, 0, 255 ), duration, false );

			if( bestTouchedPowers[BOUNCE_POWER].pPaintPower )
			{
				IHandleEntity* pHandleEnt = bestTouchedPowers[BOUNCE_POWER].pPaintPower->m_HandleToOther.Get();
				CBaseEntity* pBaseEnt = pHandleEnt ? EntityFromEntityHandle( pHandleEnt ) : 0;
				char const* entName;
				entName = pBaseEnt ? pBaseEnt->GetClassname() : "";

				if( bestTouchedPowers[BOUNCE_POWER].pPaintPower == pBestLookAheadSuperJumpPower )
					DevMsg( "Early jump off of %s!\n", entName );
				else if( bestTouchedPowers[BOUNCE_POWER].pPaintPower == &m_CachedJumpPower )
					DevMsg( "Late jump off of %s!\n", entName );
				else if( IsTryingToSuperJump( bestTouchedPowers[BOUNCE_POWER].pPaintPower ) )
					DevMsg( "Punctual jump off of %s!\n", entName );
			}
		}
	}

	// Play transition sounds of walking onto/off of paint
	PlayPaintSounds( bestTouchedPowers );

	// Set the new powers if we found one
	for( int i = 0; i < bestTouchedPowers.Count(); ++i )
	{
		const PaintPowerInfo_t* pNewPower = bestTouchedPowers[i].pPaintPower;
		if( pNewPower )
		{
			//#ifndef CLIENT_DLL
			//// Only send the event if the powers are actually different
			//if( AreDifferentPowers( *pNewPower, GetPaintPower( pNewPower->m_PaintPowerType ) ) )
			//{
			//	IGameEvent *event = gameeventmanager->CreateEvent( "touched_paint" );
			//	if ( event )
			//	{
			//		event->SetInt("userid", GetUserID() );
			//		event->SetInt( "painttype", pNewPower->m_PaintPowerType );

			//		PaintSurfaceType surfaceType;
			//		if( pNewPower->m_SurfaceNormal.z > 0.5f )
			//		{
			//			surfaceType = FLOOR_SURFACE;
			//		}
			//		else if( pNewPower->m_SurfaceNormal.z < -0.5f )
			//		{
			//			surfaceType = CEILING_SURFACE;
			//		}
			//		else
			//		{
			//			surfaceType = WALL_SURFACE;
			//		}
			//		event->SetInt( "surfacedir", surfaceType );

			//		gameeventmanager->FireEvent( event );
			//	}
			//}
			//#endif //!CLIENT_DLL

			if( !bestTouchedPowers[i].bWasIgnored )
				activePowers.AddToTail( *pNewPower );
		}
	}

	// Cache off the choice results
	CachePaintPowerChoiceResults( bestTouchedPowers );

	// If the player is paintable and has a painted power, try to use it
	if( player_can_use_painted_power.GetBool() && m_PortalLocal.m_PaintedPowerType != NO_POWER )
	{
		// Copy all the surface data, and replace the powers with the painted power
		PaintPowerInfoVector paintedPowerQuery;
		paintedPowerQuery.EnsureCapacity( GetCountFromRange( touchedPowerRange ) );
		for( PaintPowerConstIter i = touchedPowerRange.first; i != touchedPowerRange.second; ++i )
		{
			if( i->m_PaintPowerType != INVALID_PAINT_POWER )
			{
				PaintPowerInfo_t paintedPower = *i;
				paintedPower.m_PaintPowerType = m_PortalLocal.m_PaintedPowerType;
				paintedPowerQuery.AddToTail( paintedPower );
			}
		}
		
		// Choose the best surface to use the painted power on
		PaintPowerChoiceResultArray bestPaintedPowers;
		PaintPowerChoiceCriteria_t paintedChoiceCriteria = choiceCriteria;
		ChooseBestPaintPowersInRange( bestPaintedPowers,
			GetConstBegin( paintedPowerQuery ),
			GetConstEnd( paintedPowerQuery ),
			paintedChoiceCriteria );

		// If they weren't all ignored, give the player her painted power
		if( bestPaintedPowers[m_PortalLocal.m_PaintedPowerType].pPaintPower &&
			!bestPaintedPowers[m_PortalLocal.m_PaintedPowerType].bWasIgnored )
		{
			activePowers.AddToTail( *bestPaintedPowers[m_PortalLocal.m_PaintedPowerType].pPaintPower );
		}
	}

	if( show_player_paint_power_debug.GetBool() )
	{
		DevMsg( "Active Powers:\n" );
		for( int i = 0; i < activePowers.Count(); ++i )
		{
			DevMsg( "%s Power\n", PowerTypeToString( activePowers[i] ) );
		}
		DevMsg("==============================\n");
	}
}


ConVar sv_paint_trigger_sound_delay( "sv_paint_trigger_sound_delay", "0.1f", FCVAR_REPLICATED );
void CPortal_Player::PlayPaintSounds( const PaintPowerChoiceResultArray& touchedPowers )
{
	const char* const soundList[] =
	{
		"Player.EnterBouncePaint",
		"Player.ExitBouncePaint",
		"Player.EnterStickPaint",
		"Player.ExitStickPaint",
		"Player.EnterSpeedPaint",
		"Player.ExitSpeedPaint"
	};

	CRecipientFilter filter;
	filter.AddRecipient( this );

	bool isTouchingPower[PAINT_POWER_TYPE_COUNT - 1] = { false };
	isTouchingPower[BOUNCE_POWER] = BOUNCE_POWER < touchedPowers.Count() &&
									touchedPowers[BOUNCE_POWER].pPaintPower != NULL;
	isTouchingPower[SPEED_POWER] = SPEED_POWER < touchedPowers.Count() &&
								   touchedPowers[SPEED_POWER].pPaintPower != NULL &&
								   !touchedPowers[SPEED_POWER].bWasIgnored;

	// Go through all usable powers and check if we just stepped onto or off of it
	for( int i = 0; i < PAINT_POWER_TYPE_COUNT - 1; ++i )
	{
		// If we're on this power
		if( isTouchingPower[i] )
		{
			// Just stepping onto this power
			if( m_flTimeSinceLastTouchedPower[i] > sv_paint_trigger_sound_delay.GetFloat() )
			{
				EmitSound( filter, this->entindex(), soundList[i*2] );
			}

			// Reset timer
			m_flTimeSinceLastTouchedPower[i] = 0.f;
		}
		// Not on this power
		else
		{
			// Stepping off of this power.  Use a delay so we don't deactivate when we very briefly lose a power
			if( m_flTimeSinceLastTouchedPower[i] < sv_paint_trigger_sound_delay.GetFloat() && 
				m_flTimeSinceLastTouchedPower[i] + gpGlobals->frametime >= sv_paint_trigger_sound_delay.GetFloat() )
			{
				EmitSound( filter, this->entindex(), soundList[i*2+1] );
			}

			// Increase timer
			m_flTimeSinceLastTouchedPower[i] += gpGlobals->frametime;
		}
	}
}



const Vector& CPortal_Player::GetInputVector() const
{
	return m_vInputVector;
}


void CPortal_Player::SetInputVector( const Vector& vInput )
{
	m_vInputVector = vInput;
}