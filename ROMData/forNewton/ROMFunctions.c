/*
	File:		ROMFunctions.c

	Contains:	Newton object system ROM Ref* C functions.
					This file is built automatically by ResMaker from the ROM image and RS symbols.
	Written by:	Newton Research Group, 2016.
*/

/* The plain C functions of the ROM's function frames, written into their
   funcPtr slots (gROMFunctionSlots, ROMData.c) by InstallROMFunctions().
   Only a program that runs ROM code links this file (and so every one of
   these functions). */

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>

/* their real types don't matter here */
extern void AddBookmark(void);
extern void AddHistory(void);
extern void AddInkMarks(void);
extern void AddToContentArea(void);
extern void Append(void);
extern void AuthorData(void);
extern void BookClosed(void);
extern void BookTitle(void);
extern void Bookmarks(void);
extern void CFDispose(void);
extern void CFInstantiate(void);
extern void CFRecord(void);
extern void CIAbort(void);
extern void CIAccept(void);
extern void CIBytesAvailable(void);
extern void CICaller(void);
extern void CIConnect(void);
extern void CIDisconnect(void);
extern void CIDispose(void);
extern void CIDisposeLeavingTEndpoint(void);
extern void CIFlushInput(void);
extern void CIFlushOutput(void);
extern void CIFlushPartial(void);
extern void CIGetOptions(void);
extern void CIInput(void);
extern void CIInputAvailable(void);
extern void CIInstantiate(void);
extern void CIInstantiateFromEndpoint(void);
extern void CIJustBind(void);
extern void CIJustClose(void);
extern void CIJustConnect(void);
extern void CIJustDisconnect(void);
extern void CIJustListen(void);
extern void CIJustOpen(void);
extern void CIJustRelease(void);
extern void CIJustUnBind(void);
extern void CIListen(void);
extern void CINewAbort(void);
extern void CINewAccept(void);
extern void CINewBind(void);
extern void CINewConnect(void);
extern void CINewDisconnect(void);
extern void CINewDispose(void);
extern void CINewDisposeLeavingTEndpoint(void);
extern void CINewDisposeLeavingTool(void);
extern void CINewFlushInput(void);
extern void CINewFlushPartial(void);
extern void CINewInput(void);
extern void CINewInstantiate(void);
extern void CINewInstantiateFromEndpoint(void);
extern void CINewListen(void);
extern void CINewOption(void);
extern void CINewOutput(void);
extern void CINewPartial(void);
extern void CINewSetInputSpec(void);
extern void CINewSetState(void);
extern void CINewState(void);
extern void CINewUnBind(void);
extern void CIOutput(void);
extern void CIOutputDone(void);
extern void CIOutputFrame(void);
extern void CIPartial(void);
extern void CIReadyForOutput(void);
extern void CIReject(void);
extern void CIRelease(void);
extern void CIRequestsPending(void);
extern void CISNewInstantiate(void);
extern void CISNewInstantiateFromEndpoint(void);
extern void CISStreamIn(void);
extern void CISStreamOut(void);
extern void CISetInputSpec(void);
extern void CISetOptions(void);
extern void CISetSync(void);
extern void CIStartCCL(void);
extern void CIState(void);
extern void CIStopCCL(void);
extern void CSDispose(void);
extern void CSGetDefaultConfig(void);
extern void CSInstantiate(void);
extern void CSSetDefaultConfig(void);
extern void CallOnlineServices(void);
extern void CheezyIntersect(void);
extern void CheezySubsumption(void);
extern void CircleDistance(void);
extern void CleanString(void);
extern void CommonAncestors(void);
extern void CommonSoupGetName(void);
extern void CommonSoupQuery(void);
extern void CompositeClass(void);
extern void CoordinateToLatitude(void);
extern void CoordinateToLongitude(void);
extern void CorrectSelect(void);
extern void CorrectWord(void);
extern void CountPages(void);
extern void CuFind(void);
extern void CurrentBook(void);
extern void CurrentKiosk(void);
extern void CurrentPage(void);
extern void CursorClone(void);
extern void CursorCountEntries(void);
extern void CursorEntry(void);
extern void CursorEntryKey(void);
extern void CursorGoto(void);
extern void CursorGotoKey(void);
extern void CursorIndexPath(void);
extern void CursorMove(void);
extern void CursorNext(void);
extern void CursorPrev(void);
extern void CursorReset(void);
extern void CursorResetToEnd(void);
extern void CursorSoup(void);
extern void CursorStatus(void);
extern void CursorWhichEnd(void);
extern void DSAddLexiconFrame(void);
extern void DSConstructSubjectLine(void);
extern void DSFilterStrings(void);
extern void DSFilterStringsAux(void);
extern void DSFindPossibleLocation(void);
extern void DSFindPossibleName(void);
extern void DSFindPossiblePhone(void);
extern void DSGetMatchedEntries(void);
extern void DSPrevSubStr(void);
extern void DSResolveString(void);
extern void DSScanForWackyName(void);
extern void DSTagString(void);
extern void DisconnectOnlineServices(void);
extern void DumpDict(void);
extern void EWBufferStart(void);
extern void EWConnectToHost(void);
extern void EWDecodePacket(void);
extern void EWEscape(void);
extern void EWFileToFrame(void);
extern void EWFrameToFile(void);
extern void EWGetData(void);
extern void EWGetError(void);
extern void EWGetMacHeader(void);
extern void EWInstantiate(void);
extern void EWOutput(void);
extern void EWOutputAtom(void);
extern void EWSend(void);
extern void EWSetError(void);
extern void EWUnEscape(void);
extern void EWUniAtomHandler(void);
extern void EWUploadFile(void);
extern void FAbs(void);
extern void FActivate1XPackage(void);
extern void FActivateTestAgent(void);
extern void FAdd(void);
extern void FAddArraySlot(void);
extern void FAddAutoAdd(void);
extern void FAddCapitalizedEntry(void);
extern void FAddDeferredAction(void);
extern void FAddDeferredCall(void);
extern void FAddDeferredSend(void);
extern void FAddDelayedAction(void);
extern void FAddDelayedCall(void);
extern void FAddDelayedSend(void);
extern void FAddDictionary(void);
extern void FAddInk(void);
extern void FAddKeyCommand(void);
extern void FAddKeyCommands(void);
extern void FAddStepView(void);
extern void FAddTryString(void);
extern void FAddUndoAction(void);
extern void FAddUndoCall(void);
extern void FAddUndoSend(void);
extern void FAddUnitInfo(void);
extern void FAddView(void);
extern void FAddWordInfo(void);
extern void FAdjustParagraph(void);
extern void FAirusAddWord(void);
extern void FAirusAttributeSize(void);
extern void FAirusChangeAttribute(void);
extern void FAirusDeletePrefix(void);
extern void FAirusDeleteWord(void);
extern void FAirusDictionaryType(void);
extern void FAirusDispose(void);
extern void FAirusIteratorClone(void);
extern void FAirusIteratorDispose(void);
extern void FAirusIteratorMake(void);
extern void FAirusIteratorNextWord(void);
extern void FAirusIteratorPreviousWord(void);
extern void FAirusIteratorReset(void);
extern void FAirusIteratorThisWord(void);
extern void FAirusLookupWord(void);
extern void FAirusNew(void);
extern void FAirusRegisterDictionary(void);
extern void FAirusResult(void);
extern void FAirusUnregisterDictionary(void);
extern void FAirusWalkDictionary(void);
extern void FAnimateSimpleStroke(void);
extern void FAppleTalkOpenCount(void);
extern void FApply(void);
extern void FAref(void);
extern void FArray(void);
extern void FArrayInsert(void);
extern void FArrayMunger(void);
extern void FArrayPos(void);
extern void FArrayRemoveCount(void);
extern void FArrayToPoints(void);
extern void FAutoAdd(void);
extern void FAutoRemove(void);
extern void FBDelete(void);
extern void FBDifference(void);
extern void FBFetch(void);
extern void FBFetchRight(void);
extern void FBFind(void);
extern void FBFindRight(void);
extern void FBInsert(void);
extern void FBInsertRight(void);
extern void FBIntersect(void);
extern void FBMerge(void);
extern void FBSearchLeft(void);
extern void FBSearchRight(void);
extern void FBackLight(void);
extern void FBackLightStatus(void);
extern void FBackupPatchPackage(void);
extern void FBatteryCount(void);
extern void FBatteryLevel(void);
extern void FBatteryRawStatus(void);
extern void FBatteryStatus(void);
extern void FBeginsWith(void);
extern void FBinEqual(void);
extern void FBinaryMunger(void);
extern void FBitAnd(void);
extern void FBitNot(void);
extern void FBitOr(void);
extern void FBitXor(void);
extern void FBlockKeyCommand(void);
extern void FBlockStrokes(void);
extern void FBookAvailable(void);
extern void FBookRemoved(void);
extern void FBoolAnd(void);
extern void FBoolOr(void);
extern void FBootSucceeded(void);
extern void FBreakLoop(void);
extern void FBubbleArraySlot(void);
extern void FBuildContext(void);
extern void FBuildRecConfig(void);
extern void FBusyBoxControl(void);
extern void FCHR(void);
extern void FCalcBundleBounds(void);
extern void FCalcInkBounds(void);
extern void FCalibrateTablet(void);
extern void FCancelOnlineService(void);
extern void FCapitalize(void);
extern void FCapitalizeWords(void);
extern void FCaretRelativeToVisibleRect(void);
extern void FCategorizeKeyCommands(void);
extern void FCeiling(void);
extern void FChangeStylesOfRange(void);
extern void FCharPos(void);
extern void FCheckCardBattery(void);
extern void FChildTemplateFromTopic(void);
extern void FChildViewFramesX(void);
extern void FClassInfoByName(void);
extern void FClassInfoRegistrySeed(void);
extern void FClassOf(void);
extern void FClearCorrectionInfo(void);
extern void FClearHardKeymap(void);
extern void FClearPopup(void);
extern void FClearTryString(void);
extern void FClearUndoStacks(void);
extern void FClearWordInfoFlags(void);
extern void FClickLetterShapes(void);
extern void FClicker(void);
extern void FClipboardCommand(void);
extern void FClone(void);
extern void FCloseAppleTalk(void);
extern void FCloseRemote(void);
extern void FCloseX(void);
extern void FCollapseTopic(void);
extern void FCollect(void);
extern void FCommandKeyboardConnected(void);
extern void FCompile(void);
extern void FCompressStrokes(void);
extern void FCompressStrokesToInk(void);
extern void FComputeParagraphHeight(void);
extern void FConnAbort(void);
extern void FConnBuildStoreFrame(void);
extern void FConnBytesAvailable(void);
extern void FConnConnect(void);
extern void FConnDesktopType(void);
extern void FConnDoConnection(void);
extern void FConnDoKeyboardPassthrough(void);
extern void FConnEntriesEqual(void);
extern void FConnFlushCommandData(void);
extern void FConnGetCurrentStore(void);
extern void FConnGetSyncChanges(void);
extern void FConnInstallProtocolExtension(void);
extern void FConnInstantiate(void);
extern void FConnReadBytes(void);
extern void FConnReadCommand(void);
extern void FConnReadCommandData(void);
extern void FConnReadCommandHeader(void);
extern void FConnRemoveProtocolExtension(void);
extern void FConnRetryPassword(void);
extern void FConnSetState(void);
extern void FConnStop(void);
extern void FConnWriteBytes(void);
extern void FConnWriteCommand(void);
extern void FConnWriteCommandHeader(void);
extern void FConnectPassthruKeyboard(void);
extern void FConnectionState(void);
extern void FConvertDictionaryData(void);
extern void FConvertForMP(void);
extern void FConvertFromMP(void);
extern void FConvertToSoundFrame(void);
extern void FCopyBits(void);
extern void FCountGesturePoints(void);
extern void FCountLetterShapes(void);
extern void FCountPoints(void);
extern void FCountStrokes(void);
extern void FCountUnitStrokes(void);
extern void FCurrentException(void);
extern void FCurrentExports(void);
extern void FCurrentImports(void);
extern void FDESCreatePasswordKey(void);
extern void FDV(void);
extern void FDate(void);
extern void FDateFromSeconds(void);
extern void FDateNTime(void);
extern void FDeActivate1XPackage(void);
extern void FDeactivateTestAgent(void);
extern void FDebug(void);
extern void FDebugMemoryStats(void);
extern void FDebugRunUntilIdle(void);
extern void FDecibelsToVolume(void);
extern void FDecodeRichString(void);
extern void FDeepClone(void);
extern void FDefGlobalFn(void);
extern void FDeinstallPackage(void);
extern void FDeleteStoreObject(void);
extern void FDeleteX(void);
extern void FDestroyProtocol(void);
extern void FDirtyBoxX(void);
extern void FDirtyX(void);
extern void FDismissPopup(void);
extern void FDispatchProtocol(void);
extern void FDisplay(void);
extern void FDisplaySplashGraphic(void);
extern void FDisposeTrainingData(void);
extern void FDiv(void);
extern void FDivide(void);
extern void FDoCursiveTraining(void);
extern void FDoDrawing(void);
extern void FDoEntryLearning(void);
extern void FDoPopup(void);
extern void FDoScrubEffect(void);
extern void FDowncase(void);
extern void FDragAndDrop(void);
extern void FDragAndDropLtd(void);
extern void FDragX(void);
extern void FDrawDateLabels(void);
extern void FDrawExpando(void);
extern void FDrawIntoBitmap(void);
extern void FDrawLetterShapes(void);
extern void FDrawMeetingGrid(void);
extern void FDrawOriginal(void);
extern void FDrawPolygons(void);
extern void FDrawShape(void);
extern void FDrawStringShapes(void);
extern void FDrawXBitmap(void);
extern void FDropHilites(void);
extern void FEffectX(void);
extern void FEnablePowerStats(void);
extern void FEndsWith(void);
extern void FEnsureInternal(void);
extern void FEntryCachedObject(void);
extern void FEntryChange(void);
extern void FEntryChangeVerbatim(void);
extern void FEntryChangeWithModTime(void);
extern void FEntryCopy(void);
extern void FEntryFlush(void);
extern void FEntryHandler(void);
extern void FEntryIsResident(void);
extern void FEntryModTime(void);
extern void FEntryMove(void);
extern void FEntryRemoveFromSoup(void);
extern void FEntryReplace(void);
extern void FEntryReplaceWithModTime(void);
extern void FEntrySetCachedObject(void);
extern void FEntrySetHandler(void);
extern void FEntrySize(void);
extern void FEntrySizeWithoutVBOs(void);
extern void FEntrySoup(void);
extern void FEntryStore(void);
extern void FEntryTextSize(void);
extern void FEntryUndoChanges(void);
extern void FEntryUniqueId(void);
extern void FEntryValid(void);
extern void FEqual(void);
extern void FEvalStringer(void);
extern void FEventPause(void);
extern void FExitBreakLoop(void);
extern void FExitModalDialog(void);
extern void FExpandInk(void);
extern void FExpandTopic(void);
extern void FExpandUnit(void);
extern void FExtractByte(void);
extern void FExtractBytes(void);
extern void FExtractCString(void);
extern void FExtractChar(void);
extern void FExtractData(void);
extern void FExtractLong(void);
extern void FExtractPString(void);
extern void FExtractRange(void);
extern void FExtractRangeAsRichString(void);
extern void FExtractRichStringFromParaSlots(void);
extern void FExtractTextRange(void);
extern void FExtractUniChar(void);
extern void FExtractWord(void);
extern void FExtractXLong(void);
extern void FFilterDialog(void);
extern void FFindDictionaryFrame(void);
extern void FFindKeyCommand(void);
extern void FFindNewInfo(void);
extern void FFindShape(void);
extern void FFindStringInArray(void);
extern void FFindStringInFrame(void);
extern void FFindWordInfo(void);
extern void FFinishRecognizing(void);
extern void FFloor(void);
extern void FFlushImports(void);
extern void FFlushStrokes(void);
extern void FFontAscent(void);
extern void FFontDescent(void);
extern void FFontHeight(void);
extern void FFontLeading(void);
extern void FForLoop(void);
extern void FForkScript(void);
extern void FFormattedNumberStr(void);
extern void FFrameDirty(void);
extern void FFramesStringer(void);
extern void FFulfillImportTable(void);
extern void FGC(void);
extern void FGatherKeyCommands(void);
extern void FGenFullCommands(void);
extern void FGestalt(void);
extern void FGesturePoint(void);
extern void FGestureType(void);
extern void FGetAllMeetings(void);
extern void FGetAllMeetingsUnique(void);
extern void FGetAlternates(void);
extern void FGetBinaryCompander(void);
extern void FGetBinaryCompanderData(void);
extern void FGetBinaryStore(void);
extern void FGetBinaryStoredSize(void);
extern void FGetBitmapInfo(void);
extern void FGetBitmapPixel(void);
extern void FGetBlue(void);
extern void FGetCalibration(void);
extern void FGetCapability(void);
extern void FGetCardInfo(void);
extern void FGetCardSlotStores(void);
extern void FGetCardTypes(void);
extern void FGetCaretBox(void);
extern void FGetCaretInfo(void);
extern void FGetChannelInputGain(void);
extern void FGetChannelVolume(void);
extern void FGetChar(void);
extern void FGetClipboard(void);
extern void FGetClipboardIcon(void);
extern void FGetDefaultFont(void);
extern void FGetDictionaryData(void);
extern void FGetDrawBoxX(void);
extern void FGetEditArray(void);
extern void FGetExportTableClients(void);
extern void FGetFlags(void);
extern void FGetFontFace(void);
extern void FGetFontFamilyNum(void);
extern void FGetFontFamilySym(void);
extern void FGetFontSize(void);
extern void FGetFrameStuff(void);
extern void FGetFunctionArgCount(void);
extern void FGetGlobals(void);
extern void FGetGreen(void);
extern void FGetHeapStats(void);
extern void FGetHiliteIndex(void);
extern void FGetHiliteOffsets(void);
extern void FGetHiliteWeight(void);
extern void FGetHilitedTextItems(void);
extern void FGetId(void);
extern void FGetIndexChar(void);
extern void FGetInkAt(void);
extern void FGetInkWordInfo(void);
extern void FGetInsertionStyle(void);
extern void FGetKeyView(void);
extern void FGetLCDContrast(void);
extern void FGetLearningData(void);
extern void FGetLetterIndex(void);
extern void FGetLetterWeights(void);
extern void FGetLocale(void);
extern void FGetMeetingIcon(void);
extern void FGetMeetingTypeInfo(void);
extern void FGetMyZone(void);
extern void FGetNames(void);
extern void FGetNextMeetingTime(void);
extern void FGetOnlineEndpoint(void);
extern void FGetOrientation(void);
extern void FGetPackageStore(void);
extern void FGetPackageStores(void);
extern void FGetPackages(void);
extern void FGetPath(void);
extern void FGetPkgInfoFromPSSid(void);
extern void FGetPkgRefInfo(void);
extern void FGetPoint(void);
extern void FGetPointsArray(void);
extern void FGetPointsArrayXY(void);
extern void FGetPolygons(void);
extern void FGetPopup(void);
extern void FGetPowerStats(void);
extern void FGetRamParaData(void);
extern void FGetRandomDictionaryWord(void);
extern void FGetRandomState(void);
extern void FGetRandomWord(void);
extern void FGetRangeText(void);
extern void FGetRed(void);
extern void FGetRemoteWriting(void);
extern void FGetRichString(void);
extern void FGetRoot(void);
extern void FGetScoreArray(void);
extern void FGetSelectionStack(void);
extern void FGetSerialNumber(void);
extern void FGetShapeInfo(void);
extern void FGetSiblingSlot(void);
extern void FGetSlot(void);
extern void FGetSortId(void);
extern void FGetStoreCardSlot(void);
extern void FGetStoreCardType(void);
extern void FGetStoreObjectSize(void);
extern void FGetStores(void);
extern void FGetStroke(void);
extern void FGetStrokeBounds(void);
extern void FGetStrokePoint(void);
extern void FGetStrokePointsArray(void);
extern void FGetStyleAtOffset(void);
extern void FGetStylesOfRange(void);
extern void FGetSystemVolume(void);
extern void FGetTextFlags(void);
extern void FGetTone(void);
extern void FGetTrainingData(void);
extern void FGetTrueModifiers(void);
extern void FGetUndoState(void);
extern void FGetUnionSoup(void);
extern void FGetUnionSoupAlways(void);
extern void FGetUnitDownTime(void);
extern void FGetUnitEndTime(void);
extern void FGetUnitStartTime(void);
extern void FGetUnitUpTime(void);
extern void FGetValue(void);
extern void FGetVar(void);
extern void FGetVariable(void);
extern void FGetView(void);
extern void FGetVolume(void);
extern void FGetWordArray(void);
extern void FGetWordInfo(void);
extern void FGetWordList(void);
extern void FGetZoneFromName(void);
extern void FGetZoneList(void);
extern void FGlobalBoxX(void);
extern void FGlobalOuterBoxX(void);
extern void FGrayShrink(void);
extern void FGreaterOrEqual(void);
extern void FGreaterThan(void);
extern void FHandleInkWord(void);
extern void FHandleInsertItems(void);
extern void FHandleKeyEvents(void);
extern void FHandleRawInk(void);
extern void FHandleUnit(void);
extern void FHasCapability(void);
extern void FHasPath(void);
extern void FHasSiblingSlot(void);
extern void FHasSlot(void);
extern void FHasVar(void);
extern void FHasVariable(void);
extern void FHaveZones(void);
extern void FHideCaret(void);
extern void FHideX(void);
extern void FHiliteOwner(void);
extern void FHiliteUniqueX(void);
extern void FHiliteViewChildren(void);
extern void FHiliteX(void);
extern void FHiliter(void);
extern void FHitShape(void);
extern void FHobbleTablet(void);
extern void FHourMinute(void);
extern void FIAatWork(void);
extern void FInRepeatedKeyCommand(void);
extern void FInTryString(void);
extern void FIncrementMonth(void);
extern void FInitSerialDebugging(void);
extern void FInkConvert(void);
extern void FInkOff(void);
extern void FInkOffUnHobbled(void);
extern void FInkOn(void);
extern void FInsertItemsAtCaret(void);
extern void FInsertRange(void);
extern void FInsertStyledText(void);
extern void FInsertTabletSample(void);
extern void FInsertionSort(void);
extern void FInsetRect(void);
extern void FInstallPackage(void);
extern void FIntern(void);
extern void FInvertRect(void);
extern void FIsAlphaNumeric(void);
extern void FIsArray(void);
extern void FIsBinary(void);
extern void FIsCharacter(void);
extern void FIsCollapsed(void);
extern void FIsCommandKeystroke(void);
extern void FIsDirty(void);
extern void FIsEntryAlias(void);
extern void FIsEqualTone(void);
extern void FIsFrame(void);
extern void FIsFunction(void);
extern void FIsImmediate(void);
extern void FIsInkChar(void);
extern void FIsInstance(void);
extern void FIsInteger(void);
extern void FIsKeyDown(void);
extern void FIsLargeBinary(void);
extern void FIsMagicPtr(void);
extern void FIsNativeFunction(void);
extern void FIsNumber(void);
extern void FIsPackage(void);
extern void FIsPathExpr(void);
extern void FIsPrimShape(void);
extern void FIsProtocolPartInUse(void);
extern void FIsProxyEntry(void);
extern void FIsPtInRect(void);
extern void FIsReadOnly(void);
extern void FIsReal(void);
extern void FIsRichString(void);
extern void FIsSameEntry(void);
extern void FIsSoupEntry(void);
extern void FIsString(void);
extern void FIsSubclass(void);
extern void FIsSubexception(void);
extern void FIsSymbol(void);
extern void FIsTabletCalibrationNeeded(void);
extern void FIsValid(void);
extern void FIsValidDate(void);
extern void FIsValidString(void);
extern void FIsWhiteSpace(void);
extern void FJournalReplayALine(void);
extern void FJournalReplayAStroke(void);
extern void FJournalReplayBusy(void);
extern void FJournalReplayStrokes(void);
extern void FJournalStartRecord(void);
extern void FJournalStopRecord(void);
extern void FKeyHelpSlipDraw(void);
extern void FKeyHelpSlipSetup(void);
extern void FKeyIn(void);
extern void FKeyboardConnected(void);
extern void FKeyboardInputX(void);
extern void FLBAlloc(void);
extern void FLBAllocCompressed(void);
extern void FLBClearCache(void);
extern void FLBRollback(void);
extern void FLFetch(void);
extern void FLSearch(void);
extern void FLShift(void);
extern void FLayoutTableX(void);
extern void FLayoutVerticallyX(void);
extern void FLength(void);
extern void FLessEqualOrGreater(void);
extern void FLessOrEqual(void);
extern void FLessOrGreater(void);
extern void FLessThan(void);
extern void FListBottom(void);
extern void FLoad(void);
extern void FLoadFontCache(void);
extern void FLoadOnlinePackage(void);
extern void FLocalBoxX(void);
extern void FLocalVar(void);
extern void FLockScreen(void);
extern void FLongDateStr(void);
extern void FLookupCompletions(void);
extern void FLookupWord(void);
extern void FMakeBinary(void);
extern void FMakeBitmap(void);
extern void FMakeCompactFont(void);
extern void FMakeDragRef(void);
extern void FMakeEntryAlias(void);
extern void FMakeInk(void);
extern void FMakeLine(void);
extern void FMakeOval(void);
extern void FMakePict(void);
extern void FMakePolygon(void);
extern void FMakeRect(void);
extern void FMakeRegion(void);
extern void FMakeRichString(void);
extern void FMakeRoundRect(void);
extern void FMakeShape(void);
extern void FMakeStrokeBundle(void);
extern void FMakeText(void);
extern void FMakeTextBox(void);
extern void FMakeTextLines(void);
extern void FMakeWedge(void);
extern void FMakeWordInfo(void);
extern void FMap(void);
extern void FMapCursor(void);
extern void FMapPtX(void);
extern void FMapPtY(void);
extern void FMapRect(void);
extern void FMarkerBounds(void);
extern void FMatchKeyMessage(void);
extern void FMatchedChar(void);
extern void FMax(void);
extern void FMeasuredNumberStr(void);
extern void FMergeInk(void);
extern void FMergeStrokes(void);
extern void FMergeWordInfo(void);
extern void FMin(void);
extern void FMinimumBatteryCheck(void);
extern void FMod(void);
extern void FModalDialog(void);
extern void FModalRecognitionOff(void);
extern void FModalRecognitionOn(void);
extern void FModalState(void);
extern void FMoveBehindX(void);
extern void FMoveCorrectionInfo(void);
extern void FMoveWordFirst(void);
extern void FMultiply(void);
extern void FMungeBitmap(void);
extern void FMungeShape(void);
extern void FMungeStyles(void);
extern void FNBPGetCount(void);
extern void FNBPGetLookupNames(void);
extern void FNBPGetNames(void);
extern void FNBPLookupCount(void);
extern void FNBPStart(void);
extern void FNBPStartLookup(void);
extern void FNBPStop(void);
extern void FNBPStopLookup(void);
extern void FNTKAlive(void);
extern void FNTKDownload(void);
extern void FNTKListener(void);
extern void FNTKSend(void);
extern void FNegate(void);
extern void FNewByName(void);
extern void FNewIterator(void);
extern void FNewProtocol(void);
extern void FNewProxyEntry(void);
extern void FNewStoreObject(void);
extern void FNewWeakArray(void);
extern void FNextClassInfo(void);
extern void FNextInkIndex(void);
extern void FNextKeyView(void);
extern void FNextMeeting(void);
extern void FNot(void);
extern void FNumInkWordsInRange(void);
extern void FNumberStr(void);
extern void FORD(void);
extern void FObjectPid(void);
extern void FObjectPkgRef(void);
extern void FOffsetCorrectionInfo(void);
extern void FOffsetRect(void);
extern void FOffsetShape(void);
extern void FOffsetView(void);
extern void FOpenAppleTalk(void);
extern void FOpenRemote(void);
extern void FOpenX(void);
extern void FOrigPhrase(void);
extern void FOtherViewInUse(void);
extern void FPSSidToPid(void);
extern void FPSSidToPkgRef(void);
extern void FPackRGB(void);
extern void FParaContainsInk(void);
extern void FParamStr(void);
extern void FParentX(void);
extern void FParseUtter(void);
extern void FPenPos(void);
extern void FPendingImports(void);
extern void FPerform(void);
extern void FPerformIfDefined(void);
extern void FPickViewGetScollerValues(void);
extern void FPickViewKeyDown(void);
extern void FPickViewScroll(void);
extern void FPictToShape(void);
extern void FPidToPackage(void);
extern void FPidToPackageLite(void);
extern void FPidToPkgRef(void);
extern void FPlaySound(void);
extern void FPlaySoundEffect(void);
extern void FPlaySoundIrregardless(void);
extern void FPlaySoundSync(void);
extern void FPointToCharOffset(void);
extern void FPointToWord(void);
extern void FPointsArrayToStroke(void);
extern void FPointsToArray(void);
extern void FPolyContainsInk(void);
extern void FPositionCaret(void);
extern void FPositionToTime(void);
extern void FPostAndDo(void);
extern void FPostCommand(void);
extern void FPostCommandParam(void);
extern void FPostKeyString(void);
extern void FPowerOff(void);
extern void FPreInitSerialDebugging(void);
extern void FPrevMeeting(void);
extern void FPrimClassOf(void);
extern void FPrint(void);
extern void FProcessBuiltinCommand(void);
extern void FProtoPerform(void);
extern void FProtoPerformIfDefined(void);
extern void FProtocolImplementationName(void);
extern void FProtocolInterfaceName(void);
extern void FProtocolSignature(void);
extern void FProtocolVersion(void);
extern void FPtInPicture(void);
extern void FPurgeAreaCache(void);
extern void FQuery(void);
extern void FQuickLookDone(void);
extern void FQuickSort(void);
extern void FRShift(void);
extern void FRandom(void);
extern void FReadCursiveOptions(void);
extern void FReadStoreObject(void);
extern void FReal(void);
extern void FReboot(void);
extern void FRecSettingsChanged(void);
extern void FRecognize(void);
extern void FRecognizeInkWord(void);
extern void FRecognizePara(void);
extern void FRecognizePoly(void);
extern void FRecognizeTextInStyles(void);
extern void FRectsOverlap(void);
extern void FRedoChildrenX(void);
extern void FRefreshViews(void);
extern void FRegTaskTemplate(void);
extern void FRegisterGestalt(void);
extern void FRegisterOpenKeyboard(void);
extern void FRelBounds(void);
extern void FRemove1XPackage(void);
extern void FRemoveAutoAdd(void);
extern void FRemoveCorrectionInfo(void);
extern void FRemoveSlot(void);
extern void FRemoveStepView(void);
extern void FRemoveToggledEntries(void);
extern void FRemoveView(void);
extern void FRepeatInfoToText(void);
extern void FReplaceGestalt(void);
extern void FReplaceObject(void);
extern void FResetLearningDefaults(void);
extern void FResetPowerStats(void);
extern void FResolveEntryAlias(void);
extern void FRestoreKeyView(void);
extern void FRestorePatchPackage(void);
extern void FRethrow(void);
extern void FRevealEffectX(void);
extern void FRosettaExtension(void);
extern void FRunInitScripts(void);
extern void FSPrintObject(void);
extern void FScaleShape(void);
extern void FScanNextWord(void);
extern void FScanPrevWordEnd(void);
extern void FScanWordEnd(void);
extern void FScanWordStart(void);
extern void FSectRect(void);
extern void FSendCode(void);
extern void FSendKeyMessage(void);
extern void FSendOnline(void);
extern void FSetAdd(void);
extern void FSetAref(void);
extern void FSetBatteryType(void);
extern void FSetBounds(void);
extern void FSetCalibration(void);
extern void FSetCaretInfo(void);
extern void FSetChannelInputGain(void);
extern void FSetChannelVolume(void);
extern void FSetChar(void);
extern void FSetClass(void);
extern void FSetClipboard(void);
extern void FSetContains(void);
extern void FSetDebugMode(void);
extern void FSetDictionaryData(void);
extern void FSetDifference(void);
extern void FSetFontFace(void);
extern void FSetFontFamily(void);
extern void FSetFontParms(void);
extern void FSetFontSize(void);
extern void FSetGlobal(void);
extern void FSetHiliteIndex(void);
extern void FSetHiliteNoUpdateX(void);
extern void FSetHiliteWeight(void);
extern void FSetHiliteX(void);
extern void FSetInkerPenSize(void);
extern void FSetInputGain(void);
extern void FSetKeyView(void);
extern void FSetLCDContrast(void);
extern void FSetLearningData(void);
extern void FSetLength(void);
extern void FSetLetterWeights(void);
extern void FSetLocale(void);
extern void FSetOnlineDisconnect(void);
extern void FSetOrientation(void);
extern void FSetOriginX(void);
extern void FSetOutputDevice(void);
extern void FSetOverlaps(void);
extern void FSetPath(void);
extern void FSetPopup(void);
extern void FSetRamParaData(void);
extern void FSetRandomSeed(void);
extern void FSetRandomState(void);
extern void FSetRemoteWriting(void);
extern void FSetRemove(void);
extern void FSetSlot(void);
extern void FSetSortId(void);
extern void FSetStoreObjectSize(void);
extern void FSetSysAlarm(void);
extern void FSetSystemVolume(void);
extern void FSetTimeInSeconds(void);
extern void FSetUnion(void);
extern void FSetValue(void);
extern void FSetVariable(void);
extern void FSetVolume(void);
extern void FSetWordInfoFlags(void);
extern void FSetWordList(void);
extern void FSetupIdleX(void);
extern void FSetupTetheredListener(void);
extern void FSetupVisibleChildren(void);
extern void FShapeBounds(void);
extern void FShellSort(void);
extern void FShortDate(void);
extern void FShortDateStr(void);
extern void FShowCaret(void);
extern void FShowX(void);
extern void FSignum(void);
extern void FSleep(void);
extern void FSlideEffectX(void);
extern void FSmartConcat(void);
extern void FSmartStart(void);
extern void FSmartStop(void);
extern void FSoundCheck(void);
extern void FSoundClose(void);
extern void FSoundIsActive(void);
extern void FSoundIsPaused(void);
extern void FSoundOpen(void);
extern void FSoundPause(void);
extern void FSoundPlayEnabled(void);
extern void FSoundSchedule(void);
extern void FSoundStart(void);
extern void FSoundStop(void);
extern void FSpellCheck(void);
extern void FSpellCorrect(void);
extern void FSpellDocBegin(void);
extern void FSpellDocEnd(void);
extern void FSpellSkip(void);
extern void FSplitInkAt(void);
extern void FSplitString(void);
extern void FStableSort(void);
extern void FStackTrace(void);
extern void FStartBypassTablet(void);
extern void FStats(void);
extern void FStdioOff(void);
extern void FStdioOn(void);
extern void FStopBypassTablet(void);
extern void FStoreAbort(void);
extern void FStrCompare(void);
extern void FStrConcat(void);
extern void FStrEqual(void);
extern void FStrExactCompare(void);
extern void FStrFilled(void);
extern void FStrFontWidth(void);
extern void FStrHexDump(void);
extern void FStrLen(void);
extern void FStrMunger(void);
extern void FStrPos(void);
extern void FStrReplace(void);
extern void FStrTruncate(void);
extern void FStrWidth(void);
extern void FStringAnnotate(void);
extern void FStringAssoc(void);
extern void FStringFilter(void);
extern void FStringFormat(void);
extern void FStringShorten(void);
extern void FStringToDate(void);
extern void FStringToDateFrame(void);
extern void FStringToFrameMapper(void);
extern void FStringToNumber(void);
extern void FStringToTime(void);
extern void FStripDiacriticals(void);
extern void FStripInk(void);
extern void FStripRecognitionWord(void);
extern void FStripRecognitionWordDiacritsOK(void);
extern void FStrokeBounds(void);
extern void FStrokeBundleToInkWord(void);
extern void FStrokeDone(void);
extern void FStrokeInPicture(void);
extern void FStrokesAfterUnit(void);
extern void FStuffByte(void);
extern void FStuffCString(void);
extern void FStuffChar(void);
extern void FStuffLong(void);
extern void FStuffPString(void);
extern void FStuffUniChar(void);
extern void FStuffWord(void);
extern void FStyleArrayContainsInk(void);
extern void FStyledStrTruncate(void);
extern void FSubstituteChars(void);
extern void FSubstr(void);
extern void FSubtract(void);
extern void FSuckPackageFromBinary(void);
extern void FSuckPackageFromEnpoint(void);
extern void FSuckPackageOffDeskTop(void);
extern void FSymbolCompareLex(void);
extern void FSymbolName(void);
extern void FSyncChildrenX(void);
extern void FSyncScrollX(void);
extern void FSyncViewX(void);
extern void FTXChangeRangeRulers(void);
extern void FTXChangeRangeRuns(void);
extern void FTXCharToPoint(void);
extern void FTXClear(void);
extern void FTXCopy(void);
extern void FTXCut(void);
extern void FTXExternalize(void);
extern void FTXFinderFindString(void);
extern void FTXFinderGetCountCharacters(void);
extern void FTXFinderGetRangeText(void);
extern void FTXGetContinuousRun(void);
extern void FTXGetCountCharacters(void);
extern void FTXGetCountPages(void);
extern void FTXGetHiliteRange(void);
extern void FTXGetLineRange(void);
extern void FTXGetParagraphRange(void);
extern void FTXGetRangeData(void);
extern void FTXGetScrollValues(void);
extern void FTXGetTextViewRect(void);
extern void FTXGetTotalHeight(void);
extern void FTXGetTotalWidth(void);
extern void FTXGetWordRange(void);
extern void FTXHideRuler(void);
extern void FTXInsertPageBreak(void);
extern void FTXInternalize(void);
extern void FTXIsModified(void);
extern void FTXIsRulerShown(void);
extern void FTXPaste(void);
extern void FTXPointToChar(void);
extern void FTXReplace(void);
extern void FTXReplaceAll(void);
extern void FTXScroll(void);
extern void FTXSetDrawOrigin(void);
extern void FTXSetGeometry(void);
extern void FTXSetHiliteRange(void);
extern void FTXSetReadOnly(void);
extern void FTXSetStore(void);
extern void FTXShowRuler(void);
extern void FTXUpdateRulerInfo(void);
extern void FTXViewFindString(void);
extern void FTabletBufferEmpty(void);
extern void FTestExit(void);
extern void FTestFlushReportQueue(void);
extern void FTestGetParameterArray(void);
extern void FTestGetParameterString(void);
extern void FTestMDropConnection(void);
extern void FTestMGetReportMsg(void);
extern void FTestMSetParameterString(void);
extern void FTestMStartTestCase(void);
extern void FTestMStartTestFrame(void);
extern void FTestReadDataFile(void);
extern void FTestReadTextFile(void);
extern void FTestReportError(void);
extern void FTestReportMessage(void);
extern void FTestWillCallExit(void);
extern void FTestWordInfoFlags(void);
extern void FTextBounds(void);
extern void FTextBox(void);
extern void FThrow(void);
extern void FTicks(void);
extern void FTieViews(void);
extern void FTime(void);
extern void FTimeFrameStr(void);
extern void FTimeInSeconds(void);
extern void FTimeStr(void);
extern void FTimeToPosition(void);
extern void FToggleX(void);
extern void FTopicBottom(void);
extern void FTopicByName(void);
extern void FTopicIndexToView(void);
extern void FTotalClone(void);
extern void FTotalMinutes(void);
extern void FTrackButtonX(void);
extern void FTrackHiliteX(void);
extern void FTranslate(void);
extern void FTranslateKey(void);
extern void FTriggerWordRecognition(void);
extern void FTrimString(void);
extern void FTryStringLength(void);
extern void FUnRegTaskTemplate(void);
extern void FUnblockStrokes(void);
extern void FUnionPoint(void);
extern void FUnionRect(void);
extern void FUnmountCard(void);
extern void FUnordered(void);
extern void FUnorderedGreaterOrEqual(void);
extern void FUnorderedLessOrEqual(void);
extern void FUnorderedLessOrGreater(void);
extern void FUnorderedOrEqual(void);
extern void FUnorderedOrGreater(void);
extern void FUnorderedOrLess(void);
extern void FUnregisterOpenKeyboard(void);
extern void FUpcase(void);
extern void FUriah(void);
extern void FUriahBinaryObjects(void);
extern void FUseWRec(void);
extern void FValidateWord(void);
extern void FViewAllowsInk(void);
extern void FViewAllowsInkWords(void);
extern void FViewAutopsy(void);
extern void FViewContainsCaretView(void);
extern void FViewIntoBitmap(void);
extern void FViewWorksWithCombCorrector(void);
extern void FVisibleBox(void);
extern void FVisibleTopicIndex(void);
extern void FVolumeToDecibels(void);
extern void FVoteOnWordUnit(void);
extern void FWRecIsBeingUsed(void);
extern void FWeekNumber(void);
extern void FWrite(void);
extern void FWriteEntireStoreObject(void);
extern void FWriteStoreObject(void);
extern void FYieldToFork(void);
extern void Facos(void);
extern void Facosh(void);
extern void Fannuity(void);
extern void Fasin(void);
extern void Fasinh(void);
extern void FastStringLookup(void);
extern void Fatan(void);
extern void Fatan2(void);
extern void Fatanh(void);
extern void FavorAction(void);
extern void FavorObject(void);
extern void Fcompound(void);
extern void Fcopysign(void);
extern void Fcos(void);
extern void Fcosh(void);
extern void Fdim(void);
extern void Ferf(void);
extern void Ferfc(void);
extern void Fexp(void);
extern void Fexpm1(void);
extern void Ffabs(void);
extern void Ffeclearexcept(void);
extern void Ffegetenv(void);
extern void Ffegetexcept(void);
extern void Ffegetround(void);
extern void Ffeholdexcept(void);
extern void Fferaiseexcept(void);
extern void Ffesetenv(void);
extern void Ffesetexcept(void);
extern void Ffesetround(void);
extern void Ffetestexcept(void);
extern void Ffeupdateenv(void);
extern void Ffmod(void);
extern void Fgamma(void);
extern void Fhypot(void);
extern void FindContentBySlot(void);
extern void FindContentByValue(void);
extern void FindPageByContent(void);
extern void FindPageBySubject(void);
extern void FindPageByValue(void);
extern void Fisfinite(void);
extern void Fisnan(void);
extern void Fisnormal(void);
extern void Fldexp(void);
extern void Flgamma(void);
extern void Flog(void);
extern void Flog10(void);
extern void Flog1p(void);
extern void Flogb(void);
extern void Fmax(void);
extern void Fmin(void);
extern void Fnearbyint(void);
extern void Fnextafterd(void);
extern void FormatVertical(void);
extern void FpkgDownload(void);
extern void Fpow(void);
extern void Frandomx(void);
extern void Fref(void);
extern void FrefOf(void);
extern void Fremainder(void);
extern void Fremquo(void);
extern void Frint(void);
extern void Frinttol(void);
extern void Fround(void);
extern void Fscalb(void);
extern void Fsignbit(void);
extern void Fsin(void);
extern void Fsinh(void);
extern void Fsqrt(void);
extern void Ftan(void);
extern void Ftanh(void);
extern void Ftrunc(void);
extern void GenPhoneTypeList(void);
extern void GeneratePhrases(void);
extern void GenerateSubstrings(void);
extern void GetDictItem(void);
extern void GetRelevantTemplates(void);
extern void GlueStrings(void);
extern void GuessAddressee(void);
extern void HiliteBlock(void);
extern void History(void);
extern void IASmartCFLookup(void);
extern void ISATest(void);
extern void InkMarks(void);
extern void LatitudeToCoordinate(void);
extern void LayoutMeeting(void);
extern void LexDateLookup(void);
extern void LexPhoneLookup(void);
extern void LexTimeLookup(void);
extern void LongitudeToCoordinate(void);
extern void MakeLowerCase(void);
extern void MakePhrasalLexEntry(void);
extern void MapSymToFrame(void);
extern void PageContents(void);
extern void PageScroll(void);
extern void PageThumbnail(void);
extern void PathToRoot(void);
extern void PhoneIndexToValue(void);
extern void PhoneStringToValue(void);
extern void PhoneSymToIndex(void);
extern void PhoneSymToString(void);
extern void PhraseFilter(void);
extern void PlainSoupAdd(void);
extern void PlainSoupAddIndex(void);
extern void PlainSoupAddTags(void);
extern void PlainSoupAddWithUniqueId(void);
extern void PlainSoupCopyEntries(void);
extern void PlainSoupCopyEntriesWithCallBack(void);
extern void PlainSoupFlush(void);
extern void PlainSoupGetAllInfo(void);
extern void PlainSoupGetIndexes(void);
extern void PlainSoupGetInfo(void);
extern void PlainSoupGetNextUId(void);
extern void PlainSoupGetSignature(void);
extern void PlainSoupGetSize(void);
extern void PlainSoupGetStore(void);
extern void PlainSoupGetTags(void);
extern void PlainSoupHasTags(void);
extern void PlainSoupIndexSizes(void);
extern void PlainSoupMakeKey(void);
extern void PlainSoupModifyTag(void);
extern void PlainSoupRemoveAllEntries(void);
extern void PlainSoupRemoveFromStore(void);
extern void PlainSoupRemoveIndex(void);
extern void PlainSoupRemoveTags(void);
extern void PlainSoupSetAllInfo(void);
extern void PlainSoupSetInfo(void);
extern void PlainSoupSetName(void);
extern void PlainSoupSetSignature(void);
extern void PrefixP(void);
extern void PrepBook(void);
extern void PrepRecConfig(void);
extern void PreviousPage(void);
extern void ReFlow(void);
extern void ReadDomainOptions(void);
extern void ReflowPreflight(void);
extern void RefreshTopics(void);
extern void RegisterBookRef(void);
extern void ScrollToCurrent(void);
extern void SendAbort(void);
extern void SoupAddFlushed(void);
extern void SoupCollect(void);
extern void SoupGetFlags(void);
extern void SoupGetIndexesModTime(void);
extern void SoupGetInfoModTime(void);
extern void SoupIsValid(void);
extern void SoupSetFlags(void);
extern void StartIRSniffing(void);
extern void StopIRSniffing(void);
extern void Store1XPackageToVBO(void);
extern void StoreCheckUnion(void);
extern void StoreCheckWriteProtect(void);
extern void StoreConvertSoupSortTables(void);
extern void StoreCreateSoup(void);
extern void StoreErase(void);
extern void StoreGetAllInfo(void);
extern void StoreGetInfo(void);
extern void StoreGetKind(void);
extern void StoreGetName(void);
extern void StoreGetSignature(void);
extern void StoreGetSoup(void);
extern void StoreGetSoupNames(void);
extern void StoreHasPassword(void);
extern void StoreHasSoup(void);
extern void StoreIsReadOnly(void);
extern void StoreIsValid(void);
extern void StoreLock(void);
extern void StorePackageRestore(void);
extern void StoreSegmentedPackageRestore(void);
extern void StoreSetAllInfo(void);
extern void StoreSetInfo(void);
extern void StoreSetName(void);
extern void StoreSetPassword(void);
extern void StoreSetSignature(void);
extern void StoreTotalSize(void);
extern void StoreUnlock(void);
extern void StoreUsedSize(void);
extern void SuffixP(void);
extern void TableLookup(void);
extern void TurnToContent(void);
extern void TurnToPage(void);
extern void UnionSoupAddIndex(void);
extern void UnionSoupAddTags(void);
extern void UnionSoupFlush(void);
extern void UnionSoupGetSize(void);
extern void UnionSoupGetTags(void);
extern void UnionSoupModifyTag(void);
extern void UnionSoupRemoveIndex(void);
extern void UnionSoupRemoveTags(void);
extern void UniqueAppendList(void);
extern void UniqueAppendListGen(void);
extern void UnmatchedWords(void);
extern void UnregisterBookRef(void);
extern void UpdateBookmarks(void);
extern void UseTrainingDataForRecognition(void);
extern void WhereIsBook(void);
extern void ZapCancel(void);
extern void ZapReceive(void);
extern void ZapSend(void);
extern void ZoomView(void);

