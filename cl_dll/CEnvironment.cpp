#include "hud.h"
#include "cl_util.h"
#include "event_api.h"
#include "triangleapi.h"

#include "pm_shared.h"
#include "pm_defs.h"
#include "com_model.h"

#include "r_studioint.h"

#include "particleman.h"

#include "CPartSnowFlake.h"

#include "CEnvironment.h"

#define _USE_MATH_DEFINES
#include <math.h>

static // ripped this out of the engine
float	UTIL_AngleMod(float a)
{
    if (a < 0)
    {
        a = a + 360 * ((int)(a / 360) + 1);
    }
    else if (a >= 360)
    {
        a = a - 360 * ((int)(a / 360));
    }
    // a = (360.0/65536) * ((int)(a*(65536/360.0)) & 65535);
    return a;
}

static float UTIL_ApproachAngle(float target, float value, float speed)
{
    target = UTIL_AngleMod(target);
    value = UTIL_AngleMod(target);

    float delta = target - value;

    // Speed is assumed to be positive
    if (speed < 0)
        speed = -speed;

    if (delta < -180)
        delta += 360;
    else if (delta > 180)
        delta -= 360;

    if (delta > speed)
        value += speed;
    else if (delta < -speed)
        value -= speed;
    else
        value = target;

    return value;
}

//TODO: move - Solokiller
extern engine_studio_api_t IEngineStudio;

extern Vector g_vPlayerVelocity;

CEnvironment g_Environment;

cvar_t* cl_weather = NULL;

void CEnvironment::Initialize()
{
    m_vecWeatherOrigin = vec3_origin;
    m_flWeatherTime = 0;

    m_WeatherType = 0;

    m_pSnowSprite = const_cast<model_t*>(gEngfuncs.GetSpritePointer(gEngfuncs.pfnSPR_Load("sprites/snowflake.spr")));

    m_flWeatherValue = cl_weather->value;
}

void CEnvironment::Update()
{
    Vector vecOrigin = gHUD.m_vecOrigin;

    if (g_iUser1 > 0 && g_iUser1 != OBS_ROAMING)
    {
        if (cl_entity_t* pFollowing = gEngfuncs.GetEntityByIndex(g_iUser2))
        {
            vecOrigin = pFollowing->origin;
        }
    }

    vecOrigin.z += 36.0f;

    if (cl_weather->value > 3.0)
    {
        gEngfuncs.Cvar_SetValue("cl_weather", 3.0);
    }

    m_flWeatherValue = cl_weather->value;

    if (!IEngineStudio.IsHardware())
        m_flWeatherValue = 0;

    m_vecWeatherOrigin = vecOrigin;

    UpdateWind();

    if (m_flWeatherTime <= gEngfuncs.GetClientTime())
    {
        switch (m_WeatherType)
        {
            case 0: break;

            case 1:
            {
                UpdateSnow();
                break;
            }
        }
    }

    m_flOldTime = gEngfuncs.GetClientTime();
}

void CEnvironment::UpdateWind()
{
    // Ship goes forward, so snow must go back.
    Vector vecNewWind;
    AngleVectors(Vector(180, 0, 0), vecNewWind, NULL, NULL);
    m_vecWind = vecNewWind * 420.0f;
}

void CEnvironment::UpdateSnow()
{
    m_flWeatherTime = gEngfuncs.GetClientTime() + 0.5f;

    Vector vecPlayerDir = g_vPlayerVelocity;

    vecPlayerDir = vecPlayerDir.Normalize();
    const float flSpeed = vecPlayerDir.Length();

    if (m_flWeatherValue > 0.0f)
    {
        Vector vecOrigin;
        Vector vecEndPos;

        Vector vecWindOrigin;

        pmtrace_t trace;

        for (int i = 0; i < 150; i++)
        {
            vecOrigin = m_vecWeatherOrigin;

            vecOrigin.x += gEngfuncs.pfnRandomFloat(-1200.0f, 1200.0f);
            vecOrigin.y += gEngfuncs.pfnRandomFloat(-1200.0f, 1200.0f);
            vecOrigin.z += gEngfuncs.pfnRandomFloat(100.0f, 300.0f);

            vecEndPos.x = vecOrigin.x + (gEngfuncs.pfnRandomLong(0, 5) > 2) ? g_vPlayerVelocity.x : -g_vPlayerVelocity.x;
            vecEndPos.y = vecOrigin.y + g_vPlayerVelocity.y;
            vecEndPos.z = 8000.0f;

            gEngfuncs.pEventAPI->EV_SetTraceHull(2);
            gEngfuncs.pEventAPI->EV_PlayerTrace(vecOrigin, vecEndPos, PM_WORLD_ONLY, -1, &trace);
            const char* pszTexture = gEngfuncs.pEventAPI->EV_TraceTexture(trace.ent, vecOrigin, trace.endpos);

            if (pszTexture && strncmp(pszTexture, "sky", 3) == 0)
            {
                CreateSnowFlake(vecOrigin);
            }
        }
    }
}

void CEnvironment::CreateSnowFlake(const Vector& vecOrigin)
{
    if (!m_pSnowSprite)
    {
        return;
    }

    CPartSnowFlake* pParticle = new CPartSnowFlake();

    if (pParticle == NULL)
    {
        return;
    }

    pParticle->InitializeSprite(vecOrigin, vec3_origin, m_pSnowSprite, gEngfuncs.pfnRandomFloat(4.0, 4.5), 1.0);

    strcpy(pParticle->m_szClassname, "snow_particle");

    pParticle->m_iNumFrames = m_pSnowSprite->numframes;

    pParticle->m_vVelocity.x = m_vecWind.x / gEngfuncs.pfnRandomFloat(1.0, 2.0);
    pParticle->m_vVelocity.y = m_vecWind.y / gEngfuncs.pfnRandomFloat(1.0, 2.0);
    pParticle->m_vVelocity.z = gEngfuncs.pfnRandomFloat(-100.0, -200.0);

    pParticle->SetCollisionFlags(TRI_COLLIDEWORLD);

    const float flFrac = gEngfuncs.pfnRandomFloat(0.0, 1.0);

    if (flFrac >= 0.1)
    {
        if (flFrac < 0.2)
        {
            pParticle->m_vVelocity.z = -65.0;
        }
        else if (flFrac < 0.3)
        {
            pParticle->m_vVelocity.z = -75.0;
        }
    }
    else
    {
        pParticle->m_vVelocity.x *= 0.5;
        pParticle->m_vVelocity.y *= 0.5;
    }

    pParticle->m_iRendermode = kRenderTransAdd;

    pParticle->SetCullFlag(RENDER_FACEPLAYER | LIGHT_NONE | CULL_PVS | CULL_FRUSTUM_SPHERE);

    pParticle->m_flScaleSpeed = 0;
    pParticle->m_flDampingTime = 0;
    pParticle->m_iFrame = 0;
    pParticle->m_flMass = 1.0;

    pParticle->m_flGravity = 0;
    pParticle->m_flBounceFactor = 0;

    pParticle->m_vColor.x = pParticle->m_vColor.y = pParticle->m_vColor.z = 128.0f;

    pParticle->m_flDieTime = gEngfuncs.GetClientTime() + 6.0;

    pParticle->m_bSpiral = gEngfuncs.pfnRandomLong(0, 1) != 0;

    pParticle->m_flSpiralTime = gEngfuncs.GetClientTime() + gEngfuncs.pfnRandomLong(2, 4);
}
