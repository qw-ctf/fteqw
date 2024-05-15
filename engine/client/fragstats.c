
#include "quakedef.h"
#include "fragstats.h"

#ifdef QUAKEHUD

static void TrackerCallback(struct cvar_s *var, char *oldvalue);
static cvar_t r_tracker_frags = CVARD("r_tracker_frags", "0", "0: like vanilla quake\n1: shows only your kills/deaths\n2: shows all kills\n");
static cvar_t r_tracker_time = CVARCD("r_tracker_time", "4", TrackerCallback, "how long it takes for r_tracker messages to start fading\n");
static cvar_t r_tracker_fadetime = CVARCD("r_tracker_fadetime", "1", TrackerCallback, "how long it takes for r_tracker messages to fully fade once they start fading\n");
static cvar_t r_tracker_x = CVARCD("r_tracker_x", "0.5", TrackerCallback, "left position of the r_tracker messages, as a fraction of the screen's width, eg 0.5\n");
static cvar_t r_tracker_y = CVARCD("r_tracker_y", "0.333", TrackerCallback, "top position of the r_tracker messages, as a fraction of the screen's height, eg 0.333\n");
static cvar_t r_tracker_w = CVARCD("r_tracker_w", "0.5", TrackerCallback, "width of the r_tracker messages, as a fraction of the screen's width, eg 0.5\n");
static cvar_t r_tracker_lines = CVARCD("r_tracker_lines", "8", TrackerCallback, "number of r_tracker messages to display\n");
static void Tracker_Update(console_t *tracker)
{
   tracker->notif_l = tracker->maxlines = max(1,r_tracker_lines.ival);
   tracker->notif_x = r_tracker_x.value;
   tracker->notif_y = r_tracker_y.value;
   tracker->notif_w = max(0,r_tracker_w.value);
   tracker->notif_t = max(0,r_tracker_time.value);
   tracker->notif_fade = max(0,r_tracker_fadetime.value);

   //if its mostly on one side of the screen, align it accordingly.
   if (tracker->notif_x + tracker->notif_w*0.5 >= 0.5)
       tracker->flags |= CONF_NOTIFY_RIGHT;
   else
       tracker->flags &= ~(CONF_NOTIFY_RIGHT);
}
static void TrackerCallback(struct cvar_s *var, char *oldvalue)
{
   console_t *tracker = Con_FindConsole("tracker");
   if (tracker)
       Tracker_Update(tracker);
}


fragstats_t fragstats;

int Stats_GetKills(int playernum)
{
	return fragstats.clienttotals[playernum].kills;
}
int Stats_GetTKills(int playernum)
{
	return fragstats.clienttotals[playernum].teamkills;
}
int Stats_GetDeaths(int playernum)
{
	return fragstats.clienttotals[playernum].deaths;
}
int Stats_GetTouches(int playernum)
{
	return fragstats.clienttotals[playernum].grabs;
}
int Stats_GetCaptures(int playernum)
{
	return fragstats.clienttotals[playernum].caps;
}

qboolean Stats_HaveFlags(int showtype)
{
	int i;
	if (showtype)
		return fragstats.readcaps;
	for (i = 0; i < cl.allocated_client_slots; i++)
	{
		if (fragstats.clienttotals[i].caps ||
			fragstats.clienttotals[i].drops ||
			fragstats.clienttotals[i].grabs)
			return fragstats.readcaps;
	}
	return false;
}
qboolean Stats_HaveKills(void)
{
	return fragstats.readkills;
}

static char lastownfragplayer[64];
static float lastownfragtime;
float Stats_GetLastOwnFrag(int seat, char *res, int reslen)
{
	if (seat)
	{
		if (reslen)
			*res = 0;
		return 0;
	}

	//erk, realtime was reset?
	if (lastownfragtime > (float)realtime)
		lastownfragtime = 0;

	Q_strncpyz(res, lastownfragplayer, reslen);
	return realtime - lastownfragtime;
};
static void Stats_OwnFrag(char *name)
{
	Q_strncpyz(lastownfragplayer, name, sizeof(lastownfragplayer));
	lastownfragtime = realtime;
}

void VARGS Stats_Message(char *msg, ...);

