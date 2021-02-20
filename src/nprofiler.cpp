#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <thread>
#include <chrono>

#include <imgui/imgui.h>
#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui_impl_opengl3.h>

#include <GL/gl3w.h>  // Initialize with gl3wInit()
#include <GLFW/glfw3.h> // Include glfw3.h after our OpenGL definitions

#include "nprofilerlib.h"

#define NAME   "N++ Control Center"
#define AUTHOR "Eddy"
#define DATE   "2020/11/02"
#define WIDTH  1280
#define HEIGHT 720
#define PAUSED 1  // Seconds to wait between checks when paused
#define DATE_S 24 // Characters to store a date
#define TIME_S 12 // Characters to store a time

enum logtypes { INFO, WARN, ERROR };
char* logbuf;

// Win32 exceptions
#if defined(_MSC_VER) && (_MSC_VER >= 1900) && !defined(IMGUI_DISABLE_WIN32_FUNCTIONS)
#pragma comment(lib, "legacy_stdio_definitions")
#endif

static void glfw_error_callback(int error, const char* description)
{
  fprintf(stderr, "Glfw Error %d: %s\n", error, description);
}

void npp_time(char* dest, time_t t = 0, bool date = true) {
  size_t datebuf_s = date ? DATE_S : TIME_S;
  time_t rawtime;
  struct tm* timeinfo;
  char datebuf[datebuf_s];
  rawtime = t == 0 ? time(NULL) : t;
  timeinfo = localtime(&rawtime);
  strftime(datebuf, datebuf_s, date ? "[%F %T] " : "[%H:%M:%S] ", timeinfo);
  strncpy(dest, datebuf, datebuf_s - 1);
  dest[datebuf_s - 1] = 0;
}

void log(char** logbuf, const char* msg, enum logtypes type) {
  // Retrieve and format time
  char datebuf[TIME_S];
  npp_time(datebuf, 0, false);

  // Log type
  char typebuf[9];
  switch(type) {
    case INFO:
      sprintf(typebuf, "[INFO]  ");
      break;
    case WARN:
      sprintf(typebuf, "[WARN]  ");
      break;
    case ERROR:
      sprintf(typebuf, "[ERROR] ");
      break;
  }

  // Concatenate new msg to existing log
  *logbuf = (char*) realloc(*logbuf, strlen(*logbuf) + strlen(datebuf) + strlen(typebuf) + strlen(msg) + 1 + 1);
  strcat(*logbuf, datebuf);
  strcat(*logbuf, typebuf);
  strcat(*logbuf, msg);
  strcat(*logbuf, "\n");
}

static void Tooltip(const char* desc) {
  if (ImGui::IsItemHovered()) {
    ImGui::BeginTooltip();
    ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
    ImGui::TextUnformatted(desc);
    ImGui::PopTextWrapPos();
    ImGui::EndTooltip();
  }
}

static void HelpMarker(const char* desc) {
  ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "(?)");
  Tooltip(desc);
}

static void RangeInt(int* counter, int padding, int limitinf, int limitsup, const char* header = "") {
  ImGui::PushButtonRepeat(true);
  if (ImGui::ArrowButton("##left", ImGuiDir_Left) && *counter > limitinf) *counter--;
  ImGui::SameLine();
  ImGui::Text("%s%0*d", header, padding, *counter);
  ImGui::SameLine();
  if (ImGui::ArrowButton("##right", ImGuiDir_Right) && *counter < limitsup) *counter++;
  ImGui::PopButtonRepeat();
}

