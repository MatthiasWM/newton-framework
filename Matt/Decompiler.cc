
/*
 File:    MattsDecompiler.cc

 Decompile a NewtonScript function.

 Written by:  Matt, 2025.
 */

#include "Matt/Decompiler.h"

#include "Matt/AST.h"
#include "Matt/ASTAdmin.h"
#include "Matt/ASTDataFlow.h"
#include "Matt/ASTControlFlow.h"
#include "Matt/ASTControlFlowHelper.h"

#include "Frames/Frames.h"
#include "Frames/Iterators.h"

#include <algorithm>
#include <tuple>

using namespace ast;

/*
 NTK Settings:
 Platform: platform file used is reflected in the "info" entry. All other
    changes are based on the platform file that was used
 Compile for debugging: adds 'DebuggerInfo slots after 'numArgs and possibly
    more
 Use StepChildren slot: if not checked. the stepChildren slot is an array with
    a different format vs. the more usual frame format
 Compile for Profiling: 'DebuggerInfo slot gets some additional information
 Newton 2.0 Platform only: create package1 instead of package0, no other changes?!
 Faster Functions: use kPlainFuncClass instead of 'CodeBlock, corresponds to
    nos2 vs. nos1. There is no flag in the header that indicates this!
 Tighter Object Packing: set the nos2 bit in the package part: "nos2: true"
 */

// Reverse int CCompiler::walkForCode(RefArg inGraph, bool inFinalNode)

/* TODO: all allocated AST nodes should be kept in a single vector and never
    be deleted during eval, but instead when the Decompiler is deleted. When
    new nodes are created at eval time, the must be added to that
    same cleanup vector.
 */
/* TODO: in NTK, we can check a box to create debug information. The decompiler should be aware of
  debug information in the code. Especially with nos2, this can restore argument
  names. In any format, it can give names to our views in the stepChildren array.
 */

