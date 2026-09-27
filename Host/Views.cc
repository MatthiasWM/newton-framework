/*
 File: Views.cc

 Newton views: view frames. See Views.h.
 */

#include "Host/Views.h"
#include "Host/Root.h"
#include "Utilities/Unimplemented.h"
#if NEWTC_USES_FLTK
#include "Host/FLTK/Links.h"
#endif

#include "Frames/Frames.h"
#include "Frames/Lookup.h"
#include "Frames/Globals.h"
#include "ViewFlags.h"
#include "ROMResources.h"

namespace {

// View classes with this bit keep their template as data (realData) and get
// a stationery's form as their _proto (CView::buildContext).
const long kDataViewClass = 0x00010000;

// The view frames to clone (ROM, magic pointers): {_parent, _proto,
// viewCObject}, and the same with realData for data views.
const int kViewContext = 29;
const int kDataViewContext = 31;

long ViewClassNumber(RefArg inViewClass)
{
  return ISINT(inViewClass) ? RINT(inViewClass) : 0;
}

// A template's own frame to clone (its _cacheContext slot), if it has one
// (GetCacheContext, Views/View.cc).
Ref CacheContext(RefArg inProto)
{
  RefVar context(GetFrameSlot(inProto, SYMA(_cacheContext)));
  if (NOTNIL(context)) {
    context = Clone(context);
    SetFrameSlot(context, SYMA(_proto), inProto);
    SetFrameSlot(context, SYMA(_parent), RootView());
  }
  return context;
}

} // namespace


Ref BuildViewContext(RefArg inTemplate, bool inEvenIfHidden)
{
  RefVar viewClass(GetProtoVariable(inTemplate, SYMA(viewClass)));
  RefVar proto(inTemplate);
  RefVar data;
  bool isInk = false;

  // no view class: a stationery ('viewStationery, or ink), whose form in
  // stdForms has the view class
  if (ISNIL(viewClass)) {
    viewClass = GetProtoVariable(inTemplate, SYMA(viewStationery));
    if (ISNIL(viewClass) && NOTNIL(GetProtoVariable(inTemplate, SYMA(ink)))) {
      viewClass = SYMA(poly);
      isInk = true;
    }
    if (ISNIL(viewClass))
      ThrowErr(exRootException, -8502);   // no view class
    RefVar stdForms(GetGlobalVar(SYMA(stdForms)));
    RefVar stdForm(GetProtoVariable(stdForms, viewClass));
    if (ISNIL(stdForm))
      ThrowErr(exRootException, -8503);   // unknown view class
    viewClass = GetVariable(stdForm, SYMA(viewClass));
    data = inTemplate;
    if (ViewClassNumber(viewClass) & kDataViewClass) {
      if (ObjectFlags(proto) & kObjReadOnly)
        proto = Clone(proto);
      SetFrameSlot(proto, SYMA(_proto), stdForm);
    } else {
      proto = stdForm;
    }
  }

  if (!inEvenIfHidden) {
    RefVar viewFlags(GetProtoVariable(proto, SYMA(viewFlags)));
    if (ISNIL(viewFlags))
      ThrowErr(exRootException, -8504);   // no view flags
    if ((RINT(viewFlags) & vVisible) == 0)
      return NILREF;
  }

  RefVar context(CacheContext(proto));
  if (ViewClassNumber(viewClass) & kDataViewClass) {
    data = inTemplate;
    if (ISNIL(context))
      context = Clone(MAKEMAGICPTR(kDataViewContext));
  }
  if (ISNIL(context))
    context = Clone(MAKEMAGICPTR(kViewContext));
  if (isInk)
    SetFrameSlot(context, SYMA(viewStationery), SYMA(poly));
  SetFrameSlot(context, SYMA(_proto), proto);
  SetFrameSlot(context, SYMA(_parent), RootView());
  if (NOTNIL(data))
    SetFrameSlot(context, SYMA(realData), data);
  return context;
}


// BuildContext(template): the ROM builds even a hidden view (FBuildContext
// passes true).
Ref FBuildContext(RefArg rcvr, RefArg inTemplate)
{
  return BuildViewContext(inTemplate, true);
}


#if NEWTC_USES_FLTK

// view:_Open() (the ROM's Open calls it): open the view with FLTK.
Ref FOpenX(RefArg rcvr)
{
  return nfl::OpenView(rcvr);
}

// view:Close(): close it. (The ROM sends a close command that runs when the
// events are handled; newtc closes at once.)
Ref FCloseX(RefArg rcvr)
{
  return nfl::CloseView(rcvr);
}

#else

// No views without FLTK (NEWTC_USES_FLTK): the stubs say so.
NS_STUB(FOpenX, RefArg rcvr)
NS_STUB(FCloseX, RefArg rcvr)

#endif
