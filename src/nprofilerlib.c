#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <limits.h>
#include <stdbool.h>

#include "curl/curl.h"
#include "cJSON/cJSON.h"
#include "nprofilerlib.h"

#ifdef _MSC_VER
#define strdup(p) _strdup(p)
#endif

/* Global variables (I know, ugly!) */
enum orders mainorder = ID;    // Order to sort main table (used in blkcmp and blksort)
bool mainorder_rev    = false; // Sort main table in reverse order

// Buffer to store the last error msg, function to print an error msg.
char* errbuffer;
void seterr(const char* msg) { memcpy(errbuffer, msg, ERRBUF_SIZE); }
void puterr(const char* msg) { printf("[ERROR] %s: %s.\n", msg, errbuffer); }

// Buffer to store the last log msg, function to print a log msg.
char* logbuffer;
void setlog(const char* msg) { memcpy(logbuffer, msg, LOGBUF_SIZE); }
void putlog(const char* msg) { printf("[INFO] %s: %s.\n", msg, logbuffer); }

// Initialize program
void initialize() {
  // Create error and log buffers
  errbuffer = (char*) calloc(ERRBUF_SIZE + 1, sizeof(char));
  logbuffer = (char*) calloc(LOGBUF_SIZE + 1, sizeof(char));
}

// Exit the program, 0 for success.
void kill(int status) {
  //getchar(); // For pausing
  printf("Happy highscoring!\n");
  exit(status);
}

// Clean the line
void cls(int count) {
  char line[count + 2];
  memset(line, ' ', count);
  line[count] = '\r';
  line[count + 1] = 0;
  printf("%s", line);
}

// Read raw file, store content in an unsigned char buffer, return pointer.
int read(unsigned char **buffer, const char* filename) {
  FILE *file;
  long size;
  size_t result;

  // Try to read the file
  file = fopen(filename, "rb");
  if (file == NULL) {
    seterr("Error opening file");
    return 0;
  }

  // Obtain file size
  fseek(file, 0, SEEK_END); // Change cursor to EOF
  size = ftell(file);       // Retrieve position of cursor
  rewind(file);             // Change cursor to byte 0

  // Allocate memory
  *buffer = (unsigned char*) malloc(sizeof(unsigned char) * size);
  memset(*buffer, 0, sizeof(unsigned char) * size);       // Initialize to 0
  if (*buffer == NULL) {
    seterr("Error allocating memory to read file");
    return 0;
  }

  // Copy file content into buffer
  result = fread(*buffer, 1, size, file);
  if (result != size) {
    seterr("Error reading file");
    return 0;
  }

  // Terminate
  fclose(file);
  return result;
}

// Save raw binary data into a file
int save(unsigned char* data, int size, const char* filename) {
  FILE *file;
  size_t result;

  /* Try to open file for writting */
  file = fopen(filename, "wb");
  if (file == NULL) {
    seterr("Error saving file");
    return 0;
  }

  /* Copy data into file */
  result = fwrite(data, sizeof(unsigned char), size, file);
  if (result != size) {
    seterr("Error writing to file");
    return 0;
  }

  /* Terminate */
  fclose(file);
  return result;
}

// Array manipulation functions
void arrdel(char* arr, int i, int* count, int sz, bool freeable) {
  (*count)--;
  if (freeable) (*(void**)(arr + i * sz));
  memmove(arr + i * sz, arr + (i + 1) * sz, (*count - i) * sz);
}

void* arradd(char* arr, char* elm, int* count, int sz, int i) {
  (*count)++;
  arr = (char*) realloc(arr, *count * sz);
  if (i >= 0 && i < *count - 1) {
    memmove(arr + (i + 1) * sz, arr + i * sz, (*count - 1 - i) * sz);
    memmove(arr + i * sz, elm, sz);
  } else {
    memmove(arr + (*count - 1) * sz, elm, sz);
  }
  return arr;
}

// Auxiliar functions to concatenate memory
void memcat(unsigned char* dest, unsigned char* src, unsigned int sz, unsigned int* offset) {
  if (src != NULL) memcpy((void*) (dest + *offset), src, sz);
  else memset((void*) (dest + *offset), 0, sz);
  *offset += sz;
}
void memcati(unsigned char* dest, int src, unsigned int* offset) {
  memcat(dest, (unsigned char*) &src, sizeof(int), offset);
}
void memcatc(unsigned char* dest, unsigned char src, unsigned int* offset) {
  memcat(dest, &src, sizeof(unsigned char), offset);
}
void memcats(unsigned char* dest, const char* src, unsigned int* offset) {
  memcat(dest, (unsigned char*) src, strlen(src) + 1, offset);
}
void memcatul(unsigned char* dest, uint64_t src, unsigned int* offset) {
  memcat(dest, (unsigned char*) &src, sizeof(uint64_t), offset);
}

void playerdealloc(struct player** players, size_t sz) {
  for (int i = 0; i < sz; i++) {
    if ((*players)[i].name != NULL) {
      free((void*) (*players)[i].name);
      (*players)[i].name = NULL;
    }
  }
  free(*players);
  *players = NULL;
}