/*

 Test apps
 =========

 Untested
 --------

 ./testdec '/Users/matt/dev/Einstein/Sample Code/Application Design/Altered States-6/Using NTK Layout/Altered States.pkg'
 open -a xcode '/Users/matt/dev/Einstein/Sample Code/Application Design/Altered States-6/Using NTK Layout/Altered States.text'
 ./testdec '/Users/matt/dev/Einstein/Sample Code/Application Design/Altered States-6/Using Text Files/Altered States.pkg'
 open -a xcode '/Users/matt/dev/Einstein/Sample Code/Application Design/Altered States-6/Using Text Files/Altered States.text'
 ./testdec '/Users/matt/dev/Einstein/Sample Code/Application Design/ChezDTS-2/ChezDTS.pkg'
 open -a xcode '/Users/matt/dev/Einstein/Sample Code/Application Design/ChezDTS-2/ChezDTS.text'
 ./testdec '/Users/matt/dev/Einstein/Sample Code/Application Design/ChezDTS-2/French Onion.pkg'
 open -a xcode '/Users/matt/dev/Einstein/Sample Code/Application Design/ChezDTS-2/French Onion.text'
 ./testdec '/Users/matt/dev/Einstein/Sample Code/Application Design/ChezDTS-2/Gnocchi.pkg'
 open -a xcode '/Users/matt/dev/Einstein/Sample Code/Application Design/ChezDTS-2/Gnocchi.text'
 ./testdec '/Users/matt/dev/Einstein/Sample Code/Application Design/ChezDTS-2/Salmon.pkg'
 open -a xcode '/Users/matt/dev/Einstein/Sample Code/Application Design/ChezDTS-2/Salmon.text'
 ./testdec '/Users/matt/dev/Einstein/Sample Code/Application Design/ChezDTS-2/Soufflé.pkg'
 open -a xcode '/Users/matt/dev/Einstein/Sample Code/Application Design/ChezDTS-2/Soufflé.text'
 ./testdec '/Users/matt/dev/Einstein/Sample Code/Application Design/DeletionScript-2/Deletion Script.pkg'
 open -a xcode '/Users/matt/dev/Einstein/Sample Code/Application Design/DeletionScript-2/Deletion Script.text'
 ./testdec '/Users/matt/dev/Einstein/Sample Code/Application Design/ExtensionTap-1/ExtensionTap.π.pkg'
 open -a xcode '/Users/matt/dev/Einstein/Sample Code/Application Design/ExtensionTap-1/ExtensionTap.π.text'
 ./testdec '/Users/matt/dev/Einstein/Sample Code/Application Design/True Grid-5/True Grid.pkg'
 open -a xcode '/Users/matt/dev/Einstein/Sample Code/Application Design/True Grid-5/True Grid.text'
 ./testdec '/Users/matt/dev/Einstein/Sample Code/Controls and Other Protos/Gauges-2/Gauges.pkg'
 open -a xcode '/Users/matt/dev/Einstein/Sample Code/Controls and Other Protos/Gauges-2/Gauges.text'
 ./testdec '/Users/matt/dev/Einstein/Sample Code/Controls and Other Protos/Glancing-2/Glancing.pkg'
 open -a xcode '/Users/matt/dev/Einstein/Sample Code/Controls and Other Protos/Glancing-2/Glancing.text'
 ./testdec '/Users/matt/dev/Einstein/Sample Code/Controls and Other Protos/NouveauScroll-2/NouveauScroll.pkg'
 open -a xcode '/Users/matt/dev/Einstein/Sample Code/Controls and Other Protos/NouveauScroll-2/NouveauScroll.text'
 ./testdec '/Users/matt/dev/Einstein/Sample Code/Controls and Other Protos/RadioCluster-3/Cluster.pkg'
 open -a xcode '/Users/matt/dev/Einstein/Sample Code/Controls and Other Protos/RadioCluster-3/Cluster.text'
 ./testdec '/Users/matt/dev/Einstein/Sample Code/Controls and Other Protos/protoVertSlider-1/protoVertSlider test.pkg'
 open -a xcode '/Users/matt/dev/Einstein/Sample Code/Controls and Other Protos/protoVertSlider-1/protoVertSlider test.text'
 ./testdec '/Users/matt/dev/Einstein/Sample Code/Desktop Connectivity/Mini-MetaData-1/Newton Source/Mini-MetaData.pkg'
 '/Users/matt/dev/Einstein/Sample Code/Desktop Connectivity/SoupDrink-Newton-4/SoupDrink.pkg'
 open -a xcode ./testdec '/Users/matt/dev/Einstein/Sample Code/Desktop Connectivity/SoupDrink-Newton-4/SoupDrink.text'
 ./testdec '/Users/matt/dev/Einstein/Sample Code/Desktop Connectivity/SuiteP-Mac-2/SoupDrink-4.pkg'
 ./testdec '/Users/matt/dev/Einstein/Sample Code/Desktop Connectivity/SuiteP-Windows-2/SoupDrink-4.pkg'
 ./testdec '/Users/matt/dev/Einstein/Sample Code/Developer Tools/MonacoTest-5/Monaco Font/Monaco.pkg'
 open -a xcode '/Users/matt/dev/Einstein/Sample Code/Developer Tools/MonacoTest-5/Monaco Font/Monaco.text'
 ./testdec '/Users/matt/dev/Einstein/Sample Code/Developer Tools/MonacoTest-5/MonacoTest.pkg'
 open -a xcode '/Users/matt/dev/Einstein/Sample Code/Developer Tools/MonacoTest-5/MonacoTest.text'
 ./testdec '/Users/matt/dev/Einstein/Sample Code/Developer Tools/MooUnit-1/MooUnit.pkg'
 open -a xcode '/Users/matt/dev/Einstein/Sample Code/Developer Tools/MooUnit-1/MooUnit.text'
 ./testdec '/Users/matt/dev/Einstein/Sample Code/Developer Tools/MooUnit-1/MooUser.pkg'
 open -a xcode '/Users/matt/dev/Einstein/Sample Code/Developer Tools/MooUnit-1/MooUser.text'
 ./testdec '/Users/matt/dev/Einstein/Sample Code/Digital Books/Beyond Help-5/Beyond Help.pkg'
 open -a xcode '/Users/matt/dev/Einstein/Sample Code/Digital Books/Beyond Help-5/Beyond Help.text'
 ./testdec '/Users/matt/dev/Einstein/Sample Code/Digital Books/Book Maker Examples-1/BigPicture/BigPicture.pkg'
 ./testdec '/Users/matt/dev/Einstein/Sample Code/Digital Books/Book Maker Examples-1/Browser/Browser.pkg'
 ./testdec '/Users/matt/dev/Einstein/Sample Code/Digital Books/Book Maker Examples-1/Flags/Flags.pkg'
 ./testdec '/Users/matt/dev/Einstein/Sample Code/Digital Books/Book Maker Examples-1/Kiosk/Kiosk.pkg'
 ./testdec '/Users/matt/dev/Einstein/Sample Code/Digital Books/Book Maker Examples-1/Layout/Layout.pkg'
 ./testdec '/Users/matt/dev/Einstein/Sample Code/Digital Books/Book Maker Examples-1/MoreBrowsing/MoreBrowsing.pkg'
 ./testdec '/Users/matt/dev/Einstein/Sample Code/Digital Books/Book Maker Examples-1/Picture/Picture.pkg'
 ./testdec '/Users/matt/dev/Einstein/Sample Code/Digital Books/Book Maker Examples-1/Simple/Simple.pkg'
 ./testdec '/Users/matt/dev/Einstein/Sample Code/Digital Books/BookSample-4/BookSample.π.pkg'
 ./testdec '/Users/matt/dev/Einstein/Sample Code/Drawing and Graphics/Bitmap-2/Bitmap.pkg'
 open -a xcode '/Users/matt/dev/Einstein/Sample Code/Drawing and Graphics/Bitmap-2/Bitmap.text'
 ./testdec '/Users/matt/dev/Einstein/Sample Code/Drawing and Graphics/Dot2Dot-3/Dot2Dot.pkg'
 open -a xcode '/Users/matt/dev/Einstein/Sample Code/Drawing and Graphics/Dot2Dot-3/Dot2Dot.text'
 ./testdec '/Users/matt/dev/Einstein/Sample Code/Drawing and Graphics/Drawing-4/Drawing.pkg'
 open -a xcode '/Users/matt/dev/Einstein/Sample Code/Drawing and Graphics/Drawing-4/Drawing.text'
 ./testdec '/Users/matt/dev/Einstein/Sample Code/Drawing and Graphics/Photo Album-1/Photo Album.pkg'
 open -a xcode '/Users/matt/dev/Einstein/Sample Code/Drawing and Graphics/Photo Album-1/Photo Album.text'
 ./testdec '/Users/matt/dev/Einstein/Sample Code/Drawing and Graphics/Up In Smoke-33&2:3/UpInSmoke.π.pkg'
 open -a xcode '/Users/matt/dev/Einstein/Sample Code/Drawing and Graphics/Up In Smoke-33&2:3/UpInSmoke.π.text'
 ./testdec '/Users/matt/dev/Einstein/Sample Code/Endpoints/Basic Modem-2/Basic Modem.pkg'
 open -a xcode '/Users/matt/dev/Einstein/Sample Code/Endpoints/Basic Modem-2/Basic Modem.text'
 ./testdec '/Users/matt/dev/Einstein/Sample Code/Endpoints/Basic Serial-2/Basic Serial.pkg'
 open -a xcode '/Users/matt/dev/Einstein/Sample Code/Endpoints/Basic Serial-2/Basic Serial.text'
 ./testdec '/Users/matt/dev/Einstein/Sample Code/Endpoints/Comms FSM-6/Comms FSM.pkg'
 open -a xcode '/Users/matt/dev/Einstein/Sample Code/Endpoints/Comms FSM-6/Comms FSM.text'
 ./testdec '/Users/matt/dev/Einstein/Sample Code/Endpoints/Thumb-8/Thumb.pkg'
 open -a xcode '/Users/matt/dev/Einstein/Sample Code/Endpoints/Thumb-8/Thumb.text'
 ./testdec '/Users/matt/dev/Einstein/Sample Code/Endpoints/Tool Time-2/Application/Tool Time.pkg'
 open -a xcode '/Users/matt/dev/Einstein/Sample Code/Endpoints/Tool Time-2/Application/Tool Time.text'
 ./testdec '/Users/matt/dev/Einstein/Sample Code/Endpoints/Tool Time-2/Modules/Standard Tools.pkg'
 open -a xcode '/Users/matt/dev/Einstein/Sample Code/Endpoints/Tool Time-2/Modules/Standard Tools.text'
 open -a xcode '/Users/matt/dev/Einstein/Sample Code/Endpoints/Tool Time-2/Stream File/Tool Time Stream File.text'
 ./testdec '/Users/matt/dev/Einstein/Sample Code/Endpoints/Tool Time-2/Test Monitor/Test Monitor.pkg'
 open -a xcode '/Users/matt/dev/Einstein/Sample Code/Endpoints/Tool Time-2/Test Monitor/Test Monitor.text'
 ./testdec '/Users/matt/dev/Einstein/Sample Code/Endpoints/Tool Time-2/Tool Time Cleanup/Tool Time Cleanup.pkg'
 open -a xcode '/Users/matt/dev/Einstein/Sample Code/Endpoints/Tool Time-2/Tool Time Cleanup/Tool Time Cleanup.text'
 ./testdec '/Users/matt/dev/Einstein/Sample Code/Localization/CreatingALocale-2/CreatingALocale.pkg'
 open -a xcode '/Users/matt/dev/Einstein/Sample Code/Localization/CreatingALocale-2/CreatingALocale.text'
 ./testdec '/Users/matt/dev/Einstein/Sample Code/Modem Setup/Modem Setup-2/Brand X Modem.pkg'
 open -a xcode '/Users/matt/dev/Einstein/Sample Code/Modem Setup/Modem Setup-2/Brand X Modem.text'
 ./testdec '/Users/matt/dev/Einstein/Sample Code/NewtApp/Checkbook-8/Checkbook.pkg'
 open -a xcode '/Users/matt/dev/Einstein/Sample Code/NewtApp/Checkbook-8/Checkbook.text'
 ./testdec '/Users/matt/dev/Einstein/Sample Code/NewtApp/newtLabelPicker-1/testNewtLabelPicker.pkg'
 open -a xcode '/Users/matt/dev/Einstein/Sample Code/NewtApp/newtLabelPicker-1/testNewtLabelPicker.text'
 ./testdec '/Users/matt/dev/Einstein/Sample Code/NewtonScript/Inspector Gadget-4/Inspect.pkg'
 open -a xcode '/Users/matt/dev/Einstein/Sample Code/NewtonScript/Inspector Gadget-4/Inspect.text'
 ./testdec '/Users/matt/dev/Einstein/Sample Code/Pickers, Popups, and Overviews/ListPickerSamples-2/1. ListPickerArray/ListPickerArray.pkg'
 open -a xcode '/Users/matt/dev/Einstein/Sample Code/Pickers, Popups, and Overviews/ListPickerSamples-2/1. ListPickerArray/ListPickerArray.text'
 ./testdec '/Users/matt/dev/Einstein/Sample Code/Pickers, Popups, and Overviews/ListPickerSamples-2/2. ListPickerPopUp/ListPickerPopUp.pkg'
 open -a xcode '/Users/matt/dev/Einstein/Sample Code/Pickers, Popups, and Overviews/ListPickerSamples-2/2. ListPickerPopUp/ListPickerPopUp.text'
 ./testdec '/Users/matt/dev/Einstein/Sample Code/Pickers, Popups, and Overviews/ListPickerSamples-2/3. listPickerSoup/ListPickerSoup.pkg'
 open -a xcode '/Users/matt/dev/Einstein/Sample Code/Pickers, Popups, and Overviews/ListPickerSamples-2/3. listPickerSoup/ListPickerSoup.text'
 ./testdec '/Users/matt/dev/Einstein/Sample Code/Pickers, Popups, and Overviews/ListPickerSamples-2/4. listPickerSoupNewEntry/ListPickerNewEntry.pkg'
 open -a xcode '/Users/matt/dev/Einstein/Sample Code/Pickers, Popups, and Overviews/ListPickerSamples-2/4. listPickerSoupNewEntry/ListPickerNewEntry.text'
 ./testdec '/Users/matt/dev/Einstein/Sample Code/Pickers, Popups, and Overviews/ListPickerSamples-2/5. ListPickerChange/ListPickerChange.pkg'
 open -a xcode '/Users/matt/dev/Einstein/Sample Code/Pickers, Popups, and Overviews/ListPickerSamples-2/5. ListPickerChange/ListPickerChange.text'
 ./testdec '/Users/matt/dev/Einstein/Sample Code/Pickers, Popups, and Overviews/ListPickerSamples-2/6. listPickerIcon/ListPickerIcon.pkg'
 open -a xcode '/Users/matt/dev/Einstein/Sample Code/Pickers, Popups, and Overviews/ListPickerSamples-2/6. listPickerIcon/ListPickerIcon.text'
 ./testdec '/Users/matt/dev/Einstein/Sample Code/Pickers, Popups, and Overviews/PictIndex-1/pictIndex.pkg'
 open -a xcode '/Users/matt/dev/Einstein/Sample Code/Pickers, Popups, and Overviews/PictIndex-1/pictIndex.text'
 ./testdec '/Users/matt/dev/Einstein/Sample Code/Pickers, Popups, and Overviews/WhereInTheWorld-1/WhereInTheWorld.pkg'
 open -a xcode '/Users/matt/dev/Einstein/Sample Code/Pickers, Popups, and Overviews/WhereInTheWorld-1/WhereInTheWorld.text'
 ./testdec '/Users/matt/dev/Einstein/Sample Code/Pickers, Popups, and Overviews/protoNumberPicker_TDS-1/NumberPicker.pkg'
 open -a xcode '/Users/matt/dev/Einstein/Sample Code/Pickers, Popups, and Overviews/protoNumberPicker_TDS-1/NumberPicker.text'
 ./testdec '/Users/matt/dev/Einstein/Sample Code/Pickers, Popups, and Overviews/protoOverview-2/protoOverview.pkg'
 open -a xcode '/Users/matt/dev/Einstein/Sample Code/Pickers, Popups, and Overviews/protoOverview-2/protoOverview.text'
 ./testdec '/Users/matt/dev/Einstein/Sample Code/Pickers, Popups, and Overviews/protoSlimPicker-1/slimFaxPicker/slimFaxPicker.pkg'
 open -a xcode '/Users/matt/dev/Einstein/Sample Code/Pickers, Popups, and Overviews/protoSlimPicker-1/slimFaxPicker/slimFaxPicker.text'
 ./testdec '/Users/matt/dev/Einstein/Sample Code/Pickers, Popups, and Overviews/protoSlimPicker-1/slimPeoplePicker/slimPeoplePicker.pkg'
 open -a xcode '/Users/matt/dev/Einstein/Sample Code/Pickers, Popups, and Overviews/protoSlimPicker-1/slimPeoplePicker/slimPeoplePicker.text'
 ./testdec '/Users/matt/dev/Einstein/Sample Code/Recognition/CharEdit-2/CharEdit.pkg'
 open -a xcode '/Users/matt/dev/Einstein/Sample Code/Recognition/CharEdit-2/CharEdit.text'
 ./testdec '/Users/matt/dev/Einstein/Sample Code/Recognition/WordArray-2/WordArray.pkg'
 open -a xcode '/Users/matt/dev/Einstein/Sample Code/Recognition/WordArray-2/WordArray.text'
 ./testdec '/Users/matt/dev/Einstein/Sample Code/Routing/AutoRoute-4/AutoRoute.pkg'
 open -a xcode '/Users/matt/dev/Einstein/Sample Code/Routing/AutoRoute-4/AutoRoute.text'
 ./testdec '/Users/matt/dev/Einstein/Sample Code/Routing/CustomRoute-2/CustomRoute.pkg'
 open -a xcode '/Users/matt/dev/Einstein/Sample Code/Routing/CustomRoute-2/CustomRoute.text'
 ./testdec '/Users/matt/dev/Einstein/Sample Code/Routing/MultiRoute-1/MultiRoute.pkg'
 open -a xcode '/Users/matt/dev/Einstein/Sample Code/Routing/MultiRoute-1/MultiRoute.text'
 ./testdec '/Users/matt/dev/Einstein/Sample Code/Routing/VariRoute-1/VariRoute.pkg'
 open -a xcode '/Users/matt/dev/Einstein/Sample Code/Routing/VariRoute-1/VariRoute.text'
 ./testdec '/Users/matt/dev/Einstein/Sample Code/Sound/Bitching Piano-3/Piano.pkg'
 open -a xcode '/Users/matt/dev/Einstein/Sample Code/Sound/Bitching Piano-3/Piano.text'
 ./testdec '/Users/matt/dev/Einstein/Sample Code/Sound/Serenade-1/Serenade.pkg'
 open -a xcode '/Users/matt/dev/Einstein/Sample Code/Sound/Serenade-1/Serenade.text'
 ./testdec '/Users/matt/dev/Einstein/Sample Code/Sound/Sound Advice (Mac)-3/soundAdvice.pkg'
 open -a xcode '/Users/matt/dev/Einstein/Sample Code/Sound/Sound Advice (Mac)-3/soundAdvice.text'
 ./testdec '/Users/matt/dev/Einstein/Sample Code/Sound/Sound Tricks-4/SoundTricks.pkg'
 open -a xcode '/Users/matt/dev/Einstein/Sample Code/Sound/Sound Tricks-4/SoundTricks.text'
 ./testdec '/Users/matt/dev/Einstein/Sample Code/Sound/SoundStudio-2/SoundStudio.pkg'
 open -a xcode '/Users/matt/dev/Einstein/Sample Code/Sound/SoundStudio-2/SoundStudio.text'
 ./testdec '/Users/matt/dev/Einstein/Sample Code/Stationery/WhoOwesWhom-5/Card Project.pkg'
 open -a xcode '/Users/matt/dev/Einstein/Sample Code/Stationery/WhoOwesWhom-5/Card Project.text'
 ./testdec '/Users/matt/dev/Einstein/Sample Code/Stationery/WhoOwesWhom-5/Extend Notes Project.pkg'
 open -a xcode '/Users/matt/dev/Einstein/Sample Code/Stationery/WhoOwesWhom-5/Extend Notes Project.text'
 ./testdec '/Users/matt/dev/Einstein/Sample Code/Stationery/WhoOwesWhom-5/Page Project.pkg'
 open -a xcode '/Users/matt/dev/Einstein/Sample Code/Stationery/WhoOwesWhom-5/Page Project.text'
 ./testdec '/Users/matt/dev/Einstein/Sample Code/Stationery/WhoOwesWhom-5/Roll Project.pkg'
 open -a xcode '/Users/matt/dev/Einstein/Sample Code/Stationery/WhoOwesWhom-5/Roll Project.text'
 ./testdec '/Users/matt/dev/Einstein/Sample Code/System Data and Built-in Apps/Cardfile Extensions-1/Cardfile Extensions.pkg'
 open -a xcode '/Users/matt/dev/Einstein/Sample Code/System Data and Built-in Apps/Cardfile Extensions-1/Cardfile Extensions.text'
 ./testdec '/Users/matt/dev/Einstein/Sample Code/System Data and Built-in Apps/Extra Change-3/ExtraChange.pkg'
 open -a xcode '/Users/matt/dev/Einstein/Sample Code/System Data and Built-in Apps/Extra Change-3/ExtraChange.text'
 ./testdec '/Users/matt/dev/Einstein/Sample Code/System Data and Built-in Apps/HandWrite-1/HandWrite.pkg'
 open -a xcode '/Users/matt/dev/Einstein/Sample Code/System Data and Built-in Apps/HandWrite-1/HandWrite.text'
 ./testdec '/Users/matt/dev/Einstein/Sample Code/System Data and Built-in Apps/Party Time-1/Party Time.pkg'
 open -a xcode '/Users/matt/dev/Einstein/Sample Code/System Data and Built-in Apps/Party Time-1/Party Time.text'
 ./testdec '/Users/matt/dev/Einstein/Sample Code/System Data and Built-in Apps/PeoplePicker-1/PeoplePicker.pkg'
 open -a xcode '/Users/matt/dev/Einstein/Sample Code/System Data and Built-in Apps/PeoplePicker-1/PeoplePicker.text'
 ./testdec '/Users/matt/dev/Einstein/Sample Code/System Data and Built-in Apps/Sketch-1/Sketch.pkg'
 open -a xcode '/Users/matt/dev/Einstein/Sample Code/System Data and Built-in Apps/Sketch-1/Sketch.text'
 ./testdec '/Users/matt/dev/Einstein/Sample Code/System Data and Built-in Apps/Stamps&Patterns-1/Stamps&Patterns.pkg'
 open -a xcode '/Users/matt/dev/Einstein/Sample Code/System Data and Built-in Apps/Stamps&Patterns-1/Stamps&Patterns.text'
 ./testdec '/Users/matt/dev/Einstein/Sample Code/System Services/PeriodicElements-1/PeriodicElements.pkg'
 open -a xcode '/Users/matt/dev/Einstein/Sample Code/System Services/PeriodicElements-1/PeriodicElements.text'
 ./testdec '/Users/matt/dev/Einstein/Sample Code/Text Input/InkForm-1/InkForm.pkg'
 open -a xcode '/Users/matt/dev/Einstein/Sample Code/Text Input/InkForm-1/InkForm.text'
 ./testdec '/Users/matt/dev/Einstein/Sample Code/Text Input/InkTranslate-1/InkTranslate.pkg'
 open -a xcode '/Users/matt/dev/Einstein/Sample Code/Text Input/InkTranslate-1/InkTranslate.text'
 ./testdec '/Users/matt/dev/Einstein/Sample Code/Text Input/Keyboardin-1/Keyboardin.pkg'
 open -a xcode '/Users/matt/dev/Einstein/Sample Code/Text Input/Keyboardin-1/Keyboardin.text'
 ./testdec '/Users/matt/dev/Einstein/Sample Code/Text Input/Keys-4/Keys.pkg'
 open -a xcode '/Users/matt/dev/Einstein/Sample Code/Text Input/Keys-4/Keys.text'
 ./testdec '/Users/matt/dev/Einstein/Sample Code/Text Input/QWERTY-3/QWERTY.pkg'
 open -a xcode '/Users/matt/dev/Einstein/Sample Code/Text Input/QWERTY-3/QWERTY.text'
 ./testdec '/Users/matt/dev/Einstein/Sample Code/Text Input/TXWord-2/TXWord.pkg'
 open -a xcode '/Users/matt/dev/Einstein/Sample Code/Text Input/TXWord-2/TXWord.text'
 ./testdec '/Users/matt/dev/Einstein/Sample Code/Transports/ArchiveTransport-4/ArchiveTransport.pkg'
 open -a xcode '/Users/matt/dev/Einstein/Sample Code/Transports/ArchiveTransport-4/ArchiveTransport.text'

 Currently Testing
 -----------------

 ./testdec '/Users/matt/dev/Einstein/Sample Code/Transports/MinMail-3/MinMail.pkg'
 open -a xcode '/Users/matt/dev/Einstein/Sample Code/Transports/MinMail-3/MinMail.text'

 Optimizing Issues
 -----------------

 ---- Generating unnecessary bytecode: push nil, pop, push nil
 ./testdec '/Users/matt/dev/Einstein/Sample Code/Views/Paragraph Scroll-4/ParagraphScroll.pkg'
 open -a xcode '/Users/matt/dev/Einstein/Sample Code/Views/Paragraph Scroll-4/ParagraphScroll.text'
 ---- Locals are generated in a different order when using loops
 ./testdec '/Users/matt/dev/Einstein/Sample Code/Views/DragonDrop-1/DragonDrop.pkg'
 open -a xcode '/Users/matt/dev/Einstein/Sample Code/Views/DragonDrop-1/DragonDrop.text'

 Tested and Working
 ------------------

 ./testdec '/Users/matt/dev/Einstein/Sample Code/Transports/StatusReport-1/StatusReport.pkg'
 open -a xcode '/Users/matt/dev/Einstein/Sample Code/Transports/StatusReport-1/StatusReport.text'
 ./testdec '/Users/matt/dev/Einstein/Sample Code/Views/Clock-2/Clock.pkg'
 open -a xcode '/Users/matt/dev/Einstein/Sample Code/Views/Clock-2/Clock.text'
 ./testdec '/Users/matt/dev/Einstein/Sample Code/Views/ViewScripts-3/ViewScripts.pkg'
 open -a xcode open -a xcode '/Users/matt/dev/Einstein/Sample Code/Views/ViewScripts-3/ViewScripts.text'
 ./testdec '/Users/matt/dev/Einstein/Sample Code/Views/Thumbnail-1/Thumbnail.pkg'
 open -a xcode '/Users/matt/dev/Einstein/Sample Code/Views/Thumbnail-1/Thumbnail.text'
 ./testdec '/Users/matt/dev/Einstein/Sample Code/Views/TabsNStyles-3/TabsNStyles.pkg'
 open -a xcode '/Users/matt/dev/Einstein/Sample Code/Views/TabsNStyles-3/TabsNStyles.text'
 ./testdec '/Users/matt/dev/Einstein/Sample Code/Views/SyncScroll-1/SyncScroll.pkg'
 open -a xcode '/Users/matt/dev/Einstein/Sample Code/Views/SyncScroll-1/SyncScroll.text'
 ./testdec '/Users/matt/dev/Einstein/Sample Code/Views/DatePick-2/DatePick.pkg'
 open -a xcode '/Users/matt/dev/Einstein/Sample Code/Views/DatePick-2/DatePick.text'
 ./testdec '/Users/matt/dev/Einstein/Sample Code/User Interface/PreeferMadnessTNG-1/PreeferMadnessTNG.pkg'
 open -a xcode '/Users/matt/dev/Einstein/Sample Code/User Interface/PreeferMadnessTNG-1/PreeferMadnessTNG.text'
 ./testdec '/Users/matt/dev/Einstein/Sample Code/User Interface/AdjustoButton-1/AdjustoButton.pkg'
 open -a xcode '/Users/matt/dev/Einstein/Sample Code/User Interface/AdjustoButton-1/AdjustoButton.text'


 */