static void create_window(const char* window_name, int window_x, int window_y, int window_w, int window_h) {
  ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoTitleBar |
                                  ImGuiWindowFlags_NoScrollbar |
                                  ImGuiWindowFlags_NoMove |
                                  ImGuiWindowFlags_NoResize |
                                  ImGuiWindowFlags_NoCollapse |
                                  ImGuiWindowFlags_AlwaysUseWindowPadding;
  ImGui::SetNextWindowPos(ImVec2(window_x, window_y), ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSize(ImVec2(window_w, window_h), ImGuiCond_FirstUseEver);
  ImGui::Begin(window_name, NULL, window_flags);
}

static void make_table(const char* name, int rows, int cols, const char** row_headers, const char** col_headers) {
  ImGuiTableFlags flags = ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersOuter;
  if (ImGui::BeginTable(name, cols, flags, ImVec2(0, ImGui::GetTextLineHeightWithSpacing() * rows))) {
    for (int i = 0; i < cols; i++) {
      ImGui::TableSetupColumn(col_headers[i]);
    }
    ImGui::TableHeadersRow();
    for (int i = 0; i < rows - 1; i++) {
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("%s", row_headers[i]);
      for (int j = 0; j < cols - 1; j++) {
        ImGui::TableNextColumn();
        ImGui::Text("100");
      }
    }
    ImGui::EndTable();
  }
}

static void make_list(const char* name, const char** headers) {
  ImGuiTableFlags flags = ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_RowBg;
  if (ImGui::BeginTable(name, 3, flags, ImVec2(0, ImGui::GetTextLineHeightWithSpacing() * 21))) {
    ImGui::TableSetupColumn(headers[0], ImGuiTableColumnFlags_WidthFixed);
    ImGui::TableSetupColumn(headers[1], ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn(headers[2], ImGuiTableColumnFlags_WidthFixed);
    ImGui::TableHeadersRow();
    for (int i = 0; i < 20; i++) {
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("%d", i);
      ImGui::TableNextColumn();
      ImGui::Text("%.25s", "-");
      ImGui::TableNextColumn();
      ImGui::Text("%.3f", 0.0f);;
    }
    ImGui::EndTable();
  }
}

static void make_leaderboard(const char* name, struct block* block) {
  ImGuiTableFlags flags = ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_RowBg;
  if (ImGui::BeginTable(name, 3, flags, ImVec2(0, ImGui::GetTextLineHeightWithSpacing() * 21))) {
    ImGui::TableSetupColumn("Rank",   ImGuiTableColumnFlags_WidthFixed);
    ImGui::TableSetupColumn("Player", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn("Score",  ImGuiTableColumnFlags_WidthFixed);
    ImGui::TableHeadersRow();
    bool has_scores = block != NULL && block->scores != NULL;
    const char* name = NULL;
    unsigned int score = -1;
    for (int i = 0; i < 20; i++) {
      if (has_scores) {
        score = block->scores[i].score;
        struct player* p = block->scores[i].player;
        if (p != NULL) name = p->name;
      }
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("%d", i);
      ImGui::TableNextColumn();
      char buf[25];
      snprintf(buf, 25, "%s", name);
      ImGui::Text("%.25s", name == NULL ? "-" : buf);
      ImGui::TableNextColumn();
      ImGui::Text("%10.3f", score == -1 ? 0.0f : (float) score / 1000);
    }
    ImGui::EndTable();
  }
}

static void download_scores(struct env* env, clock_t time, char* currdate)
{
  /* Initialize variables */
  int* flags = (int*) &env->flags;
  sflag(flags, DownloadFlags_Busy);
  cflag(flags, DownloadFlags_Complete);
  sflag(flags, DownloadFlags_Download);
  cflag(flags, DownloadFlags_Paused);
  env->lcount = 0;
  unsigned int obcount = 0;
  int ret_code;
  for (int i = 0; i < env->tcount; i++) {
    if (env->tabs[i].online) obcount += env->tabs[i].size;
    for (int j = 0; j < env->tabs[i].size; j++) {
      env->tabs[i].blocks[j].updated   = false;
      env->tabs[i].blocks[j].score     = -1;
      env->tabs[i].blocks[j].rank      = -1;
      env->tabs[i].blocks[j].tied_rank = -1;
      env->tabs[i].blocks[j].copy->updated   = false;
      env->tabs[i].blocks[j].copy->score     = -1;
      env->tabs[i].blocks[j].copy->rank      = -1;
      env->tabs[i].blocks[j].copy->tied_rank = -1;
// TODO: Initialize scores as well
    }
  }
  while (env->lcount < obcount) {
    if (!gflag(flags, DownloadFlags_Download)) break;
    if (gflag(flags, DownloadFlags_Paused)) {
      std::this_thread::sleep_for(std::chrono::seconds(PAUSED));
      continue;
    }
    for (int i = 0; i < env->tcount; i++)
      for (int j = 0; j < env->tabs[i].size; j++)
        env->tabs[i].blocks[j].retries = 0;
    ret_code = update_scores(env);
    if (ret_code == -1) {
      sflag(flags, DownloadFlags_PopupInactive);
      sflag(flags, DownloadFlags_Paused);
      continue;
    }
    if (ret_code == 1) {
      sflag(flags, DownloadFlags_PopupFailed);
      sflag(flags, DownloadFlags_Paused);
      continue;
    }
  }
  if (env->lcount == obcount) {
    sflag(flags, DownloadFlags_Complete);
    char buf[64];
    sprintf(buf, "Downloaded scores successfully in %.3f seconds.", ((float)(clock() - time)) / CLOCKS_PER_SEC);
    log(&logbuf, (const char*) buf, INFO);
    npp_time(currdate);
  } else {
    cflag(flags, DownloadFlags_Complete);
    if (gflag(flags, DownloadFlags_Download)) {
      log(&logbuf, "Scores failed to download completely.", ERROR);
    } else {
      log(&logbuf, "Scores download cancelled.", INFO);
    }
  }
  cflag(flags, DownloadFlags_Busy);
  cflag(flags, DownloadFlags_Download);
  cflag(flags, DownloadFlags_Paused);
}

int main(int, char**)
{
  // Setup window
  glfwSetErrorCallback(glfw_error_callback);
  if (!glfwInit())
  return 1;

  // Decide GL+GLSL versions
  #ifdef __APPLE__
  // GL 3.2 + GLSL 150
  const char* glsl_version = "#version 150";
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // Required on Mac
  #else
  // GL 3.0 + GLSL 130
  const char* glsl_version = "#version 130";
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
  //glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
  //glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // 3.0+ only
  #endif
  glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);
  //glfwWindowHint(GLFW_DECORATED, GL_FALSE);

  // Create window with graphics context
  GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, NAME, NULL, NULL);
  if (window == NULL)
  return 1;
  glfwMakeContextCurrent(window);
  glfwSwapInterval(1); // Enable vsync

  // Initialize OpenGL loader
  if (gl3wInit() != 0)
  {
    fprintf(stderr, "Failed to initialize OpenGL loader!\n");
    return 1;
  }

  // Setup Dear ImGui context
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO(); (void)io;
  //io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
  //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

  // Setup Dear ImGui style
  ImGui::StyleColorsDark();

  // Setup Platform/Renderer bindings
  ImGui_ImplGlfw_InitForOpenGL(window, true);
  ImGui_ImplOpenGL3_Init(glsl_version);

  // Don't create ini config file
  io.IniFilename = NULL;

  // Background
  ImVec4 clear_color = ImVec4(0.0586f, 0.0586f, 0.0586f, 0.9375f);

  /* Logging */
  logbuf = (char*) calloc(80, sizeof(char));
  log(&logbuf, "Initialized program.", INFO);

  /* Prepare main variables */
  unsigned int bcount  = L_COUNT + E_COUNT; // Block count
  unsigned int tcount  = TAB_COUNT;         // Tab count
  unsigned int obcount = 0;                 // Count of blocks to be downloaded
  unsigned int pcount  = 0;                 // Player count
  unsigned int scount  = 0;                 // Score count
  unsigned int lcount  = 0;                 // Leaderboard count

  struct profile* profile  = (struct profile*) calloc(1,            sizeof(struct profile));
  struct block* blocks_raw = (struct block*)   calloc(bcount,       sizeof(struct block));
  struct tab* tabs         = (struct tab*)     calloc(tcount,       sizeof(struct tab));
  create_tabs(tabs);

  for (int i = 0; i < tcount; i++) if (tabs[i].online) obcount += tabs[i].size;
  struct score* scores     = (struct score*)   calloc(20 * obcount, sizeof(struct score));
  struct player* players   = (struct player*)  calloc(PLAYER_MAX, sizeof(struct player));

  /* Initialize program and load configuration. */
  initialize();
  struct config* config = parse_config(players, &pcount);
  log(&logbuf, "Read configuration file.", INFO);

  /* Initialize cURL */
  struct curl* curl = (struct curl*) calloc(1, sizeof(struct curl));
  if (curlinit(curl) != 0) {
    puterr("cURL could not be initialized.");
    log(&logbuf, "cURL could not be initialized.", ERROR);
    kill(1);
  }

  /* Read nprofile */
  unsigned char *f;
  int size = read(&f, FILENAME);
  if (size == 0) {
    puterr("Error reading nprofile");
    log(&logbuf, "Error reading nprofile", ERROR);
    kill(1);
  }
  if (size != FILESIZE) {
    puterr("Incorrect nprofile size");
    log(&logbuf, "Incorrect nprofile size, information may be incorrect.", WARN);
    kill(1);
  }

  /* Parse nprofile */
  fill_blocks(tabs, tcount, blocks_raw, scores);
  parse_tabs(f, tabs);
  parse_profile(f, profile);
  log(&logbuf, "Parsed savefile.", INFO);

  /* Create block duplicate, which will be used for reordering in place, printing... */
  struct block* blocks = (struct block*) calloc(bcount, sizeof(struct block));
  memcpy(blocks, blocks_raw, bcount * sizeof(struct block));
  for (int i = 0; i < bcount; i++) {
    blocks_raw[i].copy = &blocks[i];
    blocks[i].copy = &blocks[i];
  }

  /* Other state variables */
  static int board_index     = 0;
  static bool busy           = false;   // Are we downloading or loading scores?
  static bool refresh        = false;   // Refresh rankings, etc on this frame
  static bool complete       = false;   // Whether the currently downloaded scores are complete or not
  static bool popup_inactive = false;   // Show the "Inactive Steam ID" popup
  static bool popup_failed   = false;   // Show the "Failed download" popup
  static bool download       = false;   // Download the scores
  static bool paused         = false;   // Pause download of scores
  static clock_t time        = clock(); // Current time, for benchmarking
  char* currdate = (char*) calloc(DATE_S, sizeof(char)); // For displaying in the currently loaded scores
  strcpy(currdate, "None");

  /* Store everything inside the working environment */
  struct env env = (struct env) {
    config,
    curl,
    profile,
    tabs,
    blocks,
    players,
    scores,
    tcount,
    bcount,
    pcount,
    scount,
    lcount,
    (DownloadFlags) 0
  };

  /* Free memory storing nprofile */
  free(f);

  /* Do things */
  //compute_tab(tabs);      // Calculate total SI level score
  //list(tabs, 1);          // Print most improvable SI level scores
  //update_tab(curl, tabs); // Update SI level scores and 0ths
  //print_profile(profile); // Print profile info

  // Main GUI loop
  while (!glfwWindowShouldClose(window))
  {
    // Poll and handle events (inputs, window resize, etc.)
    // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
    // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application.
    // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application.
    // Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
    glfwPollEvents();

    // Start the Dear ImGui frame
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    const char* s_tabs[6]   = { "SI", "S", "SU", "SL", "?", "!" };
    const char* s_types[3]  = { "Levels", "Episodes", "Stories" };
    const char* s_modes[4]  = { "Solo", "Coop", "Race", "Hardcore" };
    int win1_x = 0;
    int win1_y = 0;
    int win1_w = 640;
    int win1_h = ImGui::GetTextLineHeightWithSpacing() * 40.5;
    int win2_x = win1_x + win1_w;
    int win2_y = 0;
    int win2_w = 640;
    int win2_h = ImGui::GetTextLineHeightWithSpacing() * 40.5;
    int win3_x = 0;
    int win3_y = win1_x + win1_h;
    int win3_w = WIDTH;
    int win3_h = HEIGHT - win1_h;
    int* dflags = (int*) &env.flags;
    {
      /* Header */
      create_window("scores", win1_x, win1_y, win1_w, win1_h);
      ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "HIGHSCORE ANALYSIS"); ImGui::SameLine();
      ImGui::Text("Loaded:"); ImGui::SameLine();
      ImGui::Text("%s", currdate); ImGui::SameLine(ImGui::GetWindowWidth() - 30);
      HelpMarker("This section will analyze the highscores from the server. \
                  You first have to load some scores, either by downloading them, \
                  or by loading them from a file. You can then save these scores \
                  to be able to load them at a later point (recommended).");
      if (gflag(dflags, DownloadFlags_PopupInactive)) {
        ImGui::OpenPopup("SteamIDInactive");
        if(ImGui::BeginPopupModal("SteamIDInactive", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
          ImGui::Text("Steam ID inactive, please open N++ and click OK to continue downloading,\nor click Cancel to stop downloading.");
          ImGui::Text(" ");
          ImGui::SameLine(ImGui::GetWindowWidth() / 3 - 60);
          if (ImGui::Button("OK", ImVec2(120, 0))) {
            cflag(dflags, DownloadFlags_Paused);
            cflag(dflags, DownloadFlags_PopupInactive);
            ImGui::CloseCurrentPopup();
          }
          ImGui::SameLine(2 * ImGui::GetWindowWidth() / 3 - 60);
          if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            cflag(dflags, DownloadFlags_Download);
            cflag(dflags, DownloadFlags_PopupInactive);
            ImGui::CloseCurrentPopup();
          }
          ImGui::EndPopup();
        }
      }
      if (gflag(dflags, DownloadFlags_PopupFailed)) {
        ImGui::OpenPopup("FailedDownload");
        if(ImGui::BeginPopupModal("FailedDownload", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
          ImGui::Text("Some scores failed to download, press OK\nto retry them or Cancel to stop downloading.");
          ImGui::Text(" ");
          ImGui::SameLine(ImGui::GetWindowWidth() / 3 - 60);
          if (ImGui::Button("OK", ImVec2(120, 0))) {
            cflag(dflags, DownloadFlags_Paused);
            cflag(dflags, DownloadFlags_PopupFailed);
            ImGui::CloseCurrentPopup();
          }
          ImGui::SameLine(2 * ImGui::GetWindowWidth() / 3 - 60);
          if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            cflag(dflags, DownloadFlags_Download);
            cflag(dflags, DownloadFlags_PopupFailed);
            ImGui::CloseCurrentPopup();
          }
          ImGui::EndPopup();
        }
      }
      if (ImGui::SmallButton("Download scores")) {
        if (gflag(dflags, DownloadFlags_Busy)) {
          ImGui::OpenPopup("Busy");
        } else {
          time     = clock();
          std::thread downloader(download_scores, &env, time, currdate);
          downloader.detach();
        }
      }
      ImGui::SameLine();
      if (ImGui::SmallButton("Load scores")) {
        if (gflag(dflags, DownloadFlags_Busy)) {
          ImGui::OpenPopup("Busy");
        } else {
          sflag(dflags, DownloadFlags_Busy);
          // Load dialog (in main thread)
          if (parse_scores(&env) == 0) npp_time(currdate, config->time);
          cflag(dflags, DownloadFlags_Busy);
        }
      }
      ImGui::SameLine();
      if (ImGui::SmallButton("Save scores")) {
        if (busy) {
          ImGui::OpenPopup("Busy");
        } else {
          sflag(dflags, DownloadFlags_Busy);
          // Save dialog (in main thread)
          save_scores(&env);
          cflag(dflags, DownloadFlags_Busy);
        }
      }
      if (gflag(dflags, DownloadFlags_Download)) {
        ImGui::SameLine();
        if (ImGui::SmallButton("Cancel")) cflag(dflags, DownloadFlags_Download);
      }
      ImGui::SameLine(ImGui::GetWindowWidth() - 100);
      if (ImGui::SmallButton("Config")) ImGui::OpenPopup("Config");
      ImGui::SameLine(ImGui::GetWindowWidth() - 45);
      if (ImGui::SmallButton("Help")) ImGui::OpenPopup("Help");
      if (ImGui::BeginPopupModal("Config", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2, 0));
        struct config* conf = env.config;
        static bool conf_start   = true;
        static bool conf_refresh = true;
        static char** hacker_names  = NULL;
        static char** hacker_ids    = NULL;
        static char** cheater_names = NULL;
        static char** cheater_ids   = NULL;
        static int    hacker_count  = conf->hacker_count;
        static int    cheater_count = conf->cheater_count;
        /* Array initialization - execute this block only on first frame */
        if (conf_start) {
          hacker_names  = (char**) calloc(hacker_count,  sizeof(char*));
          hacker_ids    = (char**) calloc(hacker_count,  sizeof(char*));
          cheater_names = (char**) calloc(cheater_count, sizeof(char*));
          cheater_ids   = (char**) calloc(cheater_count, sizeof(char*));
          for (int i = 0; i < hacker_count; i++) {
            hacker_names[i] = strdup(conf->hackers[i].name);
            hacker_ids[i] = (char*) calloc(8, sizeof(char));
            if (conf->hackers[i].id != -1) sprintf(hacker_ids[i], "%d", conf->hackers[i].id);
          }
          for (int i = 0; i < cheater_count; i++) {
            cheater_names[i] = strdup(conf->cheaters[i].name);
            cheater_ids[i] = (char*) calloc(8, sizeof(char));
            if (conf->cheaters[i].id != -1) sprintf(cheater_ids[i], "%d", conf->cheaters[i].id);
          }
          conf_start = false;
        }
        // TODO: Some of these values must be read from the config,
        //       like confbuf_hacks and confbuf_cheats.
        static char confbuf_name[128]    = ""; // Default username
        static char confbuf_id[18]       = ""; // Default Steam ID (for downloading)
        static int  confbuf_hacks        = 4;  // Default action to handle hackers
        static int  confbuf_cheats       = 2;  // Default action to handle cheaters
        static char confbuf_hnameh[128]  = ""; // Name of hacker being edited
        static char confbuf_hidh[8]      = ""; // User id of hacker being edited
        static char confbuf_hnamec[128]  = ""; // Name of cheater being edited
        static char confbuf_hidc[8]      = ""; // User id of cheater being edited
        static int  confbuf_hindexh      = 0;  // Index of selected hacker
        static int  confbuf_hindexc      = 0;  // Index of selected cheater
        if (conf_refresh) {
          printf("REFRESH\n");
          if (confbuf_hindexh < hacker_count) {
            strcpy(confbuf_hnameh, hacker_names[confbuf_hindexh]);
            strcpy(confbuf_hidh,   hacker_ids[confbuf_hindexh]);
          } else {
            confbuf_hnameh[0] = 0;
            confbuf_hidh[0]   = 0;
          }
          if (confbuf_hindexc < cheater_count) {
            strcpy(confbuf_hnamec, cheater_names[confbuf_hindexc]);
            strcpy(confbuf_hidc,   cheater_ids[confbuf_hindexc]);
          } else {
            confbuf_hnamec[0] = 0;
            confbuf_hidc[0]   = 0;
          }
          conf_refresh = false;
        }
        ImGui::InputText("Default username", confbuf_name, 128);
        ImGui::InputText("Default Steam ID", confbuf_id, 18, ImGuiInputTextFlags_CharsDecimal);
        ImGui::SameLine();
        HelpMarker("Your Steam ID is necessary to download N++'s scores. Go to Help for further info about how to obtain it.");
        ImGui::Separator();
        ImGui::PushItemWidth(120);
        if (ImGui::BeginTable("config", 2)) {
          ImGui::TableNextRow();
          ImGui::TableNextColumn();
          ImGui::Text("Hackers");
          if (ImGui::ListBox("##h", &confbuf_hindexh, hacker_names, hacker_count, 6)) conf_refresh = true;
          ImGui::TableNextColumn();
          ImGui::Text("Cheaters");
          if (ImGui::ListBox("##c", &confbuf_hindexc, cheater_names, cheater_count, 6)) conf_refresh = true;
          ImGui::TableNextRow();
          ImGui::TableNextColumn();
          ImGui::InputText("Name##hacker_name", confbuf_hnameh, 128);
          ImGui::InputText("ID##hacker_id",   confbuf_hidh,     8, ImGuiInputTextFlags_CharsDecimal); ImGui::SameLine();
          HelpMarker("Leave blank if unknown.");
          if (ImGui::SmallButton("Add##H")) {
              hacker_names = (char**) arradd((char*) hacker_names, strdup(confbuf_hnameh), &hacker_count, sizeof(*hacker_names));
              hacker_count--;
              hacker_ids = (char**) arradd((char*) hacker_ids, strdup(confbuf_hidh), &hacker_count, sizeof(*hacker_names));
              conf_refresh = true;
          } ImGui::SameLine();
          if (ImGui::SmallButton("Edit##H")) {
            if (hacker_count > 0) {
              strcpy(hacker_names[confbuf_hindexh], confbuf_hnameh);
              strcpy(hacker_ids[confbuf_hindexh],   confbuf_hidh);
            }
          } ImGui::SameLine();
          if (ImGui::SmallButton("Delete##H")) {
            if (hacker_count > 0) {
              arrdel((char*) hacker_names, confbuf_hindexh, &hacker_count, sizeof(*hacker_names));
              hacker_count++; // We use the same counter for the 2 arrays, so we need to restore
              arrdel((char*) hacker_ids, confbuf_hindexh, &hacker_count, sizeof(*hacker_names));
              if (confbuf_hindexh > 0) confbuf_hindexh--;
              conf_refresh = true;
            }
          } ImGui::SameLine();
          if (ImGui::SmallButton("-->")) {
            if (hacker_count > 0) {
              /* Move both name and id to cheater list */
              cheater_names = (char**) arradd((char*) cheater_names, (char*) (hacker_names + confbuf_hindexh), &cheater_count, sizeof(*cheater_names));
              cheater_count--;
              cheater_ids = (char**) arradd((char*) cheater_ids, (char*) (hacker_ids + confbuf_hindexh), &cheater_count, sizeof(*cheater_names));

              /* Delete both name and id from hacker list */
              arrdel((char*) hacker_names, confbuf_hindexh, &hacker_count, sizeof(*hacker_names));
              hacker_count++;
              arrdel((char*) hacker_ids, confbuf_hindexh, &hacker_count, sizeof(*hacker_names));
              if (confbuf_hindexh > 0) confbuf_hindexh--;
              conf_refresh = true;
            }
          }
          ImGui::TableNextColumn();
          ImGui::InputText("Name##cheater_name", confbuf_hnamec, 128);
          ImGui::InputText("ID##cheater_id",   confbuf_hidc,     8, ImGuiInputTextFlags_CharsDecimal); ImGui::SameLine();
          HelpMarker("Leave blank if unknown.");
          if (ImGui::SmallButton("Add##C")) {

          } ImGui::SameLine();
          if (ImGui::SmallButton("Edit##C")) {
            if (cheater_count > 0) {
              strcpy(cheater_names[confbuf_hindexc], confbuf_hnamec);
              strcpy(cheater_ids[confbuf_hindexc],   confbuf_hidc);
            }
          } ImGui::SameLine();
          if (ImGui::SmallButton("Delete##C")) {
            if (cheater_count > 0) {
              arrdel((char*) cheater_names, confbuf_hindexc, &cheater_count, sizeof(*cheater_names));
              cheater_count++; // We use the same counter for the 2 arrays, so we need to restore
              arrdel((char*) cheater_ids, confbuf_hindexc, &cheater_count, sizeof(*cheater_names));
              if (confbuf_hindexc > 0) confbuf_hindexc--;
              conf_refresh = true;
            }
          } ImGui::SameLine();
          if (ImGui::SmallButton("<--")) {
            if (cheater_count > 0) {
              /* Move both name and id to hacker list */
              hacker_names = (char**) arradd((char*) hacker_names, (char*) (cheater_names + confbuf_hindexc), &hacker_count, sizeof(*hacker_names));
              hacker_count--;
              hacker_ids = (char**) arradd((char*) hacker_ids, (char*) (cheater_ids + confbuf_hindexc), &hacker_count, sizeof(*hacker_names));

              /* Delete both name and id from hacker list */
              arrdel((char*) cheater_names, confbuf_hindexc, &cheater_count, sizeof(*cheater_names));
              cheater_count++;
              arrdel((char*) cheater_ids, confbuf_hindexc, &cheater_count, sizeof(*cheater_names));
              if (confbuf_hindexc > 0) confbuf_hindexc--;
              conf_refresh = true;
            }
          }
          ImGui::EndTable();
        }
        ImGui::PopItemWidth();
        ImGui::Separator();
        ImGui::Text("Action to handle hackers:");
        ImGui::RadioButton("Don't do anything.",                             &confbuf_hacks, 0);
        ImGui::RadioButton("Highlight name everywhere.",                     &confbuf_hacks, 1);
        ImGui::RadioButton("Ignore in rankings, highlight everywhere else.", &confbuf_hacks, 2);
        ImGui::RadioButton("Ignore everywhere.",                             &confbuf_hacks, 3);
        ImGui::RadioButton("Wipe from database (irreversible!).",            &confbuf_hacks, 4);
        ImGui::Separator();
        ImGui::Text("Action to handle cheaters:");
        ImGui::RadioButton("Don't do anything.",                             &confbuf_cheats, 0);
        ImGui::RadioButton("Highlight name everywhere.",                     &confbuf_cheats, 1);
        ImGui::RadioButton("Ignore in rankings, highlight everywhere else.", &confbuf_cheats, 2);
        ImGui::RadioButton("Ignore everywhere.",                             &confbuf_cheats, 3);
        ImGui::RadioButton("Wipe from database (irreversible!).",            &confbuf_cheats, 4);
        ImGui::PopStyleVar();
        ImGui::Text(" ");
        ImGui::SameLine(ImGui::GetWindowWidth() / 2 - 60);
        if (ImGui::Button("OK", ImVec2(120, 0))) {
          // TODO: Update env->config here
          // TODO: Also update env->players and therefore env->blocks and env->scores, wherever players are referenced
          for (int i = 0; i < hacker_count; i++) {
            free(hacker_names[i]);
            free(hacker_ids[i]);
          }
          for (int i = 0; i < cheater_count; i++) {
            free(cheater_names[i]);
            free(cheater_ids[i]);
          }
          free(hacker_names);
          free(cheater_names);
          // TODO: May have to restore some other static variables.
          hacker_count    = conf->hacker_count;
          cheater_count   = conf->cheater_count;
          confbuf_hindexh = 0;
          confbuf_hindexc = 0;
          conf_start      = true;
          conf_refresh    = true;
          ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
      }
      if (ImGui::BeginPopupModal("Help", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Where to find your savefile (nprofile):");
        ImGui::Text(" ");
        ImGui::SameLine(ImGui::GetWindowWidth() / 2 - 60);
        if (ImGui::Button("OK", ImVec2(120, 0))) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
      }
      if (ImGui::BeginPopupModal("Busy", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Scores are currently being downloaded / loaded, please wait until\nthey finish or press Cancel to stop abort the process.");
        ImGui::Text(" ");
        ImGui::SameLine(ImGui::GetWindowWidth() / 3 - 60);
        if (ImGui::Button("OK", ImVec2(120, 0))) {
          ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine(2 * ImGui::GetWindowWidth() / 3 - 60);
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
          cflag(dflags, DownloadFlags_Download);
          ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
      }
      char buf[32];
      sprintf(buf, "%d/%d", env.lcount, obcount);
      ImGui::ProgressBar((float) env.lcount / obcount, ImVec2(-1.0f, 0.0f), buf);
      if (ImGui::BeginTable("log", 2, 0)) {
        ImGui::TableSetupColumn(NULL, ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn(NULL, ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::InputTextMultiline("##log", logbuf, IM_ARRAYSIZE(logbuf), ImVec2(ImGui::GetWindowWidth() - 100, ImGui::GetTextLineHeightWithSpacing() * 4), ImGuiInputTextFlags_ReadOnly);
        ImGui::TableNextColumn();
        ImGui::Text("Database:");
        ImGui::Text("Boards  - %5d", env.lcount);
        ImGui::Text("Players - %5d", env.pcount);
        ImGui::Text("Scores  - %5d", env.scount);
        ImGui::EndTable();
      }

      /* Stat summary table */
      ImGuiTableFlags flags = 0;
      if (ImGui::BeginTable("highscoring", 2, flags)) {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();

        ImGui::Text("          PERSONAL HIGHSCORING STATS");
        const char* row_headers[7] = { "SI", "S", "SU", "SL", "?", "!", "Total" };
        const char* col_headers[5] = { "Tabs", "Top20", "Top10", "Top5", "0th" };
        ImGuiTabBarFlags tab_flags = ImGuiTabBarFlags_None;
        if (ImGui::BeginTabBar("stat_tabs", tab_flags)) {
          ImGui::TabItemButton("?", ImGuiTabItemFlags_Leading | ImGuiTabItemFlags_NoTooltip);
          Tooltip("Solo includes both levels and episodes from solo mode, that \
                   is, the standard highscoring metric used in the community.");
          if (ImGui::BeginTabItem("Solo")) {
            make_table("solo", 8, 5, row_headers, col_headers);
            ImGui::EndTabItem();
          }
          if (ImGui::BeginTabItem("Levels")) {
            make_table("levels", 8, 5, row_headers, col_headers);
            ImGui::EndTabItem();
          }
          if (ImGui::BeginTabItem("Episodes")) {
            make_table("episodes", 8, 5, row_headers, col_headers);
            ImGui::EndTabItem();
          }
          if (ImGui::BeginTabItem("Stories")) {
            make_table("stories", 8, 5, row_headers, col_headers);
            ImGui::EndTabItem();
          }
          ImGui::EndTabBar();
        }

        /* Additional highscoring stats table */
        const char* col_headers2[5] = { "Tabs", "Level", "Episode", "Story", "Total" };
        if (ImGui::BeginTabBar("varied_tabs", tab_flags)) {
          ImGui::TabItemButton("?", ImGuiTabItemFlags_Leading | ImGuiTabItemFlags_NoTooltip);
          Tooltip("'Total score' adds up all your scores in each tab. 'Points' \
                   awards points for each highscore you have: 20 points for a 0th, \
                   19 for 1st... up to 1 for 19th.");
          if (ImGui::BeginTabItem("Total score")) {
            make_table("total_score", 8, 5, row_headers, col_headers2);
            ImGui::EndTabItem();
          }
          if (ImGui::BeginTabItem("Points")) {
            make_table("points", 8, 5, row_headers, col_headers2);
            ImGui::EndTabItem();
          }
          ImGui::EndTabBar();
        }

        /* Histograms */
        static float arr[] = {
           0.0f,   1.0f,  5.0f, 12.0f, 17.0f,
          35.0f,  56.0f, 43.0f, 43.0f, 30.0f,
          25.0f,  28.0f, 37.0f, 68.0f, 78.0f,
          89.0f, 100.0f, 90.0f, 80.0f, 85.0f
        };
        ImGui::PlotHistogram("", arr, IM_ARRAYSIZE(arr), 0, NULL, 0, 100, ImVec2(ImGui::GetContentRegionAvail().x * 1.0f, 200));

        ImGui::TableNextColumn();

        /* Global stats (leaderboards, rankings, spreads, and lists, in a tab bar */
        ImGui::Text("          GLOBAL HIGHSCORING STATS");
        if (ImGui::BeginTabBar("global_tabs", tab_flags)) {
          if (ImGui::BeginTabItem("Leaderboards")) {
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2, 0));
            if (ImGui::BeginTable("g_leaderboards", 2, ImGuiTableFlags_SizingPolicyFixedX | ImGuiTableFlags_BordersInnerV)) {
              ImGui::TableNextRow(); ImGui::TableNextColumn();
              ImGui::Text("Type"); ImGui::TableNextColumn();
              static int leaderboard_type = 0;
              ImGui::RadioButton("Level", &leaderboard_type, 0); ImGui::SameLine();
              ImGui::RadioButton("Episode", &leaderboard_type, 1); ImGui::SameLine();
              ImGui::RadioButton("Story", &leaderboard_type, 2);

              ImGui::TableNextRow(); ImGui::TableNextColumn();
              ImGui::Text("Tab"); ImGui::TableNextColumn();
              static int leaderboard_tab = 0;
              ImGui::RadioButton("SI", &leaderboard_tab, 0); ImGui::SameLine();
              ImGui::RadioButton("S",  &leaderboard_tab, 1); ImGui::SameLine();
              ImGui::RadioButton("SU", &leaderboard_tab, 2); ImGui::SameLine();
              ImGui::RadioButton("SL", &leaderboard_tab, 3); ImGui::SameLine();
              ImGui::RadioButton("?",  &leaderboard_tab, 4); ImGui::SameLine();
              ImGui::RadioButton("!",  &leaderboard_tab, 5);

              // TODO: Disable this if we're on stories
              ImGui::TableNextRow(); ImGui::TableNextColumn();
              ImGui::Text("Row"); ImGui::TableNextColumn();
              static int leaderboard_row = 0;
              ImGui::RadioButton("A", &leaderboard_row, 0); ImGui::SameLine();
              ImGui::RadioButton("B", &leaderboard_row, 1); ImGui::SameLine();
              ImGui::RadioButton("C", &leaderboard_row, 2); ImGui::SameLine();
              ImGui::RadioButton("D", &leaderboard_row, 3); ImGui::SameLine();
              ImGui::RadioButton("E", &leaderboard_row, 4); ImGui::SameLine();
              ImGui::RadioButton("X", &leaderboard_row, 5);

              ImGui::TableNextRow(); ImGui::TableNextColumn();
              ImGui::Text("Column"); ImGui::TableNextColumn();
              static int leaderboard_col = 0;
              ImGui::PushButtonRepeat(true);
              if (ImGui::ArrowButton("##left", ImGuiDir_Left) && leaderboard_col > 0) leaderboard_col--;
              ImGui::SameLine();
              ImGui::Text("%02d", leaderboard_col);
              ImGui::SameLine();
              if (ImGui::ArrowButton("##right", ImGuiDir_Right) && leaderboard_col < 19) leaderboard_col++;
              ImGui::PopButtonRepeat();

              // TODO: Disable this if we're on episodes or stories
              ImGui::TableNextRow(); ImGui::TableNextColumn();
              ImGui::Text("Level"); ImGui::TableNextColumn();
              static int leaderboard_level = 0;
              ImGui::RadioButton("00", &leaderboard_level, 0); ImGui::SameLine();
              ImGui::RadioButton("01", &leaderboard_level, 1); ImGui::SameLine();
              ImGui::RadioButton("02", &leaderboard_level, 2); ImGui::SameLine();
              ImGui::RadioButton("03", &leaderboard_level, 3); ImGui::SameLine();
              ImGui::RadioButton("04", &leaderboard_level, 4);

              ImGui::TableNextRow(); ImGui::TableNextColumn();
              ImGui::Text(" ");
              ImGui::TableNextRow(); ImGui::TableNextColumn();
              ImGui::Text(" ");

              ImGui::EndTable();
            }
            int tab_offset = &blocks_raw[board_index] - &blocks_raw[board_index].tab->blocks[0];
            int tab_size = blocks_raw[board_index].tab->size;
            ImGui::Text(" "); ImGui::SameLine(ImGui::GetContentRegionAvail().x * 0.35f);
            ImGui::PushButtonRepeat(true);
            if (ImGui::ArrowButton("##left", ImGuiDir_Left)) {
              if (tab_offset > 0) {
                board_index--;
              } else {
                board_index += tab_size - 1;
              }
            }
            ImGui::SameLine();
            ImGui::Text("%10s", blocks_raw[board_index].name); ImGui::SameLine();
            if (ImGui::ArrowButton("##right", ImGuiDir_Right)) {
              if (tab_offset < tab_size - 1) {
                board_index++;
              } else {
                board_index -= tab_size - 1;
              }
            }
            ImGui::PopButtonRepeat();
            ImGui::PopStyleVar();

            make_leaderboard("leaderboards", &blocks_raw[board_index]);
            ImGui::EndTabItem();
          }
          if (ImGui::BeginTabItem("Rankings")) {
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2, 0));
            if (ImGui::BeginTable("g_rankings", 2, ImGuiTableFlags_SizingPolicyFixedX | ImGuiTableFlags_BordersInnerV)) {
              static bool tabs[6] = { true, true, true, true, true, true };
              static bool types[3] = { true, true, false };
              ImGui::TableNextRow(); ImGui::TableNextColumn();
              ImGui::Text("Types"); ImGui::TableNextColumn();
              ImGui::Checkbox("Levels",   &types[0]); ImGui::SameLine();
              ImGui::Checkbox("Episodes", &types[1]); ImGui::SameLine();
              ImGui::Checkbox("Stories",  &types[2]);

              ImGui::TableNextRow(); ImGui::TableNextColumn();
              ImGui::Text("Tabs"); ImGui::TableNextColumn();
              ImGui::Checkbox("SI", &tabs[0]); ImGui::SameLine();
              ImGui::Checkbox("S",  &tabs[1]); ImGui::SameLine();
              ImGui::Checkbox("SU", &tabs[2]); ImGui::SameLine();
              ImGui::Checkbox("SL", &tabs[3]); ImGui::SameLine();
              ImGui::Checkbox("?",  &tabs[4]); ImGui::SameLine();
              ImGui::Checkbox("!",  &tabs[5]);

              ImGui::TableNextRow(); ImGui::TableNextColumn();
              ImGui::Text("Ranking"); ImGui::TableNextColumn();
              static int ranking = 0;
              static int ranking_rank = 3;
              ImGui::RadioButton("0ths",           &ranking, 0); ImGui::SameLine();
              ImGui::RadioButton("Top20s",         &ranking, 1); ImGui::SameLine();
              ImGui::RadioButton("Top10s",         &ranking, 2); ImGui::SameLine();
              ImGui::RadioButton("Top5s",          &ranking, 3);
              ImGui::RadioButton("Total score",    &ranking, 4); ImGui::SameLine();
              ImGui::RadioButton("Total points",   &ranking, 5);
              ImGui::RadioButton("Avg. points",    &ranking, 6); ImGui::SameLine();
              ImGui::RadioButton("Other:",         &ranking, 7); ImGui::SameLine();
              RangeInt(&ranking_rank, 2, 0, 19, "Top "); // TODO: Disable this if ranking_rank != 7

              ImGui::TableNextRow(); ImGui::TableNextColumn();
              ImGui::Text("Ties"); ImGui::TableNextColumn();
              static int ranking_ties = 0;
              ImGui::RadioButton("Yes", &ranking_ties, 0); ImGui::SameLine();
              ImGui::RadioButton("No",  &ranking_ties, 1);

              ImGui::TableNextRow(); ImGui::TableNextColumn();
              ImGui::Text(" ");
              ImGui::TableNextRow(); ImGui::TableNextColumn();
              ImGui::Text(" ");

              ImGui::EndTable();
            }
            ImGui::PopStyleVar();
            const char* col_headers3[3] = { "Rank", "Player", "Count" };
            make_list("rankings", col_headers3);
            ImGui::EndTabItem();
          }
          if (ImGui::BeginTabItem("Spreads")) {
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2, 0));
            if (ImGui::BeginTable("g_spreads", 2, ImGuiTableFlags_SizingPolicyFixedX | ImGuiTableFlags_BordersInnerV)) {
              static bool tabs[6] = { true, true, true, true, true, true };
              static bool types[3] = { true, true, false };
              ImGui::TableNextRow(); ImGui::TableNextColumn();
              ImGui::Text("Types"); ImGui::TableNextColumn();
              ImGui::Checkbox("Levels",   &types[0]); ImGui::SameLine();
              ImGui::Checkbox("Episodes", &types[1]); ImGui::SameLine();
              ImGui::Checkbox("Stories",  &types[2]);

              ImGui::TableNextRow(); ImGui::TableNextColumn();
              ImGui::Text("Tabs"); ImGui::TableNextColumn();
              ImGui::Checkbox("SI", &tabs[0]); ImGui::SameLine();
              ImGui::Checkbox("S",  &tabs[1]); ImGui::SameLine();
              ImGui::Checkbox("SU", &tabs[2]); ImGui::SameLine();
              ImGui::Checkbox("SL", &tabs[3]); ImGui::SameLine();
              ImGui::Checkbox("?",  &tabs[4]); ImGui::SameLine();
              ImGui::Checkbox("!",  &tabs[5]);

              ImGui::TableNextRow(); ImGui::TableNextColumn();
              ImGui::Text("Order"); ImGui::TableNextColumn();
              static int spread_order = 0;
              ImGui::RadioButton("Biggest", &spread_order, 0); ImGui::SameLine();
              ImGui::RadioButton("Smallest",  &spread_order, 1);

              ImGui::TableNextRow(); ImGui::TableNextColumn();
              ImGui::Text("Range"); ImGui::TableNextColumn();
              static int spread_range_inf = 0;
              static int spread_range_sup = 19;
              ImGui::Text("From "); ImGui::SameLine();
              RangeInt(&spread_range_inf, 2, 0, 19, ""); ImGui::SameLine();
              ImGui::Text(" to "); ImGui::SameLine();
              RangeInt(&spread_range_sup, 2, 0, 19, "");

              ImGui::TableNextRow(); ImGui::TableNextColumn();
              ImGui::Text(" ");
              ImGui::TableNextRow(); ImGui::TableNextColumn();
              ImGui::Text(" ");
              ImGui::TableNextRow(); ImGui::TableNextColumn();
              ImGui::Text(" ");
              ImGui::TableNextRow(); ImGui::TableNextColumn();
              ImGui::Text(" ");

              ImGui::EndTable();
            }
            ImGui::PopStyleVar();
            const char* col_headers3[3] = { "Rank", "Player", "Time" };
            make_list("spreads", col_headers3);
            ImGui::EndTabItem();
          }
          if (ImGui::BeginTabItem("Lists")) {
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2, 0));
            if (ImGui::BeginTable("g_lists", 2, ImGuiTableFlags_SizingPolicyFixedX | ImGuiTableFlags_BordersInnerV)) {
              static bool tabs[6] = { true, true, true, true, true, true };
              static bool types[3] = { true, true, false };
              ImGui::TableNextRow(); ImGui::TableNextColumn();
              ImGui::Text("Types"); ImGui::TableNextColumn();
              ImGui::Checkbox("Levels",   &types[0]); ImGui::SameLine();
              ImGui::Checkbox("Episodes", &types[1]); ImGui::SameLine();
              ImGui::Checkbox("Stories",  &types[2]);

              ImGui::TableNextRow(); ImGui::TableNextColumn();
              ImGui::Text("Tabs"); ImGui::TableNextColumn();
              ImGui::Checkbox("SI", &tabs[0]); ImGui::SameLine();
              ImGui::Checkbox("S",  &tabs[1]); ImGui::SameLine();
              ImGui::Checkbox("SU", &tabs[2]); ImGui::SameLine();
              ImGui::Checkbox("SL", &tabs[3]); ImGui::SameLine();
              ImGui::Checkbox("?",  &tabs[4]); ImGui::SameLine();
              ImGui::Checkbox("!",  &tabs[5]);

              ImGui::TableNextRow(); ImGui::TableNextColumn();
              ImGui::Text("List"); ImGui::TableNextColumn();
              static int list = 0;
              static int list_rank = 3;
              if (ImGui::BeginTable("g_lists_internal", 2, ImGuiTableFlags_SizingPolicyFixedX)) {
                ImGui::TableNextRow(); ImGui::TableNextColumn();
                ImGui::RadioButton("Top20s",         &list, 0); ImGui::TableNextColumn();
                ImGui::RadioButton("Missing Top20s", &list, 1);
                ImGui::TableNextRow(); ImGui::TableNextColumn();
                ImGui::RadioButton("Top10s",         &list, 2); ImGui::TableNextColumn();
                ImGui::RadioButton("Missing Top10s", &list, 3);
                ImGui::TableNextRow(); ImGui::TableNextColumn();
                ImGui::RadioButton("Top5s",          &list, 4); ImGui::TableNextColumn();
                ImGui::RadioButton("Missing Top5s",  &list, 5);
                ImGui::TableNextRow(); ImGui::TableNextColumn();
                ImGui::RadioButton("0ths",           &list, 6); ImGui::TableNextColumn();
                ImGui::RadioButton("Missing 0ths",   &list, 7);
                ImGui::EndTable();
              }
              ImGui::RadioButton("Other:",         &list, 8); ImGui::SameLine();
              static int list_range_inf = 0;
              static int list_range_sup = 19;
              ImGui::Text("From "); ImGui::SameLine();
              RangeInt(&list_range_inf, 2, 0, 19, ""); ImGui::SameLine();
              ImGui::Text(" to "); ImGui::SameLine();
              RangeInt(&list_range_sup, 2, 0, 19, ""); // TODO: Disable this if ranking_rank != 8

              ImGui::TableNextRow(); ImGui::TableNextColumn();
              ImGui::Text("Ties"); ImGui::TableNextColumn();
              static int ranking_ties = 0;
              ImGui::RadioButton("Yes", &ranking_ties, 0); ImGui::SameLine();
              ImGui::RadioButton("No",  &ranking_ties, 1);

              ImGui::EndTable();
            }
            ImGui::PopStyleVar();
            const char* col_headers4[3] = { "Rank", "Player", "Score" };
            make_list("lists", col_headers4);
            ImGui::EndTabItem();
          }
          ImGui::EndTabBar();
        }
        ImGui::EndTable();
      }

      ImGui::End();
    }

    {
      /* Data */
      static bool tabs[6]     = { true, true, true, true, true, true };
      static bool states[3]   = { true, true, true };
      static bool types[3]    = { true, false, false };
      static bool modes[4]    = { true, false, false, false };
      static bool orders[7]   = { true, false, false, false, false, false, false };
      static bool rev_order   = false;
      static int block_count  = 0;
      static int scored_count = 0;
      static int ranked_count = 0;
      static int total_atts   = 0;
      static int total_vics   = 0;
      static int total_gold   = 0;
      static int total_score  = 0;
      static int total_rank   = 0;
      static float avg_atts   = -1;
      static float avg_vics   = -1;
      static float avg_gold   = -1;
      static float avg_score  = -1;
      static float avg_rank   = -1;
      const char* titles[4]   = { "Tabs", "Types", "Modes", "States" };
      const char* s_states[3] = { "Locked", "Unlocked", "Completed" };
      const char* headers[7]  = { "ID", "State", "Attempts", "Victories", "Gold", "Score", "Rank" };
      int table_lines         = 28;
      static bool reorder     = true; // Detect if checkboxes has been pressed to change filters

      /* Header */
      create_window("savefile", win2_x, win2_y, win2_w, win2_h);
      ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "SAVEFILE ANALYSIS"); ImGui::SameLine();
      ImGui::Text("Loaded:"); ImGui::SameLine();
      char buf[32];
      snprintf(buf, 32, "[%s (%u)]", profile->username, profile->id);
      ImGui::Text("%s", buf); ImGui::SameLine(ImGui::GetWindowWidth() - 30);
      HelpMarker("This section will analyze your savefile and provide stats. \
                  You first need to load it by clicking on 'Open savefile'. \
                  If you don't know where the savefile is located, click on the 'Help' menu.");
      ImGui::SmallButton("Open savefile");

      /* Checkboxes */
      ImGui::Columns(4);
      ImGui::Separator();
      for (int i = 0; i < 4; i++) {
        ImGui::Text("%s", titles[i]); ImGui::NextColumn();
      }
      ImGui::Separator();
      ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(2, 0));
      ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2, 0));
      for (int i = 0; i < 6; i++) {
                   reorder |= ImGui::Checkbox(s_tabs[i],   &tabs[i]);   ImGui::NextColumn();
        if (i < 3) reorder |= ImGui::Checkbox(s_types[i],  &types[i]);  ImGui::NextColumn();
        if (i < 4) reorder |= ImGui::Checkbox(s_modes[i],  &modes[i]);  ImGui::NextColumn();
        if (i < 3) reorder |= ImGui::Checkbox(s_states[i], &states[i]);
        if (i == 5) ImGui::Text("         Results: %d", block_count);
        ImGui::NextColumn();
      }
      reorder = true;
      ImGui::PopStyleVar(2);
      ImGui::Columns(1);
      ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Note: To obtain scores and ranks, download the scores and provide your Steam ID.");

      /* Table */
      static ImGuiTableFlags table_flags = ImGuiTableFlags_Resizable  | ImGuiTableFlags_RowBg | ImGuiTableFlags_Sortable
            | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV | ImGuiTableFlags_ScrollY;
      if (ImGui::BeginTable("blocks", 7, table_flags, ImVec2(0, ImGui::GetTextLineHeightWithSpacing() * table_lines), 0.0f)) {
        /* Create columns */
        ImGui::TableSetupColumn(headers[0], ImGuiTableColumnFlags_DefaultSort          | ImGuiTableColumnFlags_WidthStretch, -1.0f);
        ImGui::TableSetupColumn(headers[1], ImGuiTableColumnFlags_NoSort               | ImGuiTableColumnFlags_WidthStretch, -1.0f);
        ImGui::TableSetupColumn(headers[2], ImGuiTableColumnFlags_PreferSortDescending | ImGuiTableColumnFlags_WidthStretch, -1.0f);
        ImGui::TableSetupColumn(headers[3], ImGuiTableColumnFlags_PreferSortDescending | ImGuiTableColumnFlags_WidthStretch, -1.0f);
        ImGui::TableSetupColumn(headers[4], ImGuiTableColumnFlags_PreferSortDescending | ImGuiTableColumnFlags_WidthStretch, -1.0f);
        ImGui::TableSetupColumn(headers[5], ImGuiTableColumnFlags_PreferSortDescending | ImGuiTableColumnFlags_WidthStretch, -1.0f);
        ImGui::TableSetupColumn(headers[6],                                              ImGuiTableColumnFlags_WidthStretch, -1.0f);
        ImGui::TableSetupScrollFreeze(0, 1); // Header always visible after scrolling

        /* Sort data */
        if (ImGuiTableSortSpecs* sorts_specs = ImGui::TableGetSortSpecs()) {       // Try to obtain table sorting specs
          if (sorts_specs->SpecsDirty) {                                           // Detect if sorting is required
            if (sorts_specs->SpecsCount > 0) {                                     // At least one column needs sorting
              enum orders order = ID;
              const ImGuiTableSortSpecsColumn* sort_spec = &sorts_specs->Specs[0]; // Obtain sorting specs of one column
              switch(sort_spec->ColumnIndex) {                                     // Determine which column it is
                case 0:
                  order = ID;
                  break;
                case 2:
                  order = ATTEMPTS;
                  break;
                case 3:
                  order = VICTORIES;
                  break;
                case 4:
                  order = GOLD;
                  break;
                case 5:
                  order = SCORE;
                  break;
                case 6:
                  order = RANK;
                  break;
                default:
                  order = ID;
              }
              bool reverse = sort_spec->SortDirection == ImGuiSortDirection_Descending;
              blksort(blocks, bcount, order, reverse);                             // Perform the sort
            }
            sorts_specs->SpecsDirty = false;
          }
        }

        /* Display data */
        if (reorder) {
          block_count  = 0;
          scored_count = 0;
          ranked_count = 0;
          total_atts   = 0;
          total_vics   = 0;
          total_gold   = 0;
          total_score  = 0;
          total_rank   = 0;
        }
        ImGui::TableHeadersRow();
        for (int i = 0; i < bcount; i++) {
          if (types[blocks[i].tab->type] && modes[blocks[i].tab->mode] && tabs[blocks[i].tab->tab] && states[blocks[i].state]) {
            if (reorder) {
              block_count++;
              total_atts  += blocks[i].attempts;
              total_vics  += blocks[i].victories + blocks[i].victories_ep;
              total_gold  += blocks[i].gold;
              if (blocks[i].score < 1000 * MAX_SCORE) {
                scored_count++;
                total_score += blocks[i].score;
              }
              if (blocks[i].rank < 20) {
                ranked_count++;
                total_rank += blocks[i].rank;
              }
            }
            ImGui::TableNextRow(); ImGui::TableNextColumn();
            char buf[10];
            sprintf(buf, "%s", blocks[i].name);
            ImGui::Selectable(buf, false, ImGuiSelectableFlags_SpanAllColumns);
            if (ImGui::IsItemClicked()) {
              for (int j = 0; j < bcount; j++) {
                if (blocks_raw[j].tab == blocks[i].tab && blocks_raw[j].id == blocks[i].id) board_index = j;
              }
            }
            ImGui::TableNextColumn();
            switch(blocks[i].state) {
              case 0:
                ImGui::Text("Locked");
                break;
              case 1:
                ImGui::Text("Unlocked");
                break;
              case 2:
                ImGui::Text("Completed");
                break;
              default:
                ImGui::Text("Unknown");
            }
            ImGui::TableNextColumn();
            ImGui::Text("%d", blocks[i].attempts);                           ImGui::TableNextColumn();
            ImGui::Text("%d", blocks[i].victories + blocks[i].victories_ep); ImGui::TableNextColumn();
            ImGui::Text("%d", blocks[i].gold);                               ImGui::TableNextColumn();
            blocks[i].score > 1000 * MAX_SCORE ? ImGui::Text("-") : ImGui::Text("%.3f", (float) blocks[i].score / 1000); ImGui::TableNextColumn();
            blocks[i].rank > 19 ? ImGui::Text("-") : ImGui::Text("%d", blocks[i].rank);
          }
        }
        ImGui::EndTable();
      }
      if (reorder) {
        if (block_count > 0) {
          avg_atts  = (float) total_atts  / block_count;
          avg_vics  = (float) total_vics  / block_count;
          avg_gold  = (float) total_gold  / block_count;
          avg_score = scored_count > 0 ? (float) total_score / scored_count : -1;
          avg_rank  = ranked_count > 0 ? (float) total_rank  / ranked_count : -1;
        } else {
          avg_atts  = -1;
          avg_vics  = -1;
          avg_gold  = -1;
          avg_score = -1;
          avg_rank  = -1;
        }
      }

      /* Table footer */
      static ImGuiTableFlags footer_flags = ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersOuter;
      if (ImGui::BeginTable("footer", 7, footer_flags, ImVec2(0, ImGui::GetTextLineHeightWithSpacing() * 2), 0.0f)) {
        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_NoSort | ImGuiTableColumnFlags_WidthStretch, -1.0f);
        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_NoSort | ImGuiTableColumnFlags_WidthStretch, -1.0f);
        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_NoSort | ImGuiTableColumnFlags_WidthStretch, -1.0f);
        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_NoSort | ImGuiTableColumnFlags_WidthStretch, -1.0f);
        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_NoSort | ImGuiTableColumnFlags_WidthStretch, -1.0f);
        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_NoSort | ImGuiTableColumnFlags_WidthStretch, -1.0f);
        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_NoSort | ImGuiTableColumnFlags_WidthStretch, -1.0f);

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::Text("Total"); ImGui::TableNextColumn(); ImGui::TableNextColumn();
        ImGui::Text("%d", total_atts); ImGui::TableNextColumn();
        ImGui::Text("%d", total_vics); ImGui::TableNextColumn();
        ImGui::Text("%d", total_gold); ImGui::TableNextColumn();
        scored_count > 0 ? ImGui::Text("%.3f", (float) total_score / 1000) : ImGui::Text("-"); ImGui::TableNextColumn();
        ranked_count > 0 ? ImGui::Text("%d", total_rank) : ImGui::Text("-");

        ImGui::TableNextRow(); ImGui::TableNextColumn();
        ImGui::Text("Avg."); ImGui::TableNextColumn(); ImGui::TableNextColumn();
        if (block_count > 0) {
          ImGui::Text("%.3f", avg_atts);  ImGui::TableNextColumn();
          ImGui::Text("%.3f", avg_vics);  ImGui::TableNextColumn();
          ImGui::Text("%.3f", avg_gold);  ImGui::TableNextColumn();
          avg_score >= 0 ? ImGui::Text("%.3f", avg_score / 1000) : ImGui::Text("-"); ImGui::TableNextColumn();
          avg_rank >= 0 ? ImGui::Text("%.3f", avg_rank) : ImGui::Text("-");
        } else {
          for (int i = 0; i < 5; i++) {
            ImGui::Text("-");
            if (i < 4) ImGui::TableNextColumn();
          }
        }

        ImGui::EndTable();
      }

      reorder = false;
      ImGui::End();
    }

    {
      create_window("footer", win3_x, win3_y, win3_w, win3_h);
      ImGui::Text("%s v%u.%u.%u - %s, %s.", NAME, (unsigned int) MAJOR, (unsigned int) MINOR, (unsigned int) PATCH, AUTHOR, DATE); ImGui::SameLine();
      ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
      ImGui::End();
    }

    /* End frame */
    cflag(dflags, DownloadFlags_Refresh);

    /* Rendering */
    ImGui::Render();
    int display_w, display_h;
    glfwGetFramebufferSize(window, &display_w, &display_h);
    glViewport(0, 0, display_w, display_h);
    glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    glfwSwapBuffers(window);
  }

  /* Free memory */
  free(currdate);
  curldestroy(curl);
  free(curl);
  free(scores);
  free(tabs);
  playerdealloc(&players, pcount);
  blockdealloc(&blocks, bcount);
  free(profile);

  /* Cleanup */
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();

  glfwDestroyWindow(window);
  glfwTerminate();

  return 0;
}
