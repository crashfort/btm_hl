#ifndef GAME_CLIENT_EFFECTS_CENVIRONMENT_H
#define GAME_CLIENT_EFFECTS_CENVIRONMENT_H

#include "com_model.h"

/**
*	Class that manages environmental effects.
*/
class CEnvironment final
{
public:
    CEnvironment() = default;

    void Initialize();

    void Update();

    void UpdateSnow();
    void UpdateWind();

    void CreateSnowFlake(const Vector& vecOrigin);

    int m_WeatherType = 0;

    Vector m_vecWeatherOrigin;

    float m_flWeatherTime;

    model_t* m_pSnowSprite;

    float m_flWeatherValue;

    float m_flOldTime;

    Vector m_vecWind;
};

extern CEnvironment g_Environment;

extern cvar_t* cl_weather;

#endif //GAME_CLIENT_EFFECTS_CENVIRONMENT_H 