bool is_hacker(struct config* config, struct player* player) {
  if (!player || !config) return false;
  for (int i = 0; i < config->hacker_count; i++) {
    if (config->hackers[i].id == player->id || (player->name != NULL && config->cheaters[i].name != NULL && strcmp(config->hackers[i].name, player->name) == 0)) return true;
  }
  return false;
}

bool is_cheater(struct config* config, struct player* player) {
  if (!player || !config) return false;
  for (int i = 0; i < config->cheater_count; i++) {
    if (config->cheaters[i].id == player->id || (player->name != NULL && config->cheaters[i].name != NULL && strcmp(config->cheaters[i].name, player->name) == 0)) return true;
  }
  return false;
}

/* Adds player in place or, if left by default, initializes values */
// TODO: Allocate more space if it's needed here (we'll have to change the arguments)
void add_player(struct config* config, struct player* player, unsigned int id = -1, const char* name = NULL) {
  player->id      = id;
  player->name    = name != NULL ? strdup(name) : NULL;
  player->count   = 0;
  player->scores  = NULL;
  player->cheater = is_cheater(config, player);
  player->hacker  = is_hacker(config, player);
}

// Save config file
int save_config(struct config* config) {
  // TODO: Actually calculate required data buffer size
  unsigned char* data = (unsigned char*) calloc(1000, sizeof(unsigned char));
  unsigned int offset = 0;

  /* Header */
  memcat(data, (unsigned char*) MAGIC, 4, &offset); // Magic number
  memcatc(data,     0, &offset); // Filetype (0 for config, 1 for scores)
  memcatc(data, MAJOR, &offset); // Major version of the program
  memcatc(data, MINOR, &offset); // Minor version
  memcatc(data, PATCH, &offset); // Patch version

  /* Name ignore list */
  memcati(data,     0, &offset); // Size of block
  memcati(data,     0, &offset); // Number of entries

  /* ID ignore list */
  memcati(data,     0, &offset); // Size of block
  memcati(data,     0, &offset); // Number of entries

  /* Save config file */
  unsigned int result = save(data, offset, CONFIG);
  free(data);
  return result;
}

// Read config file
struct config* parse_config(struct player* players, unsigned int* pcount) {
  /* Initialize struct */
  struct config* config = (struct config*) calloc(1, sizeof(struct config));

  // TODO: Only do the following if no config file is found
  config->def_name     = USERNAME;
  config->def_steam_id = STEAM_ID;

  /* Default hackers and cheaters */
  unsigned int hacker_count  = 18;
  unsigned int cheater_count = 2;
  struct player* hackers  = (struct player*) calloc(hacker_count,  sizeof(struct player));
  struct player* cheaters = (struct player*) calloc(cheater_count, sizeof(struct player));
  const char* hacker_names[hacker_count] = {
    "Kronogenics",
    "BlueIsTrue",
    "fiordhraoi",
    "cheeseburgur101",
    "Jey",
    "jungletek",
    "Hedgy",
    "ᕈᘎᑕᒎᗩn ᙡiᗴᒪḰi",
    "Venom",
    "EpicGamer10075",
    "Altii",
    "Floof The Goof",
    "Prismo",
    "Pu𝐜ͥⷮe",
    "Chara",
    "test8378",
    "vex",
    "DBYT3"
  };
  const int hacker_ids[hacker_count] = {
    -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1,
    -1, -1, -1, // Don't know the IDs of the first ones since they were already removed
    146275, // Puce
    253161, // Chara
    253072, // test8378
    221472, // vex
    276273  // DBYT3
  };
  const char* cheater_names[cheater_count] = {
    "Mishu",
    "dimitry008"
  };
  const int cheater_ids[cheater_count] = {
    115572, // Mishu
    201322  // dimitry008
  };
  // TODO: Use add_player here
  for (int i = 0; i < cheater_count; i++) {
    players[*pcount].name    = strdup(cheater_names[i]);
    players[*pcount].id      = cheater_ids[i];
    players[*pcount].hacker  = false;
    players[*pcount].cheater = true;
    (*pcount)++;
    cheaters[i].name    = strdup(cheater_names[i]);
    cheaters[i].id      = cheater_ids[i];
    cheaters[i].cheater = true;
  }
  for (int i = 0; i < hacker_count; i++) {
    players[*pcount].name   = strdup(hacker_names[i]);
    players[*pcount].id     = hacker_ids[i];
    players[*pcount].hacker  = true;
    players[*pcount].cheater = false;
    (*pcount)++;
    hackers[i].name       = strdup(hacker_names[i]);
    hackers[i].id         = hacker_ids[i];
    hackers[i].hacker     = true;
  }
  config->hackers       = hackers;
  config->cheaters      = cheaters;
  config->hacker_count  = hacker_count;
  config->cheater_count = cheater_count;

  /* Set config flags regarding hackers and cheaters */
  config->flags = HackerFlags_RemoveScores
                | CheaterFlags_IgnoreRankings
                | CheaterFlags_HighlightLeaderboards
                | CheaterFlags_HighlightSpreads
                | CheaterFlags_HighlightLists;

  /* Initialize time */
  config->time = 0;

