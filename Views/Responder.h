/*
	File:		Responder.h

	Contains:	CResponder declarations.
					Every object in the view system responds to commands.

	Written by:	Newton Research Group.
*/

#if !defined(__RESPONDER_H)
#define __RESPONDER_H 1

#include "ViewObject.h"


class CResponder : public CViewObject
{
public:
	VIEW_HEADER_MACRO

	virtual	bool		doCommand(RefArg inCmd);
};


extern "C"
{
Ref			MakeCommand(ULong inId, CResponder * inRcvr, OpaqueRef inArg);
CResponder*	CommandReceiver(RefArg inCmd);
void			CommandSetId(RefArg inCmd, ULong inId);
ULong			CommandId(RefArg inCmd);
void			CommandSetResult(RefArg inCmd, NewtonErr inResult);
NewtonErr	CommandResult(RefArg inCmd);
void			CommandSetParameter(RefArg inCmd, OpaqueRef inArg);
OpaqueRef	CommandParameter(RefArg inCmd);
void			CommandSetFrameParameter(RefArg inCmd, RefArg inArg);
Ref			CommandFrameParameter(RefArg inCmd);
void			CommandSetIndexFrame(RefArg inCmd, ArrayIndex index, RefArg inArg);
void			CommandSetIndexParameter(RefArg inCmd, ArrayIndex index, int inArg);
int			CommandIndexParameter(RefArg inCmd, ArrayIndex index);
void			CommandSetText(RefArg inCmd, RefArg inText);
Ref			CommandText(RefArg inCmd);
void			CommandSetPoints(RefArg inCmd, RefArg inPts);
Ref			CommandPoints(RefArg inCmd);
}


/*--------------------------------------------------------------------------------
	View object classes.
--------------------------------------------------------------------------------*/

enum
{
	clObject				= kFirstClassId,
	clResponder,
	clModel,				// OS1
	clApplication,
	clNotebook,
	clMacNotebook,
	clARMNotebook,
	clWindowsNotebook,

	clView				= 10 + kFirstClassId,
	clRootView,
	clPictureView,
	clEditView,
	clContainerView,	// OS1
	clKeyboardView,
	clMonthView,
	clParagraphView,
	clPolygonView,
	clDataView,
	clMathExpView,
	clMathOpView,
	clMathLineView,
	clScheduleView,	// OS1
	clRemoteView,
	clTrainView,		// OS1
	clDayView,			// OS1
	clPickView,
	clGaugeView,
	clRepeatingView,	// OS1
	clPrintView,
	clMeetingView,
	clSliderView,
	clCharBoxView,		// OS1
	clTextView,
	clListView,
	cl100,
	clClipboard,
	cl102,
	cl103,
	clLibrarian,
	clOutline,
	cl106,
	clHelpOutline,
	clTXView
};


/*--------------------------------------------------------------------------------
	View object events.
--------------------------------------------------------------------------------*/

enum
{
	aeNoCommand,

	kRecognitionEvents = 10,
	aeClick,
	aeStroke,
	aeScrub,
	aeGesture = aeScrub,
	ae14,
	aeCaret,
	aeLine,
	aeShape,
	aeWord,
	aeMath,
	aeGetContext,
	aeInk,
	aeString,
	ae23,
	aeInkText,

	kKeyEvents = 30,
	aeKeyUp,
	aeKeyDown,
	aeKeyboardEnable,
	ae34,
	ae35,

	kViewEvents = 40,
	aeAddChild,
	aeDropChild,
	aeHide,
	aeShow,
	aeScrollUp,
	aeScrollDown,
	aeHilite,
	aeRemoveAllHilites,
	aeTap,
	aeDoubleTap,
	aeOverview,
	aeStartHilite,
	aeClickUp,
	aePickItem,
	aeTapDrag,
	aePrintAllPagesNow,

	kEditViewEvents = 60,
	aeAddData,
	aeDuplicateData,
	aeRemoveData,
	aeMoveData,
	aeDropData,
	aeScaleData,
	aeSetVertex,
	aeRemoveVertices,
	ae69,
	aeReplaceText,
	aeAddHilite,
	aeRemoveHilite,
	aeChangeStyle,
	aeDeferredRecognition,
	aeChangePen,
	aeMoveChild,

	kDebugEvents = 80,
	aeRedrawScreen,
	aeFlipBuffering,
	aeLCDScreen,
	aeToggleTracing,
	aeShowDebugWindow,
	aeDumpFields,
	aeDumpData,
	aeDumpWindows,
	aeSetNewtMachine,
	aeSetNewtonMachine,
	aeSetMacMachine,
	aeSetDOSMachine,
	aeSetRolexMachine,

	kApplicationEvents = 100,
	aeDoNothing,
	aeAbout,
	aeOpenDeskAccessory,
	aeNew,
	aeOpen,
	aeClose,
	aeSave,
	aeSaveAs,
	aeRevert,
	aePageSetUp,
	aePrint,
	aeQuit,
	aeRunScript,
	aeUndo,
	aeCut,
	aeCopy,
	aePaste,
	aeClear,
	aeSelectAll,
	aeShowClipboard
};

#endif	/* __RESPONDER_H */