qboolean Stats_TrackerImageLoaded(const char *in)
{
	int error;
	if (in)
		return Font_TrackerValid(unicode_decode(&error, in, &in, true));
	return false;
}
static char *Stats_GenTrackerImageString(char *in)
{	//images are of the form "foo \sg\ bar \q\"
	//which should eg be remapped to: "foo ^Ue200 bar foo ^Ue201"
	char res[256];
	char image[MAX_QPATH];
	char *outi;
	char *out;
	int i;
	if (!in || !*in)
		return NULL;

	for (out = res; *in && out < res+sizeof(res)-10; )
	{
		if (*in == '\\')
		{
			in++;
			for (outi = image; *in && outi < image+sizeof(image)-10; )
			{
				if (*in == '\\')
					break;
				*outi++ = *in++;
			}
			*outi = 0;
			in++;
			
			i = Font_RegisterTrackerImage(va("tracker/%s", image));
			if (i)
			{
				char hexchars[16] = "0123456789abcdef";
				*out++ = '^';
				*out++ = 'U';
				*out++ = hexchars[(i>>12)&15];
				*out++ = hexchars[(i>>8)&15];
				*out++ = hexchars[(i>>4)&15];
				*out++ = hexchars[(i>>0)&15];
			}
			else
			{
				//just copy the short name over, not much else we can do.
				for(outi = image; out < res+sizeof(res)-10 && *outi; )
					*out++ = *outi++;
			}
		}
		else
			*out++ = *in++;
	}
	*out = 0;
	return Z_StrDup(res);
}

void Stats_FragMessage(int p1, int wid, int p2, qboolean teamkill)
{
	static const char *nonplayers[] = {
		"BUG",
		"(teamkill)",
		"(suicide)",
		"(death)",
		"(unknown)",
		"(fixme)",
		"(fixme)"
	};
	char message[512];
	console_t *tracker;
	struct wt_s *w = &fragstats.weapontotals[wid];
	const char *p1n = (p1 < 0)?nonplayers[-p1]:cl.players[p1].name;
	const char *p2n = (p2 < 0)?nonplayers[-p2]:cl.players[p2].name;
	int localplayer = (cl.playerview[0].cam_state == CAM_EYECAM)?cl.playerview[0].cam_spec_track:cl.playerview[0].playernum;

#define YOU_GOOD		S_COLOR_GREEN
#define YOU_BAD			S_COLOR_BLUE
#define TEAM_GOOD		S_COLOR_GREEN
#define TEAM_BAD		S_COLOR_RED
#define TEAM_VBAD		S_COLOR_BLUE
#define TEAM_NEUTRAL	S_COLOR_WHITE	//enemy team thing that does not directly affect us
#define ENEMY_GOOD		S_COLOR_RED
#define ENEMY_BAD		S_COLOR_GREEN
#define ENEMY_NEUTRAL	S_COLOR_WHITE


	char *p1c = S_COLOR_WHITE;
	char *p2c = S_COLOR_WHITE;

	if (!r_tracker_frags.ival || cls.demoseeking)
		return;
	if (r_tracker_frags.ival < 2)
		if (p1 != localplayer && p2 != localplayer)
			return;

	if (teamkill)
	{//team kills/suicides are always considered bad.
		if (p1 == localplayer)
			p1c = YOU_BAD;
		else if (cl.teamplay && !strcmp(cl.players[p1].team, cl.players[localplayer].team))
			p1c = TEAM_VBAD;
		else
			p1c = TEAM_NEUTRAL;
		p2c = p1c;
	}
	else if (p1 == p2)
		p1c = p2c = YOU_BAD;
	else if (cl.teamplay && p1 >= 0 && p2 >= 0 && !strcmp(cl.players[p1].team, cl.players[p2].team))
		p1c = p2c = TEAM_VBAD;
	else
	{
		if (p2 >= 0)
		{
			//us/teammate killing is good - unless it was a teammate.
			if (p2 == localplayer)
				p2c = YOU_GOOD;
			else if (cl.teamplay && !strcmp(cl.players[p2].team, cl.players[localplayer].team))
				p2c = TEAM_GOOD;
			else
				p2c = ENEMY_GOOD;
		}
		if (p1 >= 0)
		{
			//us/teammate dying is bad.
			if (p1 == localplayer)
				p1c = YOU_BAD;
			else if (cl.teamplay && !strcmp(cl.players[p1].team, cl.players[localplayer].team))
				p1c = TEAM_BAD;
			else
				p1c = p2c;
		}
	}

	Q_snprintfz(message, sizeof(message), "%s%s ^7%s %s%s\n", p2c, p2n, Stats_TrackerImageLoaded(w->image)?w->image:w->abrev, p1c, p1n);

	tracker = Con_FindConsole("tracker");
	if (!tracker)
	{
		tracker = Con_Create("tracker", CONF_HIDDEN|CONF_NOTIFY|CONF_NOTIFY_BOTTOM);
		Tracker_Update(tracker);
	}
	Con_PrintCon(tracker, message, tracker->parseflags);
}

