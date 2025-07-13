#include "sql_handler.h"

#include <iostream>

int get_proc_count()
{
	auto c = std::thread::hardware_concurrency();
	if(c < 4)
	{
		c = 4;
	}
	return c;
}

void SqlHandler::get_player_stats_handler(void *ev)
{
	auto data = static_cast<GetPlayerStatsData *>(ev);
	CMysqlConfig gMysqlConfig;

		str_copy(gMysqlConfig.m_aDatabase,g_Config.m_SqlDatabase);
		str_copy(gMysqlConfig.m_aPrefix,g_Config.m_SqlPrefix);
		str_copy(gMysqlConfig.m_aUser,g_Config.m_SqlUser);
		str_copy(gMysqlConfig.m_aPass,g_Config.m_SqlPass);
		str_copy(gMysqlConfig.m_aIp,g_Config.m_SqlHost);
		gMysqlConfig.m_Port = g_Config.m_SqlPort;
		gMysqlConfig.m_Setup = g_Config.m_SqlSetup;
		gMysqlConfig.m_aBindaddr[0] = '\0';
	auto database = CreateMysqlConnection(gMysqlConfig);
	char aError[256] = "error message not initialized";
	if(database->Connect(aError, sizeof(aError)))
	{
		dbg_msg("sql", "failed connecting to db: %s", aError);
		return;
	}
	auto player_name = data->player_name;
	auto pPlayer = data->pPlayer;
	// save score
	Stats stats{};
	char error[4096] = {};
	if(!database->GetStats(player_name.c_str(), stats, error, sizeof(error)))
	{
		pPlayer->m_Kills += stats.kills;
		pPlayer->m_Deaths += stats.deaths;
		pPlayer->m_Touches += stats.touches;
		pPlayer->m_Captures += stats.captures;
		pPlayer->m_Shots += stats.shots;
		pPlayer->m_Wallshots += stats.wallshots;
		pPlayer->m_WallshotKills += stats.wallshot_kills;
		pPlayer->m_Suicides += stats.suicides;
		pPlayer->m_Score = pPlayer->m_Captures * 5 + pPlayer->m_Touches + pPlayer->m_Kills - pPlayer->m_Suicides;

		int target_value = stats.fastest_capture;
		int snapped_x;
		do
		{
			snapped_x = pPlayer->m_FastestCapture;
			if(snapped_x > target_value)
			{
				break;
			}
		} while(!pPlayer->m_FastestCapture.compare_exchange_strong(snapped_x, target_value));
	}
	else
	{
		dbg_msg("sql", "failed to read stats: %s", error);
	}
	database->Disconnect();
}

void SqlHandler::set_stats_handler(void *ev)
{
	auto data = static_cast<SetStatsData *>(ev);
	CMysqlConfig gMysqlConfig;

		str_copy(gMysqlConfig.m_aDatabase,g_Config.m_SqlDatabase);
		str_copy(gMysqlConfig.m_aPrefix,g_Config.m_SqlPrefix);
		str_copy(gMysqlConfig.m_aUser,g_Config.m_SqlUser);
		str_copy(gMysqlConfig.m_aPass,g_Config.m_SqlPass);
		str_copy(gMysqlConfig.m_aIp,g_Config.m_SqlHost);
		gMysqlConfig.m_Port = g_Config.m_SqlPort;
		gMysqlConfig.m_Setup = g_Config.m_SqlSetup;
		gMysqlConfig.m_aBindaddr[0] = '\0';
	auto database = CreateMysqlConnection(gMysqlConfig);
	char aError[256] = "error message not initialized";
	if(database->Connect(aError, sizeof(aError)))
	{
		dbg_msg("sql", "failed connecting to db: %s", aError);
		return;
	}

	auto player_name = data->m_player_name;
	auto stats = data->m_stats;
	char error[4096] = {};
	database->AddStats(player_name.c_str(), stats, error, sizeof(error));
	database->Disconnect();
}

void SqlHandler::set_server_stats_handler(void *ev)
{
	auto data = static_cast<SetServerStatsData *>(ev);
	CMysqlConfig gMysqlConfig;

		str_copy(gMysqlConfig.m_aDatabase,g_Config.m_SqlDatabase);
		str_copy(gMysqlConfig.m_aPrefix,g_Config.m_SqlPrefix);
		str_copy(gMysqlConfig.m_aUser,g_Config.m_SqlUser);
		str_copy(gMysqlConfig.m_aPass,g_Config.m_SqlPass);
		str_copy(gMysqlConfig.m_aIp,g_Config.m_SqlHost);
		gMysqlConfig.m_Port = g_Config.m_SqlPort;
		gMysqlConfig.m_Setup = g_Config.m_SqlSetup;
		gMysqlConfig.m_aBindaddr[0] = '\0';
	auto database = CreateMysqlConnection(gMysqlConfig);
	char aError[256] = "error message not initialized";
	if(database->Connect(aError, sizeof(aError)))
	{
		dbg_msg("sql", "failed connecting to db: %s", aError);
		return;
	}

	auto stats = data->stats;
	char error[4096] = {};
	if(database->AddServerStats("save_server", stats, error, sizeof(error)))
	{
		dbg_msg("sql", "failed connecting to db: %s", error);
	}
	database->Disconnect();
}

