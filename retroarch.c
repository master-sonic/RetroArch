/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2015 - Daniel De Matteis
 *  Copyright (C) 2012-2015 - Michael Lelli
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <boolean.h>
#include "libretro_version_1.h"
#include "dynamic.h"
#include "content.h"
#include "file_ops.h"
#include <file/file_path.h>
#include <file/dir_list.h>
#include "general.h"
#include "retroarch.h"
#include "settings.h"
#include <compat/strl.h>
#include "screenshot.h"
#include "performance.h"
#include "cheats.h"
#include <compat/getopt.h>
#include <compat/posix_string.h>

#include "input/keyboard_line.h"
#include "input/input_remapping.h"

#include "record/record_driver.h"

#include "git_version.h"
#include "intl/intl.h"

#ifdef HAVE_MENU
#include "menu/menu.h"
#include "menu/menu_shader.h"
#include "menu/menu_input.h"
#endif

#ifdef HAVE_NETWORKING
#include "net_compat.h"
#include "net_http.h"
#endif

#ifdef HAVE_NETPLAY
#include "netplay.h"
#endif

#ifdef _WIN32
#ifdef _XBOX
#include <xtl.h>
#else
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
#endif

/**
 * rarch_render_cached_frame:
 *
 * Renders the current video frame.
 **/
void rarch_render_cached_frame(void)
{
   void *recording   = driver.recording_data;

   /* Cannot allow recording when pushing duped frames. */
   driver.recording_data = NULL;

   /* Not 100% safe, since the library might have
    * freed the memory, but no known implementations do this.
    * It would be really stupid at any rate ...
    */
   if (driver.retro_ctx.frame_cb)
      driver.retro_ctx.frame_cb(
            (g_extern.frame_cache.data == RETRO_HW_FRAME_BUFFER_VALID)
            ? NULL : g_extern.frame_cache.data,
            g_extern.frame_cache.width,
            g_extern.frame_cache.height,
            g_extern.frame_cache.pitch);

   driver.recording_data = recording;
}

#include "config.features.h"

#define _PSUPP(var, name, desc) printf("\t%s:\n\t\t%s: %s\n", name, desc, _##var##_supp ? "yes" : "no")
static void print_features(void)
{
   puts("");
   puts("Features:");
   _PSUPP(sdl, "SDL", "SDL drivers");
   _PSUPP(sdl2, "SDL2", "SDL2 drivers");
   _PSUPP(x11, "X11", "X11 drivers");
   _PSUPP(wayland, "wayland", "Wayland drivers");
   _PSUPP(thread, "Threads", "Threading support");
   _PSUPP(opengl, "OpenGL", "OpenGL driver");
   _PSUPP(kms, "KMS", "KMS/EGL context support");
   _PSUPP(udev, "UDEV", "UDEV/EVDEV input driver support");
   _PSUPP(egl, "EGL", "EGL context support");
   _PSUPP(vg, "OpenVG", "OpenVG output support");
   _PSUPP(xvideo, "XVideo", "XVideo output");
   _PSUPP(alsa, "ALSA", "audio driver");
   _PSUPP(oss, "OSS", "audio driver");
   _PSUPP(jack, "Jack", "audio driver");
   _PSUPP(rsound, "RSound", "audio driver");
   _PSUPP(roar, "RoarAudio", "audio driver");
   _PSUPP(pulse, "PulseAudio", "audio driver");
   _PSUPP(dsound, "DirectSound", "audio driver");
   _PSUPP(xaudio, "XAudio2", "audio driver");
   _PSUPP(zlib, "zlib", "PNG encode/decode and .zip extraction");
   _PSUPP(al, "OpenAL", "audio driver");
   _PSUPP(dylib, "External", "External filter and plugin support");
   _PSUPP(cg, "Cg", "Cg pixel shaders");
   _PSUPP(libxml2, "libxml2", "libxml2 XML parsing");
   _PSUPP(sdl_image, "SDL_image", "SDL_image image loading");
   _PSUPP(fbo, "FBO", "OpenGL render-to-texture (multi-pass shaders)");
   _PSUPP(dynamic, "Dynamic", "Dynamic run-time loading of libretro library");
   _PSUPP(ffmpeg, "FFmpeg", "On-the-fly recording of gameplay with libavcodec");
   _PSUPP(freetype, "FreeType", "TTF font rendering with FreeType");
   _PSUPP(netplay, "Netplay", "Peer-to-peer netplay");
   _PSUPP(python, "Python", "Script support in shaders");
}
#undef _PSUPP

/**
 * print_compiler:
 *
 * Prints compiler that was used for compiling RetroArch.
 **/
static void print_compiler(FILE *file)
{
   fprintf(file, "\nCompiler: ");
#if defined(_MSC_VER)
   fprintf(file, "MSVC (%d) %u-bit\n", _MSC_VER, (unsigned)
         (CHAR_BIT * sizeof(size_t)));
#elif defined(__SNC__)
   fprintf(file, "SNC (%d) %u-bit\n",
      __SN_VER__, (unsigned)(CHAR_BIT * sizeof(size_t)));
#elif defined(_WIN32) && defined(__GNUC__)
   fprintf(file, "MinGW (%d.%d.%d) %u-bit\n",
      __GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__, (unsigned)
      (CHAR_BIT * sizeof(size_t)));
#elif defined(__clang__)
   fprintf(file, "Clang/LLVM (%s) %u-bit\n",
      __clang_version__, (unsigned)(CHAR_BIT * sizeof(size_t)));
#elif defined(__GNUC__)
   fprintf(file, "GCC (%d.%d.%d) %u-bit\n",
      __GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__, (unsigned)
      (CHAR_BIT * sizeof(size_t)));
#else
   fprintf(file, "Unknown compiler %u-bit\n",
      (unsigned)(CHAR_BIT * sizeof(size_t)));
#endif
   fprintf(file, "Built: %s\n", __DATE__);
}

/**
 * print_help:
 *
 * Prints help message explaining RetroArch's commandline switches.
 **/
static void print_help(void)
{
   puts("===================================================================");
#ifdef HAVE_GIT_VERSION
   printf(RETRO_FRONTEND ": Frontend for libretro -- v" PACKAGE_VERSION " -- %s --\n", rarch_git_version);
#else
   puts(RETRO_FRONTEND ": Frontend for libretro -- v" PACKAGE_VERSION " --");
#endif
   print_compiler(stdout);
   puts("===================================================================");
   puts("Usage: retroarch [content file] [options...]");
   puts("\t-h/--help: Show this help message.");
   puts("\t--menu: Do not require content or libretro core to be loaded, starts directly in menu.");
   puts("\t\tIf no arguments are passed to " RETRO_FRONTEND ", it is equivalent to using --menu as only argument.");
   puts("\t--features: Prints available features compiled into " RETRO_FRONTEND ".");
   puts("\t-s/--save: Path for save file (*.srm).");
   puts("\t-f/--fullscreen: Start " RETRO_FRONTEND " in fullscreen regardless of config settings.");
   puts("\t-S/--savestate: Path to use for save states. If not selected, *.state will be assumed.");
   puts("\t-c/--config: Path for config file." RARCH_DEFAULT_CONF_PATH_STR);
   puts("\t--appendconfig: Extra config files are loaded in, and take priority over config selected in -c (or default).");
   puts("\t\tMultiple configs are delimited by ','.");
#ifdef HAVE_DYNAMIC
   puts("\t-L/--libretro: Path to libretro implementation. Overrides any config setting.");
#endif
   puts("\t--subsystem: Use a subsystem of the libretro core. Multiple content files are loaded as multiple arguments.");
   puts("\t\tIf a content file is skipped, use a blank (\"\") command line argument");
   puts("\t\tContent must be loaded in an order which depends on the particular subsystem used.");
   puts("\t\tSee verbose log output to learn how a particular subsystem wants content to be loaded.");

   printf("\t-N/--nodevice: Disconnects controller device connected to port (1 to %d).\n", MAX_USERS);
   printf("\t-A/--dualanalog: Connect a DualAnalog controller to port (1 to %d).\n", MAX_USERS);
   printf("\t-d/--device: Connect a generic device into port of the device (1 to %d).\n", MAX_USERS);
   puts("\t\tFormat is port:ID, where ID is an unsigned number corresponding to the particular device.\n");

   puts("\t-P/--bsvplay: Playback a BSV movie file.");
   puts("\t-R/--bsvrecord: Start recording a BSV movie file from the beginning.");
   puts("\t--eof-exit: Exit upon reaching the end of the BSV movie file.");
   puts("\t-M/--sram-mode: Takes an argument telling how SRAM should be handled in the session.");
   puts("\t\t{no,}load-{no,}save describes if SRAM should be loaded, and if SRAM should be saved.");
   puts("\t\tDo note that noload-save implies that save files will be deleted and overwritten.");

#ifdef HAVE_NETPLAY
   puts("\t-H/--host: Host netplay as user 1.");
   puts("\t-C/--connect: Connect to netplay as user 2.");
   puts("\t--port: Port used to netplay. Default is 55435.");
   puts("\t-F/--frames: Sync frames when using netplay.");
   puts("\t--spectate: Netplay will become spectating mode.");
   puts("\t\tHost can live stream the game content to users that connect.");
   puts("\t\tHowever, the client will not be able to play. Multiple clients can connect to the host.");
#endif
   puts("\t--nick: Picks a username (for use with netplay). Not mandatory.");
#if defined(HAVE_NETWORK_CMD) && defined(HAVE_NETPLAY)
   puts("\t--command: Sends a command over UDP to an already running " RETRO_FRONTEND " process.");
   puts("\t\tAvailable commands are listed if command is invalid.");
#endif

   puts("\t-r/--record: Path to record video file.\n\t\tUsing .mkv extension is recommended.");
   puts("\t--recordconfig: Path to settings used during recording.");
   puts("\t--size: Overrides output video size when recording (format: WIDTHxHEIGHT).");
   puts("\t-v/--verbose: Verbose logging.");
   puts("\t-U/--ups: Specifies path for UPS patch that will be applied to content.");
   puts("\t--bps: Specifies path for BPS patch that will be applied to content.");
   puts("\t--ips: Specifies path for IPS patch that will be applied to content.");
   puts("\t--no-patch: Disables all forms of content patching.");
   puts("\t-D/--detach: Detach " RETRO_FRONTEND " from the running console. Not relevant for all platforms.");
   puts("\t--max-frames: Runs for the specified number of frames, then exits.\n");
}

static void set_basename(const char *path)
{
   char *dst = NULL;

   strlcpy(g_extern.fullpath, path, sizeof(g_extern.fullpath));
   strlcpy(g_extern.basename, path, sizeof(g_extern.basename));

#ifdef HAVE_COMPRESSION
   /* Removing extension is a bit tricky for compressed files.
    * Basename means:
    * /file/to/path/game.extension should be:
    * /file/to/path/game
    *
    * Two things to consider here are: /file/to/path/ is expected
    * to be a directory and "game" is a single file. This is used for
    * states and srm default paths.
    *
    * For compressed files we have:
    *
    * /file/to/path/comp.7z#game.extension and
    * /file/to/path/comp.7z#folder/game.extension
    *
    * The choice I take here is:
    * /file/to/path/game as basename. We might end up in a writable 
    * directory then and the name of srm and states are meaningful.
    *
    */
   path_basedir(g_extern.basename);
   fill_pathname_dir(g_extern.basename, path, "", sizeof(g_extern.basename));
#endif

   if ((dst = strrchr(g_extern.basename, '.')))
      *dst = '\0';
}

