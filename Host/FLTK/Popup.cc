/*
 File: Popup.cc

 Popup menus. See Popup.h.
 */

// FLTK first: the framework's headers #define names FLTK uses (OVERRIDE, ...)
#include <FL/Fl.H>
#include <FL/Fl_Menu_Item.H>
#include <FL/Fl_Window.H>

#include "Host/FLTK/Popup.h"
#include "Host/FLTK/Links.h"
#include "Host/Timers.h"
#include "Matt/JSON.h"

#include "Frames/Frames.h"
#include "Frames/Lookup.h"
#include "Frames/Globals.h"
#include "ROMResources.h"

#include <deque>
#include <string>
#include <vector>

namespace nfl {

namespace {

// TestPick(): the next DoPopups' answers, without showing the menu (-1:
// cancelled; else an entry of the menu).
std::deque<int> gTestPicks;

long SlotInt(RefArg inFrame, const char * inSlot)
{
  Ref v = GetFrameSlot(inFrame, MakeSymbol(inSlot));
  return ISINT(v) ? RINT(v) : 0;
}

} // namespace


Ref DoPopup(RefArg inItems, RefArg inWhere, RefArg inTop, RefArg inContext)
{
  if (!IsArray(inItems))
    return NILREF;
  // the menu: labels (kept alive here), and which item each entry is
  std::vector<std::string> labels;
  std::vector<int> flags;
  std::vector<ArrayIndex> index;
  int initial = -1;
  for (ArrayIndex i = 0, n = Length(inItems); i < n; ++i) {
    RefVar item(GetArraySlot(inItems, i));
    if (IsSymbol(item)) {   // 'pickSeparator, 'pickSolidSeparator: a line
      if (!flags.empty())
        flags.back() |= FL_MENU_DIVIDER;
      continue;
    }
    std::string label;
    int f = 0;
    if (IsString(item))
      label = UTF8FromString(item);
    else if (IsFrame(item)) {
      RefVar text(GetFrameSlot(item, SYMA(item)));
      if (IsString(text))
        label = UTF8FromString(text);
      if (NOTNIL(GetFrameSlot(item, SYMA(mark)))) {
        f |= FL_MENU_TOGGLE | FL_MENU_VALUE;
        initial = int(labels.size());
      }
      if (FrameHasSlot(item, SYMA(pickable)) && ISNIL(GetFrameSlot(item, SYMA(pickable))))
        f |= FL_MENU_INACTIVE;
    } else
      continue;
    labels.push_back(DisplayText(label));
    flags.push_back(f);
    index.push_back(i);
  }
  if (labels.empty())
    return NILREF;
  std::vector<Fl_Menu_Item> menu(labels.size() + 1);
  for (size_t i = 0; i < labels.size(); ++i) {
    menu[i] = Fl_Menu_Item();
    menu[i].text = labels[i].c_str();
    menu[i].flags = flags[i];
    menu[i].labelfont_ = FL_HELVETICA_BOLD;
    menu[i].labelsize_ = 12;
  }
  menu.back() = Fl_Menu_Item();

  // where: on the Newton display, then on the screen
  long left = 0, top = 0;
  if (IsFrame(inWhere)) {
    left = SlotInt(inWhere, "left");
    top = SlotInt(inWhere, "top");
  } else if (ISINT(inWhere)) {
    left = RINT(inWhere);
    top = ISINT(inTop) ? RINT(inTop) : 0;
  }
  Link * link = IsFrame(inContext) ? Link::Of(inContext) : nullptr;
  int x = int(left), y = int(top);
  if (link)
    link->ScreenPoint(left, top, &x, &y);
  // popup() takes the position in the last event's window
  x -= Fl::event_x_root() - Fl::event_x();
  y -= Fl::event_y_root() - Fl::event_y();
  const Fl_Menu_Item * picked;
  if (!gTestPicks.empty()) {   // a test's answer (TestPick)
    int pick = gTestPicks.front();
    gTestPicks.pop_front();
    picked = (pick >= 0 && size_t(pick) < labels.size()) ? &menu[size_t(pick)] : nullptr;
  } else
    picked = menu[0].popup(x, y, nullptr, initial >= 0 ? &menu[size_t(initial)] : nullptr);

  // the pick, after the current script
  if (IsFrame(inContext)) {
    RefVar args(MakeArray(0));
    if (picked) {
      args = MakeArray(1);
      SetArraySlot(args, 0, MAKEINT(long(index[size_t(picked - &menu[0])])));
      FAddDeferredSend(RA(NILREF), inContext, MakeSymbol("pickActionScript"), args);
    } else
      FAddDeferredSend(RA(NILREF), inContext, MakeSymbol("pickCancelledScript"), args);
  }
  return NILREF;
}

void TestPick(RefArg inEntry)
{
  gTestPicks.push_back(ISINT(inEntry) ? int(RINT(inEntry)) : -1);
}

} // namespace nfl
