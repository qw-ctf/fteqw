#include <emscripten/bind.h>
#include <emscripten/val.h>
#include "quakedef.h"
#include "fragstats.h"

using namespace emscripten;

/**
 * // Sample use of the browser API.
 * //
 * // All functions not returning plain strings and numbers return live
 * // references to the WASM heap and thus references values that update
 * // as the engine runs. This allows the frontend to fetch references
 * // at start, and continue pull data from these throughout the session.
 * //
 * // Some functions return Quake charset strings. These are exposed
 * // as Uint8Array and it's up to the user of the API transform such
 * // arrays into something presentable, with the freedom to deal with
 * // colors and special characters.
 * //
 * // Functions that return an object based on an index may throw
 * // exceptions on for example out-of-bounds.
 *
 * // Demonstration purposes, will botch special chars and colors.
 * const txt = new TextDecoder();
 *
 * const client = Module.getClientState();
 *
 * console.log(client.gametime, client.getLevelName());
 *
 * for (var it = client.getItemTimers(); it != null; it = it.getNext()) {
 *     console.log(it.getTypeName(), "picked up at ", it.getLocation());
 * }
 *
 * console.log(client.getPlayerLocation(3));
 *
 * const player = client.getPlayer(3);
 *
 * // Strings are typed as Uint8Array as they aren't UTF-8 or ASCII.
 * // Deal with remapping frontend side as this will be ugly.
 * console.log(txt.decode(player.getName()), txt.decode(player.getTeam()));
 *
 * // There are "cleaned" versions with special symbols and colors stripped.
 * console.log(player.getTeamPlain(), player.getNamePlain());
 *
 * const stats = client.getStats();
 * console.log(stats[Module.STAT_ARMOR]);

 * const items = stats[Module.STAT_ITEMS];
 * console.log("Has RL", items & Module.IT_ROCKET_LAUNCHER);
 *
 * const rlstats = player.getWeaponStats(Module.W_ROCKET_LAUNCHER);
 * console.log(rlstats.hit, rlstats.total);
 *
 * const fragstats = Module.getFragStats();
 * console.log(fragstats.totalkills);
 *
 * const player_fragstats = fragstats.getClientTotals(3);
 * console.log(player_fragstats.teamkills);
 */

typedef struct entity_mapping_st {
	const char *mdl;
	const char *tp_cvar;
	unsigned int item;
	int skin;
} entity_mapping_t;

static entity_mapping_t entity_mapping[] = {
		{ "progs/backpack.mdl", "tp_name_backpack", 0,                   -1 },
		{ "progs/ring.mdl",     "tp_name_ring",     IT_INVISIBILITY,     -1 },
		{ "progs/invulner.mdl", "tp_name_pent",     IT_INVULNERABILITY,  -1 },
		{ "progs/quaddama.mdl", "tp_name_quad",     IT_QUAD,             -1 },
		{ "progs/suit.mdl",     "tp_name_suit",     IT_SUIT,             -1 },
		{ "progs/armor.mdl",    "tp_name_ra",       IT_ARMOR3,            2 },
		{ "progs/armor.mdl",    "tp_name_ya",       IT_ARMOR2,            1 },
		{ "progs/armor.mdl",    "tp_name_ga",       IT_ARMOR1,            0 },
		{ "progs/g_shot.mdl",   "tp_name_ssg",      IT_SUPER_SHOTGUN,    -1 },
		{ "progs/g_nail.mdl",   "tp_name_ng",       IT_NAILGUN,          -1 },
		{ "progs/g_nail2.mdl",  "tp_name_sng",      IT_SUPER_NAILGUN,    -1 },
		{ "progs/g_rock.mdl",   "tp_name_gl",       IT_GRENADE_LAUNCHER, -1 },
		{ "progs/g_rock2.mdl",  "tp_name_rl",       IT_ROCKET_LAUNCHER,  -1 },
		{ "progs/g_light.mdl",  "tp_name_lg",       IT_LIGHTNING,        -1 },
		{ "maps/b_bh100.bsp",   "tp_name_mh",       IT_SUPERHEALTH,      -1 },
};