/*
 Precedence Table:
 12: slot access '.'
 11: send, conditional send
 10: array element []
 9: unary minus
 8: <<, >>
 7: divide, div, multiply, mod
 6: add, subtract
 5: stringer (&, &&)
 4: "exists"
 3: comparisons (<, >, =, <>, ...)
 2: not
 1: and, or
 0: assign :=
 TODO: Where is the if-then-else statement in this?
 */

/*
 Control Flow:
 - for:     `for` counter := expr `to` inital [`by` increment] `do` expr
 - foreach: `foreach` slot [, value] [`deeply`] `in` frame_or_array (`do` or `collect`) expr
 - loop     `loop` expr
 - while    `while` condition `do` expression
 - repeat   `repeat` expression `until` condition
 - break    can appear anywhere inside those loops
            generates "expr branch pop",
            the pop seems to be never reached, but turns `break` into a statement.
 - exceptions: try [begin] ... onexception ... do ... [end]
               throw(...) , rethrow()
 */


// -----------------------------------------------------------------------------

void Decompiler::decompile(Ref ref)
{
  Ref klass = GetFrameSlot(ref, SYMA(class));
  if (IsSymbol(klass) && SymbolCompare(klass, SYMA(CodeBlock))==0) {
    // slow NOS 1.x style function
    Ref numArgs = GetFrameSlot(ref, SYMA(numArgs));
    numArgs_ = RefToInt(numArgs);
    Ref argFrame = GetFrameSlot(ref, SYMA(argFrame));
    int argFrameLength = Length(argFrame);
    numLocals_ = argFrameLength - 3 - numArgs_;
    locals_.clear();
    // Make the list of names of the locals
    MapSlots(argFrame,
             [](RefArg tag, RefArg, uintptr_t user_data)->long {
      Decompiler *self = (Decompiler*)user_data;
      Decompiler::Local l = { tag, Local::Use::undefined };
      self->locals_.push_back(l);
      return NILREF;
    },
             (uintptr_t)this);
    locals_[0].use = Local::Use::system; // _nextArgFrame
    locals_[1].use = Local::Use::system; // _parent
    locals_[2].use = Local:: Use::system; // _implementor
    for (int i=0; i<numArgs_; i++)
      locals_[i+3].use = Local::Use::arg;
    for (int i=0; i<numLocals_; i++)
      locals_[i+3+numArgs_].use = Local::Use::local;
  } else if (klass == kPlainFuncClass) {
    // faster NOS 2.x style function
    Ref numArgs = GetFrameSlot(ref, SYMA(numArgs));
    numArgs_ = static_cast<int>((numArgs>>2) & 0x00003fff);
    numLocals_ = static_cast<int>(numArgs >> 18);
    // Make up names for the locals:
    // _nextArgFrame, _parent, _implementor, parameters, locals
    locals_.clear();
    locals_.push_back( { SYMA(_nextArgFrame), Local::Use::system } );
    locals_.push_back( { SYMA(_parent), Local::Use::system } );
    locals_.push_back( { SYMA(_implementor), Local::Use::system } );
    for (int i=0; i<numArgs_; i++) {
      char buf[32];
      snprintf(buf, 30, "arg%d", i);
      locals_.push_back( { MakeSymbol(buf), Local::Use::arg } );
    }
    for (int i=0; i<numLocals_; i++) {
      char buf[32];
      snprintf(buf, 30, "loc%d", i);
      locals_.push_back( { MakeSymbol(buf), Local::Use::local } );
    }
    // There can be additional named locals if they also appear in `literals`
    Ref argFrame = GetFrameSlot(ref, SYMA(argFrame));
    if (NOTNIL(argFrame)) {
      //int argFrameLength = Length(argFrame);
      int i = -3; // skip the system locals (not the args)
      CObjectIterator iter(argFrame);
      for ( ; !iter.done(); iter.next(), ++i)
      {
//        fprintf(stderr, "%s\n", SymbolName(iter.tag()));
        if (i >= 0) {
          RefVar tag = iter.tag();
          locals_.push_back( { tag, Local::Use::noted } );
          numLocals_++;
        }
      }
    }
  } else {
    ThrowMsg("Decompiler::decompile(): Unknown Function Signature");
    // ArrayIndex GetFunctionArgCount(Ref fn)
    // DONE: CodeBlock
    // DONE: kPlainFuncClass
    // TODO: kPlainCFunctionClass
    // TODO: kBinCFunctionClass is a Frame: code, numArgs, closure, offset, bcFunc
    // TODO: binCFunction
  }

  literals_ = GetFrameSlot(ref, SYMA(literals));
  if (!ISNIL(literals_))
    numLiterals_ = Length(literals_);

  Ref instructions = GetFrameSlot(ref, SYMA(instructions));
  generateAST(instructions);
  if ((p.DebugBC() || p.DebugAST()) && IsArray(literals_)) {
    DefGlobalVar(SYMA(printDepth), MAKEINT(0));
    printf("\n\nLiterals:\n");
    int i, n = Length(literals_);
    for (i = 0; i < n ; i++) {
      printf("%4d: ", i);
      ::PrintObject(GetArraySlot(literals_, i), 0);
      printf("\n");
    }
    printf("\n\nLocals:\n");
    n = (int)locals_.size();
    const char *useLUT[] = { "undefined", "system", "arg", "local", "loop", "iter", "limit", "noted" };
    for (i = 0; i < n ; i++) {
      printf("%4d: %s (%s)\n", i, SymbolName(locals_[i].ref), useLUT[(int)locals_[i].use] );
    }

    printAST("Initial AST");
  }
  if (!p.DebugTrap().empty() && (p.RefPath() == p.DebugTrap())) {
    __builtin_debugtrap();
  }
  solve();
  if (p.DebugAST()) printASTRoot();
}

