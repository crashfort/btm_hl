#include "hud.h"
#include "cl_util.h"

#include "CEnvironment.h"

#include "CPartSnowFlake.h"

void CPartSnowFlake::Think(float flTime)
{
    if (m_flBrightness < 130.0 && !m_bTouched)
        m_flBrightness += 4.5;

    Fade(flTime);
    Spin(flTime);

    if (m_flSpiralTime <= gEngfuncs.GetClientTime())
    {
        m_bSpiral = !m_bSpiral;

        m_flSpiralTime = gEngfuncs.GetClientTime() + gEngfuncs.pfnRandomLong(2, 4);
    }
    else
    {
    }

    if (m_bSpiral && !m_bTouched)
    {
        const float flDelta = flTime - g_Environment.m_flOldTime;

        const float flSpin = sin(flTime * 5.0 + reinterpret_cast<int>(this));

        m_vOrigin = m_vOrigin + m_vVelocity * flDelta;

        m_vOrigin.x += (flSpin * flSpin) * 0.3;
    }
    else
    {
        CalculateVelocity(flTime);
    }

    CheckCollision(flTime);
}

void CPartSnowFlake::Touch(Vector pos, Vector normal, int index)
{
    if (m_bTouched)
    {
        return;
    }

    m_bTouched = true;

    SetRenderFlag(RENDER_FACEPLAYER);

    m_flOriginalBrightness = m_flBrightness;

    m_vVelocity = vec3_origin;

    m_iRendermode = kRenderTransAdd;

    m_flFadeSpeed = 0;
    m_flScaleSpeed = 0;
    m_flDampingTime = 0;
    m_iFrame = 0;
    m_flMass = 1.0;
    m_flGravity = 0;

    m_vColor.x = m_vColor.y = m_vColor.z = 128.0;

    m_flDieTime = gEngfuncs.GetClientTime() + 0.5;

    m_flTimeCreated = gEngfuncs.GetClientTime();
}
