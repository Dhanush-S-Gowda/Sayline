const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
const path = require('node:path');
const root = path.join(__dirname, '..');
function signal() {
    const callbacks = [];
    return {connect: callback => callbacks.push(callback), emit: value => callbacks.forEach(fn => fn(value))};
}
const overlay = {resourceClass: 'org.sayline.Sayline', internalId: 'overlay', frameGeometry: {x: 0, y: 0, width: 460, height: 48}};
const laptop = {x: 0, y: 0, width: 1920, height: 1080};
const editor = {resourceClass: 'dev.zed.Zed', internalId: 'editor', area: {x: 1920, y: 100, width: 2560, height: 1400}};
const otherEditor = {resourceClass: 'dev.zed.Zed', internalId: 'other', area: laptop};
let capturedTarget = 'editor';
const windows = [editor, otherEditor, overlay];
const added = signal(), activated = signal(), calls = [];
vm.runInNewContext(fs.readFileSync(path.join(root, 'packaging/focus.js'), 'utf8'), {
    workspace: {windowAdded: added, windowActivated: activated, stackingOrder: windows, activeWindow: otherEditor,
        clientArea: (kind, target) => kind === 1 ? (target.area || laptop) : {x: 0, y: 0, width: 4480, height: 1500}},
    KWin: {WorkArea: 0, MaximizeArea: 1},
    callDBus: (...args) => {
        calls.push(args);
        if (args[3] === 'PlacementTarget') args[4](capturedTarget);
    }
});
assert.equal(overlay.keepAbove, true);
assert.equal(overlay.skipTaskbar, true);
assert.equal(overlay.skipSwitcher, true);
assert.equal(overlay.frameGeometry.x, 2970); // Captured target on external output, not the now-active laptop.
assert.equal(overlay.frameGeometry.y, 1340);
overlay.frameGeometry = {x: 200, y: 300, width: 460, height: 48};
assert.deepEqual(Array.from(overlay.desktops), []);
assert.equal(editor.keepAbove, undefined); // Never change the user's other windows.
assert.equal(calls.find(call => call[3] === 'Focus')[5], 'dev.zed.Zed');
const count = calls.filter(call => call[3] === 'Focus').length;
activated.emit(overlay);
assert.equal(calls.filter(call => call[3] === 'Focus').length, count); // Dragging the overlay must not replace the target.
overlay.keepAbove = false;
overlay.minimized = true;
activated.emit(editor);
assert.equal(overlay.minimized, false);
assert.equal(overlay.keepAbove, true); // Reassert stacking on focus changes.
assert.equal(overlay.frameGeometry.x, 200); // Do not undo dragging.
assert.equal(overlay.frameGeometry.y, 300);
const newOverlay = {resourceClass: 'sayline', internalId: 'new-overlay', frameGeometry: {x: 0, y: 0, width: 460, height: 48}};
capturedTarget = 'other';
windows.push(newOverlay);
added.emit(newOverlay);
assert.equal(newOverlay.skipPager, true);
assert.equal(newOverlay.frameGeometry.x, 730); // Warm reopen can target the laptop.
assert.equal(newOverlay.frameGeometry.y, 946);
editor.area = {x: -1600, y: -200, width: 1600, height: 900};
capturedTarget = 'editor';
const negativeOverlay = {resourceClass: 'sayline', internalId: 'negative-output', frameGeometry: {x: 0, y: 0, width: 460, height: 48}};
windows.push(negativeOverlay);
added.emit(negativeOverlay);
assert.equal(negativeOverlay.frameGeometry.x, -1030);
assert.equal(negativeOverlay.frameGeometry.y, 580);
assert(!calls.some(call => call[3] === 'BridgeEvent' && call[4] === 'error'));
let shortcut;
vm.runInNewContext(fs.readFileSync(path.join(root, 'packaging/kwin-shortcut/contents/code/main.js'), 'utf8'), {
    registerShortcut: (...args) => { shortcut = args; },
    workspace: {activeWindow: editor},
    callDBus: (...args) => calls.push(args)
});
assert.equal(shortcut[2], 'Ctrl+Space');
shortcut[3]();
assert.deepEqual(calls.at(-1), ['org.sayline.Sayline', '/Sayline', 'org.sayline.Sayline', 'ToggleOnWindow', 'editor']);
console.log('KWin overlay policy and activation shortcut checks passed');