static void set_special_paths(char **argv, unsigned num_content)
{
   unsigned i;
   union string_list_elem_attr attr;

   /* First content file is the significant one. */
   set_basename(argv[0]);

   g_extern.subsystem_fullpaths = string_list_new();
   rarch_assert(g_extern.subsystem_fullpaths);

   attr.i = 0;

   for (i = 0; i < num_content; i++)
      string_list_append(g_extern.subsystem_fullpaths, argv[i], attr);

   /* We defer SRAM path updates until we can resolve it.
    * It is more complicated for special content types. */

   if (!g_extern.has_set_state_path)
      fill_pathname_noext(g_extern.savestate_name, g_extern.basename,
            ".state", sizeof(g_extern.savestate_name));

   if (path_is_directory(g_extern.savestate_name))
   {
      fill_pathname_dir(g_extern.savestate_name, g_extern.basename,
            ".state", sizeof(g_extern.savestate_name));
      RARCH_LOG("Redirecting save state to \"%s\".\n",
            g_extern.savestate_name);
   }

   /* If this is already set,
    * do not overwrite it as this was initialized before in
    * a menu or otherwise. */
   if (!*g_settings.system_directory)
      fill_pathname_basedir(g_settings.system_directory, argv[0],
            sizeof(g_settings.system_directory));
}

static void set_paths_redirect(const char *path)
{
   if (path_is_directory(g_extern.savefile_name))
   {
      fill_pathname_dir(g_extern.savefile_name, g_extern.basename,
            ".srm", sizeof(g_extern.savefile_name));
      RARCH_LOG("Redirecting save file to \"%s\".\n", g_extern.savefile_name);
   }

   if (path_is_directory(g_extern.savestate_name))
   {
      fill_pathname_dir(g_extern.savestate_name, g_extern.basename,
            ".state", sizeof(g_extern.savestate_name));
      RARCH_LOG("Redirecting save state to \"%s\".\n", g_extern.savestate_name);
   }

   if (path_is_directory(g_extern.cheatfile_name))
   {
      fill_pathname_dir(g_extern.cheatfile_name, g_extern.basename,
            ".state", sizeof(g_extern.cheatfile_name));
      RARCH_LOG("Redirecting cheat file to \"%s\".\n", g_extern.cheatfile_name);
   }
}

static void set_paths(const char *path)
{
   set_basename(path);

   if (!g_extern.has_set_save_path)
      fill_pathname_noext(g_extern.savefile_name, g_extern.basename,
            ".srm", sizeof(g_extern.savefile_name));
   if (!g_extern.has_set_state_path)
      fill_pathname_noext(g_extern.savestate_name, g_extern.basename,
            ".state", sizeof(g_extern.savestate_name));
   fill_pathname_noext(g_extern.cheatfile_name, g_extern.basename,
         ".cht", sizeof(g_extern.cheatfile_name));

   set_paths_redirect(path);

   /* If this is already set, do not overwrite it
    * as this was initialized before in a menu or otherwise. */
   if (*g_settings.system_directory)
      return;

   fill_pathname_basedir(g_settings.system_directory, path,
         sizeof(g_settings.system_directory));
}

/**
 * parse_input:
 * @argc                 : Count of (commandline) arguments.
 * @argv                 : (Commandline) arguments. 
 *
 * Parses (commandline) arguments passed to RetroArch.
 *
 **/
static void parse_input(int argc, char *argv[])
{
   g_extern.libretro_no_content = false;
   g_extern.libretro_dummy = false;
   g_extern.has_set_save_path = false;
   g_extern.has_set_state_path = false;
   g_extern.has_set_libretro = false;
   g_extern.has_set_libretro_directory = false;
   g_extern.has_set_verbosity = false;

   g_extern.has_set_netplay_mode = false;
   g_extern.has_set_username = false;
   g_extern.has_set_netplay_ip_address = false;
   g_extern.has_set_netplay_delay_frames = false;
   g_extern.has_set_netplay_ip_port = false;

   g_extern.has_set_ups_pref = false;
   g_extern.has_set_bps_pref = false;
   g_extern.has_set_ips_pref = false;

   g_extern.ups_pref = false;
   g_extern.bps_pref = false;
   g_extern.ips_pref = false;
   *g_extern.ups_name = '\0';
   *g_extern.bps_name = '\0';
   *g_extern.ips_name = '\0';

   *g_extern.subsystem = '\0';

   if (argc < 2)
   {
      g_extern.libretro_dummy = true;
      return;
   }

   /* Make sure we can call parse_input several times ... */
   optind = 0;

   int val = 0;

   const struct option opts[] = {
#ifdef HAVE_DYNAMIC
      { "libretro", 1, NULL, 'L' },
#endif
      { "menu", 0, &val, 'M' },
      { "help", 0, NULL, 'h' },
      { "save", 1, NULL, 's' },
      { "fullscreen", 0, NULL, 'f' },
      { "record", 1, NULL, 'r' },
      { "recordconfig", 1, &val, 'R' },
      { "size", 1, &val, 's' },
      { "verbose", 0, NULL, 'v' },
      { "config", 1, NULL, 'c' },
      { "appendconfig", 1, &val, 'C' },
      { "nodevice", 1, NULL, 'N' },
      { "dualanalog", 1, NULL, 'A' },
      { "device", 1, NULL, 'd' },
      { "savestate", 1, NULL, 'S' },
      { "bsvplay", 1, NULL, 'P' },
      { "bsvrecord", 1, NULL, 'R' },
      { "sram-mode", 1, NULL, 'M' },
#ifdef HAVE_NETPLAY
      { "host", 0, NULL, 'H' },
      { "connect", 1, NULL, 'C' },
      { "frames", 1, NULL, 'F' },
      { "port", 1, &val, 'p' },
      { "spectate", 0, &val, 'S' },
#endif
      { "nick", 1, &val, 'N' },
#if defined(HAVE_NETWORK_CMD) && defined(HAVE_NETPLAY)
      { "command", 1, &val, 'c' },
#endif
      { "ups", 1, NULL, 'U' },
      { "bps", 1, &val, 'B' },
      { "ips", 1, &val, 'I' },
      { "no-patch", 0, &val, 'n' },
      { "detach", 0, NULL, 'D' },
      { "features", 0, &val, 'f' },
      { "subsystem", 1, NULL, 'Z' },
      { "max-frames", 1, NULL, 'm' },
      { "eof-exit", 0, &val, 'e' },
      { NULL, 0, NULL, 0 }
   };

#define FFMPEG_RECORD_ARG "r:"

#ifdef HAVE_DYNAMIC
#define DYNAMIC_ARG "L:"
#else
#define DYNAMIC_ARG
#endif

#ifdef HAVE_NETPLAY
#define NETPLAY_ARG "HC:F:"
#else
#define NETPLAY_ARG
#endif


#define BSV_MOVIE_ARG "P:R:M:"

   const char *optstring = "hs:fvS:A:c:U:DN:d:" BSV_MOVIE_ARG NETPLAY_ARG DYNAMIC_ARG FFMPEG_RECORD_ARG;

   for (;;)
   {
      val = 0;
      int c = getopt_long(argc, argv, optstring, opts, NULL);
      int port;

      if (c == -1)
         break;

      switch (c)
      {
         case 'h':
            print_help();
            exit(0);

         case 'Z':
            strlcpy(g_extern.subsystem, optarg, sizeof(g_extern.subsystem));
            break;

         case 'd':
         {
            unsigned id = 0;
            port = 0;
            struct string_list *list = string_split(optarg, ":");
            if (list && list->size == 2)
            {
               port = strtol(list->elems[0].data, NULL, 0);
               id = strtoul(list->elems[1].data, NULL, 0);
            }
            string_list_free(list);

            if (port < 1 || port > MAX_USERS)
            {
               RARCH_ERR("Connect device to a valid port.\n");
               print_help();
               rarch_fail(1, "parse_input()");
            }
            g_settings.input.libretro_device[port - 1] = id;
            g_extern.has_set_libretro_device[port - 1] = true;
            break;
         }

         case 'A':
            port = strtol(optarg, NULL, 0);
            if (port < 1 || port > MAX_USERS)
            {
               RARCH_ERR("Connect dualanalog to a valid port.\n");
               print_help();
               rarch_fail(1, "parse_input()");
            }
            g_settings.input.libretro_device[port - 1] = RETRO_DEVICE_ANALOG;
            g_extern.has_set_libretro_device[port - 1] = true;
            break;

         case 's':
            strlcpy(g_extern.savefile_name, optarg,
                  sizeof(g_extern.savefile_name));
            g_extern.has_set_save_path = true;
            break;

         case 'f':
            g_extern.force_fullscreen = true;
            break;

         case 'S':
            strlcpy(g_extern.savestate_name, optarg,
                  sizeof(g_extern.savestate_name));
            g_extern.has_set_state_path = true;
            break;

         case 'v':
            g_extern.verbosity = true;
            g_extern.has_set_verbosity = true;
            break;

         case 'N':
            port = strtol(optarg, NULL, 0);
            if (port < 1 || port > MAX_USERS)
            {
               RARCH_ERR("Disconnect device from a valid port.\n");
               print_help();
               rarch_fail(1, "parse_input()");
            }
            g_settings.input.libretro_device[port - 1] = RETRO_DEVICE_NONE;
            g_extern.has_set_libretro_device[port - 1] = true;
            break;

         case 'c':
            strlcpy(g_extern.config_path, optarg,
                  sizeof(g_extern.config_path));
            break;

         case 'r':
            strlcpy(g_extern.record_path, optarg,
                  sizeof(g_extern.record_path));
            g_extern.recording_enable = true;
            break;

#ifdef HAVE_DYNAMIC
         case 'L':
            if (path_is_directory(optarg))
            {
               *g_settings.libretro = '\0';
               strlcpy(g_settings.libretro_directory, optarg,
                     sizeof(g_settings.libretro_directory));
               g_extern.has_set_libretro = true;
               g_extern.has_set_libretro_directory = true;
               RARCH_WARN("Using old --libretro behavior. Setting libretro_directory to \"%s\" instead.\n", optarg);
            }
            else
            {
               strlcpy(g_settings.libretro, optarg,
                     sizeof(g_settings.libretro));
               g_extern.has_set_libretro = true;
            }
            break;
#endif
         case 'P':
         case 'R':
            strlcpy(g_extern.bsv.movie_start_path, optarg,
                  sizeof(g_extern.bsv.movie_start_path));
            g_extern.bsv.movie_start_playback  = (c == 'P');
            g_extern.bsv.movie_start_recording = (c == 'R');
            break;

         case 'M':
            if (strcmp(optarg, "noload-nosave") == 0)
            {
               g_extern.sram_load_disable = true;
               g_extern.sram_save_disable = true;
            }
            else if (strcmp(optarg, "noload-save") == 0)
               g_extern.sram_load_disable = true;
            else if (strcmp(optarg, "load-nosave") == 0)
               g_extern.sram_save_disable = true;
            else if (strcmp(optarg, "load-save") != 0)
            {
               RARCH_ERR("Invalid argument in --sram-mode.\n");
               print_help();
               rarch_fail(1, "parse_input()");
            }
            break;

#ifdef HAVE_NETPLAY
         case 'H':
            g_extern.has_set_netplay_ip_address = true;
            g_extern.netplay_enable = true;
            *g_extern.netplay_server = '\0';
            break;

         case 'C':
            g_extern.has_set_netplay_ip_address = true;
            g_extern.netplay_enable = true;
            strlcpy(g_extern.netplay_server, optarg,
                  sizeof(g_extern.netplay_server));
            break;

         case 'F':
            g_extern.netplay_sync_frames = strtol(optarg, NULL, 0);
            g_extern.has_set_netplay_delay_frames = true;
            break;
#endif

         case 'U':
            strlcpy(g_extern.ups_name, optarg,
                  sizeof(g_extern.ups_name));
            g_extern.ups_pref = true;
            g_extern.has_set_ups_pref = true;
            break;

         case 'D':
#if defined(_WIN32) && !defined(_XBOX)
            FreeConsole();
#endif
            break;

         case 'm':
            g_extern.max_frames = strtoul(optarg, NULL, 10);
            break;

         case 0:
            switch (val)
            {
               case 'M':
                  g_extern.libretro_dummy = true;
                  break;

#ifdef HAVE_NETPLAY
               case 'p':
                  g_extern.has_set_netplay_ip_port = true;
                  g_extern.netplay_port = strtoul(optarg, NULL, 0);
                  break;

               case 'S':
                  g_extern.has_set_netplay_mode = true;
                  g_extern.netplay_is_spectate = true;
                  break;

#endif
               case 'N':
                  g_extern.has_set_username = true;
                  strlcpy(g_settings.username, optarg,
                        sizeof(g_settings.username));
                  break;

#if defined(HAVE_NETWORK_CMD) && defined(HAVE_NETPLAY)
               case 'c':
                  if (network_cmd_send(optarg))
                     exit(0);
                  else
                     rarch_fail(1, "network_cmd_send()");
                  break;
#endif

               case 'C':
                  strlcpy(g_extern.append_config_path, optarg,
                        sizeof(g_extern.append_config_path));
                  break;

               case 'B':
                  strlcpy(g_extern.bps_name, optarg,
                        sizeof(g_extern.bps_name));
                  g_extern.bps_pref = true;
                  g_extern.has_set_bps_pref = true;
                  break;

               case 'I':
                  strlcpy(g_extern.ips_name, optarg,
                        sizeof(g_extern.ips_name));
                  g_extern.ips_pref = true;
                  g_extern.has_set_ips_pref = true;
                  break;

               case 'n':
                  g_extern.block_patch = true;
                  break;

               case 's':
               {
                  if (sscanf(optarg, "%ux%u", &g_extern.record_width,
                           &g_extern.record_height) != 2)
                  {
                     RARCH_ERR("Wrong format for --size.\n");
                     print_help();
                     rarch_fail(1, "parse_input()");
                  }
                  break;
               }

               case 'R':
                  strlcpy(g_extern.record_config, optarg,
                        sizeof(g_extern.record_config));
                  break;
               case 'f':
                  print_features();
                  exit(0);

               case 'e':
                  g_extern.bsv.eof_exit = true;
                  break;

               default:
                  break;
            }
            break;

         case '?':
            print_help();
            rarch_fail(1, "parse_input()");

         default:
            RARCH_ERR("Error parsing arguments.\n");
            rarch_fail(1, "parse_input()");
      }
   }

   if (g_extern.libretro_dummy)
   {
      if (optind < argc)
      {
         RARCH_ERR("--menu was used, but content file was passed as well.\n");
         rarch_fail(1, "parse_input()");
      }
   }
   else if (!*g_extern.subsystem && optind < argc)
      set_paths(argv[optind]);
   else if (*g_extern.subsystem && optind < argc)
      set_special_paths(argv + optind, argc - optind);
   else
      g_extern.libretro_no_content = true;

   /* Copy SRM/state dirs used, so they can be reused on reentrancy. */
   if (g_extern.has_set_save_path &&
         path_is_directory(g_extern.savefile_name))
      strlcpy(g_extern.savefile_dir, g_extern.savefile_name,
            sizeof(g_extern.savefile_dir));
   if (g_extern.has_set_state_path &&
         path_is_directory(g_extern.savestate_name))
      strlcpy(g_extern.savestate_dir, g_extern.savestate_name,
            sizeof(g_extern.savestate_dir));
}

