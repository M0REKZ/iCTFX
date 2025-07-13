/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "laser.h"
#include "character.h"

#include <engine/shared/config.h>

#include "character.h"
#include "../player.h"
#include <game/generated/protocol.h>
#include <game/mapitems.h>

#include <game/server/gamecontext.h>
#include <game/server/gamemodes/DDRace.h>

CLaser::CLaser(CGameWorld *pGameWorld, vec2 Pos, vec2 Direction, float StartEnergy, CPlayer *pPlayer, int Type) :
	CEntity(pGameWorld, CGameWorld::ENTTYPE_LASER)
{
	m_Pos = Pos;
	m_Owner = pPlayer->GetCID();
	m_Energy = StartEnergy;
	m_Dir = Direction;
	m_Bounces = 0;
	m_EvalTick = 0;
	m_TelePos = vec2(0, 0);
	m_WasTele = false;
	m_Type = Type;
	m_TeleportCancelled = false;
	m_IsBlueTeleport = false;
	m_StartTick = Server()->Tick();

	m_NextPos = m_Pos;
	shot_index = 0;

	m_DidHit = false;

	m_ZeroEnergyBounceInLastTick = false;
	m_TuneZone = GameServer()->Collision()->IsTune(GameServer()->Collision()->GetMapIndex(m_Pos));
	CCharacter *pOwnerChar = GameServer()->GetPlayerChar(m_Owner);
	m_TeamMask = pOwnerChar ? pOwnerChar->TeamMask() : CClientMask();
	m_BelongsToPracticeTeam = pOwnerChar && pOwnerChar->Teams()->IsPractice(pOwnerChar->Team());
	
	if (pPlayer) {
		pPlayer->m_Shots++;
	}

	m_DeathTick = 0;

	for(int j = 0; j < SHOTS_HISTORY; j++)
		for(int i = 0; i < MAX_CLIENTS; i++)
			shots[j].clientsTested[i] = false;

	GameWorld()->InsertEntity(this);
	DoBounce();
}

CLaser::~CLaser() {
	if (m_Bounces > 0) {
		CPlayer *pOwner = GameServer()->m_apPlayers[m_Owner];
		if (!pOwner) {
			return;
		}

		pOwner->m_Wallshots++;

		if (m_DidHit) {
			pOwner->m_WallshotKills++;
		}
	}
}

bool CLaser::HitCharacter(vec2 From, vec2 To)
{
	static const vec2 StackedLaserShotgunBugSpeed = vec2(-2147483648.0f, -2147483648.0f);
	vec2 At;
	CCharacter *pOwnerChar = GameServer()->GetPlayerChar(m_Owner);
	CCharacter *pHit;
	bool pDontHitSelf = g_Config.m_SvOldLaser || (m_Bounces == 0 && !m_WasTele);

	int tick = -1;

	if(GameServer()->m_apPlayers[m_Owner]->m_Rollback && g_Config.m_SvRollback)
		tick = GameServer()->m_apPlayers[m_Owner]->m_LastAckedSnapshot;
	
	shots[shot_index].tick = Server()->Tick();
	shots[shot_index].rollbackTick = tick;
	shots[shot_index].from = From;
	shots[shot_index].to = To;
	shot_index++;

	if(pOwnerChar ? (!(pOwnerChar->m_Hit & CCharacter::DISABLE_HIT_LASER) && m_Type == WEAPON_LASER) || (!(pOwnerChar->m_Hit & CCharacter::DISABLE_HIT_SHOTGUN) && m_Type == WEAPON_SHOTGUN) : g_Config.m_SvHit)
		pHit = GameServer()->m_World.IntersectCharacter(m_Pos, To, 0.f, At, pOwnerChar, m_Owner, nullptr, tick);
	else
		pHit = GameServer()->m_World.IntersectCharacter(m_Pos, To, 0.f, At, pOwnerChar, m_Owner, pOwnerChar, tick);

	m_PredHitPos = vec2(0,0);
	bool dont = false;
	if(!pHit || (pHit == pOwnerChar && g_Config.m_SvOldLaser) || (pHit != pOwnerChar && pOwnerChar ? (pOwnerChar->m_Hit & CCharacter::DISABLE_HIT_LASER && m_Type == WEAPON_LASER) || (pOwnerChar->m_Hit & CCharacter::DISABLE_HIT_SHOTGUN && m_Type == WEAPON_SHOTGUN) : !g_Config.m_SvHit))
		dont = true;
	
	if(pHit && pHit->m_pPlayer->m_Rollback && GameServer()->m_apPlayers[m_Owner] && GameServer()->m_apPlayers[m_Owner]->m_RunAhead)
		dont = true; //check when rollback is in proper position
	
	if(dont)
	{
		//try and find a prediction to potentially hit
		if(GameServer()->m_apPlayers[m_Owner] && GameServer()->m_apPlayers[m_Owner]->m_RunAhead)
		{
			for(int player = 0; player < MAX_CLIENTS; player++)
			{
				if(player == m_Owner)
					continue;
				
				if(!GameServer()->m_apPlayers[player])
					continue;
				
				if(!GameServer()->m_apPlayers[player]->m_Rollback)
					continue;
				
				int AckedTick = GameServer()->m_apPlayers[player]->m_LastAckedSnapshot;
				
				shots[shot_index-1].clientDelays[player] = Server()->Tick()-AckedTick;
				
				if(!GameServer()->m_apPlayers[player]->GetCharacter())
					continue;
				
				
				if(AckedTick < 0) //safety check
					continue;

				AckedTick = Server()->Tick() - (Server()->Tick()-AckedTick)*GameServer()->m_apPlayers[m_Owner]->m_RunAhead;

				vec2 pos;
				pos.x = GameServer()->m_apPlayers[player]->m_CoreAheads[AckedTick % POSITION_HISTORY].m_X;
				pos.y = GameServer()->m_apPlayers[player]->m_CoreAheads[AckedTick % POSITION_HISTORY].m_Y;
				
				vec2 IntersectPos;

				CCharacter * p = GameServer()->m_apPlayers[player]->GetCharacter();

				if(closest_point_on_line(From, To, pos, IntersectPos))
				{
					float Len = distance(pos, IntersectPos);
					if(Len < p->m_ProximityRadius)
					{
						m_PredHitPos = IntersectPos;
						return false;
					}
				}
			}
		}
		
		return false;
	}
	
	m_From = From;
	m_Pos = At;
	m_Energy = -1;
	m_DidHit = true;
	pHit->TakeDamage(vec2(0.f, 0.f), GameServer()->Tuning()->m_LaserDamage, m_Owner, WEAPON_LASER, m_StartTick);
	return true;
}