void Update_FlagStatus(int pidx, char *team, int got_flag)
{
	unsigned int flag = IT_KEY1 | IT_KEY2;
	player_info_t *pl = &cl.players[pidx];

	if (!strcmp(team, "blue")) {
		flag = IT_KEY2;
	} else if (!strcmp(team, "red")) {
		flag = IT_KEY1;
	}

	if (got_flag) {
		pl->tinfo.items |= flag;
	} else {
		pl->tinfo.items &= ~flag;
	}
}

void Stats_FlagMessage(fragfilemsgtypes_t type, int p1, int count)
{
	char message[512];
	console_t *tracker;

	const char *p1n = cl.players[p1].name;

	const char *c = S_COLOR_WHITE;
	const char *fc = !stricmp(cl.players[p1].team, "red") ? S_COLOR_BLUE : S_COLOR_RED;
	const char *fn = !stricmp(cl.players[p1].team, "red") ? "blue" : "red";

	if (!r_tracker_frags.ival || cls.demoseeking)
		return;

	if (type == ff_flagcaps) {
		Q_snprintfz(message, sizeof(message), "%s%s captured the %s%s%s flag! (%d captures)\n", c, p1n, fc, fn, c, count);
	}
	else if (type == ff_flagdrops) {
		Q_snprintfz(message, sizeof(message), "%s%s dropped the %s%s%s flag! (%d drops)\n", c, p1n, fc, fn, c, count);
	}
	else if (type == ff_flagtouch) {
		Q_snprintfz(message, sizeof(message), "%s%s took the %s%s%s flag! (%d takes)\n", c, p1n, fc, fn, c, count);
	} else {
		const char *rune = NULL;
		const char *rc = NULL;
		switch (type) {
			case ff_rune_res:
				rune = "Resistance";
				rc = S_COLOR_GREEN;
				break;
			case ff_rune_str:
				rune = "Strength";
				rc = S_COLOR_RED;
				break;
			case ff_rune_hst:
				rune = "Haste";
				rc = S_COLOR_YELLOW;
				break;
			case ff_rune_reg:
				rune = "Regeneration";
				rc = S_COLOR_CYAN;
				break;
			default:
				return;
		}
		Q_snprintfz(message, sizeof(message), "%s%s took the %s%s%s rune!\n", c, p1n, rc, rune, c);
	}	

	tracker = Con_FindConsole("tracker");
	if (!tracker)
	{
		tracker = Con_Create("tracker", CONF_HIDDEN|CONF_NOTIFY|CONF_NOTIFY_RIGHT|CONF_NOTIFY_BOTTOM);
		//this stuff should be configurable
		tracker->notif_l = tracker->maxlines = 8;
		tracker->notif_x = 0.5;
		tracker->notif_y = 0.333;
		tracker->notif_w = 1-tracker->notif_x;
		tracker->notif_t = 4;
		tracker->notif_fade = 1;
	}
	Con_PrintCon(tracker, message, tracker->parseflags);
}

// QTube
void CL_SetStat_Internal (int pnum, int stat, int ivalue, float fvalue);

static void Stats_SyncRuneView(int pnum)
{
	int i;

	for (i = 0; i < MAX_SPLITS; i++) {
		if (cl.playerview[i].cam_spec_track == pnum && cl.playerview[i].cam_state != CAM_FREECAM)
			CL_SetStat_Internal(i, STAT_ITEMS, cl.players[pnum].stats[STAT_ITEMS], 0.0f);
	}
}

#define RUNE_MASK (IT_SIGIL1 | IT_SIGIL2 | IT_SIGIL3 | IT_SIGIL4)

static void Stats_SetRune(int pnum, int rune)
{
	int i;
	// Only one person can have a specific rune, and this rune status
	// based on messages is a bit hacky, always clear everyone else.
	for (i = 0; i < MAX_CLIENTS; i++) {
		cl.players[i].stats[STAT_ITEMS] &= ~rune;
		cl.players[i].tinfo.items &= ~rune;
	}
	// Can only have one rune at a time
	cl.players[pnum].stats[STAT_ITEMS] &= ~RUNE_MASK;
	cl.players[pnum].stats[STAT_ITEMS] |= rune;
	cl.players[pnum].tinfo.items &= ~RUNE_MASK;
	cl.players[pnum].tinfo.items |= rune;

	Stats_SyncRuneView(pnum);
}

