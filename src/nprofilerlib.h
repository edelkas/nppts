#pragma once
#include "curl/curl.h"

//-----------------------------------------------------------------------------
// Constants
//-----------------------------------------------------------------------------

// General program constant
#define MAGIC          "NPRO"
#define MAJOR          1
#define MINOR          0
#define PATCH          0
#define ERRBUF_SIZE    80
#define LOGBUF_SIZE    80
#define URL            "https://dojo.nplusplus.ninja/prod/steam/get_scores?steam_id=%lu&steam_auth=&%s_id=%d"
#define RETRIES        50
#define USERNAME       "EddyMataGallos"
#define STEAM_ID       76561198031272062
#define INVALID_RES    "-1337"
#define FILENAME       "bin/nprofile"
#define CONFIG         "bin/config"
#define SCORES         "bin/scores"
#define SETOPT(x,e)    curl->code=x;if(curl->code!=CURLE_OK){printf("%s\n%s\n",e,curl->error);return 1;}

// General N++ constants
#define FILESIZE           70501008
#define MAX_SCORE          3000
#define PLAYER_MAX         1000     // How many to allocate at the start
#define NPP_USER_ID        0xA04
#define NPP_USERNAME       0xA08
#define NPP_PALETTE_ID     0xA18
#define NPP_KILLCOUNT      0x1244
#define NPP_GOLD           0x12BC
#define NPP_USERNAME_SIZE  17
#define ID_LENGTH          10

// Level, episode and story offset and counts
#define L_OFFSET       0x80D320
#define E_OFFSET       0x8F7920
#define S_OFFSET       0x926720
#define L_COUNT        2165
#define E_COUNT        385
#define S_COUNT        65
#define TAB_COUNT      10
#define BLOCK_SIZE     48

#define L_OFFSET_SI    0
#define L_OFFSET_S     600
#define L_OFFSET_SU    2400
#define L_OFFSET_SL    1200
#define L_OFFSET_SS    1800
#define L_OFFSET_SS2   3000
#define L_COUNT_SI     125
#define L_COUNT_S      600
#define L_COUNT_SU     600
#define L_COUNT_SL     600
#define L_COUNT_SS     120
#define L_COUNT_SS2    120

#define E_OFFSET_SI    0
#define E_OFFSET_S     120
#define E_OFFSET_SU    480
#define E_OFFSET_SL    240
#define E_COUNT_SI     25
#define E_COUNT_S      120
#define E_COUNT_SU     120
#define E_COUNT_SL     120

#define S_OFFSET_SI    0
#define S_OFFSET_S     24
#define S_OFFSET_SU    96
#define S_OFFSET_SL    48
#define S_COUNT_SI     5
#define S_COUNT_S      20
#define S_COUNT_SU     20
#define S_COUNT_SL     20

//-----------------------------------------------------------------------------
// Forward declarations and basic types
//-----------------------------------------------------------------------------

enum platforms { PC, PS4, XBOX, SWITCH, KARTRIDGE };
enum modes     { SOLO, COOP, RACE, HARDCORE };
enum types     { LEVEL, EPISODE, STORY };
enum tabs      { SI, S, SU, SL, SS, SS2 };
enum orders    { ID, ATTEMPTS, VICTORIES, GOLD, SCORE, RANK };

enum ConfigFlags {
  HackerFlags_DoNothing              = 1 << 0,
  HackerFlags_IgnoreLeaderboards     = 1 << 1,
  HackerFlags_IgnoreRankings         = 1 << 2,
  HackerFlags_IgnoreSpreads          = 1 << 3,
  HackerFlags_IgnoreLists            = 1 << 4,
  HackerFlags_HighlightLeaderboards  = 1 << 5,
  HackerFlags_HighlightRankings      = 1 << 6,
  HackerFlags_HighlightSpreads       = 1 << 7,
  HackerFlags_HighlightLists         = 1 << 8,
  HackerFlags_RemoveScores           = 1 << 9,
  CheaterFlags_DoNothing             = 1 << 10,
  CheaterFlags_IgnoreLeaderboards    = 1 << 11,
  CheaterFlags_IgnoreRankings        = 1 << 12,
  CheaterFlags_IgnoreSpreads         = 1 << 13,
  CheaterFlags_IgnoreLists           = 1 << 14,
  CheaterFlags_HighlightLeaderboards = 1 << 15,
  CheaterFlags_HighlightRankings     = 1 << 16,
  CheaterFlags_HighlightSpreads      = 1 << 17,
  CheaterFlags_HighlightLists        = 1 << 18,
  CheaterFlags_RemoveScores          = 1 << 19
};
inline ConfigFlags operator|(ConfigFlags a, ConfigFlags b) { return (ConfigFlags)((int) a | (int) b); }

enum DownloadFlags {
  DownloadFlags_Busy          = 1 << 0,
  DownloadFlags_Refresh       = 1 << 1,
  DownloadFlags_Complete      = 1 << 2,
  DownloadFlags_PopupInactive = 1 << 3,
  DownloadFlags_PopupFailed   = 1 << 4,
  DownloadFlags_Download      = 1 << 5,
  DownloadFlags_Paused        = 1 << 6
};
inline DownloadFlags operator|(DownloadFlags a, DownloadFlags b) { return (DownloadFlags)((int) a | (int) b); }