/**
 * init_controllers:
 *
 * Initialize libretro controllers.
 **/
static void init_controllers(void)
{
   unsigned i;

   for (i = 0; i < MAX_USERS; i++)
   {
      unsigned device = g_settings.input.libretro_device[i];
      const struct retro_controller_description *desc = NULL;
      const char *ident = NULL;

      if (i < g_extern.system.num_ports)
         desc = libretro_find_controller_description(
               &g_extern.system.ports[i], device);

      if (desc)
         ident = desc->desc;

      if (!ident)
      {
         /* If we're trying to connect a completely unknown device,
          * revert back to JOYPAD. */
         
         if (device != RETRO_DEVICE_JOYPAD && device != RETRO_DEVICE_NONE)
         {
            /* Do not fix g_settings.input.libretro_device[i],
             * because any use of dummy core will reset this,
             * which is not a good idea. */
            RARCH_WARN("Input device ID %u is unknown to this libretro implementation. Using RETRO_DEVICE_JOYPAD.\n", device);
            device = RETRO_DEVICE_JOYPAD;
         }
         ident = "Joypad";
      }

      switch (device)
      {
         case RETRO_DEVICE_NONE:
            RARCH_LOG("Disconnecting device from port %u.\n", i + 1);
            pretro_set_controller_port_device(i, device);
            break;
         case RETRO_DEVICE_JOYPAD:
            break;
         default:
            /* Some cores do not properly range check port argument.
             * This is broken behavior of course, but avoid breaking
             * cores needlessly. */
            RARCH_LOG("Connecting %s (ID: %u) to port %u.\n", ident,
                  device, i + 1);
            pretro_set_controller_port_device(i, device);
            break;
      }
   }
}

static bool load_save_files(void)
{
   unsigned i;

   if (!g_extern.savefiles || g_extern.sram_load_disable)
      return false;

   for (i = 0; i < g_extern.savefiles->size; i++)
      load_ram_file(g_extern.savefiles->elems[i].data,
            g_extern.savefiles->elems[i].attr.i);
    
   return true;
}

static void save_files(void)
{
   unsigned i;

   if (!g_extern.savefiles || !g_extern.use_sram)
      return;

   for (i = 0; i < g_extern.savefiles->size; i++)
   {
      unsigned type    = g_extern.savefiles->elems[i].attr.i;
      const char *path = g_extern.savefiles->elems[i].data;
      RARCH_LOG("Saving RAM type #%u to \"%s\".\n", type, path);
      save_ram_file(path, type);
   }
}

static void init_remapping(void)
{
   if (!g_settings.input.remap_binds_enable)
      return;

   input_remapping_load_file(g_settings.input.remapping_path);
}

static void init_cheats(void)
{
   bool allow_cheats = true;
#ifdef HAVE_NETPLAY
   allow_cheats &= !driver.netplay_data;
#endif
   allow_cheats &= !g_extern.bsv.movie;

   if (!allow_cheats)
      return;

   /* TODO/FIXME - add some stuff here. */
}

static void init_rewind(void)
{
   void *state = NULL;
#ifdef HAVE_NETPLAY
   if (driver.netplay_data)
      return;
#endif

   if (!g_settings.rewind_enable || g_extern.rewind.state)
      return;

   if (g_extern.system.audio_callback.callback)
   {
      RARCH_ERR(RETRO_LOG_REWIND_INIT_FAILED_THREADED_AUDIO);
      return;
   }

   g_extern.rewind.size = pretro_serialize_size();

   if (!g_extern.rewind.size)
   {
      RARCH_ERR(RETRO_LOG_REWIND_INIT_FAILED_NO_SAVESTATES);
      return;
   }

   RARCH_LOG(RETRO_MSG_REWIND_INIT "%u MB\n",
         (unsigned)(g_settings.rewind_buffer_size / 1000000));

   g_extern.rewind.state = state_manager_new(g_extern.rewind.size,
         g_settings.rewind_buffer_size);

   if (!g_extern.rewind.state)
      RARCH_WARN(RETRO_LOG_REWIND_INIT_FAILED);

   state_manager_push_where(g_extern.rewind.state, &state);
   pretro_serialize(state, g_extern.rewind.size);
   state_manager_push_do(g_extern.rewind.state);
}

static void init_movie(void)
{
   if (g_extern.bsv.movie_start_playback)
   {
      if (!(g_extern.bsv.movie = bsv_movie_init(g_extern.bsv.movie_start_path,
                  RARCH_MOVIE_PLAYBACK)))
      {
         RARCH_ERR("Failed to load movie file: \"%s\".\n",
               g_extern.bsv.movie_start_path);
         rarch_fail(1, "init_movie()");
      }

      g_extern.bsv.movie_playback = true;
      msg_queue_push(g_extern.msg_queue, "Starting movie playback.", 2, 180);
      RARCH_LOG("Starting movie playback.\n");
      g_settings.rewind_granularity = 1;
   }
   else if (g_extern.bsv.movie_start_recording)
   {
      char msg[PATH_MAX_LENGTH];
      snprintf(msg, sizeof(msg), "Starting movie record to \"%s\".",
            g_extern.bsv.movie_start_path);

      msg_queue_clear(g_extern.msg_queue);

      if (!(g_extern.bsv.movie = bsv_movie_init(g_extern.bsv.movie_start_path,
                  RARCH_MOVIE_RECORD)))
      {
         msg_queue_push(g_extern.msg_queue, "Failed to start movie record.", 1, 180);
         RARCH_ERR("Failed to start movie record.\n");
         return;
      }

      msg_queue_push(g_extern.msg_queue, msg, 1, 180);
      RARCH_LOG("Starting movie record to \"%s\".\n",
            g_extern.bsv.movie_start_path);
      g_settings.rewind_granularity = 1;
   }
}

#define RARCH_DEFAULT_PORT 55435

#ifdef HAVE_NETPLAY
/**
 * init_netplay:
 *
 * Initializes netplay.
 *
 * If netplay is already initialized, will return false (0).
 *
 * Returns: true (1) if successful, otherwise false (0).
 **/

static bool init_netplay(void)
{
   struct retro_callbacks cbs = {0};

   if (!g_extern.netplay_enable)
      return false;

   if (g_extern.bsv.movie_start_playback)
   {
      RARCH_WARN(RETRO_LOG_MOVIE_STARTED_INIT_NETPLAY_FAILED);
      return false;
   }

   retro_set_default_callbacks(&cbs);

   if (*g_extern.netplay_server)
   {
      RARCH_LOG("Connecting to netplay host...\n");
      g_extern.netplay_is_client = true;
   }
   else
      RARCH_LOG("Waiting for client...\n");

   driver.netplay_data = (netplay_t*)netplay_new(
         g_extern.netplay_is_client ? g_extern.netplay_server : NULL,
         g_extern.netplay_port ? g_extern.netplay_port : RARCH_DEFAULT_PORT,
         g_extern.netplay_sync_frames, &cbs, g_extern.netplay_is_spectate,
         g_settings.username);

   if (driver.netplay_data)
      return true;

   g_extern.netplay_is_client = false;
   RARCH_WARN(RETRO_LOG_INIT_NETPLAY_FAILED);

   if (g_extern.msg_queue)
      msg_queue_push(g_extern.msg_queue,
            RETRO_MSG_INIT_NETPLAY_FAILED,
            0, 180);
   return false;
}
#endif

#ifdef HAVE_COMMAND
static void init_command(void)
{
   if (!g_settings.stdin_cmd_enable && !g_settings.network_cmd_enable)
      return;

   if (g_settings.stdin_cmd_enable && driver.stdin_claimed)
   {
      RARCH_WARN("stdin command interface is desired, but input driver has already claimed stdin.\n"
            "Cannot use this command interface.\n");
   }

   if (!(driver.command = rarch_cmd_new(g_settings.stdin_cmd_enable
               && !driver.stdin_claimed,
               g_settings.network_cmd_enable, g_settings.network_cmd_port)))
      RARCH_ERR("Failed to initialize command interface.\n");
}
#endif

#if defined(HAVE_THREADS)
static void init_autosave(void)
{
   unsigned i;

   if (g_settings.autosave_interval < 1 || !g_extern.savefiles)
      return;

   if (!(g_extern.autosave = (autosave_t**)calloc(g_extern.savefiles->size,
               sizeof(*g_extern.autosave))))
      return;

   g_extern.num_autosave = g_extern.savefiles->size;

   for (i = 0; i < g_extern.savefiles->size; i++)
   {
      const char *path = g_extern.savefiles->elems[i].data;
      unsigned    type = g_extern.savefiles->elems[i].attr.i;

      if (pretro_get_memory_size(type) <= 0)
         continue;

      g_extern.autosave[i] = autosave_new(path,
            pretro_get_memory_data(type),
            pretro_get_memory_size(type),
            g_settings.autosave_interval);
      if (!g_extern.autosave[i])
         RARCH_WARN(RETRO_LOG_INIT_AUTOSAVE_FAILED);
   }
}

static void deinit_autosave(void)
{
   unsigned i;

   for (i = 0; i < g_extern.num_autosave; i++)
      autosave_free(g_extern.autosave[i]);

   if (g_extern.autosave)
      free(g_extern.autosave);
   g_extern.autosave = NULL;

   g_extern.num_autosave = 0;
}
#endif