void Stats_Evaluate(fragfilemsgtypes_t mt, int wid, int p1, int p2)
{
	qboolean u1;
	qboolean u2;

	if (mt == ff_frags || mt == ff_tkills)
	{
		int tmp = p1;
		p1 = p2;
		p2 = tmp;
	}

	u1 = (p1 == (cl.playerview[0].playernum));
	u2 = (p2 == (cl.playerview[0].playernum));

	if (p1 == -1)
		p1 = p2;
	if (p2 == -1)
		p2 = p1;

	//messages are killed weapon killer
	switch(mt)
	{
	case ff_death:
		if (u1)
		{
			fragstats.weapontotals[wid].owndeaths++;
			fragstats.weapontotals[wid].ownkills++;
		}

		fragstats.weapontotals[wid].kills++;
		if (p1 >= 0)
			fragstats.clienttotals[p1].deaths++;
		fragstats.totaldeaths++;

		// QTube: Clear rune status on death
		cl.players[p1].stats[STAT_ITEMS] &= ~RUNE_MASK;
		cl.players[p1].tinfo.items &= ~RUNE_MASK;
		Stats_SyncRuneView(p1);
		Stats_FragMessage(p1, wid, -3, true);

		if (u1)
			Stats_Message("You died\n%s deaths: %i\n", fragstats.weapontotals[wid].fullname, fragstats.weapontotals[wid].owndeaths);
		break;
	case ff_suicide:
		if (u1)
		{
			fragstats.weapontotals[wid].ownsuicides++;
			fragstats.weapontotals[wid].owndeaths++;
			fragstats.weapontotals[wid].ownkills++;
		}

		fragstats.weapontotals[wid].suicides++;
		fragstats.weapontotals[wid].kills++;
		if (p1 >= 0)
		{
			fragstats.clienttotals[p1].suicides++;
			fragstats.clienttotals[p1].deaths++;
		}
		fragstats.totalsuicides++;
		fragstats.totaldeaths++;

		// QTube: Clear rune status on death
		cl.players[p1].stats[STAT_ITEMS] &= ~RUNE_MASK;
		cl.players[p1].tinfo.items &= ~RUNE_MASK;
		Stats_SyncRuneView(p1);
		Stats_FragMessage(p1, wid, -2, true);
		if (u1)
			Stats_Message("You killed your own dumb self\n%s suicides: %i (%i)\n", fragstats.weapontotals[wid].fullname, fragstats.weapontotals[wid].ownsuicides, fragstats.weapontotals[wid].suicides);
		break;
	case ff_bonusfrag:
		if (u1)
			fragstats.weapontotals[wid].ownkills++;
		fragstats.weapontotals[wid].kills++;
		if (p1 >= 0)
			fragstats.clienttotals[p1].kills++;
		fragstats.totalkills++;

		Stats_FragMessage(-4, wid, p1, false);
		if (u1)
		{
			Stats_OwnFrag("someone");
			Stats_Message("You killed someone\n%s kills: %i\n", fragstats.weapontotals[wid].fullname, fragstats.weapontotals[wid].ownkills);
		}
		break;
	case ff_tkbonus:
		if (u1)
			fragstats.weapontotals[wid].ownkills++;
		fragstats.weapontotals[wid].kills++;
		fragstats.clienttotals[p1].kills++;
		fragstats.totalkills++;

		if (u1)
			fragstats.weapontotals[wid].ownteamkills++;
		fragstats.weapontotals[wid].teamkills++;
		if (p1 >= 0)
			fragstats.clienttotals[p1].teamkills++;
		fragstats.totalteamkills++;

		Stats_FragMessage(-1, wid, p1, true);

		if (u1)
		{
			Stats_Message("You killed your teammate\n%s teamkills: %i\n", fragstats.weapontotals[wid].fullname, fragstats.weapontotals[wid].ownteamkills);
		}
		break;
	case ff_flagtouch:
		fragstats.clienttotals[p1].grabs++;
		fragstats.totaltouches++;

		Update_FlagStatus(p1, cl.players[p1].team, true);
		Stats_FlagMessage(ff_flagtouch, p1, fragstats.clienttotals[p1].grabs);
		break;
	case ff_flagcaps:
		if (p1 >= 0)
			fragstats.clienttotals[p1].caps++;
		fragstats.totalcaps++;

		Update_FlagStatus(p1, cl.players[p1].team, false);
		Stats_FlagMessage(ff_flagcaps, p1, fragstats.clienttotals[p1].caps);
		break;
	case ff_flagdrops:
		if (p1 >= 0)
			fragstats.clienttotals[p1].drops++;
		fragstats.totaldrops++;

		Update_FlagStatus(p1, cl.players[p1].team, false);
		Stats_FlagMessage(ff_flagdrops, p1, fragstats.clienttotals[p1].drops);
		break;
	case ff_rune_res:
		Stats_SetRune(p1, IT_SIGIL1);
		Stats_FlagMessage(ff_rune_res, p1, 0);
		break;
	case ff_rune_str:
		Stats_SetRune(p1, IT_SIGIL2);
		Stats_FlagMessage(ff_rune_str, p1, 0);
		break;
	case ff_rune_hst:
		Stats_SetRune(p1, IT_SIGIL3);
		Stats_FlagMessage(ff_rune_hst, p1, 0);
		break;
	case ff_rune_reg:
		Stats_SetRune(p1, IT_SIGIL4);
		Stats_FlagMessage(ff_rune_reg, p1, 0);
		break;
	//p1 died, p2 killed
	case ff_frags:
	case ff_fragedby:
		fragstats.weapontotals[wid].kills++;

		if (p1 >= 0)
			fragstats.clienttotals[p1].deaths++;
		fragstats.totaldeaths++;
		if (u1)
		{
			fragstats.weapontotals[wid].owndeaths++;
			if (p1 >= 0 && p2 >= 0)
				Stats_Message("%s killed you\n%s deaths: %i (%i/%i)\n", cl.players[p2].name, fragstats.weapontotals[wid].fullname, fragstats.clienttotals[p2].owndeaths, fragstats.weapontotals[wid].owndeaths, fragstats.totaldeaths);
		}

		if (p2 >= 0)
			fragstats.clienttotals[p2].kills++;
		fragstats.totalkills++;
		if (u2)
		{
			if (p1 >= 0)
				Stats_OwnFrag(cl.players[p1].name);
			fragstats.weapontotals[wid].ownkills++;
			if (p1 >= 0 && p2 >= 0)
				Stats_Message("You killed %s\n%s kills: %i (%i/%i)\n", cl.players[p1].name, fragstats.weapontotals[wid].fullname, fragstats.clienttotals[p2].kills, fragstats.weapontotals[wid].kills, fragstats.totalkills);
		}
		// QTube: Clear rune status on death
		cl.players[p1].stats[STAT_ITEMS] &= ~RUNE_MASK;
		cl.players[p1].tinfo.items &= ~RUNE_MASK;
		Stats_SyncRuneView(p1);
		Stats_FragMessage(p1, wid, p2, false);
		break;
	case ff_tkdeath:
		//killed by a teammate, but we don't know who
		//kinda useless, but this is all some mods give us
		fragstats.weapontotals[wid].teamkills++;
		fragstats.weapontotals[wid].kills++;
		fragstats.totalkills++;		//its a kill, but we don't know who from
		fragstats.totalteamkills++;

		if (u1)
			fragstats.weapontotals[wid].owndeaths++;
		fragstats.clienttotals[p1].teamdeaths++;
		fragstats.clienttotals[p1].deaths++;
		fragstats.totaldeaths++;

		// QTube: Clear rune status on death
		cl.players[p1].stats[STAT_ITEMS] &= ~RUNE_MASK;
		cl.players[p1].tinfo.items &= ~RUNE_MASK;
		Stats_SyncRuneView(p1);
		Stats_FragMessage(p1, wid, -1, true);

		if (u1)
			Stats_Message("Your teammate killed you\n%s deaths: %i\n", fragstats.weapontotals[wid].fullname, fragstats.weapontotals[wid].owndeaths);
		break;

	case ff_tkills:
	case ff_tkilledby:
		//p1 killed by p2 (kills is already inverted)
		fragstats.weapontotals[wid].teamkills++;
		fragstats.weapontotals[wid].kills++;

		if (u1)
		{
			fragstats.weapontotals[wid].ownteamdeaths++;
			fragstats.weapontotals[wid].owndeaths++;
		}
		if (p1 >= 0)
		{
			fragstats.clienttotals[p1].teamdeaths++;
			fragstats.clienttotals[p1].deaths++;
		}
		fragstats.totaldeaths++;

		if (u2)
		{
			fragstats.weapontotals[wid].ownkills++;
			fragstats.weapontotals[wid].ownkills++;
		}
		if (p2 >= 0)
		{
			fragstats.clienttotals[p2].teamkills++;
			fragstats.clienttotals[p2].kills++;
		}
		fragstats.totalkills++;

		fragstats.totalteamkills++;

		Stats_FragMessage(p1, wid, p2, false);

		// QTube: Clear rune status on death
		cl.players[p1].stats[STAT_ITEMS] &= ~RUNE_MASK;
		cl.players[p1].tinfo.items &= ~RUNE_MASK;
		Stats_SyncRuneView(p1);

		if (u1 && p2 >= 0) {
			Stats_Message("%s killed you\n%s deaths: %i (%i/%i)\n", cl.players[p2].name, fragstats.weapontotals[wid].fullname, fragstats.clienttotals[p2].owndeaths, fragstats.weapontotals[wid].owndeaths, fragstats.totaldeaths);
		}
		if (u2 && p1 >= 0 && p2 >= 0)
		{
			Stats_OwnFrag(cl.players[p1].name);
			Stats_Message("You killed %s\n%s kills: %i (%i/%i)\n", cl.players[p1].name, fragstats.weapontotals[wid].fullname, fragstats.clienttotals[p2].kills, fragstats.weapontotals[wid].kills, fragstats.totalkills);
		}
		break;
	}
}

