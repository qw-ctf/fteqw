#include "quakedef.h"

typedef enum {
	//one componant
	ff_death,
	ff_tkdeath,
	ff_suicide,
	ff_bonusfrag,
	ff_tkbonus,
	ff_flagtouch,
	ff_flagcaps,
	ff_flagdrops,
	ff_rune_res,
	ff_rune_str,
	ff_rune_hst,
	ff_rune_reg,

	//two componant
	ff_frags,		//must be the first of the two componant
	ff_fragedby,
	ff_tkills,
	ff_tkilledby,
} fragfilemsgtypes_t;

typedef struct statmessage_s {
	fragfilemsgtypes_t type;
	int wid;
	size_t l1, l2;
	char *msgpart1;
	char *msgpart2;
	struct statmessage_s *next;
} statmessage_t;


#define MAX_WEAPONS 64 //fixme: make dynamic.

typedef unsigned short stat;

typedef struct {
	stat totaldeaths;
	stat totalsuicides;
	stat totalteamkills;
	stat totalkills;
	stat totaltouches;
	stat totalcaps;
	stat totaldrops;

	//I was going to keep track of kills with a certain gun - too much memory
	//track only your own and total weapon kills rather than per client
	struct wt_s {
		//these include you.
		stat kills;
		stat teamkills;
		stat suicides;

		stat ownkills;
		stat owndeaths;
		stat ownteamkills;
		stat ownteamdeaths;
		stat ownsuicides;
		char *fullname;
		char *abrev;
		char *image;
		char *codename;
	} weapontotals[MAX_WEAPONS];

	struct ct_s {
		stat caps;		//times they captured the flag
		stat drops;		//times lost the flag
		stat grabs;		//times grabbed flag

		stat owndeaths;	//times you killed them
		stat ownkills;	//times they killed you
		stat deaths;	//times they died (including by you)
		stat kills;		//times they killed (including by you)
		stat teamkills;	//times they killed a team member.
		stat teamdeaths;	//times they died to a team member.
		stat suicides;	//times they were stupid.
	} clienttotals[MAX_CLIENTS];

	qboolean readcaps;
	qboolean readkills;
	statmessage_t *message;
} fragstats_t;
