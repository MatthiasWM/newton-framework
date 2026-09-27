/*
 File: Boxtypes.h

 FLTK box types for the NewtonOS look (frames and backgrounds of views),
 registered with Fl::set_boxtype(). RegisterBoxtypes() registers them once;
 OpenView() (Links.h) calls it before the first view (and so the first
 window) opens.

 To add one: a number in the enum below (after the last one), its draw
 function in Boxtypes.cc, and an Fl::set_boxtype() call in
 RegisterBoxtypes().
 */

#ifndef HOST_FLTK_BOXTYPES_H
#define HOST_FLTK_BOXTYPES_H

// FLTK first: the framework's headers #define names FLTK uses (OVERRIDE, ...)
#include <FL/Enumerations.H>

namespace nfl {

/** The box types, from FLTK's first free one on. */
enum : int {
  FLOATER_BOX_ = FL_FREE_BOXTYPE,   // the frame of a floating view (protoFloater)
  LAST_BOX_
};

const Fl_Boxtype FLOATER_BOX = Fl_Boxtype(FLOATER_BOX_);

/** Register the box types with FLTK (only the first call does). */
void RegisterBoxtypes();

} // namespace nfl

#endif // HOST_FLTK_BOXTYPES_H