static int Stats_FindWeapon(char *codename, qboolean create)
{
	int i;

	if (!strcmp(codename, "NONE"))
		return 0;
	if (!strcmp(codename, "NULL"))
		return 0;
	if (!strcmp(codename, "NOWEAPON"))
		return 0;

	for (i = 1; i < MAX_WEAPONS; i++)
	{
		if (!fragstats.weapontotals[i].codename)
		{
			fragstats.weapontotals[i].codename = Z_Malloc(strlen(codename)+1);
			strcpy(fragstats.weapontotals[i].codename, codename);
			return i;
		}

		if (!stricmp(fragstats.weapontotals[i].codename, codename))
		{
			if (create)
				return -2;
			return i;
		}
	}
	return -1;
}

static void Stats_StatMessage(fragfilemsgtypes_t type, int wid, char *token1, char *token2)
{
	statmessage_t *ms;
	char *t;
	ms = Z_Malloc(sizeof(statmessage_t) + strlen(token1)+1 + (token2 && *token2?strlen(token2)+1:0));
	t = (char *)(ms+1);
	ms->msgpart1 = t;
	strcpy(t, token1);
	ms->l1 = strlen(ms->msgpart1);
	if (token2 && *token2)
	{
		t += strlen(t)+1;
		ms->msgpart2 = t;
		strcpy(t, token2);
		ms->l2 = strlen(ms->msgpart2);
	}
	ms->type = type;
	ms->wid = wid;

	ms->next = fragstats.message;
	fragstats.message = ms;

	//we have a message type, save the fact that we have it.
	if (type == ff_flagtouch || type == ff_flagcaps || type == ff_flagdrops)
		fragstats.readcaps = true;
	if (type == ff_frags || type == ff_fragedby)
		fragstats.readkills = true;
}