/**
 \brief Append a new node to the giveNode.
 This is optimized, so 'node' must not have a 'next' link. This also does not
 update 'first_' or 'last_'. This is used for initialization.
 \return the new node
 */
Node *Decompiler::Append(Node *node, Node *newNode) {
  assert(node);
  assert(newNode);
  node->next = newNode;
  newNode->prev = node;
  return newNode;
}

/**
 Create a node that corresponds to the given bytecode.
 */
Bytecode *Decompiler::NewBytecodeNode(int pc, int a, int b)
{
  switch (a) {
    case 0:
      switch (b) {
        case 0: return new BCPop(*this, pc, a, b);
        case 1: return new BCDup(*this, pc, a, b);
        case 2: return new BCReturn(*this, pc, a, b);
        case 3: return new BCPushSelf(*this, pc, a, b);
        case 4: return new BCSetLexScope(*this, pc, a, b);//        case 4: bc.bc = BC::SetLexScope; break;
        case 5: return new BCIterNext(*this, pc, a, b);
        case 6: return new BCIterDone(*this, pc, a, b);
        case 7: return new BCPopHandlers(*this, pc, a, b);
      };
      break;
    case 3: return new BCPush(*this, pc, a, b);
    case 4: return new BCPushConst(*this, pc, a, b);
    case 5: return new BCCall(*this, pc, a, b);
    case 6: return new BCInvoke(*this, pc, a, b);
    case 7: return new BCSend(*this, pc, a, b, false); // Send
    case 8: return new BCSend(*this, pc, a, b, true); // SendIfDefined
    case 9: return new BCResend(*this, pc, a, b, false); // Resend
    case 10: return new BCResend(*this, pc, a, b, true); // ResendIfDefined
    case 11: return new BCBranch(*this, pc, a, b);
    case 12: return new BCBranchIfTrue(*this, pc, a, b);
    case 13: return new BCBranchIfFalse(*this, pc, a, b);
    case 14: return new BCFindVar(*this, pc, a, b);
    case 15: return new BCGetVar(*this, pc, a, b);
    case 16: return new BCMakeFrame(*this, pc, a, b);
    case 17:
      if (b == 0xFFFF)
        return new BCNewArray(*this, pc, a, b);
      else
        return new BCMakeArray(*this, pc, a, b);
    case 18: return new BCGetPath(*this, pc, a, b);
    case 19: return new BCSetPath(*this, pc, a, b);
    case 20: return new BCSetVar(*this, pc, a, b);
    case 21: return new BCFindAndSetVar(*this, pc, a, b);
    case 22: return new BCIncrVar(*this, pc, a, b);
    case 23: return new BCBranchLoop(*this, pc, a, b);
    case 24:
      switch (b) {
        case 0: return new BinaryOperator(*this, pc, a, b, "+", kPrecedenceAddSub); // Add
        case 1: return new BinaryOperator(*this, pc, a, b, "-", kPrecedenceAddSub); // Sub
        case 2: return new BCARef(*this, pc, a, b);
        case 3: return new BCSetARef(*this, pc, a, b);
        case 4: return new BinaryOperator(*this, pc, a, b, "=", kPrecedenceCompare); // Equals
        case 5: return new BCNot(*this, pc, a, b);
        case 6: return new BinaryOperator(*this, pc, a, b, "<>", kPrecedenceCompare); // NotEquals
        case 7: return new BinaryOperator(*this, pc, a, b, "*", kPrecedenceMulDiv); // Multiply
        case 8: return new BinaryOperator(*this, pc, a, b, "/", kPrecedenceMulDiv); // Divide
        case 9: return new BinaryOperator(*this, pc, a, b, "div", kPrecedenceMulDiv); // 'div'
        case 10: return new BinaryOperator(*this, pc, a, b, "<", kPrecedenceCompare); // LessThan
        case 11: return new BinaryOperator(*this, pc, a, b, ">", kPrecedenceCompare); // GreaterThan
        case 12: return new BinaryOperator(*this, pc, a, b, ">=", kPrecedenceCompare); // GreaterOrEqual
        case 13: return new BinaryOperator(*this, pc, a, b, "<=", kPrecedenceCompare); // LessOrEqual
        case 14: return new BinaryFunction(*this, pc, a, b, "bAnd"); // BitAnd
        case 15: return new BinaryFunction(*this, pc, a, b, "bOr"); // BitOr
        case 16: return new BCBitNot(*this, pc, a, b);
        case 17: return new BCNewIter(*this, pc, a, b);
        case 18: return new BCLength(*this, pc, a, b);
        case 19: return new BCClone(*this, pc, a, b);
        case 20: return new BCSetClass(*this, pc, a, b);
        case 21: return new BCAddArraySlot(*this, pc, a, b);
        case 22: return new BCStringer(*this, pc, a, b);
        case 23: return new BCHasPath(*this, pc, a, b);
        case 24: return new BCClassOf(*this, pc, a, b);
      }
      break;
    case 25: return new BCNewHandler(*this, pc, a, b);
  }
  return new Bytecode(*this, pc, a, b);
}

