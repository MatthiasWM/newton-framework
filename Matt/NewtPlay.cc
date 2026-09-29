/*
 File: NewtPlay.cc

 NewtPlay: newtc as a Mac app that runs Newton packages. See NewtPlay.h.
 */

// FLTK first: the framework's headers #define names FLTK uses (OVERRIDE, ...)
#include <FL/Fl.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Menu_Button.H>
#include <FL/Fl_PNG_Image.H>
#include <FL/Fl_Preferences.H>
#include <FL/Fl_RGB_Image.H>
#include <FL/Fl_Sys_Menu_Bar.H>
#include <FL/fl_draw.H>
#include <FL/Fl_Native_File_Chooser.H>
#include <FL/fl_ask.H>
#include <FL/platform.H>   // fl_open_display()

#include "Matt/NewtPlay.h"

#include <mach-o/dyld.h>
#include <sys/stat.h>
#include <dirent.h>
#include <spawn.h>
#include <climits>
#include <ctime>
#include <unistd.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <memory>

#ifndef NEWTPLAY_VERSION
#define NEWTPLAY_VERSION "0.1"
#endif

extern char ** environ;

namespace newtplay {

namespace {

// The package that runs (RunPackage), for CheckStarted.
std::string gPackagePath, gPackageName, gStorePath;

// Files the Finder opens with NewtPlay (a double click, a drop on the app or
// its Dock icon, Open With): macOS starts the app, then sends them as
// events (FLTK: fl_open_callback). Before a package runs they are the ones
// to run (gOpened); after that each goes to a NewtPlay of its own.
std::vector<std::string> gOpened;
bool gRunning = false;

// A NewtPlay of its own for a package (the same app: `open -n`; outside a
// bundle, the program itself).
void RunInOtherNewtPlay(const std::string & inPath)
{
  char exe[PATH_MAX], real[PATH_MAX];
  uint32_t size = sizeof(exe);
  if (_NSGetExecutablePath(exe, &size) != 0 || realpath(exe, real) == nullptr)
    return;
  std::string program(real);
  size_t macos = program.rfind("/Contents/MacOS/");
  std::vector<std::string> args;
  if (macos != std::string::npos) {
    // (open starts it without our environment: its HOME, for its stores)
    args = { "/usr/bin/open", "-n", "-a", program.substr(0, macos) };
    for (const char * name : { "HOME", "NEWTPLAY_TEST_ANSWER", "NEWTPLAY_TEST_CHOICE" })
      if (const char * value = getenv(name))   // (the test hooks too)
        args.insert(args.end(), { "--env", std::string(name) + "=" + value });
    args.insert(args.end(), { "--args", inPath });
  }
  else
    args = { program, inPath };
  std::vector<char *> argv;
  for (std::string & arg : args)
    argv.push_back(&arg[0]);
  argv.push_back(nullptr);
  pid_t pid;
  if (posix_spawn(&pid, argv[0], nullptr, nullptr, argv.data(), environ) != 0)
    fprintf(stderr, "NewtPlay: can't start another NewtPlay for %s\n", inPath.c_str());
}

void Opened(const char * inPath)
{
  if (gRunning)
    RunInOtherNewtPlay(inPath);
  else
    gOpened.push_back(inPath);
}

bool IsFile(const std::string & inPath)
{
  struct stat info;
  return stat(inPath.c_str(), &info) == 0 && S_ISREG(info.st_mode);
}

bool HasPackageExtension(const std::string & inName)
{
  for (const char * ext : { ".nspkg", ".newtonpkg", ".pkg" }) {
    size_t n = strlen(ext);
    if (inName.size() > n && strcasecmp(inName.c_str() + inName.size() - n, ext) == 0)
      return true;
  }
  return false;
}

// The package in the app's own bundle (Contents/Resources), if there is
// one: the first by name.
std::string BundledPackage()
{
  char path[PATH_MAX];
  uint32_t size = sizeof(path);
  if (_NSGetExecutablePath(path, &size) != 0)
    return {};
  char real[PATH_MAX];
  if (realpath(path, real) == nullptr)
    return {};
  std::string exe(real);                  // .../X.app/Contents/MacOS/X
  size_t macos = exe.rfind("/Contents/MacOS/");
  if (macos == std::string::npos)
    return {};                            // not in a bundle
  std::string resources = exe.substr(0, macos) + "/Contents/Resources";
  std::vector<std::string> found;
  if (DIR * dir = opendir(resources.c_str())) {
    while (dirent * entry = readdir(dir))
      if (HasPackageExtension(entry->d_name))
        found.push_back(resources + "/" + entry->d_name);
    closedir(dir);
  }
  std::sort(found.begin(), found.end());
  return found.empty() ? std::string() : found.front();
}

// The user chooses a package (the splash window's buttons, File > Open,
// Make Shortcut, Make App). Empty if cancelled.
std::string ChoosePackage(const char * inTitle = "Run a Newton Package")
{
  fl_open_display();
  for (int i = 0; i < 5; ++i)
    Fl::wait(0.02);   // (let the app finish launching: before that, the panel doesn't show)
  // for tests: the choice, without the panel (all else as for a user)
  if (const char * choice = getenv("NEWTPLAY_TEST_CHOICE"))
    return choice;
  Fl_Native_File_Chooser chooser;
  chooser.title(inTitle);
  chooser.type(Fl_Native_File_Chooser::BROWSE_FILE);
  chooser.filter("Newton Packages\t*.{nspkg,newtonpkg,pkg}");
  if (chooser.show() != 0 || chooser.filename() == nullptr)
    return {};
  return chooser.filename();
}

// Tell the user (NewtPlay runs from the Finder: there is no terminal).
void Tell(const char * inText, const std::string & inPath)
{
  fprintf(stderr, "NewtPlay: %s: %s\n", inText, inPath.c_str());
  fl_open_display();
  fl_message_title("NewtPlay");
  const char * file = strrchr(inPath.c_str(), '/');
  fl_alert("%s:\n%s", inText, file ? file + 1 : inPath.c_str());
}

// The app's folder (.../NewtPlay.app), or empty outside a bundle.
std::string BundlePath()
{
  char exe[PATH_MAX], real[PATH_MAX];
  uint32_t size = sizeof(exe);
  if (_NSGetExecutablePath(exe, &size) != 0 || realpath(exe, real) == nullptr)
    return {};
  std::string program(real);
  size_t macos = program.rfind("/Contents/MacOS/");
  return macos == std::string::npos ? std::string() : program.substr(0, macos);
}


/* The history: the packages run, newest first, at most kRecent
   (~/Library/Preferences/newton-framework.org/NewtPlay.prefs). */

const int kRecent = 10;

struct Recent
{
  std::string path, name;
  unsigned long version = 0;
};

std::vector<Recent> ReadRecent()
{
  Fl_Preferences prefs(Fl_Preferences::USER_L, "newton-framework.org", "NewtPlay");
  Fl_Preferences recent(prefs, "recent");
  std::vector<Recent> list;
  for (int i = 0; i < recent.groups(); ++i) {
    Fl_Preferences entry(recent, recent.group(i));
    char * path = nullptr, * name = nullptr;
    int version = 0;
    entry.get("path", path, "");
    entry.get("name", name, "");
    entry.get("version", version, 0);
    if (path && *path)
      list.push_back({ path, name ? name : "", (unsigned long)version });
    free(path);
    free(name);
  }
  return list;
}

void WriteRecent(const std::vector<Recent> & inList)
{
  Fl_Preferences prefs(Fl_Preferences::USER_L, "newton-framework.org", "NewtPlay");
  prefs.delete_group("recent");
  Fl_Preferences recent(prefs, "recent");
  for (size_t i = 0; i < inList.size() && i < size_t(kRecent); ++i) {
    char key[8];
    snprintf(key, sizeof(key), "%02d", int(i));
    Fl_Preferences entry(recent, key);
    entry.set("path", inList[i].path.c_str());
    entry.set("name", inList[i].name.c_str());
    entry.set("version", int(inList[i].version));
  }
  prefs.flush();
}

void AddRecent(const Recent & inRecent)
{
  std::vector<Recent> list = ReadRecent();
  list.erase(std::remove_if(list.begin(), list.end(),
                            [&](const Recent & r) { return r.path == inRecent.path; }), list.end());
  list.insert(list.begin(), inRecent);
  WriteRecent(list);
}

void ForgetRecent(const std::string & inPath)
{
  std::vector<Recent> list = ReadRecent();
  list.erase(std::remove_if(list.begin(), list.end(),
                            [&](const Recent & r) { return r.path == inPath; }), list.end());
  WriteRecent(list);
}

// A history entry as a menu item: "Battleship:ATOW (v8) - Battleship2.5.pkg"
// (FLTK's menus take / as a submenu and & as a shortcut: escaped).
std::string RecentLabel(const Recent & inRecent)
{
  const char * file = strrchr(inRecent.path.c_str(), '/');
  std::string text = inRecent.name + " (v" + std::to_string(inRecent.version) + ") - "
                   + (file ? file + 1 : inRecent.path.c_str());
  std::string label;
  for (char c : text) {
    if (c == '/' || c == '\\' || c == '&' || c == '_')
      label += '\\';
    label += c;
  }
  return label;
}


/* Choosing a package: before one runs (the splash window), it is the one to
   run (gChoice); while one runs (the menu bar), it gets a NewtPlay of its
   own. */

std::string gChoice;

void Choose(const std::string & inPath)
{
  if (inPath.empty())
    return;
  if (!IsFile(inPath)) {
    Tell("NewtPlay can't find this package any more", inPath);
    ForgetRecent(inPath);
    return;
  }
  if (gRunning)
    RunInOtherNewtPlay(inPath);
  else
    gChoice = inPath;
}

void ChooseRecent(Fl_Widget *, void * inPath)
{
  Choose(*static_cast<std::string *>(inPath));
}

void ChooseFile(Fl_Widget * = nullptr, void * = nullptr)
{
  Choose(ChoosePackage());
}

// Make Shortcut, Make App (inApp): for the package that runs, else for one
// the user chooses; the Finder shows it.
void MakeFor(bool inApp)
{
  std::string package = gRunning ? gPackagePath
                      : ChoosePackage(inApp ? "Make an App for a Newton Package"
                                            : "Make a Shortcut for a Newton Package");
  if (package.empty())
    return;
  std::string bundle, error;
  if (MakeBundle(package, inApp, &bundle, &error))
    ShowInFinder(bundle);
  else if (!error.empty())
    Tell(error.c_str(), package);
}

void MakeShortcut(Fl_Widget * = nullptr, void * = nullptr)
{
  MakeFor(false);
}

void MakeApp(Fl_Widget * = nullptr, void * = nullptr)
{
  MakeFor(true);
}

// A history menu: its items (the paths kept for the callbacks).
void FillRecentMenu(Fl_Menu_ * ioMenu, const char * inPrefix, std::vector<std::unique_ptr<std::string>> & ioPaths)
{
  ioPaths.clear();
  for (const Recent & recent : ReadRecent()) {
    ioPaths.push_back(std::make_unique<std::string>(recent.path));
    ioMenu->add((std::string(inPrefix) + RecentLabel(recent)).c_str(), 0, ChooseRecent, ioPaths.back().get());
  }
}


/* About NewtPlay: its version, and who made what it is built on. */

const char * kCredits =
  "NewtPlay runs packages for the Apple Newton. It is newtc,\n"
  "the NewtonScript compiler and runtime of newton-framework,\n"
  "a reimplementation of the Newton OS (Simon Bell), with the\n"
  "Newton views drawn by FLTK (fltk.org).\n\n"
  "NewtPlay and newtc: Matthias Melcher.\n"
  "Newton and MessagePad are trademarks of Apple.\n"
  "NewtPlay is not made by Apple.";

void ShowAbout(Fl_Widget * = nullptr, void * = nullptr)
{
  fl_message_title("About NewtPlay");
  fl_message("NewtPlay %s\n\n%s", NEWTPLAY_VERSION, kCredits);
}


/* The menu bar: File > Open..., Open Recent, Make Shortcut..., Make App...;
   NewtPlay > About NewtPlay. */

Fl_Sys_Menu_Bar * gMenuBar = nullptr;
std::vector<std::unique_ptr<std::string>> gMenuPaths;

void UpdateMenuBar()
{
  if (gMenuBar == nullptr)
    return;
  gMenuBar->clear();
  gMenuBar->add("&File/&Open a Package...", FL_COMMAND + 'o', ChooseFile);
  FillRecentMenu(gMenuBar, "&File/Open &Recent/", gMenuPaths);
  if (gMenuPaths.empty())
    gMenuBar->add("&File/Open &Recent/(none)", 0, nullptr, nullptr, FL_MENU_INACTIVE);
  // (while a package runs: for it)
  if (Fl_Menu_Item * recent = const_cast<Fl_Menu_Item *>(gMenuBar->find_item("&File/Open &Recent")))
    recent->flags |= FL_MENU_DIVIDER;
  gMenuBar->add("&File/Make &Shortcut...", 0, MakeShortcut);
  gMenuBar->add("&File/Make &App...", 0, MakeApp);
}

void MakeMenuBar()
{
  if (gMenuBar)
    return;
  fl_open_display();
  Fl_Group::current(nullptr);
  gMenuBar = new Fl_Sys_Menu_Bar(0, 0, 0, 0);
  fl_mac_set_about(ShowAbout, nullptr);
  UpdateMenuBar();
}


/* The splash window: NewtPlay started without a package. */

// NewtPlay's picture (Resources/NewtPlay.png in its bundle).
Fl_Image * Logo()
{
  static std::unique_ptr<Fl_PNG_Image> logo;
  if (!logo) {
    std::string path = BundlePath() + "/Contents/Resources/NewtPlay.png";
    if (IsFile(path))
      logo = std::make_unique<Fl_PNG_Image>(path.c_str());
  }
  return (logo && logo->fail() == 0) ? logo.get() : nullptr;
}

// For tests (NEWTPLAY_TEST_SPLASH=<file.png>): a picture of the window, then
// nothing chosen.
void SnapshotSplash(Fl_Window * inWindow, const char * inPath)
{
  for (int i = 0; i < 20; ++i)
    Fl::wait(0.05);
  std::unique_ptr<Fl_RGB_Image> image(fl_capture_window(inWindow, 0, 0, inWindow->w(), inWindow->h()));
  if (!image || fl_write_png(inPath, image.get()) != 0)
    fprintf(stderr, "NewtPlay: can't save the splash window as %s\n", inPath);
}

// The package to run: chosen, from the history, or opened by the Finder
// meanwhile (a drop on NewtPlay); empty: quit.
std::string Splash()
{
  gChoice.clear();
  Fl_Group::current(nullptr);
  Fl_Double_Window window(460, 316, "NewtPlay");
  window.color(FL_WHITE);
  Fl_Box logo(24, 24, 96, 96);
  logo.image(Logo());
  Fl_Box title(140, 22, 300, 40, "NewtPlay");
  title.labelsize(30);
  title.labelfont(FL_HELVETICA_BOLD);
  title.align(FL_ALIGN_INSIDE | FL_ALIGN_LEFT);
  std::string versionText = std::string("Version ") + NEWTPLAY_VERSION + ": plays Newton packages";
  Fl_Box version(140, 60, 300, 20, versionText.c_str());
  version.labelsize(13);
  version.align(FL_ALIGN_INSIDE | FL_ALIGN_LEFT);
  Fl_Box hint(140, 82, 300, 40,
              "Double-click a package (.nspkg, .newtonpkg), drop one on NewtPlay, or choose one here:");
  hint.labelsize(12);
  hint.align(FL_ALIGN_INSIDE | FL_ALIGN_LEFT | FL_ALIGN_TOP | FL_ALIGN_WRAP);
  Fl_Button run(140, 130, 150, 28, "Run a Package...");
  run.callback(ChooseFile);
  run.shortcut(FL_Enter);
  Fl_Menu_Button recent(300, 130, 140, 28, "Recent");
  std::vector<std::unique_ptr<std::string>> recentPaths;
  FillRecentMenu(&recent, "", recentPaths);
  if (recentPaths.empty())
    recent.deactivate();
  Fl_Button shortcut(140, 164, 150, 28, "Make a Shortcut...");
  shortcut.callback(MakeShortcut);
  shortcut.tooltip("A small app next to the package that runs it with NewtPlay");
  Fl_Button app(300, 164, 140, 28, "Make an App...");
  app.callback(MakeApp);
  app.tooltip("An app next to the package that runs it by itself (NewtPlay inside)");
  Fl_Box credits(24, 208, 416, 94, kCredits);
  credits.labelsize(11);
  credits.labelcolor(fl_rgb_color(96, 96, 96));
  credits.align(FL_ALIGN_INSIDE | FL_ALIGN_LEFT | FL_ALIGN_TOP | FL_ALIGN_WRAP);
  window.end();
  window.show();
  if (const char * snapshot = getenv("NEWTPLAY_TEST_SPLASH")) {
    SnapshotSplash(&window, snapshot);
    return {};
  }
  while (window.shown() && gChoice.empty() && gOpened.empty())
    Fl::wait();
  window.hide();
  if (gChoice.empty() && !gOpened.empty()) {
    gChoice = gOpened.front();
    gOpened.erase(gOpened.begin());
  }
  return gChoice;
}


// A package's arguments: newtc -store <its store> -pkg <file> -run
std::vector<std::string> RunPackage(const char * inProgram, const std::string & inPath)
{
  std::string name;
  unsigned long version = 0;
  if (!ReadPackageName(inPath, &name, &version)) {
    Tell("This is not a Newton package", inPath);
    return {};
  }
  if (name.empty()) {   // (a package without a name: its file's)
    const char * file = strrchr(inPath.c_str(), '/');
    name = file ? file + 1 : inPath;
  }
  std::vector<std::string> args = { inProgram };
  std::string store = StorePath(name, version);
  gPackagePath = inPath;
  gPackageName = name;
  gStorePath = store;
  AddRecent({ inPath, name, version });
  UpdateMenuBar();
  if (!store.empty()) {
    args.push_back("-store");
    args.push_back(store);
  }
  args.insert(args.end(), { "-pkg", inPath, "-run" });
  return args;
}

} // namespace


void CheckStarted(const std::string & inError)
{
  if (gPackagePath.empty() || Fl::first_window() != nullptr)
    return;
  fl_message_title("NewtPlay");
  // for tests: the button, without the alert (0 Quit, 1 Start with New Data)
  const char * answer = getenv("NEWTPLAY_TEST_ANSWER");
  if (inError.empty()) {
    fprintf(stderr, "NewtPlay: %s has nothing to show\n", gPackageName.c_str());
    if (answer == nullptr)
      fl_alert("%s has nothing to show: it opens no app.", gPackageName.c_str());
    return;
  }
  fprintf(stderr, "NewtPlay: %s stopped: %s\n", gPackageName.c_str(), inError.c_str());
  if (getenv("NEWTPLAY_NEW_DATA")) {   // started again with new data: not the data, then
    fprintf(stderr, "NewtPlay: %s stopped also with new data\n", gPackageName.c_str());
    if (answer == nullptr)
      fl_alert("%s stopped while it started, also with new data:\n%s", gPackageName.c_str(), inError.c_str());
    return;
  }
  int choice = answer ? atoi(answer) : fl_choice("%s stopped while it started:\n%s\n\n"
                         "Its saved data may come from another version of it. "
                         "Start it with new data? (The old data is kept.)",
                         "Quit", "Start with New Data", nullptr,
                         gPackageName.c_str(), inError.c_str());
  if (choice != 1 || gStorePath.empty())
    return;
  char stamp[32];
  time_t now = time(nullptr);
  strftime(stamp, sizeof(stamp), "%Y%m%d-%H%M%S", localtime(&now));
  rename(gStorePath.c_str(), (gStorePath + ".old-" + stamp).c_str());
  // again, with the package (not through atexit: the store as it is now
  // mustn't be saved over the new one)
  char program[PATH_MAX];
  uint32_t size = sizeof(program);
  setenv("NEWTPLAY_NEW_DATA", "1", 1);   // (if it stops again, it isn't its data)
  if (_NSGetExecutablePath(program, &size) == 0)
    execl(program, program, gPackagePath.c_str(), (char *)nullptr);
  fprintf(stderr, "NewtPlay: can't start again\n");
}


bool ReadPackageName(const std::string & inPath, std::string * outName, unsigned long * outVersion)
{
  outName->clear();
  if (outVersion)
    *outVersion = 0;
  FILE * f = fopen(inPath.c_str(), "rb");
  if (f == nullptr)
    return false;
  unsigned char header[52];
  bool ok = fread(header, 1, sizeof(header), f) == sizeof(header)
         && (memcmp(header, "package0", 8) == 0 || memcmp(header, "package1", 8) == 0);
  if (ok) {
    // the name: an InfoRef (offset, length; big-endian) into the data after
    // the part entries (32 bytes each), in UTF-16
    auto word = [&](int at) { return (header[at] << 8) | header[at + 1]; };
    auto lword = [&](int at) { return (unsigned long)(word(at) << 16 | word(at + 2)); };
    if (outVersion)
      *outVersion = lword(16);   // the package's version number
    unsigned long numParts = lword(48);
    unsigned long offset = word(24), length = word(26);
    unsigned long at = 52 + numParts * 32 + offset;
    std::vector<unsigned char> name(length);
    if (numParts < 1000 && length > 0 && length < 512 && fseek(f, long(at), SEEK_SET) == 0
        && fread(name.data(), 1, length, f) == length) {
      for (unsigned long i = 0; i + 1 < length; i += 2) {
        unsigned ch = (name[i] << 8) | name[i + 1];
        if (ch == 0)
          break;
        *outName += ch < 128 ? char(ch) : '_';
      }
    }
  }
  fclose(f);
  return ok;
}


std::string StorePath(const std::string & inPackageName, unsigned long inVersion)
{
  const char * home = getenv("HOME");
  if (home == nullptr)
    return {};
  // a name safe for a file name ("Battleship:ATOW" -> "Battleship_ATOW")
  std::string name;
  for (char c : inPackageName)
    name += (isalnum((unsigned char)c) || c == ' ' || c == '.' || c == '-' || c == '_') ? c : '_';
  if (name.empty() || name[0] == '.')
    name = "_" + name;
  std::string folder = std::string(home);
  for (const char * sub : { "/Library", "/Application Support", "/NewtPlay" }) {
    folder += sub;
    mkdir(folder.c_str(), 0755);
  }
  folder += "/" + name;
  mkdir(folder.c_str(), 0755);
  return folder + "/" + name + "-v" + std::to_string(inVersion) + ".store";
}


std::vector<std::string> Arguments(int argc, char ** argv)
{
  // a shortcut or an app for a package, from the command line (tests)
  if (argc == 3 && (strcmp(argv[1], "-make-shortcut") == 0 || strcmp(argv[1], "-make-app") == 0)) {
    std::string bundle, error;
    if (MakeBundle(argv[2], strcmp(argv[1], "-make-app") == 0, &bundle, &error))
      printf("%s\n", bundle.c_str());
    else
      fprintf(stderr, "NewtPlay: %s\n", error.empty() ? "cancelled" : error.c_str());
    return {};
  }
  // a package given: NewtPlay game.pkg (also `open -a NewtPlay --args ...`)
  if (argc == 2 && argv[1][0] != '-' && IsFile(argv[1])) {
    fl_open_callback(Opened);   // (more from the Finder: NewtPlays of their own)
    MakeMenuBar();
    std::vector<std::string> args = RunPackage(argv[0], argv[1]);
    gRunning = !args.empty();
    return args;
  }
  // from the Finder (older macOS added -psn_...): a package the Finder
  // opens with NewtPlay (its event comes right after the start), else the
  // bundle's package, else the user's choice
  if (argc == 1 || (argc == 2 && strncmp(argv[1], "-psn_", 5) == 0)) {
    fl_open_callback(Opened);
    MakeMenuBar();
    for (int i = 0; i < 20 && gOpened.empty(); ++i)
      Fl::wait(0.05);
    std::string package;
    if (!gOpened.empty()) {
      package = gOpened.front();
      gOpened.erase(gOpened.begin());
    }
    if (package.empty())
      package = BundledPackage();
    if (package.empty())   // (tests choose without the window)
      package = getenv("NEWTPLAY_TEST_CHOICE") ? ChoosePackage() : Splash();
    if (package.empty())
      return {};
    std::vector<std::string> args = RunPackage(argv[0], package);
    gRunning = !args.empty();
    for (const std::string & more : gOpened)   // (several opened at once)
      RunInOtherNewtPlay(more);
    gOpened.clear();
    return args;
  }
  // newtc
  return std::vector<std::string>(argv, argv + argc);
}

} // namespace newtplay
