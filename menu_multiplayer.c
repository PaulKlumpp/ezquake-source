
#include "quakedef.h"
#include "keys.h"
#include "EX_browser.h"
#include "settings.h"
#include "settings_page.h"
#include "Ctrl.h"
#include "Ctrl_Tab.h"
#include "menu.h"
#ifdef GLQUAKE
#include "gl_model.h"
#include "gl_local.h"
#endif
#include "menu_multiplayer.h"

#define BROWSERPADDING 4

CTab_t sb_tab;

extern cvar_t scr_scaleMenu;
#ifdef GLQUAKE
extern int menuwidth;
extern int menuheight;
#else
#define menuwidth vid.width
#define menuheight vid.height
#endif

static settings_page sbsettings;
static setting sbsettings_arr[] = {
	ADDSET_SEPARATOR("Server Filters"),
	ADDSET_BOOL		("Hide Empty", sb_hideempty),
	ADDSET_BOOL		("Hide Full", sb_hidefull),
	ADDSET_BOOL		("Hide Not Empty", sb_hidenotempty),
	ADDSET_BOOL		("Hide Dead", sb_hidedead),

	ADDSET_SEPARATOR("Display Columns"),
	ADDSET_BOOL		("Show Ping", sb_showping),
	ADDSET_BOOL		("Show Map", sb_showmap),
	ADDSET_BOOL		("Show Gamedir", sb_showgamedir),
	ADDSET_BOOL		("Show Players", sb_showplayers),
	ADDSET_BOOL		("Show Timelimit", sb_showtimelimit),
	ADDSET_BOOL		("Show Fraglimit", sb_showfraglimit),
	ADDSET_BOOL		("Show Server Address", sb_showaddress),

	ADDSET_SEPARATOR("Display"),
	ADDSET_BOOL		("Server Status", sb_status),

	ADDSET_SEPARATOR("Network Filters"),
	ADDSET_NUMBER	("Ping Timeout", sb_pingtimeout, 50, 1000, 50),
	ADDSET_NUMBER	("Pings Per Server", sb_pings, 1, 5, 1),
	ADDSET_NUMBER	("Pings Per Second", sb_pingspersec, 10, 300, 10),
	ADDSET_NUMBER	("Master Timeout", sb_mastertimeout, 50, 1000, 50),
	ADDSET_NUMBER	("Master Retries", sb_masterretries, 1, 5, 1),
	ADDSET_NUMBER	("Info Timeout", sb_infotimeout, 50, 1000, 50),
	ADDSET_NUMBER	("Info Retries", sb_inforetries, 0, 4, 1),
	ADDSET_NUMBER	("Infos Per Second", sb_infospersec, 10, 1000, 10)
};

/* generates a toggle function for custom enum in ADDSET_CUSTOM setting type */
#define GENERATE_ENUM_TOGGLE_PROC(basename,var,upperbound)	\
	static void M_CG_##basename##_t(qbool back) {		\
		(var) += back ? -1 : 1;			\
		if ((var) >= upperbound) {		\
			(var) = 0;					\
		} else if ((var) < 0) {			\
			(var) = upperbound - 1;		\
		}								\
	}

/* generates a reading function for custom enum in ADDSET_CUSTOM setting type */
#define GENERATE_ENUM_READ_PROC(basename,var,ubound,descarray)	\
	static const char* M_CG_##basename##_r(void) {	\
		if (((var) > 0) && ((var) < ubound)) {			\
			return descarray[var];						\
		} else {										\
			return descarray[0];						\
		}												\
	}

