//========= Copyright © Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//==========================================================================//
#include "cbase.h"
#include "isaverestore.h"
#include "paint_stream_manager.h"

#ifdef GAME_DLL

#include "paint_database.h"
#else

#include "c_world.h"

#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

class CPaintSaveRestoreBlockHandler : public CDefSaveRestoreBlockHandler
{
	virtual const char *GetBlockName() { return "PaintDatabase"; };

	virtual void PreSave( CSaveRestoreData * ) {}

	virtual void Save( ISave *pSave )
	{
		if ( !HASPAINTMAP )
		{
			return;
		}

#ifdef GAME_DLL
		// save paintmap data
		PaintDatabase.SavePaintmapData( pSave );
#endif
	}

	virtual void WriteSaveHeaders( ISave * ) {}

	virtual void PostSave() {}

	virtual void PreRestore()
	{
		if ( !HASPAINTMAP )
		{
			return;
		}

#ifdef GAME_DLL
		PaintDatabase.RemoveAllPaint();
#endif
	}

	virtual void ReadRestoreHeaders( IRestore * ) {}
	
	virtual void Restore( IRestore *pRestore, bool fCreatePlayers )
	{
		if ( !HASPAINTMAP )
		{
			return;
		}

#ifdef GAME_DLL
		// restore paintmap data
		PaintDatabase.RestorePaintmapData( pRestore );
#endif
	}

	virtual void PostRestore()
	{
	}
};

CPaintSaveRestoreBlockHandler g_PaintSaveRestoreBlockHandler;

ISaveRestoreBlockHandler *GetPaintSaveRestoreBlockHandler()
{
	return &g_PaintSaveRestoreBlockHandler;
}