static void set_savestate_auto_index(void)
{
   char state_dir[PATH_MAX_LENGTH], state_base[PATH_MAX_LENGTH];
   size_t i;
   struct string_list *dir_list = NULL;
   unsigned max_idx = 0;

   if (!g_settings.savestate_auto_index)
      return;

   /* Find the file in the same directory as g_extern.savestate_name
    * with the largest numeral suffix.
    *
    * E.g. /foo/path/content.state, will try to find
    * /foo/path/content.state%d, where %d is the largest number available.
    */

   fill_pathname_basedir(state_dir, g_extern.savestate_name,
         sizeof(state_dir));
   fill_pathname_base(state_base, g_extern.savestate_name,
         sizeof(state_base));

   if (!(dir_list = dir_list_new(state_dir, NULL, false)))
      return;

   for (i = 0; i < dir_list->size; i++)
   {
      unsigned idx;
      char elem_base[PATH_MAX_LENGTH];
      const char *end = NULL;
      const char *dir_elem = dir_list->elems[i].data;

      fill_pathname_base(elem_base, dir_elem, sizeof(elem_base));

      if (strstr(elem_base, state_base) != elem_base)
         continue;

      end = dir_elem + strlen(dir_elem);
      while ((end > dir_elem) && isdigit(end[-1]))
         end--;

      idx = strtoul(end, NULL, 0);
      if (idx > max_idx)
         max_idx = idx;
   }

   dir_list_free(dir_list);

   g_settings.state_slot = max_idx;
   RARCH_LOG("Found last state slot: #%d\n", g_settings.state_slot);
}

static void rarch_init_savefile_paths(void)
{
   rarch_main_command(RARCH_CMD_SAVEFILES_DEINIT);

   g_extern.savefiles = string_list_new();
   rarch_assert(g_extern.savefiles);

   if (*g_extern.subsystem)
   {
      /* For subsystems, we know exactly which RAM types are supported. */

      unsigned i, j;
      const struct retro_subsystem_info *info = 
         libretro_find_subsystem_info(
               g_extern.system.special, g_extern.system.num_special,
               g_extern.subsystem);

      /* We'll handle this error gracefully later. */
      unsigned num_content = min(info ? info->num_roms : 0,
            g_extern.subsystem_fullpaths ?
            g_extern.subsystem_fullpaths->size : 0);

      bool use_sram_dir = path_is_directory(g_extern.savefile_name);

      for (i = 0; i < num_content; i++)
      {
         for (j = 0; j < info->roms[i].num_memory; j++)
         {
            union string_list_elem_attr attr;
            char path[PATH_MAX_LENGTH], ext[32];
            const struct retro_subsystem_memory_info *mem =
               (const struct retro_subsystem_memory_info*)
               &info->roms[i].memory[j];

            snprintf(ext, sizeof(ext), ".%s", mem->extension);

            if (use_sram_dir)
            {
               /* Redirect content fullpath to save directory. */
               strlcpy(path, g_extern.savefile_name, sizeof(path));
               fill_pathname_dir(path,
                     g_extern.subsystem_fullpaths->elems[i].data, ext,
                     sizeof(path));
            }
            else
            {
               fill_pathname(path, g_extern.subsystem_fullpaths->elems[i].data,
                     ext, sizeof(path));
            }

            attr.i = mem->type;
            string_list_append(g_extern.savefiles, path, attr);
         }
      }

      /* Let other relevant paths be inferred from the main SRAM location. */
      if (!g_extern.has_set_save_path)
         fill_pathname_noext(g_extern.savefile_name, g_extern.basename, ".srm",
               sizeof(g_extern.savefile_name));
      if (path_is_directory(g_extern.savefile_name))
      {
         fill_pathname_dir(g_extern.savefile_name, g_extern.basename, ".srm",
               sizeof(g_extern.savefile_name));
         RARCH_LOG("Redirecting save file to \"%s\".\n",
               g_extern.savefile_name);
      }
   }
   else
   {
      char savefile_name_rtc[PATH_MAX_LENGTH];
      union string_list_elem_attr attr;

      attr.i = RETRO_MEMORY_SAVE_RAM;
      string_list_append(g_extern.savefiles, g_extern.savefile_name, attr);

      /* Infer .rtc save path from save ram path. */
      attr.i = RETRO_MEMORY_RTC;
      fill_pathname(savefile_name_rtc,
            g_extern.savefile_name, ".rtc", sizeof(savefile_name_rtc));
      string_list_append(g_extern.savefiles, savefile_name_rtc, attr);
   }
}

static void fill_pathnames(void)
{
   rarch_init_savefile_paths();
   fill_pathname(g_extern.bsv.movie_path, g_extern.savefile_name, "",
         sizeof(g_extern.bsv.movie_path));

   if (!*g_extern.basename)
      return;

   if (!*g_extern.ups_name)
      fill_pathname_noext(g_extern.ups_name, g_extern.basename, ".ups",
            sizeof(g_extern.ups_name));
   if (!*g_extern.bps_name)
      fill_pathname_noext(g_extern.bps_name, g_extern.basename, ".bps",
            sizeof(g_extern.bps_name));
   if (!*g_extern.ips_name)
      fill_pathname_noext(g_extern.ips_name, g_extern.basename, ".ips",
            sizeof(g_extern.ips_name));
}

static void load_auto_state(void)
{
   char msg[PATH_MAX_LENGTH];
   char savestate_name_auto[PATH_MAX_LENGTH];
   bool ret;

#ifdef HAVE_NETPLAY
   if (g_extern.netplay_enable && !g_extern.netplay_is_spectate)
      return;
#endif

   if (!g_settings.savestate_auto_load)
      return;

   fill_pathname_noext(savestate_name_auto, g_extern.savestate_name,
         ".auto", sizeof(savestate_name_auto));

   if (!path_file_exists(savestate_name_auto))
      return;

   ret = load_state(savestate_name_auto);

   RARCH_LOG("Found auto savestate in: %s\n", savestate_name_auto);

   snprintf(msg, sizeof(msg), "Auto-loading savestate from \"%s\" %s.",
         savestate_name_auto, ret ? "succeeded" : "failed");
   msg_queue_push(g_extern.msg_queue, msg, 1, 180);
   RARCH_LOG("%s\n", msg);
}

static bool save_auto_state(void)
{
   bool ret;
   char savestate_name_auto[PATH_MAX_LENGTH];

   if (!g_settings.savestate_auto_save || g_extern.libretro_dummy ||
       g_extern.libretro_no_content)
       return false;

   fill_pathname_noext(savestate_name_auto, g_extern.savestate_name,
         ".auto", sizeof(savestate_name_auto));

   ret = save_state(savestate_name_auto);
   RARCH_LOG("Auto save state to \"%s\" %s.\n", savestate_name_auto, ret ?
         "succeeded" : "failed");
    
   return true;
}

/**
 * rarch_load_state
 * @path            : Path to state.
 * @msg             : Message.
 * @sizeof_msg      : Size of @msg.
 *
 * Loads a state with path being @path.
 **/
static void rarch_load_state(const char *path,
      char *msg, size_t sizeof_msg)
{
   if (!load_state(path))
   {
      snprintf(msg, sizeof_msg,
            "Failed to load state from \"%s\".", path);
      return;

   }

   if (g_settings.state_slot < 0)
      snprintf(msg, sizeof_msg,
            "Loaded state from slot #-1 (auto).");
   else
      snprintf(msg, sizeof_msg,
            "Loaded state from slot #%d.", g_settings.state_slot);
}

/**
 * rarch_save_state
 * @path            : Path to state.
 * @msg             : Message.
 * @sizeof_msg      : Size of @msg.
 *
 * Saves a state with path being @path.
 **/
static void rarch_save_state(const char *path,
      char *msg, size_t sizeof_msg)
{
   if (!save_state(path))
   {
      snprintf(msg, sizeof_msg,
            "Failed to save state to \"%s\".", path);
      return;
   }

   if (g_settings.state_slot < 0)
      snprintf(msg, sizeof_msg,
            "Saved state to slot #-1 (auto).");
   else
      snprintf(msg, sizeof_msg,
            "Saved state to slot #%d.", g_settings.state_slot);
}

static void main_state(unsigned cmd)
{
   char path[PATH_MAX_LENGTH], msg[PATH_MAX_LENGTH];

   if (g_settings.state_slot > 0)
      snprintf(path, sizeof(path), "%s%d",
            g_extern.savestate_name, g_settings.state_slot);
   else if (g_settings.state_slot < 0)
      snprintf(path, sizeof(path), "%s.auto",
            g_extern.savestate_name);
   else
      strlcpy(path, g_extern.savestate_name, sizeof(path));

   if (pretro_serialize_size())
   {
      if (cmd == RARCH_CMD_SAVE_STATE)
         rarch_save_state(path, msg, sizeof(msg));
      else if (cmd == RARCH_CMD_LOAD_STATE)
         rarch_load_state(path, msg, sizeof(msg));
   }
   else
      strlcpy(msg, "Core does not support save states.", sizeof(msg));

   msg_queue_clear(g_extern.msg_queue);
   msg_queue_push(g_extern.msg_queue, msg, 2, 180);
   RARCH_LOG("%s\n", msg);
}

/**
 * rarch_disk_control_append_image:
 * @path                 : Path to disk image. 
 *
 * Appends disk image to disk image list.
 **/
void rarch_disk_control_append_image(const char *path)
{
   char msg[PATH_MAX_LENGTH];
   unsigned new_idx;
   const struct retro_disk_control_callback *control = 
      (const struct retro_disk_control_callback*)&g_extern.system.disk_control;
   struct retro_game_info info = {0};
   rarch_disk_control_set_eject(true, false);

   control->add_image_index();
   new_idx = control->get_num_images();
   if (!new_idx)
      return;
   new_idx--;

   info.path = path;
   control->replace_image_index(new_idx, &info);

   snprintf(msg, sizeof(msg), "Appended disk: %s", path);
   RARCH_LOG("%s\n", msg);
   msg_queue_clear(g_extern.msg_queue);
   msg_queue_push(g_extern.msg_queue, msg, 0, 180);

   rarch_main_command(RARCH_CMD_AUTOSAVE_DEINIT);

   /* TODO: Need to figure out what to do with subsystems case. */
   if (!*g_extern.subsystem)
   {
      /* Update paths for our new image.
       * If we actually use append_image, we assume that we
       * started out in a single disk case, and that this way
       * of doing it makes the most sense. */
      set_paths(path);
      fill_pathnames();
   }

   rarch_main_command(RARCH_CMD_AUTOSAVE_INIT);

   rarch_disk_control_set_eject(false, false);
}

/**
 * rarch_disk_control_set_eject:
 * @new_state            : Eject or close the virtual drive tray.
 *                         false (0) : Close
 *                         true  (1) : Eject
 * @print_log            : Show message onscreen.
 *
 * Ejects/closes of the virtual drive tray.
 **/
void rarch_disk_control_set_eject(bool new_state, bool print_log)
{
   char msg[PATH_MAX_LENGTH];
   const struct retro_disk_control_callback *control = 
      (const struct retro_disk_control_callback*)&g_extern.system.disk_control;
   bool error = false;

   if (!control->get_num_images)
      return;

   *msg = '\0';

   if (control->set_eject_state(new_state))
      snprintf(msg, sizeof(msg), "%s virtual disk tray.",
            new_state ? "Ejected" : "Closed");
   else
   {
      error = true;
      snprintf(msg, sizeof(msg), "Failed to %s virtual disk tray.",
            new_state ? "eject" : "close");
   }

   if (*msg)
   {
      if (error)
         RARCH_ERR("%s\n", msg);
      else
         RARCH_LOG("%s\n", msg);

      /* Only noise in menu. */
      if (print_log)
      {
         msg_queue_clear(g_extern.msg_queue);
         msg_queue_push(g_extern.msg_queue, msg, 1, 180);
      }
   }
}

/**
 * rarch_disk_control_set_index:
 * @idx                : Index of disk to set as current.
 *
 * Sets current disk to @index.
 **/