void SqlHandler::show_rank_handler(void *ev)
{
	auto data = static_cast<GetPlayerStatsData *>(ev);
	CMysqlConfig gMysqlConfig;

		str_copy(gMysqlConfig.m_aDatabase,g_Config.m_SqlDatabase);
		str_copy(gMysqlConfig.m_aPrefix,g_Config.m_SqlPrefix);
		str_copy(gMysqlConfig.m_aUser,g_Config.m_SqlUser);
		str_copy(gMysqlConfig.m_aPass,g_Config.m_SqlPass);
		str_copy(gMysqlConfig.m_aIp,g_Config.m_SqlHost);
		gMysqlConfig.m_Port = g_Config.m_SqlPort;
		gMysqlConfig.m_Setup = g_Config.m_SqlSetup;
		gMysqlConfig.m_aBindaddr[0] = '\0';
	auto database = CreateMysqlConnection(gMysqlConfig);
	char aError[256] = "error message not initialized";
	if(database->Connect(aError, sizeof(aError)))
	{
		dbg_msg("sql", "failed connecting to db: %s", aError);
		return;
	}
	auto player_name = data->player_name;
	auto pPlayer = data->pPlayer;
	// save score
	int rank;
	char error[4096] = {};
	if(!database->GetRank(player_name.c_str(), rank, error, sizeof(error)))
	{
		char aBuf [128];
		str_format(aBuf, sizeof(aBuf), "Your rank is: %i", rank);
		pPlayer->SendChat(aBuf);
	}
	else
	{
		dbg_msg("sql", "failed to read stats: %s", error);
	}
	database->Disconnect();
}

void SqlHandler::show_top5_handler(void *ev)
{
	auto data = static_cast<ShowTop5Data *>(ev);
	CMysqlConfig gMysqlConfig;

		str_copy(gMysqlConfig.m_aDatabase,g_Config.m_SqlDatabase);
		str_copy(gMysqlConfig.m_aPrefix,g_Config.m_SqlPrefix);
		str_copy(gMysqlConfig.m_aUser,g_Config.m_SqlUser);
		str_copy(gMysqlConfig.m_aPass,g_Config.m_SqlPass);
		str_copy(gMysqlConfig.m_aIp,g_Config.m_SqlHost);
		gMysqlConfig.m_Port = g_Config.m_SqlPort;
		gMysqlConfig.m_Setup = g_Config.m_SqlSetup;
		gMysqlConfig.m_aBindaddr[0] = '\0';
	auto database = CreateMysqlConnection(gMysqlConfig);
	char aError[256] = "error message not initialized";
	if(database->Connect(aError, sizeof(aError)))
	{
		dbg_msg("sql", "failed connecting to db: %s", aError);
		return;
	};
	auto pPlayer = data->pPlayer;
	// save score
	std::vector<PlayerWithScore> top5{};
	char error[4096] = {};
	if(!database->GetTop5(top5, error, sizeof(error)))
	{
		char aBuf [128];
		pPlayer->SendChat("Top5:");
		for (int i = 0; i < top5.size(); i++) {
			str_format(aBuf, sizeof(aBuf), "%i. %s with %i points", i + 1, top5[i].name, top5[i].score);
			pPlayer->SendChat(aBuf);
		}
	}
	else
	{
		dbg_msg("sql", "failed to read stats: %s", error);
	}
	database->Disconnect();
}

SqlHandler::SqlHandler() :
	m_thread_pool(get_proc_count())
{
	m_thread_pool.set_pause(false);
	m_should_stop.store(true);
	// add handler for eventid
	m_queue.appendListener(EventType::CreatePlayer, make_callback(&SqlHandler::get_player_stats_handler));
	m_queue.appendListener(EventType::SetStats, make_callback(&SqlHandler::set_stats_handler));
	m_queue.appendListener(EventType::SetServerStats, make_callback(&SqlHandler::set_server_stats_handler));
	m_queue.appendListener(EventType::ShowRank, make_callback(&SqlHandler::show_rank_handler));
	m_queue.appendListener(EventType::ShowTop5, make_callback(&SqlHandler::show_top5_handler));
}

void SqlHandler::start()
{
	if(m_should_stop)
	{
		m_should_stop.store(false);

		m_thread = std::thread(&SqlHandler::threadloop, this);
	}
}

void SqlHandler::stop()
{
	if(!m_should_stop)
	{
		m_should_stop.store(true);
		m_thread.join();
	}
}

void SqlHandler::set_stats(const std::string player_name, const Stats stats)
{
	SetStatsData *data = new SetStatsData();
	data->m_player_name = player_name;
	data->m_stats = stats;
	m_queue.enqueue(EventType::SetStats, data);
}

void SqlHandler::get_player_stats(CPlayer *pPlayer, const std::string player_name)
{
	GetPlayerStatsData *data = new GetPlayerStatsData();
	data->pPlayer = pPlayer;
	data->player_name = player_name;
	m_queue.enqueue(EventType::CreatePlayer, data);
}

void SqlHandler::set_server_stats(const ServerStats stats)
{
	SetServerStatsData *data = new SetServerStatsData();
	data->stats = stats;
	m_queue.enqueue(EventType::SetServerStats, data);
}

void SqlHandler::show_top5(CPlayer *pPlayer)
{
	ShowTop5Data *data = new ShowTop5Data();
	data->pPlayer = pPlayer;
	m_queue.enqueue(EventType::ShowTop5, data);
}

void SqlHandler::show_rank(CPlayer *pPlayer, const std::string player_name)
{
	GetPlayerStatsData *data = new GetPlayerStatsData();
	data->pPlayer = pPlayer;
	data->player_name = player_name;
	m_queue.enqueue(EventType::ShowRank, data);
}

void SqlHandler::threadloop()
{
	while(!m_should_stop.load())
	{
		// time out after 1 second in order to re-check if m_should_stop is true
		if(m_queue.waitFor(std::chrono::seconds(1)))
		{
			// process all events
			m_queue.process();
		}
	}
}