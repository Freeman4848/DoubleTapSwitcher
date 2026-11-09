import Clutter from 'gi://Clutter';
import GLib from 'gi://GLib';
import Gio from 'gi://Gio';
import IBus from 'gi://IBus';
import St from 'gi://St';
import * as Main from 'resource:///org/gnome/shell/ui/main.js';
import * as PanelMenu from 'resource:///org/gnome/shell/ui/panelMenu.js';
import * as PopupMenu from 'resource:///org/gnome/shell/ui/popupMenu.js';
import {Extension} from 'resource:///org/gnome/shell/extensions/extension.js';
import * as Keyboard from 'resource:///org/gnome/shell/ui/status/keyboard.js';
import * as IBusManager from 'resource:///org/gnome/shell/misc/ibusManager.js';

const IFACE = 'org.freeman4848.DoubleTap';
const XML = `<node><interface name="${IFACE}">
<method name="Switch"><arg type="s" direction="in"/><arg type="b" direction="in"/><arg type="s" direction="out"/></method>
<method name="GetState"><arg type="s" direction="out"/></method>
<method name="SetRunning"><arg type="b" direction="in"/></method>
<method name="SetOptions"><arg type="b" direction="in"/><arg type="b" direction="in"/></method>
<signal name="Action"><arg type="s"/></signal>
<signal name="ContextChanged"/>
</interface></node>`;

export default class DoubleTap extends Extension {
    enable() {
        this._running = false;
        this._protectInput = true;
        this._manager = Keyboard.getInputSourceManager();
        // Unknown context is blocked until the application publishes its type.
        this._inputProtected = true;
        this._ibus = IBusManager.getIBusManager();
        this._inputSignals = [
            this._ibus.connect('set-content-type', (_manager, purpose, hints) => {
                this._inputProtected = this._secretType(purpose, hints);
                this._contextChanged();
            }),
            this._ibus.connect('focus-in', () => {
                this._inputProtected = true;
                this._contextChanged();
            }),
            this._ibus.connect('focus-out', () => {
                this._inputProtected = true;
                this._contextChanged();
            }),
        ];
        this._virtual = Clutter.get_default_backend().get_default_seat()
            .create_virtual_device(Clutter.InputDeviceType.KEYBOARD_DEVICE);
        this._bridge = Gio.DBusExportedObject.wrapJSObject(XML, this);
        this._bridge.export(Gio.DBus.session, '/org/freeman4848/DoubleTap');
        this._indicator = new PanelMenu.Button(0.0, 'Double Tap Switcher');
        const box = new St.BoxLayout({style_class: 'panel-status-menu-box'});
        this._icon = new St.Icon({icon_size: 18});
        this._layoutLabel = new St.Label({text: '?', y_align: Clutter.ActorAlign.CENTER});
        box.add_child(this._icon);
        box.add_child(this._layoutLabel);
        this._indicator.add_child(box);
        this._group();
        for (const source of this._sources())
            this._item(source.displayName, () => this.Switch(source.id, false));
        this._group();
        this._status = new PopupMenu.PopupMenuItem('Double tap: stopped', {reactive: false});
        this._menuGroup.addMenuItem(this._status);
        this._item('Start / Stop', () => this._action('toggle'));
        this._item('Settings...', () => this._launch('--settings'));
        this._item('Restart Hook', () => this._action('restart'));
        this._group();
        this._item('Report a bug', () => this._launch('--report-bug'));
        this._item('About', () => this._launch('--about'));
        this._item('Exit', () => this._action('exit'));
        Main.panel.addToStatusArea(this.uuid, this._indicator);
        this._changed = this._manager.connect('current-source-changed', () => this._update());
        this._appWatch = Gio.bus_watch_name(Gio.BusType.SESSION,
            'org.freeman4848.DoubleTapApp', Gio.BusNameWatcherFlags.NONE,
            () => {}, () => this.SetRunning(false));
        this._update();
    }

    _sources() {
        return Object.values(this._manager.inputSources).filter(s => s.type === 'xkb');
    }

    _group() {
        this._menuGroup = new PopupMenu.PopupMenuSection();
        this._menuGroup.actor.set_style('border: 1px solid rgba(128,128,128,0.45); border-radius: 10px; margin: 4px 8px; padding: 4px 0;');
        this._indicator.menu.addMenuItem(this._menuGroup);
    }

