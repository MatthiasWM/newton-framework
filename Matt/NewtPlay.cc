/*
 File: NewtPlay.cc

 NewtPlay: newtc as a Mac app that runs Newton packages. See NewtPlay.h.
 */

// FLTK first: the framework's headers #define names FLTK uses (OVERRIDE, ...)
#include <FL/Fl.H>
#include <FL/Fl_Native_File_Chooser.H>
#include <FL/fl_ask.H>
#include <FL/platform.H>   // fl_open_display()

#include "Matt/NewtPlay.h"

#include <mach-o/dyld.h>
#include <sys/stat.h>
#include <dirent.h>
#include <climits>
#include <ctime>
#include <unistd.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>

namespace newtplay {

namespace {

// The package that runs (RunPackage), for CheckStarted.
std::string gPackagePath, gPackageName, gStorePath;

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

// The user chooses a package (the splash window's Run, 11.3, will call
// this too). Empty if cancelled.
std::string ChoosePackage()
{
  fl_open_display();
  for (int i = 0; i < 5; ++i)
    Fl::wait(0.02);   // (let the app finish launching: before that, the panel doesn't show)
  // for tests: the choice, without the panel (all else as for a user)
  if (const char * choice = getenv("NEWTPLAY_TEST_CHOICE"))
    return choice;
  Fl_Native_File_Chooser chooser;
  chooser.title("Run a Newton Package");
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
  std::string folder = std::string(home) + "/Library/Application Support/NewtPlay";
  mkdir(folder.c_str(), 0755);
  folder += "/" + name;
  mkdir(folder.c_str(), 0755);
  return folder + "/" + name + "-v" + std::to_string(inVersion) + ".store";
}


std::vector<std::string> Arguments(int argc, char ** argv)
{
  // a package given: NewtPlay game.pkg (also `open -a NewtPlay --args ...`)
  if (argc == 2 && argv[1][0] != '-' && IsFile(argv[1]))
    return RunPackage(argv[0], argv[1]);
  // from the Finder (older macOS added -psn_...): the bundle's package, or
  // the user's choice
  if (argc == 1 || (argc == 2 && strncmp(argv[1], "-psn_", 5) == 0)) {
    std::string package = BundledPackage();
    if (package.empty())
      package = ChoosePackage();
    if (package.empty())
      return {};
    return RunPackage(argv[0], package);
  }
  // newtc
  return std::vector<std::string>(argv, argv + argc);
}

} // namespace newtplay