// spawns a variable of given enum type and it's reading and writing menu function
#define GENERATE_ENUM_MENU_FUNC(menu_enum) \
	static menu_enum ## _t menu_enum ## _var; \
	GENERATE_ENUM_READ_PROC(menu_enum, menu_enum ## _var, menu_enum ## _max, menu_enum ## _desc) \
	GENERATE_ENUM_TOGGLE_PROC(menu_enum, menu_enum ## _var, menu_enum ## _max)


/* bot match type */
	typedef enum bm_type_e {
		bmt_arena, bmt_clarena, bmt_duel, bmt_ffa, bmt_team, bm_type_max
	} bm_type_t;
	static const char *bm_type_desc[bm_type_max] = {
		"Arena", "Clan Arena", "Duel", "Free For All", "Teamplay"
	};
	GENERATE_ENUM_MENU_FUNC(bm_type);

/* bot match map */
	typedef enum bm_map_e {
		bmm_dm3, bmm_dm4, bmm_dm6, bmm_ztndm3, bmm_e1m2, bmm_aerowalk, bmm_spinev2,
		bmm_pkeg1, bmm_ultrav, bmm_frobodm2, bmm_amphi2, bmm_povdmm4, bmm_dranzdm8,
		bm_map_max
	} bm_map_t;
	static const char *bm_map_desc[bm_map_max] = {
		"dm3", "dm4", "dm6", "ztndm3", "e1m2", "aerowalk", "spinev2", "pkeg1",
		"ultrav", "frobodm2", "amphi2", "povdmm4", "dranzdm8"
	};
	GENERATE_ENUM_MENU_FUNC(bm_map);

/* bot match starter */
	static void M_CG_BotMatch_Start(void) {
		const char *cfg;
		const char *map = bm_map_desc[bm_map_var];

		switch (bm_type_var) {
			case bmt_arena:		cfg = "arena"; break;
			case bmt_clarena:	cfg = "clarena"; break;
			case bmt_duel:		cfg = "duel"; break;
			case bmt_ffa:		cfg = "ffa"; break;
			case bmt_team:		cfg = "team"; break;
			default:			cfg = "ffa"; break;
		}

		Cbuf_AddText(va("disconnect;gamedir fbca;exec configs/%s.cfg;map %s\n",cfg,map));
		M_LeaveMenus();
	}


/* coop game difficulty */
	typedef enum game_skill_e {
		GSKILL_EASY = 0,
		GSKILL_NORMAL,
		GSKILL_HARD,
		GSKILL_NIGHTMARE,
		game_skill_max
	} game_skill_t;
	static const char* game_skill_desc[game_skill_max] = {
		"Easy", "Normal", "Hard", "Nightmare"
	};
	GENERATE_ENUM_MENU_FUNC(game_skill);

/* coop game team damage */
	typedef enum cg_teamdamage_e {
		TD_OFF, TD_ON, cg_teamdamage_max
	} cg_teamdamage_t;
	static const char* cg_teamdamage_desc[cg_teamdamage_max] = {
		"off", "on"
	};
	GENERATE_ENUM_MENU_FUNC(cg_teamdamage);

/* deathmatch map */
	typedef enum game_map_group_e {
		GMG_EPISODE1,	
		GMG_EPISODE2,
		GMG_EPISODE3,
		GMG_EPISODE4,
		GMG_CUSTOM,
		game_map_group_max
	} game_map_group_t;
	static const char* game_map_group_desc[game_map_group_max] = {
		"Doomed Dimension", "Realm of Black Magic", "Netherworld", "The Elder World", "Custom"
	};
	GENERATE_ENUM_MENU_FUNC(game_map_group);

/* deathmatch game mode */
	typedef enum DM_game_mode_e {
		DMGM_ffa, DMGM_duel, DMGM_tp, DMGM_arena, DM_game_mode_max
	} DM_game_mode_t;
	static const char* DM_game_mode_desc[DM_game_mode_max] = {
		"Free For All", "Duel", "Teamplay", "Arena"
	};
	GENERATE_ENUM_MENU_FUNC(DM_game_mode);

static int coopmaxplayers = 2;

static int dm_maxplayers = 2;
static int dm_maxspectators = 8;
static int dm_timelimit = 10;
static int dm_fraglimit = 0;

void Menu_CG_DM_StartGame(void)
{
	int dm;
	int tp;

	Cbuf_AddText("disconnect; gamedir qw; coop 0;\n");
	switch (DM_game_mode_var) {
		case DMGM_ffa: tp = 0; dm = 3; break;
		case DMGM_duel: tp = 0; dm = 3; break;
		case DMGM_tp: tp = 2; dm = 1; break;
		case DMGM_arena: tp = 0; dm = 4; break;
		default: tp = 0; dm = 3; break;
	}
	Cbuf_AddText(va("deathmatch %d;teamplay %d\n",dm,tp));
	Cbuf_AddText(va("maxclients %d;maxspectators %d\n",dm_maxplayers,dm_maxspectators));
	Cbuf_AddText(va("timelimit %d;fraglimit %d\n",dm_timelimit,dm_fraglimit));
	// todo: finish map choosing
	Cbuf_AddText("map dm4\n");
	M_LeaveMenus();
}

void Menu_CG_Coop_StartGame(void)
{
	int tp = (cg_teamdamage_var == TD_OFF) ? 1 : 2;
	int skill = game_skill_var;

	Cbuf_AddText("disconnect; gamedir qw; coop 1; deathmatch 0\n");
	Cbuf_AddText(va("teamplay %d; skill %d;maxclients %d;map start",
					tp,skill,coopmaxplayers));
	M_LeaveMenus();
}

static settings_page create_game_options;
static setting create_game_options_arr[] = {
	ADDSET_SEPARATOR("Bot Match"),
	ADDSET_CUSTOM	("Game Mode", M_CG_bm_type_r, M_CG_bm_type_t,
					 "Choose one of the three available game types"),
	ADDSET_CUSTOM	("Map", M_CG_bm_map_r, M_CG_bm_map_t, "Bots support only limit set of maps"),
	ADDSET_ACTION	("Start Bot Match", M_CG_BotMatch_Start, "Start the bot match"),

	ADDSET_SEPARATOR("Deathmatch"),
	ADDSET_INTNUMBER("Player slots", dm_maxplayers, 1, 32, 1),
	ADDSET_INTNUMBER("Spectator slots", dm_maxspectators, 0, 16, 1),
	ADDSET_CUSTOM	("Game Mode", M_CG_DM_game_mode_r, M_CG_DM_game_mode_t, "Game Mode"),
	ADDSET_INTNUMBER("Timelimit", dm_timelimit, 0, 60, 5),
	ADDSET_INTNUMBER("Fraglimit", dm_fraglimit, 0, 100, 10),
	ADDSET_CUSTOM	("Map Group", M_CG_game_map_group_r, M_CG_game_map_group_t, ""),
	/* ADDSET_CUSTOM	("Map", Menu_CG_map_r, Menu_CG_map_t, ""), */
	ADDSET_ACTION	("Start Deathmatch", Menu_CG_DM_StartGame,
					 "Start game with given parameters"),

	ADDSET_SEPARATOR("Cooperative"),
	ADDSET_CUSTOM	("Difficulty", M_CG_game_skill_r, M_CG_game_skill_t, 
					 "Monsters skill level"),
	ADDSET_CUSTOM	("Teamplay Damage", M_CG_cg_teamdamage_r, M_CG_cg_teamdamage_t,
					 "Whether Give damage to teammates if you shoot them"),
	ADDSET_INTNUMBER("Player Slots", coopmaxplayers, 1, 32, 1),
	ADDSET_ACTION	("Start Coop Game", Menu_CG_Coop_StartGame,
					 "Start game with given parameters"),
};

static void Servers_Draw (int x, int y, int w, int h, CTab_t *tab, CTabPage_t *page)
{
	SB_Servers_Draw(x,y,w,h);
}

static int Servers_Key(int key, CTab_t *tab, CTabPage_t *page)
{
	return SB_Servers_Key(key);
}

static void Servers_OnShow(void)
{
	SB_Servers_OnShow();
}

static qbool Servers_Mouse_Event(const mouse_state_t *ms)
{
	return SB_Servers_Mouse_Event(ms);
}

static void Sources_Draw (int x, int y, int w, int h, CTab_t *tab, CTabPage_t *page)
{
	SB_Sources_Draw(x,y,w,h);
}

static int Sources_Key(int key, CTab_t *tab, CTabPage_t *page)
{
	return SB_Sources_Key(key);
}

static qbool Sources_Mouse_Event(const mouse_state_t *ms)
{
	return SB_Sources_Mouse_Event(ms);
}

static void Players_Draw(int x, int y, int w, int h, CTab_t *tab, CTabPage_t *page)
{
	SB_Players_Draw(x, y, w, h);
}

static int Players_Key(int key, CTab_t *tab, CTabPage_t *page)
{
	return SB_Players_Key(key);
}
static qbool Players_Mouse_Event(const mouse_state_t *ms)
{
	return SB_Players_Mouse_Event(ms);
}

static qbool Options_Mouse_Event(const mouse_state_t *ms)
{
	return Settings_Mouse_Event(&sbsettings, ms);
}

static int Options_Key(int key, CTab_t *tab, CTabPage_t *page)
{
	return Settings_Key(&sbsettings, key);
}

static void Options_Draw(int x, int y, int w, int h, CTab_t *tab, CTabPage_t *page)
{
	Settings_Draw(x, y, w, h, &sbsettings);
}

void Menu_MultiPlayer_Draw (void)
{
	int x, y, w, h;

#ifdef GLQUAKE
	// do not scale this menu
	if (scr_scaleMenu.value)
	{
		menuwidth = vid.width;
		menuheight = vid.height;
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity ();
		glOrtho  (0, menuwidth, menuheight, 0, -99999, 99999);
	}
#endif

	w = min(640, vid.width) - BROWSERPADDING*2;
	h = min(480, vid.height) - BROWSERPADDING*2;
	x = (vid.width - w) / 2;
	y = (vid.height - h) / 2;

	Browser_window.x = x;
	Browser_window.y = y;
	Browser_window.w = w;
	Browser_window.h = h;

	CTab_Draw(&sb_tab, x, y, w, h);

	SB_Specials_Draw();
}

void Menu_MultiPlayer_Key(int key)
{
	if (SB_Specials_Key(key)) return;
	
	CTab_Key(&sb_tab, key);
}

qbool Menu_MultiPlayer_Mouse_Event(const mouse_state_t *ms)
{
	mouse_state_t nms = *ms;

    if (ms->button_up == 2) {
        Menu_MultiPlayer_Key(K_MOUSE2);
        return true;
    }

	nms.x -= Browser_window.x;
	nms.y -= Browser_window.y;
	nms.x_old -= Browser_window.x;
	nms.y_old -= Browser_window.y;

	return CTab_Mouse_Event(&sb_tab, &nms);
}

static qbool CreateGame_Mouse_Event(const mouse_state_t *ms)
{
	return Settings_Mouse_Event(&create_game_options, ms);
}

static int CreateGame_Key(int key, CTab_t *tab, CTabPage_t *page)
{
	return Settings_Key(&create_game_options, key);
}

static void CreateGame_Draw(int x, int y, int w, int h, CTab_t *tab, CTabPage_t *page)
{
	Settings_Draw(x,y,w,h,&create_game_options);
}

CTabPage_Handlers_t sb_servers_handlers = {
	Servers_Draw,
	Servers_Key,
	Servers_OnShow,
	Servers_Mouse_Event
};

CTabPage_Handlers_t sb_sources_handlers = {
	Sources_Draw,
	Sources_Key,
	NULL,
	Sources_Mouse_Event
};

CTabPage_Handlers_t sb_players_handlers = {
	Players_Draw,
	Players_Key,
	NULL,
	Players_Mouse_Event
};

CTabPage_Handlers_t sb_options_handlers = {
	Options_Draw,
	Options_Key,
	NULL,
	Options_Mouse_Event
};

CTabPage_Handlers_t sb_creategame_handlers = {
	CreateGame_Draw,
	CreateGame_Key,
	NULL,
	CreateGame_Mouse_Event
};

void Menu_MultiPlayer_Init()
{
	Settings_Page_Init(sbsettings, sbsettings_arr);
	Settings_Page_Init(create_game_options, create_game_options_arr);

	CTab_Init(&sb_tab);
	CTab_AddPage(&sb_tab, "servers", SBPG_SERVERS, &sb_servers_handlers);
	CTab_AddPage(&sb_tab, "sources", SBPG_SOURCES, &sb_sources_handlers);
	CTab_AddPage(&sb_tab, "players", SBPG_PLAYERS, &sb_players_handlers);
	CTab_AddPage(&sb_tab, "options", SBPG_OPTIONS, &sb_options_handlers);
	CTab_AddPage(&sb_tab, "create", SBPG_CREATEGAME, &sb_creategame_handlers);
	CTab_SetCurrentId(&sb_tab, SBPG_SERVERS);
}

void Menu_MultiPlayer_SwitchToServersTab(void)
{
	CTab_SetCurrentId(&sb_tab, SBPG_SERVERS);
}