  /* Attempt to read file */
  unsigned char* f;
  if (read(&f, CONFIG) == 0) {
    if (save_config(config) == 0) {
      puterr("Config file not found, failed to create a new one");
    } else {
      putlog("Config file not found, created a new one");
    }
  } else {
    putlog("Config file read successfully");
  }
  return config;
}

// TODO: Change "putlog" by actual modal windows
int parse_scores(struct env* env) {
  /* Attempt to read the file */
  unsigned char* f;
  int fsize = read(&f, SCORES);
  if (fsize == 0) { // Generic error, e.g., file doesn't exist
    putlog("Failed to load scores");
    return 1;
  }

  /* Make rutinary checks */
  if (fsize < 24) { // Not enough bytes to contain main header
    putlog("Scores file is corrupt");
    return 1;
  }
  if (memcmp(f, &MAGIC, 4) != 0) { // First 4 bytes are not magic number
    putlog("Not an N++CC file");
    return 1;
  }
  if (f[4] != 1) { // File is not of the correct type
    putlog("Not a scores file");
    return 1;
  }

  /* Parse header */
  unsigned int psize = *(unsigned int*)(f +  8); // Player count (4B)
  unsigned int tsize = *(unsigned int*)(f + 12); // Tab count (4B)
  env->config->time  = *(time_t*)      (f + 16); // UNIX timestamp (8B)

  /* Initialize variables */
  unsigned int offset = 24;
  fsize -= 24;
  unsigned int pmax = PLAYER_MAX >= psize ? PLAYER_MAX : psize;
  playerdealloc(&env->players, env->pcount);
  env->players = (struct player*) calloc(pmax, sizeof(struct player));
  env->pcount = 0;
  for (int i = 0; i < pmax; i++) {
    add_player(env->config, env->players + i);
  }
  env->scount = 0;
  env->lcount = 0;

  /* Parse players */
  for (int i = 0; i < psize; i++) {
    if (fsize < sizeof(int) + 1) { // Not enough bytes to contain another player (id + null char)
      putlog("Scores file is corrupt");
      return 1;
    }
    unsigned int id = *(unsigned int*)(f + offset);
    const char* name = (const char*)(f + offset + sizeof(int));
    add_player(env->config, env->players + i, id, name);
    offset += sizeof(int) + strlen(name) + 1;
    fsize  -= sizeof(int) + strlen(name) + 1;
    env->pcount++;
  }

  /* Parse scores */
  for (int i = 0; i < tsize; i++) {
    /* Tab header */
    if (fsize < 8) { // Not enough bytes to contain tab header
      putlog("Scores file is corrupt");
      return 1;
    }
    char t_platform     =         *(char*)(f + offset);
    char t_mode         =         *(char*)(f + offset + 1);
    char t_type         =         *(char*)(f + offset + 2);
    char t_tab          =         *(char*)(f + offset + 3);
    unsigned int t_size = *(unsigned int*)(f + offset + 4);
    int stride = t_size * (4 * sizeof(int) + 20 * 3 * sizeof(int)); // Theoretical size of tab info in scores file
    printf("stride %d\n", 8 + stride);
    offset += 8;
    fsize  -= 8;

    /* Find tab */
    struct tab* tab = NULL;
    for (int j = 0; j < env->tcount; j++) {
      struct tab* t = &env->tabs[j];
      if (t->platform == t_platform && t->mode == t_mode && t->type == t_type && t->tab == t_tab && t->size == t_size && t->online) tab = t;
    }
    if (tab == NULL) { // Tab not found, not parsing
      offset += stride;
      fsize  -= stride;
      if (fsize < 0) {  // Not enough bytes to contain tab scores
        putlog("Scores file is corrupt");
        return 1;
      } else {
        continue;
      }
    }
    fsize -= stride;
    if (fsize < 0) { // Not enough bytes to contain tab scores
      putlog("Scores file is corrupt");
      return 1;
    }

    /* Tab scores */
    struct block* block    = NULL;
    struct block* bcopy    = NULL;
    struct score* scores   = NULL;
    struct score* cscores  = NULL;
    struct player* player  = NULL;
    unsigned int index     = -1;
    unsigned int replay_id = -1;
    unsigned int score     = -1;
    unsigned int curscore  = -1;
    unsigned int rank      = -1;
    unsigned int tied_rank = -1;
    bool hacker            = false;
    bool cheater           = false;
    for (int j = 0; j < tab->size; j++) {
      /* Main block info */
      block = &tab->blocks[j];
      bcopy = block->copy;
      block->rank      = *(unsigned int*)(f + offset + 0 * sizeof(int));
      block->tied_rank = *(unsigned int*)(f + offset + 1 * sizeof(int));
      block->replay    = *(unsigned int*)(f + offset + 2 * sizeof(int));
      block->score     = *(unsigned int*)(f + offset + 3 * sizeof(int));
      bcopy->rank      = block->rank;
      bcopy->tied_rank = block->tied_rank;
      bcopy->replay    = block->replay;
      bcopy->score     = block->score;
      offset += 4 * sizeof(int);

      /* 20 leaderboard scores */
      scores    = block->scores;
      cscores   = bcopy->scores;
      rank      = -1;
      tied_rank = -1;
      env->lcount++;
      for (int k = 0; k < 20; k++) {
        /* Discard empty scores */
        index     = *(unsigned int*)(f + offset + 0 * sizeof(int));
        replay_id = *(unsigned int*)(f + offset + 1 * sizeof(int));
        score     = *(unsigned int*)(f + offset + 2 * sizeof(int));
        if (index == -1 && replay_id == -1 && score == -1) {
          offset += 3 * sizeof(int);
          continue;
        }

        /* Ignore hackers and cheaters if necessary */
        if (index != -1) {
          player  = env->players + index;
          hacker  = player != NULL ? player->hacker  : false;
          cheater = player != NULL ? player->cheater : false;
          int* flags = (int*) &env->config->flags;
          if (hacker && gflag(flags, HackerFlags_RemoveScores) || cheater && gflag(flags, CheaterFlags_RemoveScores)) {
            offset += 3 * sizeof(int);
            continue;
          }
        }

        /* Parse values */
        rank++;
        if (score != curscore) {
          tied_rank++;
          curscore = score;
        }
        scores[rank].score      = score;
        scores[rank].player     = player;
        scores[rank].replay_id  = replay_id;
        scores[rank].rank       = rank;
        scores[rank].tied_rank  = tied_rank;
        cscores[rank].score     = score;
        cscores[rank].player    = player;
        cscores[rank].replay_id = replay_id;
        cscores[rank].rank      = rank;
        cscores[rank].tied_rank = tied_rank;
        env->scount++;
        offset += 3 * sizeof(int);
      }

      /* Fill remaining empty spots (due to hackers and cheaters) with initialized scores */
      while (++rank < 20) {
        scores[rank].score      = -1;
        scores[rank].player     = NULL;
        scores[rank].replay_id  = -1;
        scores[rank].rank       = -1;
        scores[rank].tied_rank  = -1;
        cscores[rank].score     = -1;
        cscores[rank].player    = NULL;
        cscores[rank].replay_id = -1;
        cscores[rank].rank      = -1;
        cscores[rank].tied_rank = -1;
      }
    }
  }

  /* Cleanup */
  free(f);
  putlog("Loaded scores file");
  return 0;
}