static const entity_mapping_t *find_entity(int entnum) {
	entity_state_t *ent;
	const char *mdl;
	int i;

	if (entnum >= cl.maxlerpents || !cl.lerpentssequence || cl.lerpents[entnum].sequence != cl.lerpentssequence) {
		if (entnum >= 0 || entnum < cl_baselines_count) {
			ent = &cl_baselines[entnum];
		} else {
			throw std::out_of_range("itemtimer entity index out of range");
		}
	} else {
		ent = (&cl.lerpents[entnum])->entstate;
	}

	if (ent->modelindex < 0 || ent->modelindex >= MAX_PRECACHE_MODELS)
		throw std::out_of_range("itemtimer entity model index out of range");

	mdl = cl.model_name[ent->modelindex];
	if (!mdl)
		throw std::out_of_range("itemtimer entity model not found");

	for (i = 0; i < countof(entity_mapping); i++) {
		entity_mapping_t *e = &entity_mapping[i];
		if (!strcmp(e->mdl, mdl) && (e->skin == -1 || e->skin == ent->skinnum))
			return e;
	}

	throw std::invalid_argument("itemtimer entity type not supported");
}

static void collect_userinfo(void *ctx, const char *key, const char *value) {
	emscripten::val *result = (emscripten::val *) ctx;
	result->set(key, value);
}

