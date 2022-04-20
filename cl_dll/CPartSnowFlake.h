#ifndef GAME_CLIENT_EFFECTS_CPARTSNOWFLAKE_H
#define GAME_CLIENT_EFFECTS_CPARTSNOWFLAKE_H

#include "particleman.h"

class CPartSnowFlake : public CBaseParticle
{
public:
    CPartSnowFlake() = default;

    void Think(float flTime) override;

    void Touch(Vector pos, Vector normal, int index) override;

    bool m_bSpiral;
    float m_flSpiralTime;

    bool m_bTouched = false;
};

#endif //GAME_CLIENT_EFFECTS_CPARTSNOWFLAKE_H