void CLaser::DoBounce()
{
	m_EvalTick = Server()->Tick();

	if(m_Energy < 0 && m_DeathTick == 0)
	{
		m_DeathTick = Server()->Tick();
		return;
	}

	m_PrevPos = m_Pos;
	m_Pos = m_NextPos;
	vec2 Coltile;

	int Res;
	int z;

	vec2 To = m_Pos + m_Dir * m_Energy;
	vec2 Tele;

	int teleptr = 0;
	if(m_Energy > 0 && GameServer()->Collision()->IntersectLineTeleWeapon(m_Pos, To, &Tele, &To, &teleptr))
	{
		if(!HitCharacter(m_Pos, To))
		{
			// intersected
			m_From = m_Pos;
			m_Pos = To;

			vec2 TempPos = m_Pos;
			vec2 TempDir = m_Dir * 4.0f;

			int f = 0;
			if(Res == -1)
			{
				f = GameServer()->Collision()->GetTile(round_to_int(Coltile.x), round_to_int(Coltile.y));
				GameServer()->Collision()->SetCollisionAt(round_to_int(Coltile.x), round_to_int(Coltile.y), TILE_SOLID);
			}
			GameServer()->Collision()->MovePoint(&TempPos, &TempDir, 1.0f, nullptr);
			if(Res == -1)
			{
				GameServer()->Collision()->SetCollisionAt(round_to_int(Coltile.x), round_to_int(Coltile.y), f);
			}
			m_Pos = TempPos;
			
			if(!teleptr)
				m_Dir = normalize(TempDir);

			const float Distance = distance(m_From, m_Pos);
			// Prevent infinite bounces
			if(Distance == 0.0f && m_ZeroEnergyBounceInLastTick)
			{
				m_Energy = -1;
			}
			else if(!m_TuneZone)
			{
				m_Energy -= Distance + Tuning()->m_LaserBounceCost;
			}
			else
			{
				m_Energy -= distance(m_From, m_Pos) + GameServer()->TuningList()[m_TuneZone].m_LaserBounceCost;
			}
			m_ZeroEnergyBounceInLastTick = Distance == 0.0f;

			if(Res == TILE_TELEINWEAPON && !GameServer()->Collision()->TeleOuts(z - 1).empty())
			{
				int TeleOut = GameServer()->m_World.m_Core.RandomOr0(GameServer()->Collision()->TeleOuts(z - 1).size());
				m_TelePos = GameServer()->Collision()->TeleOuts(z - 1)[TeleOut];
				m_WasTele = true;
			}
			else
			{
				m_Bounces++;
			

			int BounceNum = Tuning()->m_LaserBounceNum;
			if(m_TuneZone)
				BounceNum = TuningList()[m_TuneZone].m_LaserBounceNum;

			if(m_Bounces > BounceNum)
				m_Energy = -1;

			GameServer()->CreateSound(m_Pos, SOUND_LASER_BOUNCE, m_TeamMask);
			m_NextPos = m_Pos;

			if(teleptr)
			{
				m_NextPos = Tele;
				GameServer()->CreateSound(m_Pos, SOUND_LASER_BOUNCE, m_TeamMask);
			}

		}
	}
	else
	{
		if(!HitCharacter(m_Pos, To))
		{
			m_From = m_Pos;
			m_Pos = To;
			m_NextPos = To;
			m_Energy = -1;
		}
	}

	CCharacter *pOwnerChar = GameServer()->GetPlayerChar(m_Owner);
	if(m_Owner >= 0 && m_Energy <= 0 && !m_TeleportCancelled && pOwnerChar &&
		pOwnerChar->IsAlive() && pOwnerChar->HasTelegunLaser() && m_Type == WEAPON_LASER)
	{
		vec2 PossiblePos;
		bool Found = false;

		// Check if the laser hits a player.
		bool pDontHitSelf = g_Config.m_SvOldLaser || (m_Bounces == 0 && !m_WasTele);
		vec2 At;
		CCharacter *pHit;
		if(pOwnerChar ? (!pOwnerChar->LaserHitDisabled() && m_Type == WEAPON_LASER) : g_Config.m_SvHit)
			pHit = GameServer()->m_World.IntersectCharacter(m_Pos, To, 0.f, At, pDontHitSelf ? pOwnerChar : nullptr, m_Owner);
		else
			pHit = GameServer()->m_World.IntersectCharacter(m_Pos, To, 0.f, At, pDontHitSelf ? pOwnerChar : nullptr, m_Owner, pOwnerChar);

		if(pHit)
			Found = GetNearestAirPosPlayer(pHit->m_Pos, &PossiblePos);
		else
			Found = GetNearestAirPos(m_Pos, m_From, &PossiblePos);

		if(Found)
		{
			pOwnerChar->m_TeleGunPos = PossiblePos;
			pOwnerChar->m_TeleGunTeleport = true;
			pOwnerChar->m_IsBlueTeleGunTeleport = m_IsBlueTeleport;
		}
	}
	else if(m_Owner >= 0)
	{
		int MapIndex = GameServer()->Collision()->GetPureMapIndex(Coltile);
		int TileFIndex = GameServer()->Collision()->GetFrontTileIndex(MapIndex);
		bool IsSwitchTeleGun = GameServer()->Collision()->GetSwitchType(MapIndex) == TILE_ALLOW_TELE_GUN;
		bool IsBlueSwitchTeleGun = GameServer()->Collision()->GetSwitchType(MapIndex) == TILE_ALLOW_BLUE_TELE_GUN;
		int IsTeleInWeapon = GameServer()->Collision()->IsTeleportWeapon(MapIndex);

		if(!IsTeleInWeapon)
		{
			if(IsSwitchTeleGun || IsBlueSwitchTeleGun)
			{
				// Delay specifies which weapon the tile should work for.
				// Delay = 0 means all.
				int delay = GameServer()->Collision()->GetSwitchDelay(MapIndex);

				if((delay != 3 && delay != 0) && m_Type == WEAPON_LASER)
				{
					IsSwitchTeleGun = IsBlueSwitchTeleGun = false;
				}
			}

			m_IsBlueTeleport = TileFIndex == TILE_ALLOW_BLUE_TELE_GUN || IsBlueSwitchTeleGun;

			// Teleport is canceled if the last bounce tile is not a TILE_ALLOW_TELE_GUN.
			// Teleport also works if laser didn't bounce.
			m_TeleportCancelled =
				m_Type == WEAPON_LASER && (TileFIndex != TILE_ALLOW_TELE_GUN && TileFIndex != TILE_ALLOW_BLUE_TELE_GUN && !IsSwitchTeleGun && !IsBlueSwitchTeleGun);
		}
	}

	//m_Owner = -1;
}

void CLaser::Reset()
{
	m_MarkedForDestroy = true;
}

void CLaser::Tick()
{
	if(m_DeathTick == 0 && Server()->Tick() > m_EvalTick+(Server()->TickSpeed()*GameServer()->Tuning()->m_LaserBounceDelay)/1000.0f)
		DoBounce();
	
	if(m_DeathTick)
		m_MarkedForDestroy = true;
	
	if(!GameServer()->m_apPlayers[m_Owner] || GameServer()->m_apPlayers[m_Owner]->m_RunAhead == 0.0f)
		return;
	
	for(int shot = 0; shot < shot_index; shot++)
	{
		for(int player = 0; player < MAX_CLIENTS; player++)\
		{
			if(!GameServer()->m_apPlayers[player])
				continue;
			
			if(!GameServer()->m_apPlayers[player]->m_Rollback)
				continue;
			
			shots[shot].clientDelays[player]--;
			
			if(!GameServer()->m_apPlayers[player]->GetCharacter())
				continue;			
			
			int AckedTick = GameServer()->m_apPlayers[player]->m_LastAckedSnapshot;

			AckedTick = Server()->Tick() - (Server()->Tick()-AckedTick)*GameServer()->m_apPlayers[m_Owner]->m_RunAhead;

			if(shots[shot].clientDelays[player] >= g_Config.m_SvRunAheadLaserOffset)
				m_MarkedForDestroy = false;
			
			if(shots[shot].clientDelays[player] >= g_Config.m_SvRunAheadLaserOffset || shots[shot].clientsTested[player])
				continue;
			
			shots[shot].clientsTested[player] = true;
			
			//check hit
			vec2 At;

			int tick = -1;

			if(GameServer()->m_apPlayers[m_Owner]->m_Rollback)
			{
				tick = GameServer()->m_apPlayers[m_Owner]->m_LastAckedSnapshot;
			}

			CCharacter *pOwnerChar = GameServer()->GetPlayerChar(m_Owner);

			CCharacter *pHit = GameServer()->m_World.IntersectCharacter(shots[shot].from, shots[shot].to, 0.f, At, pOwnerChar, -1, GameServer()->m_apPlayers[player]->GetCharacter(), tick);
			if(pHit)
			{
				pHit->TakeDamage(vec2(0.f, 0.f), GameServer()->Tuning()->m_LaserDamage, m_Owner, WEAPON_LASER, m_StartTick);
				pHit->m_DeathTick = Server()->Tick() + (pHit->m_DeathTick-Server()->Tick())*GameServer()->m_apPlayers[m_Owner]->m_RunAhead;
				m_Energy = -1;
				m_DidHit = true;
				m_MarkedForDestroy = true;
				return;
			}
		}
	}

	float Delay;
	if(m_TuneZone)
		Delay = TuningList()[m_TuneZone].m_LaserBounceDelay;
	else
		Delay = Tuning()->m_LaserBounceDelay;

	if((Server()->Tick() - m_EvalTick) > (Server()->TickSpeed() * Delay / 1000.0f))
		DoBounce();
}

void CLaser::TickPaused()
{
	++m_EvalTick;
}

void CLaser::Snap(int SnappingClient)
{
	if(NetworkClipped(SnappingClient) && NetworkClipped(SnappingClient, m_From) && g_Config.m_SvAntiZoom)
		return;

	if(Server()->GetClientVersion(SnappingClient) >= VERSION_DDNET_MULTI_LASER)
	{
		CNetObj_DDNetLaser *pObj = static_cast<CNetObj_DDNetLaser *>(Server()->SnapNewItem(NETOBJTYPE_DDNETLASER, GetID(), sizeof(CNetObj_DDNetLaser)));
		if(!pObj)
			return;

		pObj->m_ToX = (int)m_Pos.x;
		pObj->m_ToY = (int)m_Pos.y;
		pObj->m_FromX = (int)m_From.x;
		pObj->m_FromY = (int)m_From.y;
		pObj->m_StartTick = m_EvalTick;
		pObj->m_Owner = m_Owner;
		pObj->m_Type = 0;
		pObj->m_Subtype = 0;
		pObj->m_SwitchNumber = m_Number;
		pObj->m_Flags = 0;

		if(m_PredHitPos != vec2(0,0))
		{
			pObj->m_ToX = (int)m_PredHitPos.x;
			pObj->m_ToY = (int)m_PredHitPos.y;
		}
	}else
	{
		CNetObj_Laser *pObj = static_cast<CNetObj_Laser *>(Server()->SnapNewItem(NETOBJTYPE_LASER, GetID(), sizeof(CNetObj_Laser)));
		if(!pObj)
			return;

		pObj->m_X = (int)m_Pos.x;
		pObj->m_Y = (int)m_Pos.y;
		pObj->m_FromX = (int)m_From.x;
		pObj->m_FromY = (int)m_From.y;
		pObj->m_StartTick = m_EvalTick;

		if(m_PredHitPos != vec2(0,0))
		{
			pObj->m_X = (int)m_PredHitPos.x;
			pObj->m_Y = (int)m_PredHitPos.y;
		}
	}
}

void CLaser::SwapClients(int Client1, int Client2)
{
	m_Owner = m_Owner == Client1 ? Client2 : m_Owner == Client2 ? Client1 : m_Owner;
}
