#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "animation.h"
#include "player.h"
#include "weapons.h"
#include "const.h"
#include "enginecallback.h"
#include "client.h"
#include "eiface.h"
#include "vector.h"
#include "usercmd.h"

#include <vector>

#define MAX_REPLAYS 24 // How many bots can be playing at once.

// Params the next replay should use.
static cvar_t rp_model = { "rp_model", "barney", FCVAR_SERVER | FCVAR_NOEXTRAWHITEPACE };
static cvar_t rp_topcolor = { "rp_topcolor", "150", FCVAR_SERVER | FCVAR_NOEXTRAWHITEPACE };
static cvar_t rp_bottomcolor = { "rp_bottomcolor", "100", FCVAR_SERVER | FCVAR_NOEXTRAWHITEPACE };
static cvar_t rp_build_waves = { "rp_build_waves", "0", FCVAR_SERVER };

cvar_t rp_jet = { "rp_jet", "0", FCVAR_SERVER }; // Jet control mode.

// The bots recorded for btm31 were not under any version, but jets needed the frame
// structure to change so we now have versions. Jets were not planned at all then so there was no need
// for the expanded frame structure. Always version your binary structures.
const int REPLAY_FILE_VERSION = 2;

struct ReplayHeader
{
    int version;
    int num_frames;
};

// Mostly a copy of user cmd but has some more stuff too.
struct ReplayFrame
{
    Vector pos;
    Vector angles;
    Vector viewangles;
    unsigned short button;
    byte impulse;
    float forwardmove;
    float sidemove;
    float upmove;
    byte msec;
    char weapon_name[32];
};

struct ReplaySequence
{
    std::vector<ReplayFrame> frames;
};

static bool rp_recording = false;
static HANDLE rp_recording_file;

static edict_t* rp_bots[MAX_REPLAYS]; // Bot entities.
static bool rp_bot_slots[MAX_REPLAYS]; // What indexes are playing.
static bool rp_bot_alive[MAX_REPLAYS]; // Playing but bot should not update if dead.

static ReplaySequence rp_rec_seq;
static ReplayHeader rp_rec_header;

static ReplaySequence rp_play_seqs[MAX_REPLAYS];

static int Num_Recorded_Frames(ReplaySequence* seq)
{
    return seq->frames.size();
}

static void Clear_Sequence(ReplaySequence* seq)
{
    seq->frames.clear();
}

static void Resize_Sequence(ReplaySequence* seq, int num)
{
    seq->frames.resize(num);
}

static void Add_Frame(const ReplayFrame& frame)
{
    rp_rec_seq.frames.push_back(frame);

    if (rp_recording_file)
    {
        WriteFile(rp_recording_file, &frame, sizeof(ReplayFrame), NULL, NULL);
    }
}

static void Remove_Entity(edict_t* ed)
{
    char cmd[32];
    snprintf(cmd, sizeof(cmd), "kick \"%s\"\n", STRING(ed->v.netname));

    g_engfuncs.pfnServerCommand(cmd);
}

static void Clear_Entities()
{
    for (int i = 0; i < MAX_REPLAYS; i++)
    {
        if (rp_bot_slots[i])
        {
            Remove_Entity(rp_bots[i]);
        }
    }

    memset(rp_bots, 0, sizeof(rp_bots));
    memset(rp_bot_slots, 0, sizeof(rp_bot_slots));
    memset(rp_bot_alive, 0, sizeof(rp_bot_alive));
}

extern int g_serveractive;