/**
 \brief Create the initial AST which is not a tree at all, just a list of nodes.

 Convert every bytecode instruction into one or more nodes and chain them together in
 a doubly linked list. Jump instruction generate additional "jump targets" at
 their jump destination that are linked into the list as well. One address
 can only hold one jump target, but the target can hold multiple jump
 origins in a backwards and forwards list.
 */
void Decompiler::generateAST(Ref instructions)
{
  if (!IsBinary(instructions)) ThrowMsg("Decompiler::generateAST: `instructions` must be binary Ref.");
  uint8_t *bc = (uint8_t*)BinaryData(instructions);
  int nbc = Length(instructions);

  // TODO: horrible hack: we push every BCPushConst(int) in case we encounter BCNewHandler
  std::vector<int> pushConstList;
  std::vector<int> pushLitList;

  // Find all the jump instructions and create the target nodes.
  for (int i=0; i<nbc; i++) {
    int pc = i;
    uint8_t cmd = bc[i];
    uint8_t a = (cmd & 0xf8) >> 3;
    uint16_t b = (cmd & 0x07);
    if (b==7) { b = bc[i+1]<<8 | bc[i+2]; i += 2; }
    if ((a==11)||(a==12)||(a==13)||(a==23))
      // branch, brach-if-true, branch-if-false, branch-if-loop-not-done
      AddToTargets(b, pc);
    if (a==3) {
      // BCPush
      pushLitList.push_back(b);
    }
    if (a==4 && IsInt(b)) {
      // BCPushConst
      pushConstList.push_back(RefToInt(b));
    }
    if (a==25) {
      // BCNewHandler generates 'b' jump targets that are the
      // entry points to exception handlers
      int n = (int)pushConstList.size();
      assert(b <= n);
      int nl = (int)pushLitList.size();
      assert(b <= nl);
      for (int t=0; t<b; ++t) {
        AddToTargets(pushConstList[n-t-1], pc, pushLitList[nl-t-1]);
      }
    }
  }

  // Now run the byte codes again and create a linked list of instructions
  Node *nd = first_ = new FirstNode(*this);
  for (int i=0; i<nbc; i++) {
    int pc = i;
    uint8_t cmd = bc[i];
    uint8_t a = (cmd & 0xf8) >> 3;
    uint16_t b = (cmd & 0x07);
    if (b==7) { b = bc[i+1]<<8 | bc[i+2]; i += 2; }
    if (targetMap_.contains(pc)) {
      auto &target = targetMap_[pc];
      for (auto &t : target)
        nd = Append(nd, t.second);
    }
    nd = Append(nd, NewBytecodeNode(pc, a, b));
  }
  last_ = Append(nd, new LastNode(*this));

  // The code generator occasionally appends two consecutive return commends.
  // We fix that by deleting the second return.
  if (dynamic_cast<BCReturn*>(nd) && dynamic_cast<BCReturn*>(nd->prev))
    delete nd->Unlink();
}