void Stats_Clear(void)
{
	int i;
	statmessage_t *ms;

	while (fragstats.message)
	{
		ms = fragstats.message;
		fragstats.message = ms->next;
		Z_Free(ms);
	}

	for (i = 1; i < MAX_WEAPONS; i++)
	{
		if (fragstats.weapontotals[i].codename)	Z_Free(fragstats.weapontotals[i].codename);
		if (fragstats.weapontotals[i].fullname)	Z_Free(fragstats.weapontotals[i].fullname);
		if (fragstats.weapontotals[i].abrev)	Z_Free(fragstats.weapontotals[i].abrev);
		if (fragstats.weapontotals[i].image)	Z_Free(fragstats.weapontotals[i].image);
	}

	memset(&fragstats, 0, sizeof(fragstats));
}

#define Z_Copy(tk) tz = Z_Malloc(strlen(tk)+1);strcpy(tz, tk)	//remember the braces

void Stats_Init(void)
{
	Cvar_Register(&r_tracker_frags, NULL);
	Cvar_Register(&r_tracker_time, NULL);
	Cvar_Register(&r_tracker_fadetime, NULL);
	Cvar_Register(&r_tracker_x, NULL);
	Cvar_Register(&r_tracker_y, NULL);
	Cvar_Register(&r_tracker_w, NULL);
	Cvar_Register(&r_tracker_lines, NULL);
}