extern const size_t gROMFunctionSlotCount;
extern intptr_t * const gROMFunctionSlots[];

static void (* const gROMFunctions[1349])(void) = {
	FSetClass, FRecognizeInkWord, FExtractLong, FSetContains, FSetupTetheredListener, FAddDeferredSend, FBatteryCount, FRethrow,
	FSetOutputDevice, FSubstituteChars, FDefGlobalFn, FEntryFlush, FMatchedChar, PathToRoot, FStrHexDump, CircleDistance,
	FInTryString, FGetTrainingData, FSetLearningData, FGreaterOrEqual, FCategorizeKeyCommands, FInkOff, FGetBitmapPixel, FNewByName,
	FRecognize, FStuffChar, Fcosh, FGetCardTypes, UniqueAppendList, FDisplay, FGetTone, Ffesetround,
	FApply, FLoadOnlinePackage, Fpow, FGetZoneList, FIsProxyEntry, FSetOverlaps, FSetSysAlarm, FOtherViewInUse,
	FExtractRichStringFromParaSlots, FJournalReplayStrokes, FPidToPackageLite, FSetRamParaData, FDivide, FComputeParagraphHeight, AddHistory, FWRecIsBeingUsed,
	FClone, DumpDict, FDrawXBitmap, FPrimClassOf, FGetPkgInfoFromPSSid, FGetOrientation, FHaveZones, DSTagString,
	FPolyContainsInk, FSuckPackageOffDeskTop, DSFilterStringsAux, LexDateLookup, FSetAdd, FGetHiliteIndex, GuessAddressee, FLessThan,
	FActivateTestAgent, FFinishRecognizing, ReadDomainOptions, FSetCaretInfo, FFrameDirty, FSymbolName, FAppleTalkOpenCount, FTicks,
	FDrawStringShapes, FGetFontFamilyNum, FIsNativeFunction, FUnmountCard, FTestMDropConnection, FUnblockStrokes, FGetStores, FShellSort,
	FGesturePoint, FStringToTime, FTotalMinutes, FBackLight, FHideCaret, FMatchKeyMessage, FMakeRegion, FGetPkgRefInfo,
	FProtoPerformIfDefined, FMergeInk, FLength, Fisfinite, Fsinh, Ffetestexcept, FGetView, FIsArray,
	FPerform, StartIRSniffing, FGetKeyView, FWrite, FGetPopup, FStripInk, FArrayRemoveCount, FEntryCopy,
	FEntrySize, FSmartStop, FResetPowerStats, FGetCardInfo, FGetFontFace, FGetVariable, FCollect, FSendOnline,
	FFulfillImportTable, FJournalReplayALine, FQuickLookDone, FEntryUndoChanges, FTestReadTextFile, FUnorderedOrEqual, FMax, FNextMeeting,
	FGetHilitedTextItems, ReFlow, FStringAnnotate, FGetStroke, FGetVolume, UniqueAppendListGen, FStuffLong, FEntryModTime,
	Fannuity, FIsDirty, FTimeStr, FStringShorten, FLShift, FGetPackageStores, DSScanForWackyName, GetDictItem,
	FMinimumBatteryCheck, FGetDefaultFont, FTabletBufferEmpty, Fmin, FCapitalizeWords, FScanPrevWordEnd, FStrExactCompare, FDV,
	FIsSubclass, FIsLargeBinary, MapSymToFrame, FPSSidToPkgRef, FSetFontFamily, FStringToFrameMapper, Fcopysign, FTestWillCallExit,
	FQuickSort, FLookupCompletions, FCompressStrokes, FInitSerialDebugging, FRestorePatchPackage, FGetDictionaryData, DSGetMatchedEntries, FBitAnd,
	FGetChar, FGetBinaryCompander, FSoundCheck, Facosh, FOpenAppleTalk, FRemoveAutoAdd, FSendCode, FGetInkAt,
	FGetHiliteOffsets, FMap, FAddStepView, Ffesetexcept, CompositeClass, FShowCaret, FBackupPatchPackage, PhoneIndexToValue,
	FIsRichString, MakePhrasalLexEntry, FNextInkIndex, FStdioOn, FRemoveView, FConvertFromMP, FStrWidth, FStyledStrTruncate,
	Ffeclearexcept, FSpellDocBegin, FFontLeading, FIsImmediate, FDisposeTrainingData, FBMerge, FCloseAppleTalk, FSetCalibration,
	FDateNTime, FClassOf, FBackLightStatus, FBubbleArraySlot, FRegTaskTemplate, UniqueAppendList, FBeginsWith, FOrigPhrase,
	FAddDelayedCall, FGetRandomWord, FDeinstallPackage, FSymbolCompareLex, FArrayMunger, FTestGetParameterString, FIsKeyDown, FOpenRemote,
	FNewProxyEntry, FSetFontParms, IASmartCFLookup, FTestMSetParameterString, FLookupWord, FArrayToPoints, FBatteryStatus, FDrawDateLabels,
	FPointsToArray, FCurrentExports, Fferaiseexcept, FAbs, FEntryChange, FGetCaretBox, FOffsetShape, FScanWordEnd,
	FGetLCDContrast, FEntryChangeWithModTime, FGetId, FUnordered, FTime, FNewWeakArray, FCloseRemote, Fround,
	FRecognizePoly, FArrayPos, FStrEqual, FDrawLetterShapes, FGetBinaryStoredSize, FCalibrateTablet, FNBPLookupCount, FStringToNumber,
	FGetInsertionStyle, FReplaceObject, FBSearchLeft, FCountGesturePoints, FSetClipboard, FParaContainsInk, FGC, Fasinh,
	FTimeInSeconds, FBitOr, FDiv, Ftan, FIsPrimShape, FNTKDownload, FPostCommand, FSetRandomState,
	FGetSortId, SuffixP, FGetUnitDownTime, DSResolveString, FInRepeatedKeyCommand, FIsProtocolPartInUse, FInkOffUnHobbled, FCalcInkBounds,
	FSetSystemVolume, FEntryChangeVerbatim, FGreaterThan, FIsWhiteSpace, PhraseFilter, FCharPos, Fsignbit, FInsertItemsAtCaret,
	FMoveCorrectionInfo, FKeyIn, FLessOrGreater, FViewContainsCaretView, FSetValue, FGetLetterWeights, FPowerOff, FGetFontSize,
	Fremquo, FGetMyZone, FNumberStr, FAddUndoCall, FDeActivate1XPackage, FRecognizeTextInStyles, FGetPointsArrayXY, PhoneSymToString,
	FCeiling, FDrawMeetingGrid, FGetGlobals, Fexpm1, FDowncase, Ffegetenv, FGetNames, FLocalVar,
	FDecibelsToVolume, FPostCommandParam, FVolumeToDecibels, FBookRemoved, FFontDescent, FAddDeferredCall, FGetUnitEndTime, FHasSiblingSlot,
	FBlockStrokes, FEvalStringer, FUnorderedLessOrEqual, FAddDelayedAction, FFormattedNumberStr, FJournalStartRecord, FPrint, FScanWordStart,
	FViewAllowsInk, FStyleArrayContainsInk, FStrTruncate, FNBPStartLookup, FRestoreKeyView, Fatan2, FDecodeRichString, Flgamma,
	FFindStringInFrame, FGetBlue, FHandleKeyEvents, FIsInstance, FInsertTabletSample, FUnorderedOrGreater, FClassInfoRegistrySeed, FGetExportTableClients,
	FIsBinary, FEnablePowerStats, FIsTabletCalibrationNeeded, FHiliteOwner, FIsEqualTone, FPendingImports, FSetRemove, FGetCardSlotStores,
	FResolveEntryAlias, FStartBypassTablet, FExtractXLong, FStrokeBounds, FEntryIsResident, FTrimString, FUnionPoint, FGetPowerStats,
	FPlaySoundSync, FPidToPackage, FUnorderedLessOrGreater, FBFind, Fgamma, Flog1p, FMakeRect, FBoolAnd,
	FExtractData, FavorObject, Fmax, FEntryReplace, GetRelevantTemplates, FEntryReplaceWithModTime, FStrokeDone, LayoutMeeting,
	FNBPStart, FFramesStringer, FGetRemoteWriting, FExtractWord, GlueStrings, FPtInPicture, FStrMunger, FDismissPopup,
	FFlushImports, FGetUnionSoup, FStrFontWidth, FGetClipboardIcon, FPerformIfDefined, DSPrevSubStr, FFindKeyCommand, FGetLocale,
	CallOnlineServices, FEntryRemoveFromSoup, DSFindPossibleLocation, FMakeInk, FGetValue, FModalRecognitionOn, CorrectSelect, FMod,
	TableLookup, FGetVar, FIncrementMonth, FGetWordArray, FMergeStrokes, FSetInputGain, DSConstructSubjectLine, FGetSlot,
	FGetUnitUpTime, FGetFrameStuff, FGetUnitStartTime, FStrokesAfterUnit, FTestMStartTestFrame, FStrPos, FBusyBoxControl, FClearHardKeymap,
	FpkgDownload, FAddUndoAction, FTestReportError, FSpellDocEnd, FNegate, FLBRollback, FSetGlobal, Fatan,
	Flogb, FReal, FValidateWord, FGetPath, FGetHiliteWeight, Fldexp, FEndsWith, FAddDeferredAction,
	Fexp, FExtractByte, FSetOrientation, FPlaySound, FStripRecognitionWordDiacritsOK, FTestReportMessage, FGetBinaryCompanderData, FastStringLookup,
	FIsPackage, FBFindRight, FRecognizePara, FBinEqual, FGetGreen, CleanString, FRandom, FUpcase,
	FormatVertical, FSetHiliteIndex, FIsInteger, FBatteryLevel, FBinaryMunger, FTimeFrameStr, FEntrySetHandler, FBoolOr,
	FInkConvert, FPSSidToPid, FSplitInkAt, FStableSort, FNumInkWordsInRange, FIsString, FRepeatInfoToText, FSubstr,
	FRemoveStepView, FMakeWordInfo, FBFetch, Ffabs, FModalState, FNTKAlive, FStdioOff, FConnEntriesEqual,
	Fcos, FORD, FLongDateStr, FGetFunctionArgCount, FSetBounds, FSpellSkip, FUnionRect, FScanNextWord,
	FStringFormat, FDoPopup, FHourMinute, FRemoveSlot, FSetKeyView, FTextBounds, FEntrySoup, FScaleShape,
	FSetFontFace, FSetVariable, FMakeRichString, FExpandInk, SendAbort, ReflowPreflight, FClearPopup, FFloor,
	FPlaySoundIrregardless, FArrayInsert, FGetBinaryStore, FIsSameEntry, FNewIterator, FMapPtY, FInstallPackage, FClearTryString,
	FSetVolume, FStuffWord, FFlushStrokes, FStripRecognitionWord, FTestReadDataFile, DSAddLexiconFrame, FViewWorksWithCombCorrector, FPostKeyString,
	FAdd, FInsertStyledText, FBSearchRight, FGetIndexChar, FObjectPkgRef, FClicker, FGenFullCommands, FCapitalize,
	FUriahBinaryObjects, FConvertDictionaryData, FHitShape, FCHR, Ferf, ISATest, Fref, FPidToPkgRef,
	FSmartConcat, FAnimateSimpleStroke, LexTimeLookup, FGetLetterIndex, UnmatchedWords, FIsEntryAlias, FStringToDate, FCommandKeyboardConnected,
	FYieldToFork, FForkScript, FIsPathExpr, FStats, FStrokeBundleToInkWord, FGestureType, FGetLearningData, FCompressStrokesToInk,
	DSFindPossibleName, FBFetchRight, FIsValidDate, DSFindPossiblePhone, FGetSelectionStack, FSetDictionaryData, FDrawOriginal, FSetAref,
	FSetChar, FPlaySoundEffect, FBIntersect, Ffegetround, FIsMagicPtr, FMakeText, Fsin, FMakeTextBox,
	FSignum, FExtractCString, FIsSubexception, StopIRSniffing, FMapCursor, FStuffByte, Flog10, FConvertForMP,
	Frinttol, PhoneSymToIndex, FSmartStart, FTestMStartTestCase, FGetRamParaData, FBDelete, FRShift, FSpellCheck,
	Fscalb, PrepRecConfig, FMakePict, UseTrainingDataForRecognition, FMakeStrokeBundle, Ffeupdateenv, FEntryHandler, FBInsert,
	FNTKSend, FRegisterGestalt, FExpandUnit, FStrCompare, GenPhoneTypeList, FTieViews, FCurrentException, FGetFontFamilySym,
	FExtractPString, FGetStrokePoint, FEntryMove, FJournalReplayBusy, FGetStrokePointsArray, FHasVariable, FFindShape, FActivate1XPackage,
	FCountUnitStrokes, Ffeholdexcept, FGetCaretInfo, FLSearch, FrefOf, FMakeRoundRect, CoordinateToLongitude, LongitudeToCoordinate,
	Fisfinite, Flog, FPictToShape, FReboot, FSetLCDContrast, FBitXor, FRefreshViews, FStringFilter,
	FGetRoot, CheezySubsumption, FBDifference, Fhypot, FThrow, FEntryUniqueId, FRosettaExtension, FCountPoints,
	FAddDictionary, FDoScrubEffect, FMakeLine, FLFetch, FSendKeyMessage, FEqual, FBreakLoop, Facos,
	Ffmod, FGetFlags, FGetZoneFromName, FRemove1XPackage, FGetRandomDictionaryWord, FLessEqualOrGreater, FArray, FInkOn,
	FStrLen, FSetSortId, FQuickSort, FCheckCardBattery, FCancelOnlineService, FInsertionSort, FMakeWedge, FPackRGB,
	FMakeCompactFont, FMakeBinary, FavorAction, FShapeBounds, FDrawIntoBitmap, FExtractUniChar, Fnearbyint, FPostAndDo,
	FFindStringInArray, FNBPGetLookupNames, FBitNot, FBInsertRight, FBuildContext, FIsPtInRect, Ftrunc, GenerateSubstrings,
	FSetUnion, FSetLetterWeights, FSetFontSize, FPositionToTime, FTimeToPosition, FTestMGetReportMsg, FSetDifference, FDeactivateTestAgent,
	FModalRecognitionOff, FReplaceGestalt, Fremainder, FAref, FDate, FIsFrame, FSetTimeInSeconds, FUnorderedGreaterOrEqual,
	FFontHeight, GeneratePhrases, FExitBreakLoop, FEntrySizeWithoutVBOs, Ffesetenv, FGetTrueModifiers, FNBPGetCount, FResetLearningDefaults,
	FIntern, FLBClearCache, FExtractBytes, FStuffCString, FStrokeInPicture, FKeyboardConnected, FEntrySetCachedObject, FLessOrEqual,
	FMakeBitmap, Ffegetexcept, FNBPGetNames, FConnBuildStoreFrame, FGetRed, FDebugRunUntilIdle, FGetOnlineEndpoint, Fasin,
	Ftanh, FGetHeapStats, FGestalt, FDebugMemoryStats, FConvertToSoundFrame, FProtoPerform, FStuffPString, FCalcBundleBounds,
	FTryStringLength, FEntryStore, FGetPackages, FGetUnionSoupAlways, FGetCalibration, FSubtract, FDeepClone, FMakeShape,
	FSetDebugMode, FDateFromSeconds, FGetInkWordInfo, FGetSiblingSlot, FInsetRect, CheezyIntersect, FStrConcat, History,
	FRectsOverlap, FForLoop, FGetStrokeBounds, FPickViewKeyDown, FSetInkerPenSize, FTestExit, FSetRemoteWriting, FNextKeyView,
	FSplitString, FRecSettingsChanged, FGetAllMeetings, FIsAlphaNumeric, FIsInkChar, FSetLocale, FStripDiacriticals, FGetEditArray,
	CoordinateToLatitude, LatitudeToCoordinate, FIsValid, FMapRect, FFontAscent, Fnextafterd, FOffsetRect, FStrReplace,
	FTotalClone, FWeekNumber, FGetMeetingTypeInfo, FGetNextMeetingTime, FMakeTextLines, FGetWordInfo, FTestGetParameterArray, Fcompound,
	Fisnormal, FIsNumber, CorrectWord, FPenPos, FJournalStopRecord, FShortDateStr, FSetSlot, FTextBox,
	FBuildRecConfig, FNBPStopLookup, FBatteryRawStatus, FGetClipboard, FViewAutopsy, FGetPackageStore, FDebug, FGetBitmapInfo,
	FMakeOval, FExtractChar, FMungeBitmap, FSetOnlineDisconnect, FIsReal, FGetRandomState, FStrFilled, FClickLetterShapes,
	FGetRangeText, FGetTextFlags, FSpellCorrect, FStuffUniChar, Frandomx, FSetPath, CommonAncestors, FSetHiliteWeight,
	FEntryValid, FIsFunction, MakeLowerCase, FAddDelayedSend, FIsSymbol, FStringAssoc, FStopBypassTablet, FAddAutoAdd,
	FCountStrokes, FGetSystemVolume, FAddInk, FObjectPid, FGatherKeyCommands, FGetShapeInfo, FAddView, FClearUndoStacks,
	FMungeShape, FFindDictionaryFrame, FSectRect, FMin, FMakePolygon, FPrevMeeting, FEnsureInternal, FTriggerWordRecognition,
	FMeasuredNumberStr, Frint, FAddTryString, FJournalReplayAStroke, FNBPStop, FParseUtter, PhoneStringToValue, FQuery,
	FNextClassInfo, FEntryCachedObject, FHobbleTablet, FCurrentImports, FRunInitScripts, FTestFlushReportQueue, FSoundPlayEnabled, Append,
	FMakeEntryAlias, FStringToDateFrame, FAddArraySlot, FDESCreatePasswordKey, FGetAllMeetingsUnique, LexPhoneLookup, FConnectPassthruKeyboard, FIsReadOnly,
	FIsCommandKeystroke, FIsValidString, FGetPoint, FParamStr, FNot, FNTKListener, FMapPtX, FGetPointsArray,
	Fsqrt, FBookAvailable, FCountLetterShapes, FViewAllowsInkWords, FMultiply, FPreInitSerialDebugging, FIsSoupEntry, FPointsArrayToStroke,
	FClassInfoByName, DisconnectOnlineServices, FStackTrace, Fatanh, FSetRandomSeed, FHasVar, FRelBounds, FSetLength,
	FShortDate, FTranslate, FDoCursiveTraining, FUnRegTaskTemplate, FGetUndoState, FSPrintObject, FTranslateKey, FHasSlot,
	PrefixP, FUseWRec, FUnorderedOrLess, FReadCursiveOptions, DSFilterStrings, FPurgeAreaCache, FSetBatteryType, FBootSucceeded,
	FAddUndoSend, FIsCharacter, FEntryTextSize, FGetMeetingIcon, Ferfc, Fdim, FLoad, FCompile,
	FHasPath, Fisnan, FSleep, FUriah, FGetScoreArray, FProtocolInterfaceName, FProtocolImplementationName, FProtocolSignature,
	FProtocolVersion, FNewProtocol, FDestroyProtocol, FGetCapability, FHasCapability, FDispatchProtocol, StoreGetName, StoreHasSoup,
	StoreGetKind, FWriteEntireStoreObject, FDeleteStoreObject, StoreUsedSize, StoreCheckUnion, FLBAllocCompressed, StoreSetInfo, StoreCheckWriteProtect,
	FGetStoreCardSlot, FGetStoreCardType, StoreUnlock, StoreSetSignature, StoreSetAllInfo, StoreSetPassword, FStoreAbort, FLBAlloc,
	FSuckPackageFromEnpoint, StoreErase, StoreConvertSoupSortTables, FGetStoreObjectSize, FWriteStoreObject, StoreSetName, StoreGetSoupNames, StoreHasPassword,
	StoreTotalSize, StoreGetSoup, StoreLock, StoreCreateSoup, StoreSegmentedPackageRestore, Store1XPackageToVBO, StorePackageRestore, FReadStoreObject,
	StoreGetInfo, StoreIsValid, FSetStoreObjectSize, StoreGetSignature, FNewStoreObject, StoreIsReadOnly, StoreGetAllInfo, FSuckPackageFromBinary,
	SoupGetFlags, CommonSoupGetName, PlainSoupGetSize, SoupCollect, PlainSoupRemoveAllEntries, PlainSoupCopyEntries, PlainSoupAdd, PlainSoupHasTags,
	PlainSoupRemoveFromStore, PlainSoupSetInfo, PlainSoupGetNextUId, PlainSoupSetSignature, PlainSoupIndexSizes, PlainSoupSetAllInfo, SoupSetFlags, SoupAddFlushed,
	SoupGetIndexesModTime, PlainSoupSetName, PlainSoupAddTags, SoupGetInfoModTime, PlainSoupAddIndex, PlainSoupGetTags, PlainSoupCopyEntriesWithCallBack, PlainSoupMakeKey,
	PlainSoupAddWithUniqueId, PlainSoupGetIndexes, PlainSoupModifyTag, PlainSoupRemoveTags, PlainSoupGetStore, PlainSoupGetInfo, SoupIsValid, PlainSoupGetSignature,
	PlainSoupRemoveIndex, CommonSoupQuery, PlainSoupGetAllInfo, PlainSoupFlush, CursorMove, CursorGoto, CursorGotoKey, CursorNext,
	CursorPrev, CursorReset, CursorResetToEnd, CursorEntry, CursorClone, CursorCountEntries, CursorWhichEnd, CursorSoup,
	CursorIndexPath, CursorEntryKey, CursorStatus, PageScroll, PageThumbnail, BookTitle, PrepBook, PageContents,
	UpdateBookmarks, InkMarks, AddBookmark, PreviousPage, Bookmarks, BookClosed, AddInkMarks, CurrentKiosk,
	CurrentPage, TurnToPage, CountPages, WhereIsBook, FindContentBySlot, HiliteBlock, AddToContentArea, CurrentBook,
	AuthorData, FindContentByValue, CurrentKiosk, FindPageByValue, TurnToContent, CurrentPage, TurnToPage, RegisterBookRef,
	CountPages, WhereIsBook, FindContentBySlot, ZoomView, CuFind, HiliteBlock, FindPageByContent, AddToContentArea,
	CurrentBook, FindPageBySubject, AuthorData, UnregisterBookRef, FindContentByValue, FAirusDictionaryType, FAirusResult, FAirusLookupWord,
	FAirusDeleteWord, FAirusWalkDictionary, FAirusAttributeSize, FAirusDeletePrefix, FAirusChangeAttribute, FAirusIteratorMake, FAirusUnregisterDictionary, FAirusAddWord,
	FAirusDispose, FAirusNew, FAirusRegisterDictionary, CIConnect, CIReject, CIFlushPartial, CIRelease, CIDisconnect,
	CIGetOptions, CIDisposeLeavingTEndpoint, CIListen, CIOutput, CIInput, CIInstantiateFromEndpoint, CIJustUnBind, CIInputAvailable,
	CIAbort, CIReadyForOutput, CIPartial, CIJustConnect, CISetOptions, CIOutputFrame, CIJustRelease, CIJustOpen,
	CIJustBind, CIJustClose, CIJustListen, CIDispose, CIInstantiate, CIFlushOutput, CIJustDisconnect, CISetSync,
	CIFlushInput, CIBytesAvailable, CICaller, CISetInputSpec, CIOutputDone, CIState, CIAccept, FDrawExpando,
	FPickViewScroll, FPickViewGetScollerValues, CommonSoupGetName, UnionSoupGetSize, SoupCollect, UnionSoupAddTags, UnionSoupAddIndex, UnionSoupGetTags,
	UnionSoupModifyTag, UnionSoupRemoveTags, UnionSoupRemoveIndex, CommonSoupQuery, UnionSoupFlush, FEventPause, FAddKeyCommand, FDragAndDropLtd,
	FParentX, FSyncScrollX, FGetDrawBoxX, FVisibleBox, FDrawPolygons, FSetHiliteX, FDragAndDrop, FEffectX,
	FCloseX, FBlockKeyCommand, FSlideEffectX, FGlobalOuterBoxX, FUnregisterOpenKeyboard, FPointToCharOffset, FKeyHelpSlipSetup, FKeyHelpSlipDraw,
	ZapCancel, ZapSend, ZapReceive, RefreshTopics, ScrollToCurrent, RefreshTopics, FTopicByName, FDisplaySplashGraphic,
	FGetRichString, FGetStylesOfRange, FClipboardCommand, FHiliteX, FHideX, FHandleUnit, FGetAlternates, FLoadFontCache,
	FAddKeyCommands, FChildViewFramesX, FLocalBoxX, FHiliteUniqueX, FInvertRect, FShowX, FRevealEffectX, FLockScreen,
	FAddUndoAction, FSyncChildrenX, FRegisterOpenKeyboard, FMoveBehindX, FHiliteViewChildren, FHandleInkWord, FDropHilites, FSetPopup,
	FPointToWord, FPositionCaret, FExtractTextRange, FGetStyleAtOffset, FModalDialog, FFilterDialog, FCaretRelativeToVisibleRect, FSetupIdleX,
	FSyncViewX, FChangeStylesOfRange, FGlobalBoxX, FExitModalDialog, FVoteOnWordUnit, FSetOriginX, FDoDrawing, FDrawShape,
	FKeyboardInputX, FTrackButtonX, FRedoChildrenX, FDirtyX, FDirtyBoxX, FDragX, FViewIntoBitmap, FTrackHiliteX,
	FCopyBits, FHandleRawInk, FToggleX, FOpenX, FDeleteX, FCloseX, FSetHiliteNoUpdateX, FGrayShrink,
	FOffsetView, FGetPolygons, FLayoutTableX, FHandleInsertItems, FHiliter, FLayoutVerticallyX, FExtractRangeAsRichString, FHideX,
	FIAatWork, FAirusIteratorNextWord, FAirusIteratorClone, FAirusIteratorReset, FAirusIteratorThisWord, FAirusIteratorDispose, FAirusIteratorPreviousWord, CIStartCCL,
	CIStopCCL, CINewFlushPartial, CINewDisconnect, CINewDisposeLeavingTEndpoint, CINewBind, CINewListen, CINewOutput, CINewInput,
	CINewDisposeLeavingTool, CINewInstantiateFromEndpoint, CINewOption, CINewPartial, CIRequestsPending, CINewConnect, CINewDispose, CINewInstantiate,
	CINewFlushInput, CINewSetState, CINewAbort, CINewSetInputSpec, CINewUnBind, CINewState, CINewAccept, EWFileToFrame,
	EWFrameToFile, EWBufferStart, EWOutput, EWSend, CIStopCCL, EWGetMacHeader, EWUnEscape, EWUniAtomHandler,
	EWGetError, EWOutputAtom, EWDecodePacket, EWEscape, EWInstantiate, EWGetData, CIStartCCL, EWSetError,
	EWConnectToHost, EWUploadFile, FSoundSchedule, FGetChannelInputGain, FSoundPause, FSoundOpen, FSoundClose, FGetChannelVolume,
	FSetChannelInputGain, FSoundStop, FSoundIsPaused, FSetChannelVolume, FSoundIsActive, FSoundStart, FMakeDragRef, FListBottom,
	FIsCollapsed, FVisibleTopicIndex, FChildTemplateFromTopic, FCollapseTopic, FSetupVisibleChildren, FTopicIndexToView, FTopicBottom, FExpandTopic,
	FMarkerBounds, FAdjustParagraph, FMungeStyles, CISNewInstantiate, CISNewInstantiateFromEndpoint, CISStreamOut, CISStreamIn, CSDispose,
	CSInstantiate, CSGetDefaultConfig, CSSetDefaultConfig, TurnToPage, FOffsetCorrectionInfo, FRemoveCorrectionInfo, FFindWordInfo, FAddUnitInfo,
	FAddWordInfo, FFindNewInfo, FClearCorrectionInfo, FExtractRange, FInsertRange, FMergeWordInfo, FSetWordList, FGetWordList,
	FAutoAdd, FAutoRemove, FDoEntryLearning, FTestWordInfoFlags, FSetWordInfoFlags, FClearWordInfoFlags, FAddCapitalizedEntry, FRemoveToggledEntries,
	FMoveWordFirst, FConnReadCommandHeader, FProcessBuiltinCommand, FConnInstallProtocolExtension, FConnReadBytes, FConnInstantiate, FConnReadCommand, FConnAbort,
	FConnDoKeyboardPassthrough, FConnectionState, FConnRetryPassword, FConnGetSyncChanges, FConnStop, FConnDesktopType, FConnWriteCommandHeader, FConnDoConnection,
	FConnConnect, FConnReadCommandData, FConnFlushCommandData, FConnRemoveProtocolExtension, FConnWriteBytes, FConnSetState, FConnBytesAvailable, FConnGetCurrentStore,
	FConnWriteCommand, CFDispose, CFInstantiate, CFRecord, FTXCharToPoint, FTXExternalize, FTXPointToChar, FTXIsModified,
	FTXSetGeometry, FTXReplaceAll, FTXChangeRangeRuns, FTXScroll, FTXGetParagraphRange, FTXChangeRangeRulers, FTXGetCountCharacters, FTXSetStore,
	FTXGetHiliteRange, FTXInternalize, FTXGetRangeData, FTXGetWordRange, FTXSetDrawOrigin, FTXGetScrollValues, FTXSetReadOnly, FTXPaste,
	FTXIsRulerShown, FTXGetTotalWidth, FTXInsertPageBreak, FTXUpdateRulerInfo, FTXCopy, FTXSetHiliteRange, FTXViewFindString, FTXGetContinuousRun,
	FTXGetCountPages, FTXGetTotalHeight, FTXHideRuler, FTXGetTextViewRect, FTXCut, FTXClear, FTXShowRuler, FTXGetLineRange,
	FTXReplace, FTXFinderFindString, FTXFinderGetRangeText, FTXFinderGetCountCharacters, FGetSerialNumber
};

void
InstallROMFunctions(void)
{
	size_t i;
	if (gROMFunctionSlotCount != sizeof(gROMFunctions) / sizeof(gROMFunctions[0]))
		abort();	/* ROMData.c and ROMFunctions.c come from different ResMaker runs */
	for (i = 0; i < gROMFunctionSlotCount; ++i)
		*gROMFunctionSlots[i] = (intptr_t)gROMFunctions[i];
}