int save_scores(struct env* env) {
  size_t sz = 3 * sizeof(int) + 4 * sizeof(char) + sizeof(uint64_t); // Main header + Player count + Tab count + UNIX time
  for (int i = 0; i < env->pcount; i++) sz += sizeof(int) + strlen(env->players[i].name) + 1; // ID + Name + Null char
  for (int i = 0; i < env->tcount; i++) {
    sz += 4 * sizeof(char) + sizeof(int);           // Tab header
    sz += 4 * sizeof(int) * env->tabs[i].size;      // Block info (rank, tied rank, replay id, score)
    sz += 3 * sizeof(int) * 20 * env->tabs[i].size; // Scores (player, replay id, rank)
  }
  unsigned char* data = (unsigned char*) calloc(sz, sizeof(unsigned char));
  unsigned int offset = 0;

  /* Header */
  memcat(data, (unsigned char*) MAGIC, 4, &offset); // Magic number
  memcatc(data,     1, &offset);                    // Filetype (0 for config, 1 for scores)
  memcatc(data, MAJOR, &offset);                    // Major version of the program
  memcatc(data, MINOR, &offset);                    // Minor version
  memcatc(data, PATCH, &offset);                    // Patch version
  memcati(data, (int) env->pcount, &offset);        // Player count
  memcati(data, (int) env->tcount, &offset);        // Tab count
  memcatul(data, (uint64_t) time(NULL), &offset);   // UNIX time

  /* Players */
  for (int i = 0; i < env->pcount; i++) {
    memcati(data, env->players[i].id,   &offset);
    memcats(data, env->players[i].name, &offset);
  }

  /* Tabs w/ scores */
  struct tab* tab       = NULL;
  struct block* block   = NULL;
  struct score* scores  = NULL;
  struct player* player = NULL;
  for (int i = 0; i < env->tcount; i++) {
    tab = &env->tabs[i];
    memcatc(data, (char) tab->platform, &offset);
    memcatc(data, (char) tab->mode,     &offset);
    memcatc(data, (char) tab->type,     &offset);
    memcatc(data, (char) tab->tab,      &offset);
    memcati(data, tab->size, &offset);
    for (int j = 0; j < tab->size; j++) {
      block = &tab->blocks[j];
      memcati(data, block->rank,      &offset);
      memcati(data, block->tied_rank, &offset);
      memcati(data, block->replay,    &offset);
      memcati(data, block->score,     &offset);
      scores = block->scores;
      for (int k = 0; k < 20; k++) {
        player = scores[k].player;
        memcati(data, player != NULL ? player - env->players : -1, &offset);
        memcati(data, scores[k].replay_id,                         &offset);
        memcati(data, scores[k].score,                             &offset);
      }
    }
  }

  /* Save scores file */
  unsigned int result = save(data, offset, SCORES);
  free(data);
  putlog("Saved scores file");
  return result;
}

void parse_profile(unsigned char* f, struct profile* profile) {
  char* username = (char*) calloc(NPP_USERNAME_SIZE + 1, sizeof(char));
  strncpy(username, (const char*) (f + NPP_USERNAME), NPP_USERNAME_SIZE);

  profile->id       = *(uint32_t*) (f + NPP_USER_ID);
  profile->username = username;
}