void rarch_disk_control_set_index(unsigned idx)
{
   char msg[PATH_MAX_LENGTH];
   unsigned num_disks;
   const struct retro_disk_control_callback *control = 
      (const struct retro_disk_control_callback*)&g_extern.system.disk_control;
   bool error = false;

   if (!control->get_num_images)
      return;

   *msg = '\0';

   num_disks = control->get_num_images();

   if (control->set_image_index(idx))
   {
      if (idx < num_disks)
         snprintf(msg, sizeof(msg), "Setting disk %u of %u in tray.",
               idx + 1, num_disks);
      else
         strlcpy(msg, "Removed disk from tray.", sizeof(msg));
   }
   else
   {
      if (idx < num_disks)
         snprintf(msg, sizeof(msg), "Failed to set disk %u of %u.",
               idx + 1, num_disks);
      else
         strlcpy(msg, "Failed to remove disk from tray.", sizeof(msg));
      error = true;
   }

   if (*msg)
   {
      if (error)
         RARCH_ERR("%s\n", msg);
      else
         RARCH_LOG("%s\n", msg);
      msg_queue_clear(g_extern.msg_queue);
      msg_queue_push(g_extern.msg_queue, msg, 1, 180);
   }
}

/**
 * check_disk_eject:
 * @control              : Handle to disk control handle.
 *
 * Perform disk eject (Core Disk Options).
 **/
static void check_disk_eject(
      const struct retro_disk_control_callback *control)
{
   bool new_state = !control->get_eject_state();
   rarch_disk_control_set_eject(new_state, true);
}

/**
 * check_disk_next:
 * @control              : Handle to disk control handle.
 *
 * Perform disk cycle to next index action (Core Disk Options).
 **/
static void check_disk_next(
      const struct retro_disk_control_callback *control)
{
   unsigned num_disks        = control->get_num_images();
   unsigned current          = control->get_image_index();
   bool     disk_next_enable = num_disks && num_disks != UINT_MAX;

   if (!disk_next_enable)
   {
      RARCH_ERR("Got invalid disk index from libretro.\n");
      return;
   }

   if (current < num_disks - 1)
      current++;
   rarch_disk_control_set_index(current);
}

/**
 * check_disk_prev:
 * @control              : Handle to disk control handle.
 *
 * Perform disk cycle to previous index action (Core Disk Options).
 **/
static void check_disk_prev(
      const struct retro_disk_control_callback *control)
{
   unsigned num_disks    = control->get_num_images();
   unsigned current      = control->get_image_index();
   bool disk_prev_enable = num_disks && num_disks != UINT_MAX;

   if (!disk_prev_enable)
   {
      RARCH_ERR("Got invalid disk index from libretro.\n");
      return;
   }

   if (current > 0)
      current--;
   rarch_disk_control_set_index(current);
}

static void init_state(void)
{
   driver.video_active = true;
   driver.audio_active = true;
}

/**
 * free_temporary_content:
 *
 * Frees temporary content handle.
 **/
static void free_temporary_content(void)
{
   unsigned i;
   for (i = 0; i < g_extern.temporary_content->size; i++)
   {
      const char *path = g_extern.temporary_content->elems[i].data;

      RARCH_LOG("Removing temporary content file: %s.\n", path);
      if (remove(path) < 0)
         RARCH_ERR("Failed to remove temporary file: %s.\n", path);
   }
   string_list_free(g_extern.temporary_content);
}

/* main_clear_state_extern:
 *
 * Clears all external state.
 *
 * XXX This memset is really dangerous.
 *
 * (a) it can leak memory because the pointers 
 * in g_extern aren't freed.
 * (b) it can zero pointers that the rest of 
 * the code will look at.
 */
static void main_clear_state_extern(void)
{
   rarch_main_command(RARCH_CMD_TEMPORARY_CONTENT_DEINIT);
   rarch_main_command(RARCH_CMD_SUBSYSTEM_FULLPATHS_DEINIT);
   rarch_main_command(RARCH_CMD_RECORD_DEINIT);
   rarch_main_command(RARCH_CMD_LOG_FILE_DEINIT);
   rarch_main_command(RARCH_CMD_HISTORY_DEINIT);

   memset(&g_extern, 0, sizeof(g_extern));
}

/**
 * main_clear_state:
 * @inited               : Init the drivers after teardown?
 *
 * Will teardown drivers and clears all 
 * internal state of RetroArch.
 * If @inited is true, will initialize all
 * drivers again after teardown.
 **/
static void main_clear_state(bool inited)
{
   unsigned i;

   memset(&g_settings, 0, sizeof(g_settings));

   if (inited)
      rarch_main_command(RARCH_CMD_DRIVERS_DEINIT);

   main_clear_state_extern();

   if (inited)
      rarch_main_command(RARCH_CMD_DRIVERS_INIT);

   init_state();

   for (i = 0; i < MAX_USERS; i++)
      g_settings.input.libretro_device[i] = RETRO_DEVICE_JOYPAD;
}

void rarch_main_state_new(void)
{
   main_clear_state(g_extern.main_is_init);
   rarch_main_command(RARCH_CMD_MSG_QUEUE_INIT);
}

void rarch_main_state_free(void)
{
   rarch_main_command(RARCH_CMD_MSG_QUEUE_DEINIT);
   rarch_main_command(RARCH_CMD_LOG_FILE_DEINIT);

   main_clear_state(false);

}

#ifdef HAVE_ZLIB
#define DEFAULT_EXT "zip"
#else
#define DEFAULT_EXT ""
#endif

static void init_system_info(void)
{
   struct retro_system_info *info = (struct retro_system_info*)
      &g_extern.system.info;

   pretro_get_system_info(info);

   if (!info->library_name)
      info->library_name = "Unknown";
   if (!info->library_version)
      info->library_version = "v0";

#ifdef RARCH_CONSOLE
   snprintf(g_extern.title_buf, sizeof(g_extern.title_buf), "%s %s",
         info->library_name, info->library_version);
#else
   snprintf(g_extern.title_buf, sizeof(g_extern.title_buf),
         RETRO_FRONTEND " : %s %s",
         info->library_name, info->library_version);
#endif
   strlcpy(g_extern.system.valid_extensions, info->valid_extensions ?
         info->valid_extensions : DEFAULT_EXT,
         sizeof(g_extern.system.valid_extensions));
   g_extern.system.block_extract = info->block_extract;
}

/* 
 * verify_api_version:
 *
 * Compare libretro core API version against API version
 * used by RetroArch.
 *
 * TODO - when libretro v2 gets added, allow for switching
 * between libretro version backend dynamically.
 **/
static void verify_api_version(void)
{

   RARCH_LOG("Version of libretro API: %u\n", pretro_api_version());
   RARCH_LOG("Compiled against API: %u\n", RETRO_API_VERSION);

   if (pretro_api_version() != RETRO_API_VERSION)
      RARCH_WARN(RETRO_LOG_LIBRETRO_ABI_BREAK);
}

#define FAIL_CPU(simd_type) do { \
   RARCH_ERR(simd_type " code is compiled in, but CPU does not support this feature. Cannot continue.\n"); \
   rarch_fail(1, "validate_cpu_features()"); \
} while(0)

/* validate_cpu_features:
 *
 * Validates CPU features for given processor architecture.
 *
 * Make sure we haven't compiled for something we cannot run.
 * Ideally, code would get swapped out depending on CPU support, 
 * but this will do for now.
 */
static void validate_cpu_features(void)
{
   uint64_t cpu = rarch_get_cpu_features();
   (void)cpu;

#ifdef __SSE__
   if (!(cpu & RETRO_SIMD_SSE))
      FAIL_CPU("SSE");
#endif
#ifdef __SSE2__
   if (!(cpu & RETRO_SIMD_SSE2))
      FAIL_CPU("SSE2");
#endif
#ifdef __AVX__
   if (!(cpu & RETRO_SIMD_AVX))
      FAIL_CPU("AVX");
#endif
}

/**
 * init_system_av_info:
 *
 * Initialize system A/V information by calling the libretro core's
 * get_system_av_info function.
 **/
static void init_system_av_info(void)
{
   pretro_get_system_av_info(&g_extern.system.av_info);
   g_extern.frame_limit.last_frame_time = rarch_get_time_usec();
}

static void deinit_core(void)
{
   pretro_unload_game();
   pretro_deinit();

   rarch_main_command(RARCH_CMD_DRIVERS_DEINIT);

   uninit_libretro_sym();
}

static bool init_content(void)
{
   /* No content to be loaded for dummy core,
    * just successfully exit. */
   if (g_extern.libretro_dummy) 
      return true;

   if (!g_extern.libretro_no_content)
      fill_pathnames();

   if (!init_content_file())
      return false;

   if (g_extern.libretro_no_content)
      return true;

   set_savestate_auto_index();

   if (load_save_files())
      RARCH_LOG("Skipping SRAM load.\n");

   load_auto_state();

   rarch_main_command(RARCH_CMD_BSV_MOVIE_INIT);
   rarch_main_command(RARCH_CMD_NETPLAY_INIT);

   return true;
}

static bool init_core(void)
{
   verify_api_version();
   pretro_init();

   g_extern.use_sram = !g_extern.libretro_dummy &&
      !g_extern.libretro_no_content;

   if (!init_content())
      return false;

   retro_init_libretro_cbs(&driver.retro_ctx);
   init_system_av_info();

   return true;
}

/**
 * rarch_main_init:
 * @argc                 : Count of (commandline) arguments.
 * @argv                 : (Commandline) arguments. 
 *
 * Initializes RetroArch.
 *
 * Returns: 0 on success, otherwise 1 if there was an error.
 **/
int rarch_main_init(int argc, char *argv[])
{
   int sjlj_ret;

   init_state();

   if ((sjlj_ret = setjmp(g_extern.error_sjlj_context)) > 0)
   {
      RARCH_ERR("Fatal error received in: \"%s\"\n", g_extern.error_string);
      return sjlj_ret;
   }
   g_extern.error_in_init = true;
   parse_input(argc, argv);

   if (g_extern.verbosity)
   {
      RARCH_LOG_OUTPUT("=== Build =======================================");
      print_compiler(stderr);
      RARCH_LOG_OUTPUT("Version: %s\n", PACKAGE_VERSION);
#ifdef HAVE_GIT_VERSION
      RARCH_LOG_OUTPUT("Git: %s\n", rarch_git_version);
#endif
      RARCH_LOG_OUTPUT("=================================================\n");
   }

   validate_cpu_features();
   config_load();

   init_libretro_sym(g_extern.libretro_dummy);
   init_system_info();

   init_drivers_pre();

   if (!rarch_main_command(RARCH_CMD_CORE_INIT))
      goto error;

   rarch_main_command(RARCH_CMD_DRIVERS_INIT);
   rarch_main_command(RARCH_CMD_COMMAND_INIT);
   rarch_main_command(RARCH_CMD_REWIND_INIT);
   rarch_main_command(RARCH_CMD_CONTROLLERS_INIT);
   rarch_main_command(RARCH_CMD_RECORD_INIT);
   rarch_main_command(RARCH_CMD_CHEATS_INIT);
   rarch_main_command(RARCH_CMD_REMAPPING_INIT);

   rarch_main_command(RARCH_CMD_SAVEFILES_INIT);

   g_extern.error_in_init = false;
   g_extern.main_is_init  = true;
   return 0;

error:
   rarch_main_command(RARCH_CMD_CORE_DEINIT);

   g_extern.main_is_init = false;
   return 1;
}

/**
 * rarch_main_init_wrap:
 * @args                 : Input arguments.
 * @argc                 : Count of arguments.
 * @argv                 : Arguments.
 *
 * Generates an @argc and @argv pair based on @args
 * of type rarch_main_wrap.
 **/
void rarch_main_init_wrap(const struct rarch_main_wrap *args,
      int *argc, char **argv)
{
   *argc = 0;
   argv[(*argc)++] = strdup("retroarch");

   if (!args->no_content)
   {
      if (args->content_path)
      {
         RARCH_LOG("Using content: %s.\n", args->content_path);
         argv[(*argc)++] = strdup(args->content_path);
      }
      else
      {
         RARCH_LOG("No content, starting dummy core.\n");
         argv[(*argc)++] = strdup("--menu");
      }
   }

   if (args->sram_path)
   {
      argv[(*argc)++] = strdup("-s");
      argv[(*argc)++] = strdup(args->sram_path);
   }

   if (args->state_path)
   {
      argv[(*argc)++] = strdup("-S");
      argv[(*argc)++] = strdup(args->state_path);
   }

   if (args->config_path)
   {
      argv[(*argc)++] = strdup("-c");
      argv[(*argc)++] = strdup(args->config_path);
   }

#ifdef HAVE_DYNAMIC
   if (args->libretro_path)
   {
      argv[(*argc)++] = strdup("-L");
      argv[(*argc)++] = strdup(args->libretro_path);
   }
#endif

   if (args->verbose)
      argv[(*argc)++] = strdup("-v");

#ifdef HAVE_FILE_LOGGER
   for (i = 0; i < *argc; i++)
      RARCH_LOG("arg #%d: %s\n", i, argv[i]);
#endif
}