void Decompiler::AddToTargets(int target, int origin, int excp)
{
  // Use negative numbers to sort forward jumps closest to furthest.
  // BAckward jumps are automatically closest to furthest.
  int sort = origin < target ? -origin : target;
  if (excp == -1) {
    targetMap_[target][sort] = new JumpTarget(*this, target, origin);
  } else {
    targetMap_[target][sort] = new ExceptionHandler(*this, target, origin, excp);
  }
  if (p.DebugAST()) printf("Jump Target: from %d to %d\n", origin, target);
}


/**
 \brief Decompile the AST as much as possible in multiple loops.
 The AST starts as a linear list of Bytecode nodes. The solver runs over the
 root nodes front to back, finding patterns that can be resolved into
 dependencies. It does that until no more patterns are found.

 For example:
 ```
 push 3 (provides 1 value)
 push 3 (provides 1 value)
 add (consumes 2 values, but doesn't know yet what it provides)
 ```

 `push` provides a value on the stack. `add` expects two values on the stack,
 so the `push` nodes become dependent on `add`

 Resolving to:
 ```
   push 3 (resolved, no longer checked)
   push 5 (resolved)
 add (provides 1 value)
 ```

 One trick is that unresolved nodes can not be part of pattern. Only if all
 inputs exist will the decompiler take them into account.

 To figure out control flow, jump commands and the jump destinations are
 AST nodes as well. This will avoid conflict between data flow and
 control flow analysis. Nevertheless, data flow has always priority, and
 control flow is checked in a secondary loop.
 */