void blockdealloc(struct block** blocks,  int sz) {
  for (int i = 0; i < sz; i++) {
    if ((*blocks)[i].name != NULL) {
      free((void*) (*blocks)[i].name);
      (*blocks)[i].name = NULL;
    }
  }
  free(*blocks);
  *blocks = NULL;
}

/* Generate the name (e.g. SU-A-03-04) based on the ID */
// TODO: This doesn't support stories yet
const char* generate_id(struct tab* tab, int i) {
  char* name = (char*) calloc(ID_LENGTH + 1, sizeof(char));
  const char* key = "ABCDEX";
  char num[3] = {0, 0, 0}; // Temporary storage for the numbers
  bool epcond = tab->type != LEVEL || tab->tab == SS || tab->tab == SS2; // Is episode or secret level

  /* Tab prefix */
  strcpy(name, tab->prefix);
  strcat(name, "-");

  /* Compute row */
  bool xcond = tab->x ? i < 5 * tab->size / 6 : true;             // Determines whether we are in X row range
  int keyi = epcond ? i % 5 : (i / 5) % 5;                        // Determines key index
  strncat(name, xcond ? (const char*) &key[keyi] : (const char*) &key[5], 1);
  strcat(name, "-");

  /* Compute ep number */
  int a = epcond ? i / 5 : i / 25;                                // Value when not in X row
  int b = epcond ? i - 5 * tab->size / 6 : i / 5 - tab->size / 6; // Value when in X row
  snprintf(num, 3, "%02d", xcond ? a : b);
  strcat(name, num);

  /* Compute lvl number */
  if (!epcond) {
    strcat(name, "-");
    snprintf(num, 3, "%02d", i % 5);
    strcat(name, num);
  }

  return (const char*) name;
}

void create_tabs(struct tab* tabs) {
  tabs[0] = (struct tab) { PC, SOLO, LEVEL, SI,  "SI",  L_OFFSET_SI,  L_COUNT_SI,  NULL, false, true };
  tabs[1] = (struct tab) { PC, SOLO, LEVEL, S,   "S",   L_OFFSET_S,   L_COUNT_S,   NULL, true,  true };
  tabs[2] = (struct tab) { PC, SOLO, LEVEL, SU,  "SU",  L_OFFSET_SU,  L_COUNT_SU,  NULL, true,  true };
  tabs[3] = (struct tab) { PC, SOLO, LEVEL, SL,  "SL",  L_OFFSET_SL,  L_COUNT_SL,  NULL, true,  true };
  tabs[4] = (struct tab) { PC, SOLO, LEVEL, SS,  "?",   L_OFFSET_SS,  L_COUNT_SS,  NULL, true,  true };
  tabs[5] = (struct tab) { PC, SOLO, LEVEL, SS2, "!",   L_OFFSET_SS2, L_COUNT_SS2, NULL, true,  true };

  tabs[6] = (struct tab) { PC, SOLO, EPISODE, SI, "SI", E_OFFSET_SI, E_COUNT_SI, NULL, false, true };
  tabs[7] = (struct tab) { PC, SOLO, EPISODE, S,  "S",  E_OFFSET_S,  E_COUNT_S,  NULL, true,  true };
  tabs[8] = (struct tab) { PC, SOLO, EPISODE, SU, "SU", E_OFFSET_SU, E_COUNT_SU, NULL, true,  true };
  tabs[9] = (struct tab) { PC, SOLO, EPISODE, SL, "SL", E_OFFSET_SL, E_COUNT_SL, NULL, true,  true };
}

void fill_blocks(struct tab* tabs, size_t tab_count, struct block* blocks, struct score* scores) {
  unsigned int index = 0;
  unsigned int index_online = 0;
  for (int i = 0; i < tab_count; i++) {
    tabs[i].blocks = blocks + index;
    for (int j = 0; j < tabs[i].size; j++) {
      blocks[index].name      = generate_id(&tabs[i], j);
      blocks[index].tab       = &tabs[i];
      blocks[index].orig      = &blocks[index];
      blocks[index].copy      = &blocks[index];
      blocks[index].updated   = false;
      blocks[index].retries   = 0;
      blocks[index].score     = -1;
      blocks[index].rank      = -1;
      blocks[index].tied_rank = -1;
      blocks[index].replay    = -1;
      if (tabs[i].online) {
        struct score* entries = scores + 20 * index_online;
        blocks[index].scores = entries;
        for (int k = 0; k < 20; k++) {
          entries[k] = (struct score) {
            (unsigned int) -1,   // score
            (unsigned int) NULL, // player
            (unsigned int) -1,   // replay_id
            (unsigned int) -1,   // rank
            (unsigned int) -1    //tied_rank
          };
        }
        index_online++;
      } else {
        blocks[index].scores = NULL;
      }
      index++;
    }
  }
}

