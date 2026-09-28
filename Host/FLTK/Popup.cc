/*
 File: Popup.cc

 Popup menus. See Popup.h.
 */

// FLTK first: the framework's headers #define names FLTK uses (OVERRIDE, ...)
#include <FL/Fl.H>
#include <FL/Fl_Menu_Item.H>
#include <FL/Fl_Menu_Button.H>
#include <FL/Fl_Window.H>
#include <FL/Fl_Multi_Label.H>
#include <FL/Fl_RGB_Image.H>
#include <FL/fl_draw.H>
#include <FL/Fl_PNG_Image.H>

#include "Host/FLTK/Popup.h"
#include "Host/FLTK/Links.h"
#include "Host/FLTK/Boxtypes.h"
#include "Host/FLTK/Widgets.h"
#include "Host/Timers.h"
#include "Matt/JSON.h"

#include "Frames/Frames.h"
#include "Frames/Lookup.h"
#include "Frames/Globals.h"
#include "ROMResources.h"

#include <cstdio>
#include <deque>
#include <memory>
#include <string>
#include <vector>

namespace nfl {

namespace {

// TestPick(): the next DoPopups' answers, without showing the menu (-1:
// cancelled; else an entry of the menu).
std::deque<int> gTestPicks;

// TestMenuSnapshot(): where the next DoPopup saves a picture of its menu.
std::string gMenuSnapshot;

// While the menu is up: save its picture, then close it (Escape: cancelled).
// The menu's window is the newest; it can be captured once it is shown.
void SnapshotMenu(void *)
{
  Fl_Window * window = Fl::first_window();
  std::unique_ptr<Fl_RGB_Image> image;
  if (window && window->menu_window() && window->shown())
    image.reset(fl_capture_window(window, 0, 0, window->w(), window->h()));
  if (!image) {
    Fl::repeat_timeout(0.05, SnapshotMenu);
    return;
  }
  if (fl_write_png(gMenuSnapshot.c_str(), image.get()) != 0)
    fprintf(stderr, "newtc: TestMenuSnapshot: can't write %s\n", gMenuSnapshot.c_str());
  gMenuSnapshot.clear();
  Fl::e_keysym = FL_Escape;
  Fl::handle(FL_KEYBOARD, window);
}

long SlotInt(RefArg inFrame, const char * inSlot)
{
  Ref v = GetFrameSlot(inFrame, MakeSymbol(inSlot));
  return ISINT(v) ? RINT(v) : 0;
}

} // namespace


/**
 * Pop up a menu, either at the mouse position, or relative to inContext.
 */
Ref DoPopup(RefArg inItems, RefArg inWhere, RefArg inTop, RefArg inContext)
{
  static Fl_Menu_Button* style_reference = nullptr;
  if (!style_reference) {
    auto* bak = Fl_Group::current();
    Fl_Group::current(nullptr);
    style_reference = new Fl_Menu_Button(0, 0, 1, 1);
    style_reference->hide();
    style_reference->box(nfl::UP_BOX);
    style_reference->color(FL_WHITE);
    Fl_Group::current(bak);
  }

  if (!IsArray(inItems))
    return NILREF;
  // the menu: labels (kept alive here), and which item each entry is
  std::vector<std::string> labels;
  std::vector<int> flags;
  std::vector<ArrayIndex> index;
  std::vector<std::unique_ptr<Fl_Image>> icons;
  int initial = -1;
  bool any_checked = false;
  // build the menu:
  for (ArrayIndex i = 0, n = Length(inItems); i < n; ++i) {
    RefVar item(GetArraySlot(inItems, i));
    if (IsSymbol(item)) {   // 'pickSeparator, 'pickSolidSeparator: a line
      if (!flags.empty())
        flags.back() |= FL_MENU_DIVIDER;
      continue;
    }
    std::string label;
    int f = 0;
    std::unique_ptr<Fl_Image> image;   // the item's icon, if it has one
    if (IsString(item))
      label = UTF8FromString(item);
    else if (IsFrame(item)) {
      RefVar text(GetFrameSlot(item, SYMA(item)));
      if (IsString(text))
        label = UTF8FromString(text);
      if (NOTNIL(GetFrameSlot(item, SYMA(mark)))) {
        f |= FL_MENU_TOGGLE | FL_MENU_VALUE;
        initial = int(labels.size());
        any_checked = true;
      }
      // A bitmap frame, as returned from the compile-time function GetPictAsBits
      RefVar icon(GetFrameSlot(item, SYMA(icon)));
      if (IsFrame(icon))
        image.reset(ToFlImage(ToNewtonBitmap(icon)));
      if (FrameHasSlot(item, SYMA(pickable)) && ISNIL(GetFrameSlot(item, SYMA(pickable))))
        f |= FL_MENU_INACTIVE;
    } else
      continue;
    labels.push_back(DisplayText(label));
    flags.push_back(f);
    index.push_back(i);
    icons.push_back(std::move(image));   // one per entry, nullptr for none
  }
  if (labels.empty())
    return NILREF;
  // an entry with an icon: the icon, then the text (kept alive here, as
  // Fl_Menu_Item doesn't own its label)
  std::vector<Fl_Multi_Label> multiLabels(labels.size());
  Fl::menu_linespacing(2);
  std::vector<Fl_Menu_Item> menu(labels.size() + 1);
  for (size_t i = 0; i < labels.size(); ++i) {
    menu[i] = Fl_Menu_Item();
    // If one item gets a check box, indent all of them
    menu[i].flags = flags[i] | (any_checked ? FL_MENU_TOGGLE : 0);
    menu[i].labelfont_ = FL_HELVETICA_BOLD;
    menu[i].labelsize_ = 11;
    menu[i].text = labels[i].c_str();
    // Handling icons is a bit more involved
    if (const Fl_Image * image = icons[i].get()) {
      multiLabels[i] = Fl_Multi_Label { (const char *)image, menu[i].text,
                                        (uchar)FL_IMAGE_LABEL, FL_NORMAL_LABEL };
      menu[i].multi_label(&multiLabels[i]);
    }
  }
  menu.back() = Fl_Menu_Item();

  // where: on the Newton display, then on the screen
  long left = 0, top = 0;
  bool uses_coords_frame = false;
  if (IsFrame(inWhere)) {
    left = SlotInt(inWhere, "left");
    top = SlotInt(inWhere, "top");
    uses_coords_frame = true;
  } else if (ISINT(inWhere)) {
    left = RINT(inWhere);
    top = ISINT(inTop) ? RINT(inTop) : 0;
  }

  // By default, we pop up where the mouse event occurred
  int x = Fl::event_x();
  int y = Fl::event_y();

  // If there is a widget that triggered this event, calculate better coordinates
  Link * link = IsFrame(inContext) ? Link::Of(inContext) : nullptr;
  Fl_Widget * link_widget = link ? link->Widget() : nullptr;
  if (link_widget) {
    x = link_widget->x() + left;
    y = link_widget->y() + top;
    // Make sure that the menu appears under the button
    if (uses_coords_frame) {
      y = std::max(y, link_widget->y() + link_widget->h());
    }
  }

  const Fl_Menu_Item * picked;
  if (!gTestPicks.empty()) {   // a test's answer (TestPick)
    int pick = gTestPicks.front();
    gTestPicks.pop_front();
    picked = (pick >= 0 && size_t(pick) < labels.size()) ? &menu[size_t(pick)] : nullptr;
  } else {
    // Horrible hack to make the check box look bearable.
    Fl::set_boxtype(FL_MAX_BOXTYPE, FL_DOWN_BOX);
    Fl::set_boxtype(FL_DOWN_BOX, FL_NO_BOX);
    if (!gMenuSnapshot.empty())   // a test's picture of the menu (TestMenuSnapshot)
      Fl::add_timeout(0.1, SnapshotMenu);
    // Don't set initial item. It'll override any positioning.
    picked = menu[0].popup(x, y, nullptr /* title */,
                           nullptr, // initial >= 0 ? &menu[size_t(initial)] : nullptr,
                           style_reference);
    Fl::set_boxtype(FL_DOWN_BOX, FL_MAX_BOXTYPE);
  }

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

void TestMenuSnapshot(RefArg inPath)
{
  gMenuSnapshot = IsString(inPath) ? UTF8FromString(inPath) : std::string();
}

} // namespace nfl