void rarch_main_set_state(unsigned cmd)
{
   switch (cmd)
   {
      case RARCH_ACTION_STATE_MENU_RUNNING:
#ifdef HAVE_MENU
         {
            menu_handle_t *menu = menu_driver_resolve();
            if (!menu)
               return;

            if (driver.menu_ctx && driver.menu_ctx->toggle)
               driver.menu_ctx->toggle(true);

            /* Menu should always run with vsync on. */
            rarch_main_command(RARCH_CMD_VIDEO_SET_BLOCKING_STATE);
            /* Stop all rumbling before entering the menu. */
            rarch_main_command(RARCH_CMD_RUMBLE_STOP);

            if (g_settings.menu.pause_libretro)
               rarch_main_command(RARCH_CMD_AUDIO_STOP);

            /* Override keyboard callback to redirect to menu instead.
             * We'll use this later for something ...
             * FIXME: This should probably be moved to menu_common somehow. */
            g_extern.frontend_key_event = g_extern.system.key_event;
            g_extern.system.key_event   = menu_input_key_event;

            menu->need_refresh = true;
            g_extern.system.frame_time_last = 0;
            g_extern.is_menu = true;
         }
#endif
         break;
      case RARCH_ACTION_STATE_LOAD_CONTENT:
#ifdef HAVE_MENU
         /* If content loading fails, we go back to menu. */
         if (!menu_load_content())
            rarch_main_set_state(RARCH_ACTION_STATE_MENU_RUNNING);
#endif
         if (driver.frontend_ctx && driver.frontend_ctx->content_loaded)
            driver.frontend_ctx->content_loaded();
         break;
      case RARCH_ACTION_STATE_MENU_RUNNING_FINISHED:
#ifdef HAVE_MENU
         menu_apply_deferred_settings();

         if (driver.menu_ctx && driver.menu_ctx->toggle)
            driver.menu_ctx->toggle(false);

         g_extern.is_menu = false;

         driver_set_nonblock_state(driver.nonblock_state);

         if (g_settings.menu.pause_libretro)
            rarch_main_command(RARCH_CMD_AUDIO_START);

         /* Prevent stray input from going to libretro core */
         driver.flushing_input = true;

         /* Restore libretro keyboard callback. */
         g_extern.system.key_event = g_extern.frontend_key_event;
#endif
         if (driver.video_data && driver.video_poke &&
               driver.video_poke->set_texture_enable)
            driver.video_poke->set_texture_enable(driver.video_data,
                  false, false);
         break;
      case RARCH_ACTION_STATE_QUIT:
         g_extern.system.shutdown = true;
         rarch_main_set_state(RARCH_ACTION_STATE_MENU_RUNNING_FINISHED);
         break;
      case RARCH_ACTION_STATE_FORCE_QUIT:
         g_extern.lifecycle_state = 0;
         rarch_main_set_state(RARCH_ACTION_STATE_QUIT);
         break;
      case RARCH_ACTION_STATE_NONE:
      default:
         break;
   }
}

/**
 * save_core_config:
 *
 * Saves a new (core) configuration to a file. Filename is based
 * on heuristics to avoid typing.
 *
 * Returns: true (1) on success, otherwise false (0).
 **/
static bool save_core_config(void)
{
   bool ret = false;
   char config_dir[PATH_MAX_LENGTH], config_name[PATH_MAX_LENGTH],
        config_path[PATH_MAX_LENGTH], msg[PATH_MAX_LENGTH];
   bool found_path = false;

   *config_dir = '\0';

   if (*g_settings.menu_config_directory)
      strlcpy(config_dir, g_settings.menu_config_directory,
            sizeof(config_dir));
   else if (*g_extern.config_path) /* Fallback */
      fill_pathname_basedir(config_dir, g_extern.config_path,
            sizeof(config_dir));
   else
   {
      const char *message = "Config directory not set. Cannot save new config.";
      msg_queue_clear(g_extern.msg_queue);
      msg_queue_push(g_extern.msg_queue, message, 1, 180);
      RARCH_ERR("%s\n", message);
      return false;
   }

   /* Infer file name based on libretro core. */
   if (*g_settings.libretro && path_file_exists(g_settings.libretro))
   {
      unsigned i;

      /* In case of collision, find an alternative name. */
      for (i = 0; i < 16; i++)
      {
         char tmp[64];

         fill_pathname_base(config_name, g_settings.libretro,
               sizeof(config_name));
         path_remove_extension(config_name);
         fill_pathname_join(config_path, config_dir, config_name,
               sizeof(config_path));

         *tmp = '\0';

         if (i)
            snprintf(tmp, sizeof(tmp), "-%u.cfg", i);
         else
            strlcpy(tmp, ".cfg", sizeof(tmp));

         strlcat(config_path, tmp, sizeof(config_path));

         if (!path_file_exists(config_path))
         {
            found_path = true;
            break;
         }
      }
   }

   /* Fallback to system time... */
   if (!found_path)
   {
      RARCH_WARN("Cannot infer new config path. Use current time.\n");
      fill_dated_filename(config_name, "cfg", sizeof(config_name));
      fill_pathname_join(config_path, config_dir, config_name,
            sizeof(config_path));
   }

   if ((ret = config_save_file(config_path)))
   {
      strlcpy(g_extern.config_path, config_path,
            sizeof(g_extern.config_path));
      snprintf(msg, sizeof(msg), "Saved new config to \"%s\".",
            config_path);
      RARCH_LOG("%s\n", msg);
   }
   else
   {
      snprintf(msg, sizeof(msg), "Failed saving config to \"%s\".",
            config_path);
      RARCH_ERR("%s\n", msg);
   }

   msg_queue_clear(g_extern.msg_queue);
   msg_queue_push(g_extern.msg_queue, msg, 1, 180);

   return ret;
}

/**
 * rarch_main_command:
 * @cmd                  : Command index.
 *
 * Performs RetroArch command with index @cmd.
 *
 * Returns: true (1) on success, otherwise false (0).
 **/