void Decompiler::solve()
{
  for (;;) { // ControlFlow Pass: outer loop, run until neither changes anything
    // ---- Data Flow Pass
    numASTChanges = 0;
    for (Node *nd = first_; nd && !numASTChanges; nd = nd->Resolve(Node::Pass::DataFlow)) { }
    if (numASTChanges > 0) {
      if (p.DebugAST()) printAST("DataFlow Pass");
      continue;
    }
    if (p.DebugAST()) { p.Item(); p.Print("DataFlow passes done"); p.ItemDone(); }
    // ---- Compression Pass
    if (compressAST()) {
      if (p.DebugAST()) printAST("Compression Pass");
      continue;
    }
    if (p.DebugAST()) { p.Item(); p.Print("Compression passes done"); p.ItemDone(); }
    // ---- Control Flow Pass
    numASTChanges = 0;
    for (Node *nd = first_; nd && !numASTChanges; nd = nd->Resolve(Node::Pass::ControlFlow)) { }
    if (numASTChanges > 0) {
      if (p.DebugAST()) printAST("CodeFlow Pass");
      continue;
    }
    if (p.DebugAST()) { p.Item(); p.Print("CodeFlow passes done"); p.ItemDone(); }
    // ---- No more changes on any level
    break;
  }
}

/**
 \brief Combine multiple statements into a single Code Block.

 This method walks the Root layer of the AST and finds multiple consecutive
 statements, or one or more statements followed by an expression, and groups
 them into a new node that presents as a single statement or expression.

 This resolves only the first occurrence of this pattern and then returns true.
 To compress the entire AST, this method should be called until it returns false.

 \return true if there were any changes.
 */
