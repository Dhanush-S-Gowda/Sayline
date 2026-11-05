// Per-process bridge: use the app's PID to identify its surface, even when
// Wayland reports a different resourceClass. Never redirect focus to the overlay.
var owner = "@DBUS_OWNER@";
var token = "@BRIDGE_TOKEN@";
var appPid = Number("@APP_PID@");
var watched = {};
function event(kind, details) {
    callDBus(owner, "/Sayline", "org.sayline.Sayline", "BridgeEvent", kind, details, token, function() {});
}
function isOverlay(window) {
    if (!window) return false;
    if (Number(window.pid) === appPid) return true;
    var name = String(window.resourceClass).toLowerCase();
    return name === "org.sayline.sayline" || name === "sayline";
}
function enforceOverlay(window) {
    window.keepAbove = true;
    window.minimized = false;
    window.skipTaskbar = true;
    window.skipPager = true;
    window.skipSwitcher = true;
    window.desktops = [];
}
function findWindow(id) {
    var windows = workspace.stackingOrder;
    for (var i = 0; i < windows.length; ++i)
        if (String(windows[i].internalId) === id) return windows[i];
    return null;
}
function placeOverlay(window, targetId) {
    // The surface can disappear while this asynchronous D-Bus call is pending.
    if (!findWindow(String(window.internalId))) return;
    var target = findWindow(targetId);
    if (!target || isOverlay(target)) target = workspace.activeWindow;
    if (!target || isOverlay(target)) target = window;
    // WorkArea spans ALL outputs; MaximizeArea is output-local and excludes panels.
    var area = workspace.clientArea(KWin.MaximizeArea, target);
    var geometry = window.frameGeometry;
    var gap = Math.max(64, Math.min(120, Math.round(area.height * 0.08)));
    window.frameGeometry = {
        x: Math.round(area.x + (area.width - geometry.width) / 2),
        y: Math.round(area.y + area.height - geometry.height - gap),
        width: geometry.width,
        height: geometry.height
    };
    event("overlay_placed", JSON.stringify({target: String(target.internalId),
          area: {x: area.x, y: area.y, width: area.width, height: area.height},
          x: window.frameGeometry.x, y: window.frameGeometry.y}));
}
function configureOverlay(window) {
    if (!isOverlay(window)) return;
    try {
        enforceOverlay(window);
        var id = String(window.internalId);
        if (!watched[id]) {
            watched[id] = true;
            // The captured shortcut target survives cold D-Bus activation. Read it
            // on each new surface so a warm reopen can use a different monitor.
            callDBus(owner, "/Sayline", "org.sayline.Sayline", "PlacementTarget", function(targetId) {
                try { placeOverlay(window, targetId); }
                catch (error) { event("error", "Overlay placement: " + String(error)); }
            });
            if (window.minimizedChanged) window.minimizedChanged.connect(function() {
                if (window.minimized) window.minimized = false;
            });
            if (window.keepAboveChanged) window.keepAboveChanged.connect(function() {
                if (!window.keepAbove) window.keepAbove = true;
            });
            event("overlay_configured", JSON.stringify({pid: Number(window.pid),
                  window: id, resourceClass: String(window.resourceClass), keepAbove: window.keepAbove}));
        }
    } catch (error) {
        event("error", "Overlay configuration: " + String(error));
    }
}
function reportFocus(window) {
    if (isOverlay(window)) return;
    callDBus(owner, "/Sayline", "org.sayline.Sayline", "Focus",
             window ? String(window.internalId) : "",
             window ? String(window.resourceClass) : "", token, function() {});
}
workspace.windowAdded.connect(configureOverlay);
workspace.windowActivated.connect(function(window) {
    // Report focus before applying presentation properties, so a compositor
    // property error cannot prevent dictation from learning its target.
    reportFocus(window);
    var windows = workspace.stackingOrder;
    for (var i = 0; i < windows.length; ++i) configureOverlay(windows[i]);
});
var initialWindows = workspace.stackingOrder;
for (var i = 0; i < initialWindows.length; ++i) configureOverlay(initialWindows[i]);
reportFocus(workspace.activeWindow);
event("ready", "KWin focus and overlay hooks connected");