static void Replay_Record_Cmd()
{
    if (!g_serveractive)
    {
        return;
    }

    if (rp_recording)
    {
        return;
    }

    auto argc = CMD_ARGC();

    if (argc != 2)
    {
        ALERT(at_console, "rp_record <file>\n");
        return;
    }

    rp_recording_file = CreateFileA(CMD_ARGV(1), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

    if (rp_recording_file == INVALID_HANDLE_VALUE)
    {
        ALERT(at_console, "rp_record failed for %s\n", CMD_ARGV(1));
        rp_recording_file = NULL;
        return;
    }

    // Reserve header.

    rp_rec_header.version = REPLAY_FILE_VERSION;
    rp_rec_header.num_frames = 0;
    WriteFile(rp_recording_file, &rp_rec_header, sizeof(ReplayHeader), NULL, NULL);

    Clear_Sequence(&rp_rec_seq);
    rp_recording = true;
}

static void Close_Record_File()
{
    if (rp_recording_file)
    {
        SetFilePointer(rp_recording_file, 0, NULL, FILE_BEGIN);

        rp_rec_header.num_frames = rp_rec_seq.frames.size();
        WriteFile(rp_recording_file, &rp_rec_header, sizeof(ReplayHeader), NULL, NULL);

        CloseHandle(rp_recording_file);
        rp_recording_file = NULL;
    }
}

static void Replay_Stop_Cmd()
{
    if (!g_serveractive)
    {
        return;
    }

    if (!rp_recording)
    {
        return;
    }

    Close_Record_File();

    rp_recording = false;
}

static Vector Convert_ViewAngle_To_ModelAngle(const Vector& view_ang)
{
    auto ret = view_ang;
    ret[0] /= -3.0f;

    return ret;
}

static CBasePlayer* Get_Player_Entity(edict_t* ed)
{
    return GetClassPtr((CBasePlayer*)VARS(ed));
}

static const CBasePlayer* Get_Player_Entity(const edict_t* ed)
{
    return GetClassPtr((CBasePlayer*)VARS((edict_t*)ed));
}

static bool Replay_Load(const char* file, ReplaySequence* seq)
{
    auto h = CreateFileA(file, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

    if (h == INVALID_HANDLE_VALUE)
    {
        return false;
    }

    ReplayHeader header;
    ReadFile(h, &header, sizeof(ReplayHeader), NULL, NULL);

    if (header.version != REPLAY_FILE_VERSION)
    {
        CloseHandle(h);

        ALERT(at_console, "Wrong file version\n");
        return false;
    }

    Resize_Sequence(seq, header.num_frames);
    ReadFile(h, seq->frames.data(), sizeof(ReplayFrame) * header.num_frames, NULL, NULL);

    CloseHandle(h);
    return true;
}

static void Replay_Play(const char* file)
{
    int index = -1;

    for (int i = 0; i < MAX_REPLAYS; i++)
    {
        if (!rp_bot_slots[i])
        {
            index = i;
            break;
        }
    }

    if (index == -1)
    {
        ALERT(at_console, "rp_play no free slots\n");
        return;
    }

    if (!Replay_Load(file, &rp_play_seqs[index]))
    {
        ALERT(at_console, "rp_play cannot load %s\n", file);
        return;
    }

    auto seq = &rp_play_seqs[index];

    if (Num_Recorded_Frames(seq) == 0)
    {
        ALERT(at_console, "rp_play sequence is empty\n");
        return;
    }

    auto ed = g_engfuncs.pfnCreateFakeClient("barney");

    if (ed == NULL)
    {
        ALERT(at_console, "rp_play cannot create bot\n");
        return;
    }

    auto info_buf = g_engfuncs.pfnGetInfoKeyBuffer(ed);
    auto bot_index = ENTINDEX(ed);

    g_engfuncs.pfnSetClientKeyValue(bot_index, info_buf, "model", rp_model.string);
    g_engfuncs.pfnSetClientKeyValue(bot_index, info_buf, "topcolor", rp_topcolor.string);
    g_engfuncs.pfnSetClientKeyValue(bot_index, info_buf, "bottomcolor", rp_bottomcolor.string);

    ClientPutInServer(ed);

    ed->v.flags |= FL_FAKECLIENT;
    ed->v.solid = SOLID_NOT;
    ed->v.movetype = MOVETYPE_NOCLIP;

    // Set baseline from first frame.

    auto& fr = seq->frames[0];

    ed->v.origin = fr.pos;
    ed->v.v_angle = fr.viewangles;
    ed->v.angles = Convert_ViewAngle_To_ModelAngle(fr.angles);
    ed->v.iuser4 = 0; // Frame in playback.
    ed->v.euser4 = (edict_t*)seq;

    rp_bots[index] = ed;
    rp_bot_slots[index] = true;
    rp_bot_alive[index] = true;
}

static void Replay_Play_Cmd()
{
    if (!g_serveractive)
    {
        return;
    }

    auto argc = CMD_ARGC();

    if (argc != 2)
    {
        ALERT(at_console, "rp_play <file>\n");
        return;
    }

    Replay_Play(CMD_ARGV(1));
}

#define WAVE_UPDATE_TIME 3.0f // Only check for wave completions this often.

class CWaveStarter : public CBaseEntity
{
    int num_bots;
    string_t seq_names[MAX_REPLAYS];

public:
    void KeyValue(KeyValueData* pkvd)
    {
        if (num_bots == MAX_REPLAYS)
        {
            pkvd->fHandled = FALSE;
            ALERT(at_console, "Too many bots for wave %s!\n", STRING(pev->targetname));
            return;
        }

        char tmp[128];
        UTIL_StripToken(pkvd->szKeyName, tmp);

        seq_names[num_bots] = ALLOC_STRING(tmp);
        num_bots++;
        pkvd->fHandled = TRUE;
    }

    void Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
    {
        if (num_bots == 0)
        {
            ALERT(at_console, "No bots for wave %s!\n", STRING(pev->targetname));
            return;
        }

        // Start a wave. Will only finish when all bad guys have been defeated!

        ALERT(at_console, "Starting wave %s!\n", STRING(pev->targetname));

        pev->nextthink = gpGlobals->time + WAVE_UPDATE_TIME;
        SetThink(&CWaveStarter::WaveThink);

        // Load all bots specified by this wave.

        for (int i = 0; i < num_bots; i++)
        {
            Replay_Play(STRING(seq_names[i]));
        }
    }

    void EXPORT WaveThink()
    {
        // Clear wave when all bots are dead.

        if (rp_build_waves.value)
        {
            return; // Do not progress waves if we are building the bots for the sequence.
        }

        int alive = 0;

        for (int i = 0; i < MAX_REPLAYS; i++)
        {
            if (rp_bot_alive[i])
            {
                alive++;
            }
        }

        if (alive == 0)
        {
            ALERT(at_console, "Cleared wave %s!\n", STRING(pev->targetname));

            Clear_Entities(); // We still keep around bots if they died during their playback. Remove them now when wave is finished.

            SUB_UseTargets(this, USE_ON, 0); // Next wave can be started now.
            SUB_Remove();
        }

        else
        {
            pev->nextthink = gpGlobals->time + WAVE_UPDATE_TIME;
        }
    }
};

LINK_ENTITY_TO_CLASS(wave_start, CWaveStarter);

static void Update_All_Bots()
{
    for (int i = 0; i < MAX_REPLAYS; i++)
    {
        if (rp_bot_slots[i])
        {
            auto ed = rp_bots[i];
            auto seq = (ReplaySequence*)ed->v.euser4;
            auto& fr = seq->frames[ed->v.iuser4]; // Frame in playback.

            if (ed->v.deadflag != DEAD_NO && rp_bot_alive[i])
            {
                rp_bot_alive[i] = false;
                ClientKill(ed); // Play animation if bot dies during playback.
            }

            if (rp_bot_alive[i])
            {
                ed->v.origin = fr.pos;
                ed->v.v_angle = fr.viewangles;
                ed->v.angles = Convert_ViewAngle_To_ModelAngle(fr.angles);

                #if 0
                auto pos_diff = fr.pos - ed->v.origin;
                auto length = pos_diff.Length();
                #endif

                auto entity = Get_Player_Entity(ed);

                if (strcmp(STRING(entity->m_pActiveItem->pev->classname), fr.weapon_name))
                {
                    entity->SelectItem(fr.weapon_name); // Change weapon if a new weapon is selected.
                }

                g_engfuncs.pfnRunPlayerMove(ed, fr.viewangles, fr.forwardmove, fr.sidemove, fr.upmove, fr.button, fr.impulse, fr.msec);
            }

            else
            {
                g_engfuncs.pfnRunPlayerMove(ed, fr.viewangles, 0, 0, 0, 0, 0, 0); // Must always be run.
            }

            ed->v.iuser4++; // Frame in playback.
        }
    }
}

static void Remove_Finished_Bots()
{
    for (int i = 0; i < MAX_REPLAYS; i++)
    {
        if (rp_bot_slots[i])
        {
            auto ed = rp_bots[i];
            auto seq = (ReplaySequence*)ed->v.euser4;

            if (ed->v.iuser4 == Num_Recorded_Frames(seq) - 1)
            {
                Remove_Entity(ed);
                rp_bot_slots[i] = false;
                rp_bot_alive[i] = false;
                rp_bots[i] = NULL;
            }
        }
    }
}

static void Update_Playback()
{
    Update_All_Bots();
    Remove_Finished_Bots();
}

void Replay_Record(const edict_t* player, const struct usercmd_s* cmd)
{
    if (!rp_recording)
    {
        return;
    }

    if (player->v.deadflag != DEAD_NO)
    {
        return;
    }

    ReplayFrame frame;
    frame.pos = player->v.origin;
    frame.viewangles = cmd->viewangles;
    frame.angles = player->v.angles;
    frame.button = cmd->buttons;
    frame.impulse = cmd->impulse;
    frame.forwardmove = cmd->forwardmove;
    frame.sidemove = cmd->sidemove;
    frame.upmove = cmd->upmove;
    frame.msec = cmd->msec;

    auto entity = Get_Player_Entity(player);
    strncpy(frame.weapon_name, STRING(entity->m_pActiveItem->pev->classname), 32);

    Add_Frame(frame);
}

void Replay_Update_Playback()
{
    Update_Playback();
}

// We cannot inject BXT because those patterns don't work with this compiled binary. We also cannot use perfect autobhop by changing the movement code
// because then the game doesn't play the jump animation properly. We also cannot use _special because it doesn't exist in Steam anymore.
static void Bxt_Append_Cmd()
{
    auto argc = CMD_ARGC();

    if (argc != 2)
    {
        return;
    }

    char cmd[48];
    snprintf(cmd, sizeof(cmd), "%s\n", CMD_ARGV(1));

    g_engfuncs.pfnServerCommand(cmd);
}

void Replay_Init()
{
    g_engfuncs.pfnAddServerCommand("rp_record", Replay_Record_Cmd);
    g_engfuncs.pfnAddServerCommand("rp_stop", Replay_Stop_Cmd);
    g_engfuncs.pfnAddServerCommand("rp_play", Replay_Play_Cmd);
    g_engfuncs.pfnAddServerCommand("bxt_append", Bxt_Append_Cmd);

    CVAR_REGISTER(&rp_model);
    CVAR_REGISTER(&rp_topcolor);
    CVAR_REGISTER(&rp_bottomcolor);
    CVAR_REGISTER(&rp_build_waves);
    CVAR_REGISTER(&rp_jet);
}

void Replay_Activate()
{

}

void Replay_Shutdown()
{
    Clear_Entities();

    Clear_Sequence(&rp_rec_seq);
    Close_Record_File();
    rp_recording = false;
}