void parse_tab(unsigned char* f, struct tab* tab) {
  unsigned int offset = (tab->type == LEVEL ? L_OFFSET : E_OFFSET) + BLOCK_SIZE * tab->offset;
  for (int i = 0; i < tab->size; i++) {
    unsigned char* block = f + offset;
//    unsigned int score = *(unsigned int *) (block + 36);
//    unsigned int rank = *(unsigned int*) (block + 40);
//    bool valid = score <= MAX_SCORE * 1000;

//    tab->blocks[i].valid           = valid;
    tab->blocks[i].id              = *(unsigned int*) block;
    tab->blocks[i].attempts        = *(unsigned int*) (block +  4);
    tab->blocks[i].deaths          = *(unsigned int*) (block +  8);
    tab->blocks[i].victories       = *(unsigned int*) (block + 12);
    tab->blocks[i].victories_ep    = *(unsigned int*) (block + 16);
    tab->blocks[i].state           = *(unsigned int*) (block + 20);
    tab->blocks[i].gold            = *(unsigned int*) (block + 24);
    tab->blocks[i].score_deathless = *(unsigned int*) (block + 32);
//    tab->blocks[i].score           = valid ? score : -1;
//    tab->blocks[i].rank            = rank <= 19 ? rank : -1;
    tab->blocks[i].replay          = *(unsigned int*) (block + 44);
    offset += BLOCK_SIZE;
  }
}

void parse_tabs(unsigned char* f, struct tab* tabs) {
  for (int i = 0; i < TAB_COUNT; i++) {
    parse_tab(f, tabs + i);
  }
}

struct tab* find_tab(struct tab* tabs, int sz, enum modes mode, enum types type, enum tabs tab) {
  for (int i = 0; i < sz; i++) {
    if (tabs[i].mode == mode && tabs[i].type == type && tabs[i].tab == tab) {
      return &tabs[i];
    }
  }
  return NULL;
}

struct player* find_player_by_id(struct player* players, unsigned int pcount, unsigned int id) {
  for (int i = 0; i < pcount; i++) {
    if (players[i].id == id) return &players[i];
  }
  return NULL;
}

struct player* find_player_by_name(struct player* players, unsigned int pcount, const char* name) {
  if (name == NULL) return NULL;
  for (int i = 0; i < pcount; i++) {
    if (players[i].name != NULL && strcmp(players[i].name, name) == 0) return &players[i];
  }
  return NULL;
}

/**
 * cURL internal callback method to store result of transfer.
 *
 * @param New chunk of data
 * @param Size of chunk of data in bytes
 * @param Number of chunks of data
 * @param Cumulative destination where data is being stored
 */
size_t curlwrite(char* data, size_t size, size_t nmemb, char** res) {
  if (*res == NULL) return 0;
  size_t newsz = strlen(*res) + size * nmemb;
  *res = (char*) realloc(*res, newsz + 1);
  strncat(*res, data, size * nmemb);
  (*res)[newsz] = 0;
  return size * nmemb;
}

int curlinit(struct curl* curl) {
  /* Initialize cURL global functionality */
  curl_global_init(CURL_GLOBAL_DEFAULT);

  /* Try to initialize cURL easy interface and other struct members */
  curl->curl   = curl_easy_init();
  curl->code   = CURLE_OK;
  curl->error  = (char*) calloc(CURL_ERROR_SIZE, sizeof(char));
  curl->res    = (char*) calloc(1, sizeof(char));
  curl->safe   = true;
  curl->active = true;
  curl->count  = 0;
  if (!curl->curl) {
    puterr("cURL didn't initialize properly.");
    return 1;
  }

  /* Try to set cURL error buffer */
  SETOPT(curl_easy_setopt(curl->curl, CURLOPT_ERRORBUFFER, curl->error), "Error: cURL failed to set error buffer.");

  /* Try to set cURL redirect option */
  SETOPT(curl_easy_setopt(curl->curl, CURLOPT_FOLLOWLOCATION, 1L), "Error: cURL failed to set redirect option.");

  /* Try to set cURL writer function */
  SETOPT(curl_easy_setopt(curl->curl, CURLOPT_WRITEFUNCTION, curlwrite), "Error: cURL failed to set writer function.");

  /* Try to set cURL write data */
  SETOPT(curl_easy_setopt(curl->curl, CURLOPT_WRITEDATA, &curl->res), "Error: cURL failed to set write data.");

  if (!curl->safe) {
    /* Try to skip certificate verification */
    SETOPT(curl_easy_setopt(curl->curl, CURLOPT_SSL_VERIFYPEER, 0L), "Error: cURL failed to skip certificate verification.");

    /* Try to skip hostname verification */
    SETOPT(curl_easy_setopt(curl->curl, CURLOPT_SSL_VERIFYHOST, 0L), "Error: cURL failed to skip hostname verification.");
  }

  return 0;
}

int curldownload(struct curl* curl, const char* url) {
  /* Reinitialize response */
  free(curl->res);
  curl->res = (char*) calloc(1, sizeof(char));

  /* Perform GET request */
  curl_easy_setopt(curl->curl, CURLOPT_URL, url);
  curl->code = curl_easy_perform(curl->curl);
  if (curl->code != CURLE_OK) {
    printf("[ERROR] cURL GET request not successful: %s.\n", curl_easy_strerror(curl->code));
    return 1;
  }
  return 0;
}

