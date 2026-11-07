// Test the actual extension methods without loading or altering the live Shell.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
const path = require('node:path');
const Main = {sessionMode: {}, inputMethod: null};
const global = {stage: {key_focus: null}, get_pointer: () => [0, 0, 0]};
const Clutter = {ModifierType: {CONTROL_MASK: 4, MOD1_MASK: 8, SUPER_MASK: 64},
    KEY_BackSpace: 65288, KeyState: {PRESSED: 1, RELEASED: 0}};
const IBus = {InputPurpose: {PASSWORD: 8, PIN: 9},
    InputHints: {PRIVATE: 2048, HIDDEN_TEXT: 4096}};
const PopupMenu = {
    PopupMenuSection: class {
        constructor() { this.items=[]; this.actor={set_style: value => { this.style=value; }}; }
        addMenuItem(item) { this.items.push(item); }
    },
    PopupMenuItem: class {
        constructor(title) { this.title=title; }
        connect() {}
    },
};
const source = fs.readFileSync(path.join(__dirname, '../extension.js'), 'utf8')
    .replace(/^import .*;\n/gm, '')
    .replace('export default class', 'class');
const DoubleTap = vm.runInNewContext(source + '\nDoubleTap;',
    {Extension: class {}, Main, global, IBus, Clutter, PopupMenu,
        GLib: {Variant: class {}}, console: {log() {}}});
const extension = new DoubleTap();
extension._running = true;
extension._protectInput = true;
extension._inputProtected = true;
extension._sources = () => [{id: 'us', activate: () => { activated++; }}];
extension._manager = {currentSource: {id: 'us'}};
let activated = 0;
assert.equal(extension.Switch('us', true), 'blocked:protected-input');
assert.equal(activated, 0);
for (const [purpose, hints] of [[8, 0], [9, 0], [0, 2048], [0, 4096]]) {
    Main.inputMethod = {currentFocus: {}, _purpose: purpose, _hints: hints};
    assert.equal(extension.Switch('us', true), 'blocked:protected-input');
}
assert.equal(activated, 0);
Main.inputMethod = {currentFocus: {}, _purpose: 0, _hints: 0};
assert.equal(extension._protectedInput(), false);
global.stage.key_focus = {get_password_char: () => 42, get_parent: () => null};
assert.equal(extension._protectedInput(), true);
assert.equal(extension.Switch('us', true), 'blocked:protected-input');
// Explicit user selection remains available, even in a password field.
assert.equal(extension.Switch('us', false), 'us');
assert.equal(activated, 1);
global.stage.key_focus = null;
Main.inputMethod = null;
extension._inputProtected = false;
assert.equal(extension._protectedInput(), false);
const injected = [];
extension._virtual = {notify_keyval: (...args) => injected.push(args)};
assert.equal(extension.Switch('us', true), 'us');
assert.equal(activated, 2);
assert.deepEqual(injected.map(args => args[2]), [1, 0, 1, 0]);
let visible = true;
let actionSent = false;
extension._indicator = {hide() { visible = false; }, show() { visible = true; }};
extension._bridge = {emit_signal() { actionSent = true; }};
extension._update = () => {};
extension._action('exit');
assert.equal(actionSent, true);
assert.equal(extension._running, false);
assert.equal(visible, false);
extension.SetRunning(true);
assert.equal(visible, true);
extension._inputProtected = true;
extension.SetOptions(false, false);
assert.equal(extension.Switch('us', true), 'us');
extension.SetOptions(true, false);
assert.equal(extension.Switch('us', true), 'blocked:protected-input');
for (const [id, code] of [['us','EN'],['ru','RU'],['ua','UA'],['us+intl','EN']]) {
    extension._manager.currentSource = {id};
    assert.equal(extension._layoutCode(), code);
}
let launched = '';
extension._launch = option => { launched = option; };
extension.SetRunning(false);
extension._action('toggle');
assert.equal(launched, '--start');
launched = '';
extension.SetRunning(true);
actionSent = false;
extension._action('toggle');
assert.equal(actionSent, true);
assert.equal(launched, '');
// The actual menu source keeps layouts separate and Exit as the final action.
assert(source.indexOf("for (const source of this._sources())") < source.indexOf("this._item('Start / Stop'"));
assert(source.indexOf("this._item('Support on Ko-fi'") < source.indexOf("this._item('Exit'"));
assert(!source.includes('console.log(') && !source.includes('console.error('));
const groups=[];
extension._indicator={menu: {addMenuItem: item => groups.push(item)}};
for (const title of ['Languages','Controls','Support']) {
    extension._group(); extension._item(title,()=>{});
}
assert.equal(groups.length,3);
for (const group of groups) {
    assert(group.style.includes('border: 1px solid'));
    assert.equal(group.items.length,1);
}
console.log('Password guard and tray action tests passed; no live input injected.');
