// The app for build_app.ns: only what differs from the defaults.
newtAppName := "Hello:NEWTC";
newt.theForm := {_proto: protoFloatNGo, viewBounds: {left: 0, top: 30, right: 180, bottom: 120},
                 Greet: func() [kAppSymbol, kAppName, kPackageName, kAppString, wTitle.text]};
wTitle := {_proto: protoStaticText, text: "Hello", viewBounds: SetBounds(10, 10, 170, 30)};
AddStepForm(newt.theForm, wTitle);
StepDeclare(newt.theForm, wTitle, 'wTitle);
newt.InstallScript := func(partFrame) nil;