EMSCRIPTEN_BINDINGS(browser_api) {
	constant("STAT_HEALTH",         (int) STAT_HEALTH        );
	constant("STAT_WEAPONMODELI",   (int) STAT_WEAPONMODELI  );
	constant("STAT_AMMO",           (int) STAT_AMMO          );
	constant("STAT_ARMOR",          (int) STAT_ARMOR         );
	constant("STAT_WEAPONFRAME",    (int) STAT_WEAPONFRAME   );
	constant("STAT_SHELLS",         (int) STAT_SHELLS        );
	constant("STAT_NAILS",          (int) STAT_NAILS         );
	constant("STAT_ROCKETS",        (int) STAT_ROCKETS       );
	constant("STAT_CELLS",          (int) STAT_CELLS         );
	constant("STAT_ACTIVEWEAPON",   (int) STAT_ACTIVEWEAPON  );
	constant("STAT_TOTALSECRETS",   (int) STAT_TOTALSECRETS  );
	constant("STAT_TOTALMONSTERS",  (int) STAT_TOTALMONSTERS );
	constant("STAT_SECRETS",        (int) STAT_SECRETS       );
	constant("STAT_MONSTERS",       (int) STAT_MONSTERS      );
	constant("STAT_ITEMS",          (int) STAT_ITEMS         );
	constant("STAT_VIEWHEIGHT",     (int) STAT_VIEWHEIGHT    );
	constant("STAT_TIME",           (int) STAT_TIME          );
	constant("STAT_MATCHSTARTTIME", (int) STAT_MATCHSTARTTIME);

	constant("W_AXE",              0);
	constant("W_SHOTGUN",          1);
	constant("W_SUPER_SHOTGUN",    2);
	constant("W_NAILGUN",          3);
	constant("W_SUPER_NAILGUN",    4);
	constant("W_GRENADE_LAUNCHER", 5);
	constant("W_ROCKET_LAUNCHER",  6);
	constant("W_LIGHTNING",        7);

	constant("IT_SHOTGUN",          (int) IT_SHOTGUN         );
	constant("IT_SUPER_SHOTGUN",    (int) IT_SUPER_SHOTGUN   );
	constant("IT_NAILGUN",          (int) IT_NAILGUN         );
	constant("IT_SUPER_NAILGUN",    (int) IT_SUPER_NAILGUN   );
	constant("IT_GRENADE_LAUNCHER", (int) IT_GRENADE_LAUNCHER);
	constant("IT_ROCKET_LAUNCHER",  (int) IT_ROCKET_LAUNCHER );
	constant("IT_LIGHTNING",        (int) IT_LIGHTNING       );
	constant("IT_SUPER_LIGHTNING",  (int) IT_SUPER_LIGHTNING );
	constant("IT_SHELLS",           (int) IT_SHELLS          );
	constant("IT_NAILS",            (int) IT_NAILS           );
	constant("IT_ROCKETS",          (int) IT_ROCKETS         );
	constant("IT_CELLS",            (int) IT_CELLS           );
	constant("IT_AXE",              (int) IT_AXE             );
	constant("IT_ARMOR1",           (int) IT_ARMOR1          );
	constant("IT_ARMOR2",           (int) IT_ARMOR2          );
	constant("IT_ARMOR3",           (int) IT_ARMOR3          );
	constant("IT_SUPERHEALTH",      (int) IT_SUPERHEALTH     );
	constant("IT_KEY1",             (int) IT_KEY1            );
	constant("IT_KEY2",             (int) IT_KEY2            );
	constant("IT_INVISIBILITY",     (int) IT_INVISIBILITY    );
	constant("IT_INVULNERABILITY",  (int) IT_INVULNERABILITY );
	constant("IT_SUIT",             (int) IT_SUIT            );
	constant("IT_QUAD",             (int) IT_QUAD            );
	constant("IT_SIGIL1",           (int) IT_SIGIL1          );
	constant("IT_SIGIL2",           (int) IT_SIGIL2          );
	constant("IT_SIGIL3",           (int) IT_SIGIL3          );
	constant("IT_SIGIL4",           (int) IT_SIGIL4          );

	class_<player_info_t::wstats_s>("WeaponStats")
		.property("hit", &player_info_t::wstats_s::hit)
		.property("total", &player_info_t::wstats_s::total)
		.function("getName", +[](struct player_info_t::wstats_s& self) -> emscripten::val {
			size_t len = strnlen(self.wname, 16);
			return val(typed_memory_view(len, (unsigned char*) self.wname));
		});

	class_<player_info_t>("PlayerInfo")
		.property("userid", &player_info_t::userid)
		.property("spectator", &player_info_t::spectator)
		.property("frags", &player_info_t::frags)
		.property("topcolor", &player_info_t::rtopcolor)
		.property("bottomcolor", &player_info_t::rbottomcolor)
		.function("getName", +[](player_info_t& self) -> emscripten::val {
			size_t len = strnlen(self.name, MAX_SCOREBOARDNAME);
			return val(typed_memory_view(len, (unsigned char *) self.name));
		})
		.function("getNamePlain", +[](player_info_t& self) -> std::string {
			conchar_t buffer[MAX_SCOREBOARDNAME];
			char out[MAX_SCOREBOARDNAME];
			COM_ParseFunString(CON_WHITEMASK, self.name, buffer, sizeof(buffer), qfalse);
			COM_DeFunString(buffer, NULL, out, sizeof(out), qtrue, qfalse);
			return std::string(out);
		})
		.function("getTeam", +[](player_info_t& self) -> emscripten::val {
			size_t len = strnlen(self.team, MAX_INFO_KEY);
			return val(typed_memory_view(len, (unsigned char *) self.team));
		})
		.function("getTeamPlain", +[](player_info_t& self) -> std::string {
			conchar_t buffer[MAX_INFO_KEY];
			char out[MAX_INFO_KEY];
			COM_ParseFunString(CON_WHITEMASK, self.team, buffer, sizeof(buffer), qfalse);
			COM_DeFunString(buffer, NULL, out, sizeof(out), qtrue, qfalse);
			return std::string(out);
		})
		.function("getWeaponStats", +[](player_info_t& self, size_t index) -> struct player_info_s::wstats_s * {
			if (index < 0 && index >= 16)
				throw std::out_of_range("Weapon index out of range");
			return &(self.weaponstats[index]);
		}, allow_raw_pointers())
		.function("getStats", +[](player_info_t& self) -> emscripten::val {
			return val(typed_memory_view(MAX_QW_STATS, self.stats));
		})
		.function("getStatsFloat", +[](player_info_t& self) -> emscripten::val {
			return val(typed_memory_view(MAX_QW_STATS, self.statsf));
		})
		.function("getLocation", +[](player_info_t& self) -> std::string {
			const char *location;
			int index = &self - cl.players;
			if (index + 1 < cl.maxlerpents && cl.lerpentssequence && cl.lerpents[index + 1].sequence == cl.lerpentssequence)
				location = TP_LocationName((&cl.lerpents[index + 1])->origin);
			else if (cl.lerpentssequence && cl.lerpplayers[index].sequence == cl.lerpentssequence)
				location = TP_LocationName((&cl.lerpplayers[index])->origin);
			else
				throw std::out_of_range("Player index out of range");
			if (location != NULL)
				return std::string(location);
			return std::string("unknown");
		})
		.function("getUserInfo", +[](player_info_t& self) -> emscripten::val {
			emscripten::val result = emscripten::val::object();
			if (self.userinfovalid) {
				InfoBuf_Enumerate(&self.userinfo, &result, collect_userinfo);
			}
			return result;
		})
		.function("setUserInfo", +[](player_info_t& self, emscripten::val jskey, emscripten::val jsvalue) -> emscripten::val {
			const std::string key = jskey.as<std::string>();
			const std::string value = jsvalue.as<std::string>();
			bool result = InfoBuf_SetKey(&self.userinfo, key.c_str(), value.c_str());
			return emscripten::val(result);

		}, allow_raw_pointers());

	class_<client_state_t::itemtimer_s>("ItemTimer")
		.property("start", &client_state_t::itemtimer_s::start)
		.property("end", &client_state_t::itemtimer_s::end)
		.property("entnum", &client_state_t::itemtimer_s::entnum)
		.property("duration", &client_state_t::itemtimer_s::duration)
		.property("radius", &client_state_t::itemtimer_s::radius)
		.function("getType", +[](client_state_t::itemtimer_s& self) -> unsigned int {
			const entity_mapping_t *e = find_entity(self.entnum);
			return e->item;
		})
		.function("getTypeName", +[](client_state_t::itemtimer_s& self) -> std::string {
			const entity_mapping_t *e = find_entity(self.entnum);
			cvar_t *c = Cvar_FindVar(e->tp_cvar);
			if (!c)
				throw std::invalid_argument("itemtimer entity type not supported");
			return std::string(c->string);
		})
		.function("getLocation", +[](client_state_t::itemtimer_s& self) -> std::string {
			const char *location = TP_LocationName(self.origin);
			if (location != NULL)
				return std::string(location);
			return std::string("unknown");
		})
		.function("getNext", +[](client_state_t::itemtimer_s& self) -> client_state_t::itemtimer_s* {
			return self.next;
		}, allow_raw_pointers());

	class_<client_state_t>("ClientState")
		.property("deathmatch", &client_state_t::deathmatch)
		.property("teamplay", &client_state_t::teamplay)
		.property("allocated_client_slots", &client_state_t::allocated_client_slots)
		.property("matchgametimestart", &client_state_t::matchgametimestart)
		.property("matchstate", &client_state_t::matchstate) // enum, how
		.property("gametime", &client_state_t::gametime)
		.property("time", &client_state_t::time)
		.function("getItemTimer", +[](client_state_t& self) -> client_state_t::itemtimer_s* {
			return self.itemtimers;
		}, allow_raw_pointers())
		.function("getItemTimers", +[](client_state_t& self) -> emscripten::val {
			emscripten::val result = emscripten::val::array();
			int n_timer = 0;
			for (client_state_t::itemtimer_s *it = cl.itemtimers; it != NULL; it = it->next) {
				result.set(n_timer++, it);
			}
			return result;
		}, allow_raw_pointers())
		.function("getLevelName", +[](client_state_t& self) -> emscripten::val {
			size_t len = strnlen(self.levelname, 40);
			return val(typed_memory_view(len, (unsigned char *) self.levelname));
		})
		.function("getLevelNamePlain", +[](client_state_t& self) -> std::string {
			conchar_t buffer[40];
			char out[40];
			COM_ParseFunString(CON_WHITEMASK, self.levelname, buffer, sizeof(buffer), qfalse);
			COM_DeFunString(buffer, NULL, out, sizeof(out), qtrue, qfalse);
			return std::string(out);
		})
		.function("getPlayer", +[](client_state_t& self, size_t index) -> player_info_t* {
			if (index < 0 && index >= MAX_CLIENTS)
				throw std::out_of_range("Player index out of range");
			return &(self.players[index]);
		}, allow_raw_pointers())
		.function("getPlayers", +[](client_state_t& self) -> emscripten::val {
			emscripten::val result = emscripten::val::array();
			int n_player = 0;
			for (int i = 0; i < cl.allocated_client_slots; i++) {
				player_info_t *player = &(cl.players[i]);
				if (player->name[0] && !player->spectator) {
					result.set(n_player++, player);
				}
			}
			return result;
		}, allow_raw_pointers());

	function("getClientState", +[]() -> client_state_t* {
		return &cl;
	}, allow_raw_pointers());

	class_<fragstats_t::wt_s>("WeaponTotals")
		.property("kills", &fragstats_t::wt_s::kills)
		.property("teamkills", &fragstats_t::wt_s::teamkills)
		.property("suicides", &fragstats_t::wt_s::suicides)
		.property("ownkills", &fragstats_t::wt_s::ownkills)
		.property("owndeaths", &fragstats_t::wt_s::owndeaths)
		.property("ownteamkills", &fragstats_t::wt_s::ownteamkills)
		.property("ownteamdeaths", &fragstats_t::wt_s::ownteamdeaths)
		.property("ownsuicides", &fragstats_t::wt_s::ownsuicides)
		.function("getName", +[](fragstats_t::wt_s& self) {
			return std::string(self.fullname);
		})
		.function("getAbbrev", +[](fragstats_t::wt_s& self) {
			return std::string(self.abrev);
		})
		.function("getImage", +[](fragstats_t::wt_s& self) {
			return std::string(self.image);
		})
		.function("getCodeName", +[](fragstats_t::wt_s& self) {
			return std::string(self.codename);
		});

	class_<fragstats_t::ct_s>("ClientTotals")
		.property("caps", &fragstats_t::ct_s::caps)
		.property("drops", &fragstats_t::ct_s::drops)
		.property("grabs", &fragstats_t::ct_s::grabs)
		.property("owndeaths", &fragstats_t::ct_s::owndeaths)
		.property("ownkills", &fragstats_t::ct_s::ownkills)
		.property("deaths", &fragstats_t::ct_s::deaths)
		.property("kills", &fragstats_t::ct_s::kills)
		.property("teamkills", &fragstats_t::ct_s::teamkills)
		.property("teamdeaths", &fragstats_t::ct_s::teamdeaths)
		.property("suicides", &fragstats_t::ct_s::suicides);

	class_<fragstats_t>("FragStats")
		.property("totaldeaths", &fragstats_t::totaldeaths)
		.property("totalsuicides", &fragstats_t::totalsuicides)
		.property("totalteamkills", &fragstats_t::totalteamkills)
		.property("totalkills", &fragstats_t::totalkills)
		.property("totaltouches", &fragstats_t::totaltouches)
		.property("totalcaps", &fragstats_t::totalcaps)
		.property("totaldrops", &fragstats_t::totaldrops)
		.function("getWeaponTotals", +[](fragstats_t& self, size_t index) -> struct fragstats_t::wt_s * {
			if (index < 0 && index >= MAX_WEAPONS)
				throw std::out_of_range("Weapon index out of range");
			return &(self.weapontotals[index]);
		}, allow_raw_pointers())
		.function("getClientTotals", +[](fragstats_t& self, size_t index) -> struct fragstats_t::ct_s * {
			if (index < 0 && index >= MAX_CLIENTS)
				throw std::out_of_range("Client index out of range");
			return &(self.clienttotals[index]);
		}, allow_raw_pointers());

	function("getFragStats", +[]() -> fragstats_t* {
		extern fragstats_t fragstats;
		return &fragstats;
	}, allow_raw_pointers());

	function("getDemoTime",  +[]() -> float {
		extern float demtime;
		return demtime;
	});
}