static void Stats_LoadFragFile(char *name)
{
	char filename[MAX_QPATH];
	char *file;
	char *end;
	char *tk, *tz;
	char oend;

	Stats_Clear();

	strcpy(filename, name);
	COM_DefaultExtension(filename, ".dat", sizeof(filename));

	file = COM_LoadTempFile(filename, 0, NULL);
	if (!file || !*file)
	{
		Con_DPrintf("Couldn't load %s\n", filename);
		return;
	}
	else
		Con_DPrintf("Loaded %s\n", filename);

	oend = 1;
	for (;oend;)
	{
		for (end = file; *end && *end != '\n'; end++)
			;
		oend = *end;
		*end = '\0';
		Cmd_TokenizeString(file, true, false);
		file = end+1;
		if (!Cmd_Argc())
			continue;

		tk = Cmd_Argv(0);
		if (!stricmp(tk, "#fragfile"))
		{
			tk = Cmd_Argv(1);
				 if (!stricmp(tk, "version"))		{}
			else if (!stricmp(tk, "gamedir"))		{}
			else Con_Printf("Unrecognised #meta \"%s\"\n", tk);
		}
		else if (!stricmp(tk, "#meta"))
		{
			tk = Cmd_Argv(1);
				 if (!stricmp(tk, "title"))			{}
			else if (!stricmp(tk, "description"))	{}
			else if (!stricmp(tk, "author"))		{}
			else if (!stricmp(tk, "email"))			{}
			else if (!stricmp(tk, "webpage"))		{}
			else {Con_Printf("Unrecognised #meta \"%s\"\n", tk);continue;}
		}
		else if (!stricmp(tk, "#define"))
		{
			tk = Cmd_Argv(1);
			if (!stricmp(tk, "weapon_class") ||
				!stricmp(tk, "wc"))	
			{
				int wid;

				tk = Cmd_Argv(2);

				wid = Stats_FindWeapon(tk, true);
				if (wid == -1)
				{Con_Printf("Too many weapon definitions. The max is %i\n", MAX_WEAPONS);continue;}
				else if (wid < -1)
				{Con_Printf("Weapon \"%s\" is already defined\n", tk);continue;}
				else
				{
					fragstats.weapontotals[wid].fullname = Z_Copy(Cmd_Argv(3));
					fragstats.weapontotals[wid].abrev = Z_Copy(Cmd_Argv(4));
					fragstats.weapontotals[wid].image = Stats_GenTrackerImageString(Cmd_Argv(5));
				}
			}
			else if (!stricmp(tk, "obituary") ||
					 !stricmp(tk, "obit"))
			{
				int fftype;
				tk = Cmd_Argv(2);

					 if (!stricmp(tk, "PLAYER_DEATH"))			{fftype = ff_death;}
				else if (!stricmp(tk, "PLAYER_SUICIDE"))		{fftype = ff_suicide;}
				else if (!stricmp(tk, "X_FRAGS_UNKNOWN"))		{fftype = ff_bonusfrag;}
				else if (!stricmp(tk, "X_TEAMKILLS_UNKNOWN"))	{fftype = ff_tkbonus;}
				else if (!stricmp(tk, "X_TEAMKILLED_UNKNOWN"))	{fftype = ff_tkdeath;}
				else if (!stricmp(tk, "X_FRAGS_Y"))				{fftype = ff_frags;}
				else if (!stricmp(tk, "X_FRAGGED_BY_Y"))		{fftype = ff_fragedby;}
				else if (!stricmp(tk, "X_TEAMKILLS_Y"))			{fftype = ff_tkills;}
				else if (!stricmp(tk, "X_TEAMKILLED_BY_Y"))		{fftype = ff_tkilledby;}
				else {Con_Printf("Unrecognised obituary \"%s\"\n", tk);continue;}

				Stats_StatMessage(fftype, Stats_FindWeapon(Cmd_Argv(3), false), Cmd_Argv(4), Cmd_Argv(5));
			}
			else if (!stricmp(tk, "flag_alert") ||
					 !stricmp(tk, "flag_msg"))
			{
				int fftype;
				tk = Cmd_Argv(2);

					 if (!stricmp(tk, "X_TOUCHES_FLAG"))		{fftype = ff_flagtouch;}
				else if (!stricmp(tk, "X_GETS_FLAG"))			{fftype = ff_flagtouch;}
				else if (!stricmp(tk, "X_TAKES_FLAG"))			{fftype = ff_flagtouch;}
				else if (!stricmp(tk, "X_CAPTURES_FLAG"))		{fftype = ff_flagcaps;}
				else if (!stricmp(tk, "X_CAPS_FLAG"))			{fftype = ff_flagcaps;}
				else if (!stricmp(tk, "X_SCORES"))				{fftype = ff_flagcaps;}
				else if (!stricmp(tk, "X_DROPS_FLAG"))			{fftype = ff_flagdrops;}
				else if (!stricmp(tk, "X_FUMBLES_FLAG"))		{fftype = ff_flagdrops;}
				else if (!stricmp(tk, "X_LOSES_FLAG"))			{fftype = ff_flagdrops;}
				else {Con_Printf("Unrecognised flag alert \"%s\"\n", tk);continue;}

				Stats_StatMessage(fftype, 0, Cmd_Argv(3), NULL);
			}
			else if (!stricmp(tk, "rune_msg"))
			{
				int runetype;
				tk = Cmd_Argv(2);
					 if (!stricmp(tk, "X_RUNE_RES"))		{runetype = ff_rune_res;Con_Printf("resistance\n");}
				else if (!stricmp(tk, "X_RUNE_STR"))		{runetype = ff_rune_str;Con_Printf("strength\n");}
				else if (!stricmp(tk, "X_RUNE_HST"))		{runetype = ff_rune_hst;Con_Printf("haste\n");}
				else if (!stricmp(tk, "X_RUNE_REG"))		{runetype = ff_rune_reg;Con_Printf("regeneration\n");}
				else {Con_Printf("Unrecognised rune message \"%s\"\n", tk);continue;}

				Stats_StatMessage(runetype, 0, Cmd_Argv(3), NULL);
			}
			else
			{Con_Printf("Unrecognised directive \"%s\"\n", tk);continue;}
		}
		else
		{Con_Printf("Unrecognised directive \"%s\"\n", tk);continue;}
	}
}


