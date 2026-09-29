/*
 File: NewtPlayMake.cc

 NewtPlay's shortcuts and apps (11.4): a bundle of its own for a package,
 next to it. See MakeBundle() in NewtPlay.h.
 */

// FLTK first: the framework's headers #define names FLTK uses (OVERRIDE, ...)
#include <FL/Fl.H>
#include <FL/Fl_PNG_Image.H>
#include <FL/fl_ask.H>

#include "Matt/NewtPlay.h"

#include <mach-o/dyld.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <spawn.h>
#include <unistd.h>
#include <climits>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <fstream>
#include <regex>
#include <sstream>

extern char ** environ;

namespace newtplay {

namespace {

// Run a program, wait for it; its output if asked. Its exit status (-1: it
// didn't start).
int RunTool(const std::vector<std::string> & inArgs, std::string * outOutput = nullptr)
{
  std::vector<char *> argv;
  for (const std::string & arg : inArgs)
    argv.push_back(const_cast<char *>(arg.c_str()));
  argv.push_back(nullptr);
  posix_spawn_file_actions_t actions;
  posix_spawn_file_actions_init(&actions);
  int fds[2] = { -1, -1 };
  if (outOutput) {
    if (pipe(fds) != 0)
      return -1;
    posix_spawn_file_actions_adddup2(&actions, fds[1], STDOUT_FILENO);
    posix_spawn_file_actions_addclose(&actions, fds[0]);
  } else
    posix_spawn_file_actions_addopen(&actions, STDOUT_FILENO, "/dev/null", O_WRONLY, 0);
  posix_spawn_file_actions_addopen(&actions, STDERR_FILENO, "/dev/null", O_WRONLY, 0);
  pid_t pid;
  int started = posix_spawn(&pid, argv[0], &actions, nullptr, argv.data(), environ);
  posix_spawn_file_actions_destroy(&actions);
  if (outOutput) {
    close(fds[1]);
    if (started == 0) {
      char buffer[4096];
      ssize_t n;
      while ((n = read(fds[0], buffer, sizeof(buffer))) > 0)
        outOutput->append(buffer, size_t(n));
    }
    close(fds[0]);
  }
  if (started != 0)
    return -1;
  int status = 0;
  waitpid(pid, &status, 0);
  return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

std::string ExecutablePath()
{
  char exe[PATH_MAX], real[PATH_MAX];
  uint32_t size = sizeof(exe);
  if (_NSGetExecutablePath(exe, &size) != 0 || realpath(exe, real) == nullptr)
    return {};
  return real;
}

bool Exists(const std::string & inPath)
{
  struct stat info;
  return stat(inPath.c_str(), &info) == 0;
}

bool CopyFile(const std::string & inFrom, const std::string & inTo, mode_t inMode)
{
  std::ifstream from(inFrom, std::ios::binary);
  std::ofstream to(inTo, std::ios::binary | std::ios::trunc);
  to << from.rdbuf();
  bool ok = from.good() || from.eof();
  to.close();
  return ok && to.good() && chmod(inTo.c_str(), inMode) == 0;
}

bool WriteText(const std::string & inPath, const std::string & inText, mode_t inMode)
{
  std::ofstream out(inPath, std::ios::trunc);
  out << inText;
  out.close();
  return out.good() && chmod(inPath.c_str(), inMode) == 0;
}

std::string XMLEscaped(const std::string & inText)
{
  std::string out;
  for (char c : inText) {
    if (c == '&') out += "&amp;";
    else if (c == '<') out += "&lt;";
    else if (c == '>') out += "&gt;";
    else if (c == '"') out += "&quot;";
    else out += c;
  }
  return out;
}

// A name for a bundle identifier: letters, digits, dots and hyphens.
std::string IdentifierPart(const std::string & inName)
{
  std::string id;
  for (char c : inName)
    id += (isalnum((unsigned char)c) || c == '.' || c == '-') ? c : '-';
  return id.empty() ? std::string("package") : id;
}


/* The package's icon (a Newton bitmap), as NewtPlay's own apps have had it
   since nBattleship.app (cmake/make_app_icon.py): its pixels, big and crisp,
   on a rounded square of a Newton screen's green. */

// The icon frame, as newtc prints it: a copy of NewtPlay run as newtc prints
// the package's (its form part's) icon.
std::string PrintedIcon(const std::string & inPackage)
{
  std::string output;
  RunTool({ ExecutablePath(), "-pkg", inPackage, "-s",
            "foreach part in ref0.part do "
            "  if IsFrame(part.data) and IsFrame(part.data.icon) then Print(part.data.icon);" },
          &output);
  return output;
}

// Its pixels: rows of booleans, only the part that has any; empty if none.
std::vector<std::vector<bool>> IconBits(const std::string & inPrinted)
{
  std::smatch m;
  if (!std::regex_search(inPrinted, m, std::regex("bits:\\s*MakeBinaryFromHex\\(\"([0-9A-Fa-f]+)\"")))
    return {};
  std::string hex = m[1];
  std::vector<unsigned char> data;
  for (size_t i = 0; i + 1 < hex.size(); i += 2)
    data.push_back((unsigned char)strtol(hex.substr(i, 2).c_str(), nullptr, 16));
  if (data.size() < 16)
    return {};
  auto word = [&](size_t at) { return int(short((data[at] << 8) | data[at + 1])); };
  int rowBytes = word(4), top = word(8), left = word(10), bottom = word(12), right = word(14);
  int w = right - left, h = bottom - top;
  if (rowBytes <= 0 || w <= 0 || h <= 0 || data.size() < size_t(16 + rowBytes * h))
    return {};
  std::vector<std::vector<bool>> rows(size_t(h), std::vector<bool>(size_t(w), false));
  int x0 = w, x1 = -1, y0 = h, y1 = -1;
  for (int y = 0; y < h; ++y)
    for (int x = 0; x < w; ++x)
      if (data[16 + size_t(y * rowBytes) + size_t(x >> 3)] & (0x80 >> (x & 7))) {
        rows[size_t(y)][size_t(x)] = true;
        x0 = std::min(x0, x); x1 = std::max(x1, x);
        y0 = std::min(y0, y); y1 = std::max(y1, y);
      }
  if (x1 < 0)
    return {};
  std::vector<std::vector<bool>> ink;
  for (int y = y0; y <= y1; ++y)
    ink.emplace_back(rows[size_t(y)].begin() + x0, rows[size_t(y)].begin() + x1 + 1);
  return ink;
}

// The icon at size by size (RGBA): the rounded square (80% of it, as macOS
// draws app icons), the bitmap in its middle as big as fits in whole pixels.
std::vector<unsigned char> IconPixels(int inSize, const std::vector<std::vector<bool>> & inBits)
{
  const unsigned char paper[3] = { 190, 201, 170 }, ink[3] = { 28, 34, 24 }, edgeColor[3] = { 84, 94, 72 };
  double margin = std::round(inSize * 0.1), side = inSize - 2 * margin, radius = side * 0.225;
  double edge = std::max(1.0, std::round(inSize / 128.0));
  int bh = int(inBits.size()), bw = int(inBits[0].size());
  int scale = std::max(1, int(side * 0.78) / std::max(bw, bh));
  int ox = (inSize - bw * scale) / 2, oy = (inSize - bh * scale) / 2;
  std::vector<unsigned char> rgba(size_t(inSize) * inSize * 4, 0);
  for (int y = 0; y < inSize; ++y)
    for (int x = 0; x < inSize; ++x) {
      double cx = std::min(std::max(x + 0.5, margin + radius), margin + side - radius);
      double cy = std::min(std::max(y + 0.5, margin + radius), margin + side - radius);
      double d = std::hypot(x + 0.5 - cx, y + 0.5 - cy) - radius;
      if (d > 0.5)
        continue;
      unsigned char * p = &rgba[(size_t(y) * inSize + x) * 4];
      const unsigned char * color = d > -edge ? edgeColor : paper;
      int bx = (x - ox) / scale, by = (y - oy) / scale;
      if (x >= ox && y >= oy && bx < bw && by < bh && inBits[size_t(by)][size_t(bx)])
        color = ink;
      p[0] = color[0]; p[1] = color[1]; p[2] = color[2];
      p[3] = d <= -0.5 ? 255 : (unsigned char)std::lround(255 * (0.5 - d));
    }
  return rgba;
}

// The package's icon as an .icns (iconutil, part of macOS). False if the
// package has none.
bool WriteIcon(const std::string & inPackage, const std::string & inIcns)
{
  std::vector<std::vector<bool>> bits = IconBits(PrintedIcon(inPackage));
  if (bits.empty())
    return false;
  char folder[] = "/tmp/NewtPlayIcon.XXXXXX";
  if (mkdtemp(folder) == nullptr)
    return false;
  std::string iconset = std::string(folder) + "/icon.iconset";
  mkdir(iconset.c_str(), 0755);
  bool ok = true;
  for (int base : { 16, 32, 128, 256, 512 })
    for (int scale : { 1, 2 }) {
      int size = base * scale;
      std::string name = iconset + "/icon_" + std::to_string(base) + "x" + std::to_string(base)
                       + (scale == 2 ? "@2x" : "") + ".png";
      std::vector<unsigned char> rgba = IconPixels(size, bits);
      ok = ok && fl_write_png(name.c_str(), rgba.data(), size, size, 4) == 0;
    }
  ok = ok && RunTool({ "/usr/bin/iconutil", "-c", "icns", iconset, "-o", inIcns }) == 0;
  RunTool({ "/bin/rm", "-rf", folder });
  return ok;
}

// The Info.plist of a shortcut or an app.
std::string InfoPlist(const std::string & inName, const std::string & inExecutable, const std::string & inIdentifier,
                      const std::string & inPackageName, unsigned long inVersion, bool inHasIcon)
{
  std::string name = XMLEscaped(inName);
  std::string version = std::to_string(inVersion);
  return
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
    "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
    "<!-- made by NewtPlay (Matt/NewtPlayMake.cc) -->\n"
    "<plist version=\"1.0\">\n<dict>\n"
    "  <key>CFBundleExecutable</key>\n  <string>" + XMLEscaped(inExecutable) + "</string>\n"
    "  <key>CFBundleIdentifier</key>\n  <string>" + inIdentifier + "</string>\n"
    "  <key>CFBundleInfoDictionaryVersion</key>\n  <string>6.0</string>\n"
    "  <key>CFBundleName</key>\n  <string>" + name + "</string>\n"
    "  <key>CFBundleDisplayName</key>\n  <string>" + name + "</string>\n"
    + (inHasIcon ? "  <key>CFBundleIconFile</key>\n  <string>AppIcon</string>\n" : "") +
    "  <key>CFBundlePackageType</key>\n  <string>APPL</string>\n"
    "  <key>CFBundleShortVersionString</key>\n  <string>" + version + "</string>\n"
    "  <key>CFBundleVersion</key>\n  <string>" + version + "</string>\n"
    "  <key>NSHumanReadableCopyright</key>\n  <string>" + XMLEscaped(inPackageName) + ", played by NewtPlay</string>\n"
    "  <key>LSMinimumSystemVersion</key>\n  <string>13.0</string>\n"
    "  <key>NSHighResolutionCapable</key>\n  <true/>\n"
    "  <key>NSPrincipalClass</key>\n  <string>NSApplication</string>\n"
    "</dict>\n</plist>\n";
}

// A shortcut's program: NewtPlay (wherever it is) runs the package in the
// shortcut; without NewtPlay, it says so.
std::string ShortcutScript(const std::string & inPackageFile)
{
  return
    "#!/bin/sh\n"
    "# A NewtPlay shortcut (made by NewtPlay): NewtPlay runs the package in it.\n"
    "PKG=\"$(cd \"$(dirname \"$0\")/../Resources\" && pwd)/" + inPackageFile + "\"\n"
    "/usr/bin/open -n -b org.newton-framework.NewtPlay --env HOME=\"$HOME\" --args \"$PKG\" ||\n"
    "  /usr/bin/osascript -e 'display alert \"This shortcut needs NewtPlay\" message "
    "\"Install NewtPlay, then open the shortcut again.\"'\n";
}

// What a bundle is: "shortcut" or "app" (made by NewtPlay), else "app".
std::string BundleKind(const std::string & inBundle)
{
  std::ifstream plist(inBundle + "/Contents/Info.plist");
  std::stringstream text;
  text << plist.rdbuf();
  return text.str().find("org.newton-framework.shortcut.") != std::string::npos ? "shortcut" : "app";
}

} // namespace


bool MakeBundle(const std::string & inPackage, bool inApp, std::string * outBundle, std::string * outError)
{
  outError->clear();
  std::string name;
  unsigned long version = 0;
  if (!ReadPackageName(inPackage, &name, &version)) {
    *outError = "This is not a Newton package";
    return false;
  }
  // next to the package; for one in a shortcut or an app, next to that
  std::string folder = inPackage.substr(0, inPackage.rfind('/'));
  const std::string resourcesPath = ".app/Contents/Resources";
  size_t inBundle = folder.rfind(resourcesPath);
  if (inBundle != std::string::npos && inBundle + resourcesPath.size() == folder.size())
    folder = folder.substr(0, folder.rfind('/', inBundle));
  std::string file = inPackage.substr(inPackage.rfind('/') + 1);
  std::string base = file.substr(0, file.rfind('.'));
  if (base.empty())
    base = file;
  std::string bundle = folder + "/" + base + ".app";
  *outBundle = bundle;
  const char * kind = inApp ? "app" : "shortcut";

  // what goes in, first (the package and the program may be in the bundle
  // it replaces: made again from a shortcut or an app)
  char work[] = "/tmp/NewtPlayMake.XXXXXX";
  if (mkdtemp(work) == nullptr) {
    *outError = "NewtPlay can't make a folder in /tmp";
    return false;
  }
  std::string package = std::string(work) + "/package", program = std::string(work) + "/program";
  std::string icon = std::string(work) + "/AppIcon.icns";
  auto done = [&](bool inOk) { RunTool({ "/bin/rm", "-rf", work }); return inOk; };
  if (!CopyFile(inPackage, package, 0644) || (inApp && !CopyFile(ExecutablePath(), program, 0755))) {
    *outError = "NewtPlay can't read the package or itself";
    return done(false);
  }
  bool hasIcon = WriteIcon(package, icon);

  if (Exists(bundle)) {
    std::string there = BundleKind(bundle);
    const char * answer = getenv("NEWTPLAY_TEST_ANSWER");   // (tests: 0 Cancel, 1 Replace)
    int replace = answer ? atoi(answer)
                         : fl_choice("There is already an %s named \"%s.app\" next to the package.\n"
                                     "Replace it with a new %s?", "Cancel", "Replace", nullptr,
                                     there.c_str(), base.c_str(), kind);
    if (replace != 1)
      return done(false);
    if (RunTool({ "/bin/rm", "-rf", bundle }) != 0 || Exists(bundle)) {
      *outError = "NewtPlay can't replace " + base + ".app";
      return done(false);
    }
  }

  std::string contents = bundle + "/Contents";
  std::string resources = contents + "/Resources", macos = contents + "/MacOS";
  std::string packageFile = base + ".nspkg";
  bool ok = mkdir(bundle.c_str(), 0755) == 0 && mkdir(contents.c_str(), 0755) == 0
         && mkdir(resources.c_str(), 0755) == 0 && mkdir(macos.c_str(), 0755) == 0
         && CopyFile(package, resources + "/" + packageFile, 0644)
         && (!hasIcon || CopyFile(icon, resources + "/AppIcon.icns", 0644));
  if (ok)
    ok = inApp ? CopyFile(program, macos + "/" + base, 0755)
               : WriteText(macos + "/" + base, ShortcutScript(packageFile), 0755);
  std::string identifier = std::string("org.newton-framework.") + kind + "." + IdentifierPart(base);
  ok = ok && WriteText(contents + "/Info.plist", InfoPlist(base, base, identifier, name, version, hasIcon), 0644);
  if (!ok) {
    RunTool({ "/bin/rm", "-rf", bundle });
    *outError = std::string("NewtPlay can't make the ") + kind + " (can it write in the package's folder?)";
    return done(false);
  }
  // no quarantine or other attributes (codesign refuses them), signed ad hoc
  RunTool({ "/usr/bin/xattr", "-cr", bundle });
  if (RunTool({ "/usr/bin/codesign", "--force", "--sign", "-", bundle }) != 0)
    fprintf(stderr, "NewtPlay: can't sign %s\n", bundle.c_str());
  return done(true);
}


void ShowInFinder(const std::string & inPath)
{
  RunTool({ "/usr/bin/open", "-R", inPath });
}

} // namespace newtplay