// Struct to describe a particular tab
struct tab {
  enum platforms platform;
  enum modes mode;
  enum types type;
  enum tabs tab;
  const char* prefix;   // SI, S, SU, SL, ?, !
  unsigned int offset;  // Ingame ID of first block
  unsigned int size;    // Count of blocks
  struct block* blocks; // Offset of first block in memory
  bool x;               // Whether this tab has X row
  bool online;          // Whether we download scores
};

// Struct to describe a particular player
struct player {
  const char* name;
  unsigned int id;
  struct score* scores;
  unsigned int count;   // Length of scores array
  bool cheater;
  bool hacker;
};

// Struct to describe a particular score of the leaderboards
struct score {
  unsigned int score;
  struct player* player;
  unsigned int replay_id;
  unsigned int rank;
  unsigned int tied_rank;
};

// Struct to describe a particular level, episode or story
struct block {
  const char* name;
  struct tab* tab;
  struct block* orig;   // A pointer to the original block
  struct block* copy;   // A pointer to the copy which we maintain updated whenever we sort
  bool updated;         // Whether the block has been updated with online info
  unsigned int retries; // Number of redownload retries

  uint32_t id;
  uint32_t attempts;
  uint32_t deaths;
  uint32_t victories;
  uint32_t victories_ep;
  uint32_t state;
  uint32_t gold;
  uint32_t score_deathless;

  unsigned int score;
  unsigned int rank;
  unsigned int tied_rank;
  unsigned int replay;
  struct score* scores;
};

// Struct to hold an HTTP transfer
struct curl {
  /* Internal cURL variables */
  CURL *curl;    // CURL handle
  CURLcode code; // Operation return code
  char* error;   // Buffer to store CURL error message
  char* res;     // Response of transfer
  bool safe;     // Perform safety checks

  /* Additional project variables */
  bool active;   // Whether Steam ID is active
  int count;     // How many blocks have been updated
};

// Struct to describe the player
struct profile {
  uint32_t    id;
  uint32_t    palette_id;
  uint64_t    steam_id;
  const char* username;
};

// Struct to describe config parameters
struct config {
  const char*      def_name;
  uint64_t         def_steam_id;
  struct player*   cheaters;
  struct player*   hackers;
  unsigned int     cheater_count;
  unsigned int     hacker_count;
  time_t           time;
  enum ConfigFlags flags;
};

// General environment of the program which contains all necessary ingredients
// to be passed around functions
struct env {
  struct config*  config;
  struct curl*    curl;
  struct profile* profile;
  struct tab*     tabs;
  struct block*   blocks;
  struct player*  players;
  struct score*   scores;

  unsigned int tcount; // Tab count
  unsigned int bcount; // Block count
  unsigned int pcount; // Player count
  unsigned int scount; // Score count
  unsigned int lcount; // Leaderboard count

  DownloadFlags flags;
};

//-----------------------------------------------------------------------------
// API functions
//-----------------------------------------------------------------------------

// Error and logging functions
void seterr(const char* msg);
void puterr(const char* msg);
void setlog(const char* msg);
void putlog(const char* msg);

// Basic I/O
void kill(int status);
void cls(int count);
int read(unsigned char** buffer, const char* filename);
int save(unsigned char* data, int size, const char* filename);
void arrdel(char* arr, int i, int* count, int sz, bool freeable = true);
void* arradd(char* arr, char* elm, int* count, int sz, int i = -1);

// Flag manipulation
inline void sflag(int* flags, int flag) { *flags |= flag; }       // Set flag
inline void cflag(int* flags, int flag) { *flags &= ~flag; }      // Clear flag
inline void tflag(int* flags, int flag) { *flags ^= flag; }       // Toggle flag
inline bool gflag(int* flags, int flag) { return *flags & flag; } // Get flag

// Auxiliar
void initialize();
int save_config(struct config* config);
struct config* parse_config(struct player* players, unsigned int* pcount);
int parse_scores(struct env* env);
int save_scores(struct env* env);
void playerdealloc(struct player** players, size_t sz);
void blockdealloc(struct block** blocks,  int sz);

// Parsing nprofile
void parse_profile(unsigned char* f, struct profile* profile);
const char* generate_id(struct tab* tab, int i);
void create_tabs(struct tab* tabs);
void fill_blocks(struct tab* tabs, size_t tab_count, struct block* blocks, struct score* scores);
void parse_tab(unsigned char* f, struct tab* tab);
void parse_tabs(unsigned char* f, struct tab* tabs);
struct tab* find_tab(struct tab* tabs, int sz, enum modes mode, enum types type, enum tabs tab);

// cURL methods
size_t curlwrite(char* data, size_t size, size_t nmemb, char** res);
int curlinit(struct curl* curl);
int curldownload(struct curl* curl, const char* url);
void curlinfo(struct curl* curl);
void curldestroy(struct curl* curl);

// Downloading scores
int update_scores(struct env* env);

// Printing info
void print_profile(struct profile* profile);
void compute_tab(struct tab* tab);
int blkcmp(const void* b1, const void* b2);
void blksort(struct block* blocks, size_t sz, enum orders order, bool reverse);
