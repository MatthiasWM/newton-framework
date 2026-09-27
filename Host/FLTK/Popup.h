/*
 File: Popup.h

 Popup menus: DoPopup(items, bounds or left, top, context) (the ROM's
 PopupMenu and protoLabelPicker use it), as an FLTK menu
 (Fl_Menu_Item::popup).

 Items: strings; frames {item: text, mark: a character (shown checked),
 pickable: nil (grayed)}; 'pickSeparator, 'pickSolidSeparator (a line).
 The menu opens at the bounds' top left (or left, top), on the Newton
 display; a marked item is placed under the pen. The pick goes to the
 context after the current script (as on a Newton, where the popup is a
 view): context:pickActionScript(index) (the index in items), or
 context:pickCancelledScript().

 Not yet: icons, keyMessage items, the NewtonOS look.
 */

#ifndef HOST_FLTK_POPUP_H
#define HOST_FLTK_POPUP_H

#include "Frames/Objects.h"

namespace nfl {

Ref DoPopup(RefArg inItems, RefArg inWhere, RefArg inTop, RefArg inContext);

/** For tests (TestPick, Matt/TestWindow.h): a coming DoPopup picks this
    entry of its menu (not counting separators; nil: cancelled), without
    showing it; one answer per menu, in order. */
void TestPick(RefArg inEntry);

} // namespace nfl

#endif // HOST_FLTK_POPUP_H