bool rarch_main_command(unsigned cmd)
{
   unsigned i   = 0;
   bool boolean = false;

   (void)i;

   switch (cmd)
   {
      case RARCH_CMD_LOAD_CONTENT_PERSIST:
#ifdef HAVE_DYNAMIC
         rarch_main_command(RARCH_CMD_LOAD_CORE);
#endif
         rarch_main_set_state(RARCH_ACTION_STATE_LOAD_CONTENT);
         break;
      case RARCH_CMD_LOAD_CONTENT:
#ifdef HAVE_DYNAMIC
         rarch_main_command(RARCH_CMD_LOAD_CONTENT_PERSIST);
#else
         rarch_environment_cb(RETRO_ENVIRONMENT_SET_LIBRETRO_PATH,
               (void*)g_settings.libretro);
         rarch_environment_cb(RETRO_ENVIRONMENT_EXEC,
               (void*)g_extern.fullpath);
         rarch_main_command(RARCH_CMD_QUIT);
#endif
         break;
      case RARCH_CMD_LOAD_CORE:
         {
#ifdef HAVE_MENU
            menu_handle_t *menu = menu_driver_resolve();
            if (menu)
               rarch_update_system_info(&g_extern.menu.info,
                     &menu->load_no_content);
#endif
#ifndef HAVE_DYNAMIC
            rarch_main_command(RARCH_CMD_QUIT);
#endif
         }
         break;
      case RARCH_CMD_LOAD_STATE:
         /* Immutable - disallow savestate load when 
          * we absolutely cannot change game state. */
         if (g_extern.bsv.movie)
            return false;

#ifdef HAVE_NETPLAY
         if (driver.netplay_data)
            return false;
#endif
         main_state(cmd);
         break;
      case RARCH_CMD_RESIZE_WINDOWED_SCALE:
         if (g_extern.pending.windowed_scale == 0)
            return false;

         g_settings.video.scale = g_extern.pending.windowed_scale;

         if (!g_settings.video.fullscreen)
            rarch_main_command(RARCH_CMD_REINIT);

         g_extern.pending.windowed_scale = 0;
         break;
      case RARCH_CMD_MENU_TOGGLE:
         if (g_extern.is_menu)
            rarch_main_set_state(RARCH_ACTION_STATE_MENU_RUNNING_FINISHED);
         else
            rarch_main_set_state(RARCH_ACTION_STATE_MENU_RUNNING);
         break;
      case RARCH_CMD_CONTROLLERS_INIT:
         init_controllers();
         break;
      case RARCH_CMD_RESET:
         RARCH_LOG(RETRO_LOG_RESETTING_CONTENT);
         msg_queue_clear(g_extern.msg_queue);
         msg_queue_push(g_extern.msg_queue, "Reset.", 1, 120);
         pretro_reset();

         /* bSNES since v073r01 resets controllers to JOYPAD
          * after a reset, so just enforce it here. */
         rarch_main_command(RARCH_CMD_CONTROLLERS_INIT);
         break;
      case RARCH_CMD_SAVE_STATE:
         if (g_settings.savestate_auto_index)
            g_settings.state_slot++;

         main_state(cmd);
         break;
      case RARCH_CMD_TAKE_SCREENSHOT:
         if (!take_screenshot())
            return false;
         break;
      case RARCH_CMD_PREPARE_DUMMY:
         {
            menu_handle_t *menu = menu_driver_resolve();
            *g_extern.fullpath = '\0';

            (void)menu;

#ifdef HAVE_MENU
            if (menu)
               menu->load_no_content = false;
#endif

            rarch_main_set_state(RARCH_ACTION_STATE_LOAD_CONTENT);
            g_extern.system.shutdown = false;
         }
         break;
      case RARCH_CMD_QUIT:
         rarch_main_set_state(RARCH_ACTION_STATE_QUIT);
         break;
      case RARCH_CMD_REINIT:
         driver.video_cache_context = 
            g_extern.system.hw_render_callback.cache_context;
         driver.video_cache_context_ack = false;
         rarch_main_command(RARCH_CMD_RESET_CONTEXT);
         driver.video_cache_context = false;

         /* Poll input to avoid possibly stale data to corrupt things. */
         driver.input->poll(driver.input_data);

#ifdef HAVE_MENU
         if (g_extern.is_menu)
             rarch_main_command(RARCH_CMD_VIDEO_SET_BLOCKING_STATE);
#endif
         break;
      case RARCH_CMD_CHEATS_DEINIT:
         if (g_extern.cheat)
            cheat_manager_free(g_extern.cheat);
         g_extern.cheat = NULL;
         break;
      case RARCH_CMD_CHEATS_INIT:
         rarch_main_command(RARCH_CMD_CHEATS_DEINIT);
         init_cheats();
         break;
      case RARCH_CMD_REMAPPING_DEINIT:
         break;
      case RARCH_CMD_REMAPPING_INIT:
         rarch_main_command(RARCH_CMD_REMAPPING_DEINIT);
         init_remapping();
         break;
      case RARCH_CMD_REWIND_DEINIT:
#ifdef HAVE_NETPLAY
         if (driver.netplay_data)
            return false;
#endif
         if (g_extern.rewind.state)
            state_manager_free(g_extern.rewind.state);
         g_extern.rewind.state = NULL;
         break;
      case RARCH_CMD_REWIND_INIT:
         init_rewind();
         break;
      case RARCH_CMD_REWIND_TOGGLE:
         if (g_settings.rewind_enable)
            rarch_main_command(RARCH_CMD_REWIND_INIT);
         else
            rarch_main_command(RARCH_CMD_REWIND_DEINIT);
         break;
      case RARCH_CMD_AUTOSAVE_DEINIT:
#ifdef HAVE_THREADS
         deinit_autosave();
#endif
         break;
      case RARCH_CMD_AUTOSAVE_INIT:
         rarch_main_command(RARCH_CMD_AUTOSAVE_DEINIT);
#ifdef HAVE_THREADS
         init_autosave();
#endif
         break;
      case RARCH_CMD_AUTOSAVE_STATE:
         save_auto_state();
         break;
      case RARCH_CMD_AUDIO_STOP:
         if (!driver.audio_data)
            return false;
         if (!driver.audio->alive(driver.audio_data))
            return false;

         driver.audio->stop(driver.audio_data);
         break;
      case RARCH_CMD_AUDIO_START:
         if (!driver.audio_data || driver.audio->alive(driver.audio_data))
            return false;

         if (!g_settings.audio.mute_enable
               && !driver.audio->start(driver.audio_data))
         {
            RARCH_ERR("Failed to start audio driver. Will continue without audio.\n");
            driver.audio_active = false;
         }
         break;
      case RARCH_CMD_AUDIO_MUTE_TOGGLE:
         {
            const char *msg = !g_settings.audio.mute_enable ?
               "Audio muted." : "Audio unmuted.";

            if (!audio_driver_mute_toggle())
            {
               RARCH_ERR("Failed to unmute audio.\n");
               return false;
            }

            msg_queue_clear(g_extern.msg_queue);
            msg_queue_push(g_extern.msg_queue, msg, 1, 180);
            RARCH_LOG("%s\n", msg);
         }
         break;
      case RARCH_CMD_OVERLAY_DEINIT:
#ifdef HAVE_OVERLAY
         if (driver.overlay)
            input_overlay_free(driver.overlay);
         driver.overlay = NULL;

         memset(&driver.overlay_state, 0, sizeof(driver.overlay_state));
#endif
         break;
      case RARCH_CMD_OVERLAY_INIT:
         rarch_main_command(RARCH_CMD_OVERLAY_DEINIT);
#ifdef HAVE_OVERLAY
         if (driver.osk_enable)
         {
            if (!*g_settings.osk.overlay)
               break;
         }
         else
         {
            if (!*g_settings.input.overlay)
               break;
         }

         driver.overlay = input_overlay_new(driver.osk_enable ? g_settings.osk.overlay : g_settings.input.overlay,
               driver.osk_enable ? g_settings.osk.enable   : g_settings.input.overlay_enable,
               g_settings.input.overlay_opacity, g_settings.input.overlay_scale);
         if (!driver.overlay)
            RARCH_ERR("Failed to load overlay.\n");
#endif
         break;
      case RARCH_CMD_OVERLAY_NEXT:
#ifdef HAVE_OVERLAY
         input_overlay_next(driver.overlay, g_settings.input.overlay_opacity);
#endif
         break;
      case RARCH_CMD_DSP_FILTER_DEINIT:
         if (g_extern.audio_data.dsp)
            rarch_dsp_filter_free(g_extern.audio_data.dsp);
         g_extern.audio_data.dsp = NULL;
         break;
      case RARCH_CMD_DSP_FILTER_INIT:
         rarch_main_command(RARCH_CMD_DSP_FILTER_DEINIT);
         if (!*g_settings.audio.dsp_plugin)
            break;

         g_extern.audio_data.dsp = rarch_dsp_filter_new(
               g_settings.audio.dsp_plugin, g_extern.audio_data.in_rate);
         if (!g_extern.audio_data.dsp)
            RARCH_ERR("[DSP]: Failed to initialize DSP filter \"%s\".\n",
                  g_settings.audio.dsp_plugin);
         break;
      case RARCH_CMD_GPU_RECORD_DEINIT:
         if (g_extern.record_gpu_buffer)
            free(g_extern.record_gpu_buffer);
         g_extern.record_gpu_buffer = NULL;
         break;
      case RARCH_CMD_RECORD_DEINIT:
         if (!recording_deinit())
            return false;
         break;
      case RARCH_CMD_RECORD_INIT:
         rarch_main_command(RARCH_CMD_HISTORY_DEINIT);
         if (!recording_init())
            return false;
         break;
      case RARCH_CMD_HISTORY_DEINIT:
         if (g_defaults.history)
            content_playlist_free(g_defaults.history);
         g_defaults.history = NULL;
         break;
      case RARCH_CMD_HISTORY_INIT:
         rarch_main_command(RARCH_CMD_HISTORY_DEINIT);
         if (!g_settings.history_list_enable)
            return false;
         RARCH_LOG("Loading history file: [%s].\n", g_settings.content_history_path);
         g_defaults.history = content_playlist_init(
               g_settings.content_history_path,
               g_settings.content_history_size);
         break;
      case RARCH_CMD_CORE_INFO_DEINIT:
         if (g_extern.core_info)
            core_info_list_free(g_extern.core_info);
         g_extern.core_info = NULL;
         break;
      case RARCH_CMD_CORE_INFO_INIT:
         rarch_main_command(RARCH_CMD_CORE_INFO_DEINIT);

         if (*g_settings.libretro_directory && !g_extern.core_info)
            g_extern.core_info = core_info_list_new(g_settings.libretro_directory);
         break;
      case RARCH_CMD_CORE_DEINIT:
         deinit_core();
         break;
      case RARCH_CMD_CORE_INIT:
         if (!init_core())
            return false;
         break;
      case RARCH_CMD_VIDEO_APPLY_STATE_CHANGES:
         if (driver.video_data && driver.video_poke
               && driver.video_poke->apply_state_changes)
            driver.video_poke->apply_state_changes(driver.video_data);
         break;
      case RARCH_CMD_VIDEO_SET_NONBLOCKING_STATE:
         boolean = true; /* fall-through */
      case RARCH_CMD_VIDEO_SET_BLOCKING_STATE:
         if (driver.video && driver.video->set_nonblock_state)
            driver.video->set_nonblock_state(driver.video_data, boolean);
         break;
      case RARCH_CMD_VIDEO_SET_ASPECT_RATIO:
         if (driver.video_data && driver.video_poke
               && driver.video_poke->set_aspect_ratio)
            driver.video_poke->set_aspect_ratio(driver.video_data,
                  g_settings.video.aspect_ratio_idx);
         break;
      case RARCH_CMD_AUDIO_SET_NONBLOCKING_STATE:
         boolean = true; /* fall-through */
      case RARCH_CMD_AUDIO_SET_BLOCKING_STATE:
         if (driver.audio && driver.audio->set_nonblock_state)
            driver.audio->set_nonblock_state(driver.audio_data, boolean);
         break;
      case RARCH_CMD_OVERLAY_SET_SCALE_FACTOR:
#ifdef HAVE_OVERLAY
         input_overlay_set_scale_factor(driver.overlay,
               g_settings.input.overlay_scale);
#endif
         break;
      case RARCH_CMD_OVERLAY_SET_ALPHA_MOD:
#ifdef HAVE_OVERLAY
         input_overlay_set_alpha_mod(driver.overlay,
               g_settings.input.overlay_opacity);
#endif
         break;
      case RARCH_CMD_DRIVERS_DEINIT:
         uninit_drivers(DRIVERS_CMD_ALL);
         break;
      case RARCH_CMD_DRIVERS_INIT:
         init_drivers(DRIVERS_CMD_ALL);
         break;
      case RARCH_CMD_AUDIO_REINIT:
         uninit_drivers(DRIVER_AUDIO);
         init_drivers(DRIVER_AUDIO);
         break;
      case RARCH_CMD_RESET_CONTEXT:
         rarch_main_command(RARCH_CMD_DRIVERS_DEINIT);
         rarch_main_command(RARCH_CMD_DRIVERS_INIT);
         break;
      case RARCH_CMD_QUIT_RETROARCH:
         rarch_main_set_state(RARCH_ACTION_STATE_FORCE_QUIT);
         break;
      case RARCH_CMD_RESUME:
         rarch_main_set_state(RARCH_ACTION_STATE_MENU_RUNNING_FINISHED);
         break;
      case RARCH_CMD_RESTART_RETROARCH:
#if defined(GEKKO) && defined(HW_RVL)
         fill_pathname_join(g_extern.fullpath, g_defaults.core_dir,
               SALAMANDER_FILE,
               sizeof(g_extern.fullpath));
#endif
         if (driver.frontend_ctx && driver.frontend_ctx->set_fork)
            driver.frontend_ctx->set_fork(true, false);
         break;
      case RARCH_CMD_MENU_SAVE_CONFIG:
         if (!save_core_config())
            return false;
         break;
      case RARCH_CMD_SHADERS_APPLY_CHANGES:
#ifdef HAVE_MENU
         menu_shader_manager_apply_changes();
#endif
         break;
      case RARCH_CMD_PAUSE_CHECKS:
         if (g_extern.is_paused)
         {
            RARCH_LOG("Paused.\n");
            rarch_main_command(RARCH_CMD_AUDIO_STOP);

            if (g_settings.video.black_frame_insertion)
               rarch_render_cached_frame();
         }
         else
         {
            RARCH_LOG("Unpaused.\n");
            rarch_main_command(RARCH_CMD_AUDIO_START);
         }
         break;
      case RARCH_CMD_PAUSE_TOGGLE:
         g_extern.is_paused = !g_extern.is_paused;
         rarch_main_command(RARCH_CMD_PAUSE_CHECKS);
         break;
      case RARCH_CMD_UNPAUSE:
         g_extern.is_paused = false;
         rarch_main_command(RARCH_CMD_PAUSE_CHECKS);
         break;
      case RARCH_CMD_PAUSE:
         g_extern.is_paused = true;
         rarch_main_command(RARCH_CMD_PAUSE_CHECKS);
         break;
      case RARCH_CMD_MENU_PAUSE_LIBRETRO:
         if (g_extern.is_menu)
         {
            if (g_settings.menu.pause_libretro)
               rarch_main_command(RARCH_CMD_AUDIO_STOP);
            else
               rarch_main_command(RARCH_CMD_AUDIO_START);
         }
         else
         {
            if (g_settings.menu.pause_libretro)
               rarch_main_command(RARCH_CMD_AUDIO_START);
         }
         break;
      case RARCH_CMD_SHADER_DIR_DEINIT:
         dir_list_free(g_extern.shader_dir.list);
         g_extern.shader_dir.list = NULL;
         g_extern.shader_dir.ptr  = 0;
         break;
      case RARCH_CMD_SHADER_DIR_INIT:
         rarch_main_command(RARCH_CMD_SHADER_DIR_DEINIT);

         if (!*g_settings.video.shader_dir)
            return false;

         g_extern.shader_dir.list = dir_list_new(g_settings.video.shader_dir,
               "cg|cgp|glsl|glslp", false);

         if (!g_extern.shader_dir.list || g_extern.shader_dir.list->size == 0)
         {
            rarch_main_command(RARCH_CMD_SHADER_DIR_DEINIT);
            return false;
         }

         g_extern.shader_dir.ptr  = 0;
         dir_list_sort(g_extern.shader_dir.list, false);

         for (i = 0; i < g_extern.shader_dir.list->size; i++)
            RARCH_LOG("Found shader \"%s\"\n",
                  g_extern.shader_dir.list->elems[i].data);
         break;
      case RARCH_CMD_SAVEFILES:
         save_files();
         break;
      case RARCH_CMD_SAVEFILES_DEINIT:
         if (g_extern.savefiles)
            string_list_free(g_extern.savefiles);
         g_extern.savefiles = NULL;
         break;
      case RARCH_CMD_SAVEFILES_INIT:
         g_extern.use_sram = g_extern.use_sram && !g_extern.sram_save_disable
#ifdef HAVE_NETPLAY
            && (!driver.netplay_data || !g_extern.netplay_is_client)
#endif
            ;

         if (!g_extern.use_sram)
            RARCH_LOG("SRAM will not be saved.\n");

         if (g_extern.use_sram)
            rarch_main_command(RARCH_CMD_AUTOSAVE_INIT);
         break;
      case RARCH_CMD_MSG_QUEUE_DEINIT:
         if (g_extern.msg_queue)
            msg_queue_free(g_extern.msg_queue);
         g_extern.msg_queue = NULL;
         break;
      case RARCH_CMD_MSG_QUEUE_INIT:
         rarch_main_command(RARCH_CMD_MSG_QUEUE_DEINIT);
         if (!g_extern.msg_queue)
            rarch_assert(g_extern.msg_queue = msg_queue_new(8));
#ifdef HAVE_NETWORKING
         if (!g_extern.http.msg_queue)
            rarch_assert(g_extern.http.msg_queue = msg_queue_new(8));
#endif
         if (!g_extern.nbio.msg_queue)
            rarch_assert(g_extern.nbio.msg_queue = msg_queue_new(8));
         if (!g_extern.images.msg_queue)
            rarch_assert(g_extern.images.msg_queue = msg_queue_new(8));
         break;
      case RARCH_CMD_BSV_MOVIE_DEINIT:
         if (g_extern.bsv.movie)
            bsv_movie_free(g_extern.bsv.movie);
         g_extern.bsv.movie = NULL;
         break;
      case RARCH_CMD_BSV_MOVIE_INIT:
         rarch_main_command(RARCH_CMD_BSV_MOVIE_DEINIT);
         init_movie();
         break;
      case RARCH_CMD_NETPLAY_DEINIT:
#ifdef HAVE_NETPLAY
         {
            netplay_t *netplay = (netplay_t*)driver.netplay_data;
            if (netplay)
               netplay_free(netplay);
            driver.netplay_data = NULL;
         }
#endif
         break;
      case RARCH_CMD_NETWORK_DEINIT:
#ifdef HAVE_NETWORKING
         network_deinit();
#endif
         break;
      case RARCH_CMD_NETWORK_INIT:
#ifdef HAVE_NETWORKING
         network_init();
#endif
         break;
      case RARCH_CMD_NETPLAY_INIT:
         rarch_main_command(RARCH_CMD_NETPLAY_DEINIT);
#ifdef HAVE_NETPLAY
         if (!init_netplay())
            return false;
#endif
         break;
      case RARCH_CMD_NETPLAY_FLIP_PLAYERS:
#ifdef HAVE_NETPLAY
         {
            netplay_t *netplay = (netplay_t*)driver.netplay_data;
            if (!netplay)
               return false;
            netplay_flip_users(netplay);
         }
#endif
         break;
      case RARCH_CMD_FULLSCREEN_TOGGLE:
         if (!driver.video)
            return false;
         /* If video driver/context does not support windowed
          * mode, don't perform command. */
         if (!driver.video->has_windowed(driver.video_data))
            return false;

         /* If we go fullscreen we drop all drivers and 
          * reinitialize to be safe. */
         g_settings.video.fullscreen = !g_settings.video.fullscreen;
         rarch_main_command(RARCH_CMD_REINIT);
         break;
      case RARCH_CMD_COMMAND_DEINIT:
#ifdef HAVE_COMMAND
         if (driver.command)
            rarch_cmd_free(driver.command);
         driver.command = NULL;
#endif
         break;
      case RARCH_CMD_COMMAND_INIT:
         rarch_main_command(RARCH_CMD_COMMAND_DEINIT);

#ifdef HAVE_COMMAND
         init_command();
#endif
         break;
      case RARCH_CMD_TEMPORARY_CONTENT_DEINIT:
         if (g_extern.temporary_content)
            free_temporary_content();
         g_extern.temporary_content = NULL;
         break;
      case RARCH_CMD_SUBSYSTEM_FULLPATHS_DEINIT:
         if (g_extern.subsystem_fullpaths)
            string_list_free(g_extern.subsystem_fullpaths);
         g_extern.subsystem_fullpaths = NULL;
         break;
      case RARCH_CMD_LOG_FILE_DEINIT:
         if (g_extern.log_file)
            fclose(g_extern.log_file);
         g_extern.log_file = NULL;
         break;
      case RARCH_CMD_DISK_EJECT_TOGGLE:
         if (g_extern.system.disk_control.get_num_images)
         {
            const struct retro_disk_control_callback *control = 
               (const struct retro_disk_control_callback*)
               &g_extern.system.disk_control;

            if (control)
               check_disk_eject(control);
         }
         else
         {
            msg_queue_clear(g_extern.msg_queue);
            msg_queue_push(g_extern.msg_queue, "Core does not support Disk Options.", 1, 120);
         }
         break;
      case RARCH_CMD_DISK_NEXT:
         if (g_extern.system.disk_control.get_num_images)
         {
            const struct retro_disk_control_callback *control = 
               (const struct retro_disk_control_callback*)
               &g_extern.system.disk_control;

            if (!control)
               return false;

            if (!control->get_eject_state())
               return false;

            check_disk_next(control);
         }
         else
         {
            msg_queue_clear(g_extern.msg_queue);
            msg_queue_push(g_extern.msg_queue, "Core does not support Disk Options.", 1, 120);
         }
         break;
      case RARCH_CMD_DISK_PREV:
         if (g_extern.system.disk_control.get_num_images)
         {
            const struct retro_disk_control_callback *control = 
               (const struct retro_disk_control_callback*)
               &g_extern.system.disk_control;

            if (!control)
               return false;

            if (!control->get_eject_state())
               return false;

            check_disk_prev(control);
         }
         else
         {
            msg_queue_clear(g_extern.msg_queue);
            msg_queue_push(g_extern.msg_queue, "Core does not support Disk Options.", 1, 120);
         }
         break;
      case RARCH_CMD_RUMBLE_STOP:
         for (i = 0; i < MAX_USERS; i++)
         {
            input_driver_set_rumble_state(i, RETRO_RUMBLE_STRONG, 0);
            input_driver_set_rumble_state(i, RETRO_RUMBLE_WEAK, 0);
         }
         break;
      case RARCH_CMD_GRAB_MOUSE_TOGGLE:
         {
            static bool grab_mouse_state  = false;

            if (!driver.input || !driver.input->grab_mouse)
               return false;

            grab_mouse_state = !grab_mouse_state;
            RARCH_LOG("Grab mouse state: %s.\n",
                  grab_mouse_state ? "yes" : "no");
            driver.input->grab_mouse(driver.input_data, grab_mouse_state);

            if (driver.video_poke && driver.video_poke->show_mouse)
               driver.video_poke->show_mouse(
                     driver.video_data, !grab_mouse_state);
         }
         break;
      case RARCH_CMD_PERFCNT_REPORT_FRONTEND_LOG:
         rarch_perf_log();
         break;
   }

   return true;
}

