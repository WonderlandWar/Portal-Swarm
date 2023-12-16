//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef C_PORTALGHOSTRENDERABLE_H
#define C_PORTALGHOSTRENDERABLE_H

#ifdef _WIN32
#pragma once
#endif

//#include "iclientrenderable.h"
#include "c_baseanimating.h"

class C_PortalGhostRenderable : public C_BaseAnimating//IClientRenderable, public IClientUnknown
{
public:
	C_BaseEntity *m_pGhostedRenderable; //the renderable we're transforming and re-rendering
	
	VMatrix m_matGhostTransform;
	float *m_pSharedRenderClipPlane; //shared by all portal ghost renderables within the same portal
	bool m_bLocalPlayer; //special draw rules for the local player
	bool m_bSourceIsBaseAnimating;
	bool m_bCombatWeapon;
	bool m_bCombatWeaponWorldClone; //not actually derived from C_BaseCombatWeapon, but shares some of the same hacks
	C_Prop_Portal *m_pOwningPortal;

	struct
	{
		Vector vRenderOrigin;
		QAngle qRenderAngle;
		matrix3x4_t matRenderableToWorldTransform;
	} m_ReferencedReturns; //when returning a reference, it has to actually exist somewhere

	C_PortalGhostRenderable( C_Prop_Portal *pOwningPortal, C_BaseEntity *pGhostSource, const VMatrix &matGhostTransform, float *pSharedRenderClipPlane, bool bLocalPlayer );
	virtual ~C_PortalGhostRenderable( void );

	void PerFrameUpdate( void ); //called once per frame for misc updating

	// Data accessors
	virtual Vector const&			GetRenderOrigin( void );
	virtual QAngle const&			GetRenderAngles( void );
	virtual bool					ShouldDraw( void ) { return true; }

	// Call this to get the current bone transforms for the model.
	// currentTime parameter will affect interpolation
	// nMaxBones specifies how many matrices pBoneToWorldOut can hold. (Should be greater than or
	// equal to studiohdr_t::numbones. Use MAXSTUDIOBONES to be safe.)
	virtual bool	SetupBones( matrix3x4a_t *pBoneToWorldOut, int nMaxBones, int boneMask, float currentTime );

	// Returns the bounds relative to the origin (render bounds)
	virtual void	GetRenderBounds( Vector& mins, Vector& maxs );

	// returns the bounds as an AABB in worldspace
	virtual void	GetRenderBoundsWorldspace( Vector& mins, Vector& maxs );

	// These normally call through to GetRenderAngles/GetRenderBounds, but some entities custom implement them.
	virtual void	GetShadowRenderBounds( Vector &mins, Vector &maxs, ShadowType_t shadowType );

	// These methods return true if we want a per-renderable shadow cast direction + distance
	//virtual bool	GetShadowCastDistance( float *pDist, ShadowType_t shadowType ) const;
	//virtual bool	GetShadowCastDirection( Vector *pDirection, ShadowType_t shadowType ) const;

	// Returns the transform from RenderOrigin/RenderAngles to world
	virtual const matrix3x4_t &RenderableToWorldTransform();

	// Attachments
	virtual	bool GetAttachment( int number, Vector &origin, QAngle &angles );
	virtual bool GetAttachment( int number, matrix3x4_t &matrix );
	virtual bool GetAttachment( int number, Vector &origin );
	virtual bool GetAttachmentVelocity( int number, Vector &originVel, Quaternion &angleVel );

	// Rendering clip plane, should be 4 floats, return value of NULL indicates a disabled render clip plane
	virtual float *GetRenderClipPlane( void ) { return m_pSharedRenderClipPlane; };
	
	virtual IClientModelRenderable*	GetClientModelRenderable() { return NULL; }
	virtual int	DrawModel( int flags, const RenderableInstance_t &instance );

	// Get the model instance of the ghosted model so that decals will properly draw across portals
	virtual ModelInstanceHandle_t GetModelInstance();



	//------------------------------------------
	//IClientRenderable - Trivial or redirection
	//------------------------------------------
	virtual IClientUnknown*			GetIClientUnknown() { return this; };
	virtual void					GetColorModulation( float* color );
	virtual int						LookupAttachment( const char *pAttachmentName );
	
	virtual RenderableTranslucencyType_t	ComputeTranslucencyType( void );
	virtual int						GetRenderFlags();

};

#endif //#ifndef C_PORTALGHOSTRENDERABLE_H