/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_ENTITIES_LASER_H
#define GAME_SERVER_ENTITIES_LASER_H

#include <game/server/entity.h>
#include <game/server/player.h>

#define SHOTS_HISTORY 3
class CLaser : public CEntity
{
public:
	CLaser(CGameWorld *pGameWorld, vec2 Pos, vec2 Direction, float StartEnergy, CPlayer *pPlayer, int Type);
	virtual ~CLaser();

	void Reset() override;
	void Tick() override;
	void TickPaused() override;
	void Snap(int SnappingClient) override;
	void SwapClients(int Client1, int Client2) override;

	int GetOwnerId() const override { return m_Owner; }

protected:
	bool HitCharacter(vec2 From, vec2 To);
	void DoBounce();

private:
	vec2 m_From;
	vec2 m_Dir;
	vec2 m_TelePos;
	bool m_WasTele;
	float m_Energy;
	int m_Bounces;
	int m_EvalTick;
	int m_Owner;
	CClientMask m_TeamMask;
	bool m_ZeroEnergyBounceInLastTick;

	bool m_DidHit;

	//iCTFX
	int m_DeathTick;

	struct ShotHistory
	{
		int tick;
		int rollbackTick;
		vec2 from;
		vec2 to;
		bool clientsTested [MAX_CLIENTS];
		int clientDelays[MAX_CLIENTS];
	};

	int shot_index;
	ShotHistory shots [SHOTS_HISTORY];

	vec2 m_PredHitPos;

	// DDRace

	vec2 m_PrevPos;
	int m_Type;
	int m_TuneZone;
	bool m_TeleportCancelled;
	bool m_IsBlueTeleport;
	int m_StartTick;
	bool m_BelongsToPracticeTeam;
};

#endif
