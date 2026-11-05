// No app/model process is kept alive by this shortcut. D-Bus starts the app
// hidden if necessary, then delivers Toggle once it registers /Sayline.
registerShortcut("SaylineToggle", "Sayline: toggle dictation", "Ctrl+Space", function() {
    var target = workspace.activeWindow;
    callDBus("org.sayline.Sayline", "/Sayline", "org.sayline.Sayline", "ToggleOnWindow",
             target ? String(target.internalId) : "");
});