void curlinfo(struct curl* curl) {
  curl_version_info_data* curlinfo = curl_version_info(CURLVERSION_NOW);
  printf("cURL version: %s.\ncURL SSL version: %s.\n", curlinfo->version, curlinfo->ssl_version);
}

void curldestroy(struct curl* curl) {
  /* Perform cURL's local and global cleanup */
  curl_easy_cleanup(curl->curl);
  curl_global_cleanup();

  /* Free allocated memory */
  if (curl->error != NULL) free(curl->error);
  if (curl->res != NULL) free(curl->res);
}

int parse_json(struct env* env, struct block* block) {
  /* Parse json file */
  cJSON* json = cJSON_Parse(env->curl->res);
  if (json == NULL) { // Incorrect JSON format
    cJSON_Delete(json);
    return 1;
  }

  /* Read user info */
  const cJSON* userInfo = cJSON_GetObjectItemCaseSensitive(json, "userInfo");
  unsigned int user_score  = -1;
  unsigned int user_rank   = -1;
  unsigned int user_replay = -1;
  const char*  user_name   = NULL;
  if (userInfo != NULL && cJSON_IsObject(userInfo)) {
    const cJSON* score  = cJSON_GetObjectItemCaseSensitive(userInfo, "my_score");
    const cJSON* rank   = cJSON_GetObjectItemCaseSensitive(userInfo, "my_rank");
    const cJSON* replay = cJSON_GetObjectItemCaseSensitive(userInfo, "my_replay_id");
    const cJSON* name   = cJSON_GetObjectItemCaseSensitive(userInfo, "my_display_name");
    user_score  = score  != NULL && cJSON_IsNumber(score)  ? score->valueint   : -1;
    user_rank   = rank   != NULL && cJSON_IsNumber(rank)   ? rank->valueint    : -1;
    user_replay = replay != NULL && cJSON_IsNumber(replay) ? replay->valueint  : -1;
    user_name   = name   != NULL && cJSON_IsString(name)   ? name->valuestring : NULL;
    block->score     = user_score;
    block->replay    = user_replay;
    block->rank      = user_rank;
    block->tied_rank = -1;
    block->copy->score     = user_score;
    block->copy->replay    = user_replay;
    block->copy->rank      = user_rank;
    block->copy->tied_rank = -1;
  }

  /* Read scores */
  const cJSON* scores    = cJSON_GetObjectItemCaseSensitive(json, "scores");
  const cJSON* token     = NULL;
  unsigned int rank      = -1;
  unsigned int tied_rank = -1;
  unsigned int curscore  = 0;
  if (scores != NULL && cJSON_IsArray(scores)) {
    cJSON_ArrayForEach(token, scores) {
      /* Limit of scores reached, this should never really be triggered but
         just in case the server returns over 20 scores */
      if (rank == 19) break;

      /* Retrieve JSON fields */
      const cJSON* json_id     = cJSON_GetObjectItemCaseSensitive(token, "user_id");
      const cJSON* json_score  = cJSON_GetObjectItemCaseSensitive(token, "score");
      const cJSON* json_replay = cJSON_GetObjectItemCaseSensitive(token, "replay_id");
      const cJSON* json_name   = cJSON_GetObjectItemCaseSensitive(token, "user_name");

      /* Check validity of JSON fields and store values */
      struct player* p    = NULL;
      unsigned int id     = json_id     != NULL && cJSON_IsNumber(json_id)     ? json_id->valueint      : -1;
      unsigned int score  = json_score  != NULL && cJSON_IsNumber(json_score)  ? json_score->valueint   : -1;
      unsigned int replay = json_replay != NULL && cJSON_IsNumber(json_replay) ? json_replay->valueint  : -1;
      const char*  name   = json_name   != NULL && cJSON_IsString(json_name)   ? json_name->valuestring : NULL;

      /* Try to find player or create it otherwise */
      if (!p && id != -1) p = find_player_by_id(env->players, env->pcount, id);
      if (!p && name != NULL) p = find_player_by_name(env->players, env->pcount, name);
      if (!p) {
        env->players[env->pcount].id   = id;
        env->players[env->pcount].name = strdup(name);
        p = &env->players[env->pcount];
        env->pcount++; // TODO: Put this in mutex
      }

      /* Fill in remaining general block info */
      // TODO: Maybe do this by comparing against the user player pointer
      if (user_name != NULL && p->name != NULL && strcmp(p->name, user_name) == 0) {
        block->rank      = rank + 1;
        block->tied_rank = curscore == score ? tied_rank : tied_rank + 1;
        block->copy->rank      = rank + 1;
        block->copy->tied_rank = curscore == score ? tied_rank : tied_rank + 1;
      }

      /* Ignore hackers and cheaters if necessary */
      bool hacker  = p != NULL ? p->hacker  : false;
      bool cheater = p != NULL ? p->cheater : false;
      int* flags = (int*) &env->config->flags;
      if (hacker && gflag(flags, HackerFlags_RemoveScores) || cheater && gflag(flags, CheaterFlags_RemoveScores)) continue;

      /* Fill in block scores info */
      // TODO: Fill in scores array for player, unless it slows down the process too much (benchmark!)
      rank++;
      if (score != curscore) {
        tied_rank++;
        curscore = score;
      }
      if (block->scores[rank].score == -1) env->scount++; // Increase score count if score is new
      block->scores[rank].score     = score;
      block->scores[rank].replay_id = replay;
      block->scores[rank].rank      = rank;
      block->scores[rank].tied_rank = tied_rank;
      block->scores[rank].player    = p;
      block->copy->scores[rank].score     = score;
      block->copy->scores[rank].replay_id = replay;
      block->copy->scores[rank].rank      = rank;
      block->copy->scores[rank].tied_rank = tied_rank;
      block->copy->scores[rank].player    = p;

    }
  }

  /* Deallocate json tree */
  cJSON_Delete(json);
  return 0;
}

