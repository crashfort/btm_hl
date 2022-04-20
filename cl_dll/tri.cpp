//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

// Triangle rendering, if any

#include "winsani_in.h"
#include <windows.h>
#include "winsani_out.h"
#include <gl/gl.h>

#include "hud.h"
#include "cl_util.h"

// Triangle rendering apis are in gEngfuncs.pTriAPI

#include "const.h"
#include "entity_state.h"
#include "cl_entity.h"
#include "triangleapi.h"
#include "Exports.h"

#include "particleman.h"
#include "r_studioint.h"
#include "tri.h"
#include "CEnvironment.h"

extern IParticleMan *g_pParticleMan;

extern float g_iFogColor[ 4 ];
extern float g_iStartDist;
extern float g_iEndDist;
extern int g_iWaterLevel;
extern vec3_t FogColor;

extern engine_studio_api_t IEngineStudio;

void BlackFog( void )
{
    static float fColorBlack[ 3 ] = { 0,0,0 };
    bool bFog = g_iEndDist > g_iStartDist;
    if( bFog )
        gEngfuncs.pTriAPI->Fog( fColorBlack, g_iStartDist, g_iEndDist, bFog );
    else
        gEngfuncs.pTriAPI->Fog( g_iFogColor, g_iStartDist, g_iEndDist, bFog );
}

void RenderFog( void )
{
    float g_iFogColor[4] = { FogColor.x, FogColor.y, FogColor.z, 1.0 };
    bool bFog = g_iEndDist > g_iStartDist;
    if (bFog)
    {
        if (IEngineStudio.IsHardware() == 2)
        {
            gEngfuncs.pTriAPI->Fog(g_iFogColor, g_iStartDist, g_iEndDist, bFog);
        }
        else if (IEngineStudio.IsHardware() == 1)
        {
            glEnable(GL_FOG);
            glFogi(GL_FOG_MODE, GL_EXP);
            glFogfv(GL_FOG_COLOR, g_iFogColor);
            glFogf(GL_FOG_DENSITY, 0.0020f);
            glHint(GL_FOG_HINT, GL_DONT_CARE);
            glFogf(GL_FOG_START, 1500);
            glFogf(GL_FOG_END, 2000);
        }
    }
}

static HSPRITE jet_white_spr = 0;
static model_s* jet_white_spr_model;

void JetHudVidInit()
{
    jet_white_spr = gEngfuncs.pfnSPR_Load("sprites/white.spr");
    jet_white_spr_model = const_cast<model_s*>(gEngfuncs.GetSpritePointer(jet_white_spr));
}

// Show crosshair in world space to make aiming easier.
void DrawJetCrosshair()
{
    cl_entity_t* player = gEngfuncs.GetLocalPlayer();

    Vector aim_angles = player->angles;
    aim_angles[0] = -aim_angles[0];

    Vector forward;
    Vector up;
    gEngfuncs.pfnAngleVectors(aim_angles, forward, NULL, up);

    Vector crosshair_pos = player->origin + (forward * 4096.0f) + (up * -64); // Use same logic for shooting the rockets in player.cpp.

    gEngfuncs.pTriAPI->SpriteTexture(jet_white_spr_model, 0);
    gEngfuncs.pTriAPI->RenderMode(kRenderNormal);
    gEngfuncs.pTriAPI->CullFace(TRI_NONE);
    gEngfuncs.pTriAPI->Color4f(1.0f, 1.0f, 1.0f, 1.0f);
    gEngfuncs.pTriAPI->Begin(TRI_LINES);
    gEngfuncs.pTriAPI->Vertex3fv(player->origin);
    gEngfuncs.pTriAPI->Vertex3fv(crosshair_pos);
    gEngfuncs.pTriAPI->End();
}

void DrawJetHud()
{
    cl_entity_t* player = gEngfuncs.GetLocalPlayer();

    // Show jet speed so we can know when we are hovering.

    float PM_JetSpeed(int player);
    float jet_speed = PM_JetSpeed(player->index);

    char buf[64];
    snprintf(buf, 64, "Speed: %d", (int)(jet_speed + 0.5f));
    gEngfuncs.pfnDrawString(300, 300, buf, 255, 255, 255);
}

/*
=================
HUD_DrawNormalTriangles

Non-transparent triangles-- add them here
=================
*/
void DLLEXPORT HUD_DrawNormalTriangles( void )
{
//	RecClDrawNormalTriangles();

    if (atoi(gEngfuncs.PhysInfo_ValueForKey("jet")))
    {
        DrawJetCrosshair();
    }

    RenderFog();

	gHUD.m_Spectator.DrawOverview();
}

#if defined( _TFC )
void RunEventList( void );
#endif

/*
=================
HUD_DrawTransparentTriangles

Render any triangles with transparent rendermode needs here
=================
*/
void DLLEXPORT HUD_DrawTransparentTriangles( void )
{
//	RecClDrawTransparentTriangles();

#if defined( _TFC )
	RunEventList();
#endif

    BlackFog();

	if ( g_pParticleMan )
    {
        g_pParticleMan->Update();
        g_Environment.Update();
    }
}

void HUD_DrawOrthoTriangles()
{
    if (atoi(gEngfuncs.PhysInfo_ValueForKey("jet")))
    {
        DrawJetHud();
    }
}