bool Decompiler::compressAST()
{
  Node *nd = first_;
  while (nd) {
    if (nd->IsStatement()) {
      int numStmts = 1;
      Node *it = nd->next;
      bool isExpr = false;
      while (it) {
        if (it->IsExpr()) {
          isExpr = true;
          numStmts++;
          break;
        }
        if (!it->IsStatement()) {
          break;
        }
        numStmts++;
        it = it->next;
      }
      if (numStmts > 1) {
        CodeBlock *codeBlock = new CodeBlock(*this, nd->pc(), isExpr ? kProvidesOne : kProvidesNone);
        // Insert codeBlock before nd
        nd->InsertBefore(codeBlock);
        codeBlock->moveToBody(nd, numStmts);
        nd = codeBlock->next;
        return true;
//        nd = codeBlock;
      }
    }
    nd = nd->next;
  }
  return false;
}

/**
 \brief Print the full Abstract Syntax Tree.
 This prints the list of root nodes and all their dependencies.

 NS Bytecode's data flow is stack oriented. Dependencies are printed first
 with an indent to make it easy to follow the data flow.
 */
void Decompiler::printAST(const char *label)
{
  p.PrintDivider(label);
  p.DeepList();
  output = Print::deep;
  for (Node *nd = first_; nd; nd = nd->next) {
    nd->PrintNode(true);
  }
  p.EndList();
  p.PrintDivider("");
}


/**
 \brief Print all nodes in the first layer of the AST.
 The decompiler only ever looks at the first layer of AST nodes. Printing this
 out help us to find patterns where the decompiler git stuck.
 A completely resolved ST has a list of 0 or more statements and a single
 final expression.
 */
void Decompiler::printASTRoot()
{
  p.PrintDivider("AST Root Nodes");
  p.DeepList("");
  output = Print::bytecode;
  for (Node *nd = first_; nd; nd = nd->next) {
    nd->PrintNode(false);
  }
  p.EndList();
  p.PrintDivider("");
}


/**
 \brief Convert an Abstract Syntax Tree (AST) back into readable NewtonScript source code.
 Walks the tree and lets nodes output the appropriate source code.
 If a node can not print itself, it will output an AST node description for
 debugging.
 */
void Decompiler::printSource(bool isNative)
{
  output = Print::script;

  // Print the function header and argument list
  if (isNative)
    p.Print("func native(");
  else
    p.Print("func(");
  p.StartList(",");
  for (int i=0; i<numArgs_; i++) {
    p.Item(); p.Print(""); printLocal(i + 3);
  }
  p.EndList();
  p.Print(")");

  // Print the begin statement
  p.FreshLine();
  p.Print("begin");
  p.DeepList(";");

  // List all locals first! "local a;" ...
  if (numLocals_) {
    bool localsPrinted = false;
    for (int i = 0; i < numLocals_; ++i) {
      // Don't print locals that are used as iterators in 'for' or 'foreach'
      if (localUsedAs(i + 3 + numArgs_, Local::Use::local) || localUsedAs(i + 3 + numArgs_, Local::Use::noted)) {
        p.Item();
        p.Printf("local ");
        printLocal(i + 3 + numArgs_);
        p.ItemDone();
        localsPrinted = true;
      }
    }
    if (localsPrinted) {
      p.Tag();
      p.Print(""); // generate an empty line
    }
  }

  // Now print all the top level nodes from the AST.
  // If everything was decompiled correctly, this should be 0 or more
  // statements, followed by one expression
  for (Node *nd = first_->next; nd; nd = nd->next) {
    p.Item();
    nd->Print(kPrintSuppressList);
    p.ItemDone();
  }

  // Check if there are unresolved nodes and give an error message if so
  int numUnresolved = 0;
  for (Node *nd = first_->next; nd; nd = nd->next) {
    if (!nd->Resolved() && (nd->provides() != kSpecialNode))
      numUnresolved++;
  }
  if (numUnresolved > 0) {
    fprintf(stderr, "\nWARNING: %d unresolved nodes in AST.\n", numUnresolved);
    fprintf(stderr, "Path: '%s'\n", p.RefPath().c_str());
    fprintf(stderr, "PC:" );
    for (Node *nd = first_->next; nd; nd = nd->next) {
      if (!nd->Resolved() && (nd->provides() != kSpecialNode))
        fprintf(stderr, " %d", nd->pc() );
    }
    fprintf(stderr, "\n");
  }

  // Print the end marker of the function
  p.EndList();
  p.FreshLine();
  p.Print("end");
}


/**
 \brief Print a frame that contains a NewtonScript function.
 Check if this is actually NewtonScript. No support for native or binary.
 Is `class: #0x32` for newer apps.
 - decompress the bytecode into a flat AST
 - find all jump target addresses and store them as AST nodes as well
 - reduce the AST as much as possible, generating an actual tree
    - find data flow and move nodes into dependencies
    - find control flow patterns and reorder nodes
    - if nothing can be applied anymore, the root of the AST should be a list
      of 0 or more statements, followed by exactly one expression
 - now walk the tree and generate nicely readable source code.

 This is a description of the newer function format, using an abbreviated frame
 ```
 `DefGlobalVar(MakeSymbol("compilerCompatibility"), MAKEINT(1));`
 - `class: #0x32`:
 - `instructions`: 'instructions bytecode as binary data
 - `literals`: 'literals, an array of values
 - `numArgs`: bits 31 to 16 are the number of locals, bits 15 to 0 are the number of arguments
 - `argFrame`: always `nil` in this format
 ```

 This is the older format that still contains a lot of variable names that help
 us generate more readable code.
 ```
 `DefGlobalVar(MakeSymbol("compilerCompatibility"), MAKEINT(0));`
 - `class`: 'CodeBlock,
 - `instructions`:
 - `literals`:
 - `argFrame`: {
      _nextArgFrame: {
        _nextArgFrame: nil,
        _parent: nil,
        _implementor: nil
      },
      _parent: nil,
      _implementor: nil,
      ab: nil,  // Argument 0
      cd: nil,  // Argument 1 (see numArgs)
      x: nil,   // Local 0
      y: nil,   // ...
      z: nil    // Local 2
    },
 - `numArgs`: 2
 ```
 */
NewtonErr mDecompile(Ref ref, ObjectPrinter &printer, bool isNative, bool debugAST, bool debugBC)
{
  Decompiler d(printer);
  d.decompile(ref);
  d.printSource(isNative);
  return noErr;
}