    _item(title, callback) {
        const item = new PopupMenu.PopupMenuItem(title);
        item.connect('activate', () => {
            try { callback(); } catch (error) {
                if (this._status)
                    this._status.label.text = `Double tap: ${error.message}`;
            }
        });
        this._menuGroup.addMenuItem(item);
    }

    _launch(option) {
        Gio.Subprocess.new(['/usr/bin/double-tap-switcher', option], Gio.SubprocessFlags.NONE);
    }

    _action(action) {
        if (!this._running && (action === 'toggle' || action === 'restart')) {
            this._launch('--start');
            return;
        }
        this._bridge.emit_signal('Action', new GLib.Variant('(s)', [action]));
        if (action === 'exit') {
            this.SetRunning(false);
            this._indicator.hide();
        }
    }

    _update() {
        this._layoutLabel.text = this._layoutCode();
        const paused = this._running && this._protectInput && this._protectedInput();
        this._status.label.text = this._running ?
            (paused ? 'Double tap: protected input / unknown context' : 'Double tap: active') :
            'Double tap: stopped';
        this._icon.gicon = new Gio.FileIcon({file: Gio.File.new_for_path(
            `${this.path}/icons/dt-${this._running && !paused ? 'active' : 'inactive'}.svg`)});
    }

    SetRunning(running) {
        this._running = running;
        if (running)
            this._indicator.show();
        this._update();
    }

    _layoutCode() {
        const id = (this._manager.currentSource?.id ?? '?').split('+')[0];
        return id === 'us' ? 'EN' : id.toUpperCase();
    }

    GetState() {
        return JSON.stringify({version: 3, running: this._running,
            protectedInput: this._protectInput && this._protectedInput(),
            current: this._manager.currentSource?.id ?? '?',
            sources: this._sources().map(s => ({id: s.id, name: s.displayName}))});
    }

    Switch(target, erase) {
        if (Main.sessionMode.isLocked || Main.sessionMode.isGreeter)
            throw new Error('Session locked');
        if (erase && !this._running)
            throw new Error('Double tap is stopped');
        if (erase && this._protectInput && this._protectedInput())
            return 'blocked:protected-input';
        const source = this._sources().find(s => s.id === target);
        if (!source)
            throw new Error(`Source unavailable: ${target}`);
        if (this._manager.currentSource?.id === source.id)
            return source.id;
        if (erase) {
            const [, , modifiers] = global.get_pointer();
            if (modifiers & (Clutter.ModifierType.CONTROL_MASK |
                Clutter.ModifierType.MOD1_MASK | Clutter.ModifierType.SUPER_MASK))
                throw new Error('Shortcut modifier held; deletion cancelled');
            for (let i = 0; i < 2; i++) {
                this._virtual.notify_keyval(0, Clutter.KEY_BackSpace, Clutter.KeyState.PRESSED);
                this._virtual.notify_keyval(0, Clutter.KEY_BackSpace, Clutter.KeyState.RELEASED);
            }
        }
        source.activate(false);
        const current = this._manager.currentSource?.id ?? '?';
        return current;
    }

    SetOptions(protectInput, _diagnostics) {
        this._protectInput = protectInput;
        this._update();
    }

    _contextChanged() {
        this._bridge?.emit_signal('ContextChanged', null);
        if (this._indicator)
            this._update();
    }

    disable() {
        for (const id of this._inputSignals ?? [])
            this._ibus.disconnect(id);
        this._inputSignals = [];
        this._ibus = null;
        if (this._appWatch)
            Gio.bus_unwatch_name(this._appWatch);
        if (this._changed)
            this._manager.disconnect(this._changed);
        this._bridge?.unexport();
        this._indicator?.destroy();
        this._bridge = null;
        this._indicator = null;
        this._virtual = null;
        this._manager = null;
    }

    _secretType(purpose, hints) {
        return purpose === IBus.InputPurpose.PASSWORD ||
            purpose === IBus.InputPurpose.PIN ||
            Boolean(hints & (IBus.InputHints.PRIVATE | IBus.InputHints.HIDDEN_TEXT));
    }

    _protectedInput() {
        // Shell password entries need not expose an application IBus context.
        for (let actor = global.stage.key_focus; actor; actor = actor.get_parent()) {
            const text = actor.clutter_text ?? actor;
            if (typeof text.get_password_char === 'function' && text.get_password_char())
                return true;
        }
        // GNOME 50 caches compositor-provided Wayland content metadata here.
        const input = Main.inputMethod;
        if (input?.currentFocus)
            return this._secretType(input._purpose, input._hints);
        return this._inputProtected;
    }
}