/**
 * Return codes: -1 (Steam ID inactive), 0 (success), 1 (other error)
 */
int download(struct env* env, struct block* block) {
  if (block->updated || !block->tab->online) return 0;
  unsigned int type_id = block->tab->type;
  const char* type = type_id == LEVEL ? "level" : (type_id == EPISODE ? "episode" : "story");
  char url[128]; // Too lazy to compute the necessary size
  sprintf(url, URL, env->config->def_steam_id, type, block->id);
  int success = 1;
  for (int i = 0; i < RETRIES; i++) {
    if (block->retries > RETRIES) return 1;
    else block->retries++;
    if (curldownload(env->curl, url) != 0) continue; // Request failed
    long http_code = 0;
    curl_easy_getinfo (env->curl->curl, CURLINFO_RESPONSE_CODE, &http_code);
    if (http_code == 200 && env->curl->code != CURLE_ABORTED_BY_CALLBACK) { // Request succeeded
      if (strcmp(env->curl->res, INVALID_RES) == 0) { // Steam ID inactive
        env->curl->active = false;
        block->retries--;
        return -1;
      }
      else { // Steam ID active
        env->curl->active = true;
        if (parse_json(env, block) == 0) { // JSON parsing successful
          block->updated = true;
          env->lcount++;
          return 0;
        }
        else { // JSON parsing unsuccessful
          continue;
        }
      }
    }
    else { // Request failed, likely due to a 502 Bad Gateway
      continue;
    }
  }
  return success;
}

void print_profile(struct profile* profile) {
  printf("Username: %s\n", profile->username);
  printf("User ID: %d\n", profile->id);
}

int update_scores(struct env* env) {
  int ret_code;
  bool err = false;
  int* flags = (int*) &env->flags;
  for (int j = 0; j < env->tcount; j++) {
    if (!env->tabs[j].online) continue;
    for (int i = 0; i < env->tabs[j].size; i++) {
      if (!gflag(flags, DownloadFlags_Download)) return 0;
      if (!env->tabs[j].blocks[i].updated) {
        ret_code = download(env, &env->tabs[j].blocks[i]);
        if (ret_code == 1) err = true;
        if (ret_code == -1) return -1;
      }
      if (j % 10 == 0) sflag(flags, DownloadFlags_Refresh);
      else cflag(flags, DownloadFlags_Refresh);
    }
  }
  return err ? 1 : 0;
}

void compute_tab(struct tab* tab) {
  int score = 0;
  for (int i = 0; i < tab->size; i++)
      score += tab->blocks[i].score;
  printf("Total %s Score: %.3f\n", tab->prefix, (float) score / 1000);
}

/* Sorting (uses global vars mainorder and mainorder_rev */
int blkcmp(const void* b1, const void* b2) {
  int r, s;
  switch(mainorder) {
    case ID:
      r = (int)(((struct block*) b1)->id);
      s = (int)(((struct block*) b2)->id);
      break;
    case ATTEMPTS:
      r = (int)(((struct block*) b1)->attempts);
      s = (int)(((struct block*) b2)->attempts);
      break;
    case VICTORIES:
      r = (int)(((struct block*) b1)->victories) + (int)(((struct block*) b1)->victories_ep);
      s = (int)(((struct block*) b2)->victories) + (int)(((struct block*) b2)->victories_ep);
      break;
    case GOLD:
      r = (int)(((struct block*) b1)->gold);
      s = (int)(((struct block*) b2)->gold);
      break;
    case SCORE:
      r = (int)(((struct block*) b1)->score);
      s = (int)(((struct block*) b2)->score);
      break;
    case RANK:
      r = (int)(((struct block*) b1)->rank);
      s = (int)(((struct block*) b2)->rank);
      break;
    default:
      r = (int)(((struct block*) b1)->id);
      s = (int)(((struct block*) b2)->id);
  }
  return mainorder_rev ? (r < s) - (r > s) : (r > s) - (r < s);
}

void blksort(struct block* blocks, size_t sz, enum orders order, bool reverse) {
  /* Set table order */
  mainorder     = order;
  mainorder_rev = reverse;

  /* Sort list in place using Quicksort (stdlib) */
  qsort(blocks, sz, sizeof(struct block), blkcmp);

  /* Update pointers */
  for (int i = 0; i < sz; i++) {
    blocks[i].orig->copy = &blocks[i];
    blocks[i].copy = &blocks[i];
  }
}