static int qm_strcmp(const char *s1, const char *s2)//not like strcmp at all...
{
	while(*s1)
	{
		if ((*s1++&0x7f)!=(*s2++&0x7f))
			return 1;
	}
	return 0;
}
/*
static int qm_stricmp(char *s1, char *s2)//not like strcmp at all...
{
	int c1,c2;
	while(*s1)
	{
		c1 = *s1++&0x7f;
		c2 = *s2++&0x7f;

		if (c1 >= 'A' && c1 <= 'Z')
			c1 = c1 - 'A' + 'a';

		if (c2 >= 'A' && c2 <= 'Z')
			c2 = c2 - 'A' + 'a';

		if (c1!=c2)
			return 1;
	}
	return 0;
}
*/

static int Stats_ExtractName(const char **line)
{
	int i;
	int bm;
	int ml = 0;
	int l;
	bm = -1;
	for (i = 0; i < cl.allocated_client_slots; i++)
	{
		if (!qm_strcmp(cl.players[i].name, *line))
		{
			l = strlen(cl.players[i].name);
			if (l > ml)
			{
				bm = i;
				ml = l;
			}
		}
	}
	*line += ml;
	return bm;
}

qboolean Stats_ParsePickups(const char *line)
{
#ifdef HAVE_LEGACY
	//fixme: rework this to support custom strings, with custom pickup icons
	if (!Q_strncmp(line, "You got the ", 12))	//weapons, ammo, keys, powerups
		return true;
	if (!Q_strncmp(line, "You got armor", 13))	//caaake...
		return true;
	if (!Q_strncmp(line, "You get ", 8))	//backpacks
		return true;
	if (!Q_strncmp(line, "You receive ", 12)) //%i health\n
		return true;
#endif
	return false;
}

qboolean Stats_ParsePrintLine(const char *line)
{
	statmessage_t *ms;
	int p1;
	int p2;
	const char *m2;

	p1 = Stats_ExtractName(&line);
	if (p1<0)	//reject it.
	{
		// QTube: Might be a dem_single for runes, default to last player, msg: "You got..."
		extern int cls_lastto;
		p1 = cls_lastto;
	}
	
	for (ms = fragstats.message; ms; ms = ms->next)
	{
		if (!Q_strncmp(ms->msgpart1, line, ms->l1))
		{
			if (ms->type >= ff_frags)
			{	//two players
				m2 = line + ms->l1;
				p2 = Stats_ExtractName(&m2);
				if ((!ms->msgpart2 && *m2=='\n') || (ms->msgpart2 && !Q_strncmp(ms->msgpart2, m2, ms->l2)))
				{
					Stats_Evaluate(ms->type, ms->wid, p1, p2);
					return true;	//done.
				}
			}
			else
			{	//one player
				Stats_Evaluate(ms->type, ms->wid, p1, p1);
				return true;	//done.
			}
		}
	}
	return false;
}

void Stats_NewMap(void)
{
	Stats_LoadFragFile("fragfile");
}
#endif