/**
 * rarch_main_deinit:
 *
 * Deinitializes RetroArch.
 **/
void rarch_main_deinit(void)
{
   rarch_main_command(RARCH_CMD_NETPLAY_DEINIT);
   rarch_main_command(RARCH_CMD_COMMAND_DEINIT);

   if (g_extern.use_sram)
      rarch_main_command(RARCH_CMD_AUTOSAVE_DEINIT);

   rarch_main_command(RARCH_CMD_RECORD_DEINIT);
   rarch_main_command(RARCH_CMD_SAVEFILES);

   rarch_main_command(RARCH_CMD_REWIND_DEINIT);
   rarch_main_command(RARCH_CMD_CHEATS_DEINIT);
   rarch_main_command(RARCH_CMD_BSV_MOVIE_DEINIT);

   rarch_main_command(RARCH_CMD_AUTOSAVE_STATE);

   rarch_main_command(RARCH_CMD_CORE_DEINIT);

   rarch_main_command(RARCH_CMD_TEMPORARY_CONTENT_DEINIT);
   rarch_main_command(RARCH_CMD_SUBSYSTEM_FULLPATHS_DEINIT);
   rarch_main_command(RARCH_CMD_SAVEFILES_DEINIT);

   g_extern.main_is_init = false;
}

/**
 * rarch_playlist_load_content:
 * @playlist             : Playlist handle.
 * @idx                  : Index in playlist.
 *
 * Initializes core and loads content based on playlist entry.
 **/
void rarch_playlist_load_content(content_playlist_t *playlist,
      unsigned idx)
{
   const char *path      = NULL;
   const char *core_path = NULL;
   menu_handle_t *menu   = menu_driver_resolve();

   content_playlist_get_index(playlist,
         idx, &path, &core_path, NULL);

   strlcpy(g_settings.libretro, core_path, sizeof(g_settings.libretro));

   if (menu)
      menu->load_no_content = (path) ? false : true;

   rarch_environment_cb(RETRO_ENVIRONMENT_EXEC, (void*)path);

   rarch_main_command(RARCH_CMD_LOAD_CORE);
}

/**
 * rarch_defer_core:
 * @core_info            : Core info list handle.
 * @dir                  : Directory. Gets joined with @path.
 * @path                 : Path. Gets joined with @dir.
 * @menu_label           : Label identifier of menu setting.
 * @deferred_path        : Deferred core path. Will be filled in
 *                         by function.
 * @sizeof_deferred_path : Size of @deferred_path.
 *
 * Gets deferred core.
 *
 * Returns: 0 if there are multiple deferred cores and a 
 * selection needs to be made from a list, otherwise
 * returns -1 and fills in @deferred_path with path to core.
 **/
int rarch_defer_core(core_info_list_t *core_info, const char *dir,
      const char *path, const char *menu_label,
      char *deferred_path, size_t sizeof_deferred_path)
{
   char new_core_path[PATH_MAX_LENGTH];
   const core_info_t *info = NULL;
   size_t supported = 0;

   fill_pathname_join(deferred_path, dir, path, sizeof_deferred_path);

#ifdef HAVE_COMPRESSION
   if (path_is_compressed_file(dir))
   {
      /* In case of a compressed archive, we have to join with a hash */
      /* We are going to write at the position of dir: */
      rarch_assert(strlen(dir) < strlen(deferred_path));
      deferred_path[strlen(dir)] = '#';
   }
#endif

   if (core_info)
      core_info_list_get_supported_cores(core_info, deferred_path, &info,
            &supported);

   if (!strcmp(menu_label, "load_content"))
   {
      info = (const core_info_t*)&g_extern.core_info_current;

      if (info)
      {
         strlcpy(new_core_path, info->path, sizeof(new_core_path));
         supported = 1;
      }
   }
   else
      strlcpy(new_core_path, info->path, sizeof(new_core_path));

   /* There are multiple deferred cores and a 
    * selection needs to be made from a list, return 0. */
   if (supported != 1)
      return 0;

   strlcpy(g_extern.fullpath, deferred_path,
         sizeof(g_extern.fullpath));

   if (path_file_exists(new_core_path))
      strlcpy(g_settings.libretro, new_core_path,
            sizeof(g_settings.libretro));
   return -1;
}

/**
 * rarch_replace_config:
 * @path                 : Path to config file to replace
 *                         current config file with.
 *
 * Replaces currently loaded configuration file with
 * another one. Will load a dummy core to flush state
 * properly.
 *
 * Quite intrusive and error prone.
 * Likely to have lots of small bugs.
 * Cleanly exit the main loop to ensure that all the tiny details
 * get set properly.
 *
 * This should mitigate most of the smaller bugs.
 *
 * Returns: true (1) if successful, false (0) if @path was the
 * same as the current config file.
 **/

bool rarch_replace_config(const char *path)
{
   /* If config file to be replaced is the same as the 
    * current config file, exit. */
   if (!strcmp(path, g_extern.config_path))
      return false;

   if (g_settings.config_save_on_exit && *g_extern.config_path)
      config_save_file(g_extern.config_path);

   strlcpy(g_extern.config_path, path, sizeof(g_extern.config_path));
   g_extern.block_config_read = false;
   *g_settings.libretro = '\0'; /* Load core in new config. */

   rarch_main_command(RARCH_CMD_PREPARE_DUMMY);

   return true;
}

bool rarch_update_system_info(struct retro_system_info *_info,
      bool *load_no_content)
{
#if defined(HAVE_DYNAMIC)
   libretro_free_system_info(_info);
   if (!(*g_settings.libretro))
      return false;

   libretro_get_system_info(g_settings.libretro, _info,
         load_no_content);
#endif
   if (!g_extern.core_info)
      return false;

   if (!core_info_list_get_info(g_extern.core_info,
            g_extern.core_info_current, g_settings.libretro))
      return false;

   return true;
}
