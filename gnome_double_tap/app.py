#!/usr/bin/python3
"""Single-instance GTK settings window and GNOME layout bridge client."""
import json
import os
from pathlib import Path
import socket
import threading
import time
import gi

gi.require_version('Gtk', '3.0')
from gi.repository import Gtk, Gio, GLib, Gdk, GdkPixbuf
import settings

BASE = Path(__file__).resolve().parent
APP_ID = 'org.freeman4848.DoubleTapApp'
INTERFACE = 'org.freeman4848.DoubleTap'
OBJECT = '/org/freeman4848/DoubleTap'


class Application(Gtk.Application):
    reader_socket = '/run/double-tap-switcher.sock'
    application_id = APP_ID

    def __init__(self):
        super().__init__(application_id=self.application_id,
                         flags=Gio.ApplicationFlags.HANDLES_COMMAND_LINE)
        self.window = None
        self.connection = None
        self.generation = 0
        self.running = False
        self.want_running = False
        self.retry_source = None
        self.retry_attempts = 0
        self.started_at = None
        self.logs = []
        self.config_error = None
        try:
            self.config = settings.load()
        except (ValueError, OSError) as error:
            self.config_error = str(error)
            self.config = settings.validate(settings.DEFAULT)
        self.bus = None
        self.subscription = None
        self.context_subscription = None
        self.sources = []

    def do_command_line(self, command):
        args = command.get_arguments()[1:]
        if '--quit' in args and self.window is None:
            return 0
        self.activate()
        if '--quit' in args:
            self.exit_app()
        elif '--about' in args:
            self.about()
        elif '--start' in args:
            self.start()
        return 0

    def do_activate(self):
        if self.window:
            self.start()
            self.window.present()
            return
        self.window = Gtk.ApplicationWindow(application=self, title='Double Tap Switcher')
        self.window.set_default_size(620, 660)
        self.window.set_icon_name('double-tap-switcher')
        self.window.connect('delete-event', self.hide_window)
        self.window.add(self.build_content())
        self.hold()
        self.window.show_all()
        if self.config_error:
            self.log('Settings error: ' + self.config_error, error=True)
        try:
            self.bus = Gio.bus_get_sync(Gio.BusType.SESSION, None)
            self.subscription = self.bus.signal_subscribe('org.gnome.Shell', INTERFACE,
                'Action', OBJECT, None, Gio.DBusSignalFlags.NONE, self.on_action)
            self.context_subscription = self.bus.signal_subscribe('org.gnome.Shell', INTERFACE,
                'ContextChanged', OBJECT, None, Gio.DBusSignalFlags.NONE, self.on_context_changed)
            self.start()
        except GLib.Error as error:
            self.log('GNOME connection: ' + error.message, error=True)

    def build_content(self):
        root = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=10, margin=14)
        header = Gtk.Box(spacing=12)
        self.icon = Gtk.Image()
        self.set_icon(False)
        header.pack_start(self.icon, False, False, 0)
        self.status = Gtk.Label(label='Stopped', xalign=0)
        header.pack_start(self.status, True, True, 0)
        self.start_button = Gtk.Button(label='Start')
        self.start_button.connect('clicked', lambda _: self.stop() if self.want_running else self.start())
        header.pack_start(self.start_button, False, False, 0)
        restart = Gtk.Button(label='Restart Hook')
        restart.connect('clicked', lambda _: self.restart())
        header.pack_start(restart, False, False, 0)
        root.pack_start(header, False, False, 0)
        speed = Gtk.Box(spacing=10)
        speed.pack_start(Gtk.Label(label='Double-tap interval (ms)'), False, False, 0)
        self.threshold = Gtk.SpinButton.new_with_range(100, 1000, 10)
        self.threshold.set_value(self.config['threshold_ms'])
        speed.pack_start(self.threshold, False, False, 0)
        root.pack_start(speed, False, False, 0)
        self.diagnostics = Gtk.CheckButton(label='Diagnostics (off by default; no typed text)')
        self.diagnostics.set_active(self.config['diagnostics'])
        root.pack_start(self.diagnostics, False, False, 0)
        self.protect_input = Gtk.CheckButton(label='Pause in protected input (best effort)')
        self.protect_input.set_active(self.config['protect_input'])
        root.pack_start(self.protect_input, False, False, 0)
        root.pack_start(Gtk.Label(label='Physical keys work in every layout. Changes apply with Save.',
                                 xalign=0), False, False, 0)
        grid = Gtk.Grid(column_spacing=12, row_spacing=8)
        self.rows = []
        for index in range(5):
            key = Gtk.ComboBoxText()
            key.append('', 'Disabled')
            for name in settings.KEYS:
                key.append(name, name)
            layout = Gtk.ComboBoxText(hexpand=True)
            binding = self.config['bindings'][index] if index < len(self.config['bindings']) else None
            key.set_active_id(binding['key'] if binding else '')
            grid.attach(Gtk.Label(label=f'Hotkey {index+1}', xalign=0), 0, index, 1, 1)
            grid.attach(key, 1, index, 1, 1)
            grid.attach(Gtk.Label(label='Layout'), 2, index, 1, 1)
            grid.attach(layout, 3, index, 1, 1)
            self.rows.append((key, layout))
        root.pack_start(grid, False, False, 0)
        actions = Gtk.Box(spacing=8)
        save = Gtk.Button(label='Save Settings')
        save.connect('clicked', lambda _: self.save_settings())
        actions.pack_start(save, False, False, 0)
        refresh = Gtk.Button(label='Refresh Layouts')
        refresh.connect('clicked', lambda _: self.refresh_sources())
        actions.pack_start(refresh, False, False, 0)
        root.pack_start(actions, False, False, 0)
        self.quick = Gtk.Box(spacing=8)
        root.pack_start(self.quick, False, False, 0)
        frame = Gtk.Frame(label='Errors / optional diagnostics')
        scroll = Gtk.ScrolledWindow(hexpand=True, vexpand=True, min_content_height=170)
        self.output = Gtk.TextView(editable=False, monospace=True, wrap_mode=Gtk.WrapMode.WORD_CHAR)
        scroll.add(self.output)
        frame.add(scroll)
        root.pack_start(frame, True, True, 0)
        footer = Gtk.Box(spacing=8)
        for title, callback in [('Copy All', self.copy_logs), ('Clear', self.clear_logs),
                                ('About', self.about), ('Exit', self.exit_app)]:
            button = Gtk.Button(label=title)
            button.connect('clicked', lambda _, cb=callback: cb())
            footer.pack_start(button, False, False, 0)
        root.pack_start(footer, False, False, 0)
        return root

    def call(self, method, parameters=None, signature=None):
        if self.bus is None:
            self.bus = Gio.bus_get_sync(Gio.BusType.SESSION, None)
        return self.bus.call_sync('org.gnome.Shell', OBJECT, INTERFACE, method,
            parameters, GLib.VariantType(signature) if signature else None,
            Gio.DBusCallFlags.NONE, 1000, None)

    def refresh_sources(self):
        try:
            state = self.bridge_state()
            if state.get('version') != 3:
                raise ValueError('Old DT is loaded. Log out and back in after installation.')
            self.sources = state['sources']
            for index, (_, combo) in enumerate(self.rows):
                selected = combo.get_active_id()
                if not selected and index < len(self.config['bindings']):
                    selected = self.config['bindings'][index]['layout']
                combo.remove_all()
                for source in self.sources:
                    combo.append(source['id'], source['name'])
                if selected:
                    combo.set_active_id(selected)
                if combo.get_active() < 0 and self.sources:
                    combo.set_active(0)
            for child in self.quick.get_children():
                child.destroy()
            for index, binding in enumerate(self.config['bindings'][:3]):
                button = Gtk.Button(label=f'{index+1}: {binding["layout"]}')
                button.connect('clicked', lambda _, target=binding['layout']: self.switch(target, False))
                self.quick.pack_start(button, True, True, 0)
            self.quick.show_all()
            self.log('DT connected; current=' + state['current'])
            return True
        except (GLib.Error, ValueError, KeyError) as error:
            self.status.set_text('DT unavailable; see log')
            self.log(str(error), error=True)
            return False

    def bridge_state(self):
        try:
            reply = self.call('GetState', signature='(s)')
        except GLib.Error:
            # Installation can leave the extension present but disabled.
            self.log('DT is unavailable; enabling the installed GNOME extension...')
            result = self.bus.call_sync('org.gnome.Shell.Extensions',
                '/org/gnome/Shell/Extensions', 'org.gnome.Shell.Extensions',
                'EnableExtension', GLib.Variant('(s)', ('double-tap@freeman4848.local',)),
                GLib.VariantType('(b)'), Gio.DBusCallFlags.NONE, 2000, None)
            if not result.unpack()[0]:
                raise ValueError('GNOME could not enable DT. Log out and back in after installation.')
            reply = self.call('GetState', signature='(s)')
        return json.loads(reply.unpack()[0])

    def start(self):
        self.want_running = True
        if self.running:
            return
        if not self.refresh_sources():
            self.schedule_retry()
            return
        try:
            connection = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
            connection.settimeout(2)
            connection.connect(self.reader_socket)
            connection.sendall((json.dumps(self.config) + '\n').encode())
            connection.settimeout(None)
            self.call('SetOptions', GLib.Variant('(bb)',
                (self.config['protect_input'], self.config['diagnostics'])))
            self.call('SetRunning', GLib.Variant('(b)', (True,)))
            self.connection = connection
            self.generation += 1
            generation = self.generation
            self.running = True
            if self.retry_source:
                GLib.source_remove(self.retry_source)
                self.retry_source = None
            self.started_at = time.monotonic()
            self.set_icon(True)
            self.start_button.set_label('Stop')
            self.status.set_text('Active')
            self.log(f'Started; double-tap interval={self.config["threshold_ms"]} ms')
            threading.Thread(target=self.read_events, args=(connection, generation), daemon=True).start()
        except (OSError, GLib.Error) as error:
            if 'connection' in locals():
                connection.close()
            self.log('Cannot start reader: ' + str(error), error=True)
            self.status.set_text('Stopped; see log')
            self.schedule_retry()

    def schedule_retry(self):
        if not self.want_running or self.retry_source:
            return
        if self.retry_attempts >= 5:
            self.want_running = False
            self.status.set_text('Stopped: retry limit; press Start to retry')
            self.start_button.set_label('Start')
            self.retry_attempts = 0
            return
        delay = min(2 ** self.retry_attempts, 16)
        self.retry_attempts += 1
        self.status.set_text(f'Waiting for reader; retry in {delay}s')
        self.start_button.set_label('Stop')
        self.retry_source = GLib.timeout_add_seconds(delay, self.retry_start)

    def retry_start(self):
        self.retry_source = None
        if self.want_running:
            self.start()
        return GLib.SOURCE_REMOVE

    def read_events(self, connection, generation):
        error = 'Keyboard reader disconnected.'
        try:
            with connection.makefile('r') as stream:
                for line in stream:
                    target = json.loads(line)['target']
                    GLib.idle_add(self.on_target, generation, target)
        except (OSError, ValueError, KeyError) as exception:
            error = str(exception)
        GLib.idle_add(self.reader_ended, generation, error)

    def on_target(self, generation, target):
        if self.running and generation == self.generation:
            self.retry_attempts = 0
            self.switch(target, True)
        return GLib.SOURCE_REMOVE

    def reader_ended(self, generation, error):
        if generation == self.generation and self.running:
            wanted = self.want_running
            attempts = self.retry_attempts
            if self.started_at is not None and time.monotonic() - self.started_at > 30:
                attempts = 0
            self.log(error, error=True)
            self.stop()
            if wanted:
                self.want_running = True
                self.retry_attempts = attempts
                self.schedule_retry()
        return GLib.SOURCE_REMOVE

    def switch(self, target, erase):
        start = time.monotonic()
        try:
            current = self.call('Switch', GLib.Variant('(sb)', (target, erase)), '(s)').unpack()[0]
            if current.startswith('blocked:'):
                self.status.set_text('Paused: protected input / unknown context')
                return
            self.status.set_text('Active' if self.running else 'Stopped')
            self.log(f'Switch completed; roundtrip_ms={(time.monotonic()-start)*1000:.1f}')
        except GLib.Error as error:
            self.log('Switch error: ' + error.message, error=True)

    def stop(self):
        self.want_running = False
        self.retry_attempts = 0
        self.started_at = None
        if self.retry_source:
            GLib.source_remove(self.retry_source)
            self.retry_source = None
        self.generation += 1
        self.running = False
        self.set_icon(False)
        if self.connection:
            try:
                self.connection.shutdown(socket.SHUT_RDWR)
            except OSError:
                pass
            self.connection.close()
            self.connection = None
        try:
            self.call('SetRunning', GLib.Variant('(b)', (False,)))
        except GLib.Error:
            pass
        self.start_button.set_label('Start')
        self.status.set_text('Stopped')
        self.log('Stopped; system keyboard shortcuts remain available.')

    def restart(self):
        self.stop()
        self.start()

    def save_settings(self):
        try:
            bindings = [{'key': key.get_active_id(), 'layout': layout.get_active_id()}
                for key, layout in self.rows if key.get_active_id()]
            updated = settings.validate({'threshold_ms': self.threshold.get_value_as_int(),
                'bindings': bindings, 'diagnostics': self.diagnostics.get_active(),
                'protect_input': self.protect_input.get_active()})
            settings.save(updated)
            self.config = updated
            self.log('Settings saved.')
            if self.running:
                self.restart()
            else:
                self.refresh_sources()
        except (ValueError, OSError) as error:
            self.log('Settings not saved: ' + str(error), error=True)

    def on_action(self, _bus, _sender, _path, _iface, _signal, parameters):
        action = parameters.unpack()[0]
        if action == 'toggle':
            self.stop() if self.want_running else self.start()
        elif action == 'restart':
            self.restart()
        elif action == 'exit':
            self.exit_app()

    def on_context_changed(self, *_args):
        if self.running and self.connection:
            try:
                self.connection.sendall(b'R')
            except OSError:
                pass  # Reader EOF handles reconnection; no input data is logged.

    def log(self, message, error=False):
        if not error and not self.config['diagnostics']:
            return
        line = time.strftime('%H:%M:%S ') + message
        self.logs.append(line)
        if len(self.logs) > 2000:
            self.logs = self.logs[-2000:]
        if hasattr(self, 'output'):
            buffer = self.output.get_buffer()
            buffer.set_text('\n'.join(self.logs))
            end = buffer.get_end_iter()
            self.output.scroll_to_iter(end, 0, False, 0, 0)
        print(line, flush=True)

    def set_icon(self, active):
        name = 'dt-active.svg' if active else 'dt-inactive.svg'
        candidates = [BASE.parent / 'icons' / name,
            Path('/usr/share/gnome-shell/extensions/double-tap@freeman4848.local/icons') / name]
        for path in candidates:
            if path.exists():
                self.icon.set_from_pixbuf(GdkPixbuf.Pixbuf.new_from_file_at_scale(
                    str(path), 48, 48, True))
                return
        self.icon.set_from_icon_name('input-keyboard', Gtk.IconSize.DIALOG)

    def copy_logs(self):
        Gtk.Clipboard.get(Gdk.SELECTION_CLIPBOARD).set_text('\n'.join(self.logs), -1)

    def clear_logs(self):
        self.logs.clear()
        self.output.get_buffer().set_text('')

    def about(self):
        dialog = Gtk.AboutDialog(transient_for=self.window, modal=True,
            program_name='Double Tap Switcher', version='1.25', authors=['Freeman4848'],
            comments='Double-tap a configured physical key to select its keyboard layout.\n'
                     'Support: https://ko-fi.com/freeman4848\n'
                     'Email: freeman4848.dev@gmail.com',
            website='https://github.com/Freeman4848', website_label='GitHub',
            copyright='Copyright Freeman4848',
            license_type=Gtk.License.MIT_X11)
        dialog.run()
        dialog.destroy()

    def hide_window(self, window, _event):
        window.hide()
        return True

    def exit_app(self):
        self.stop()
        if self.subscription:
            self.bus.signal_unsubscribe(self.subscription)
        if self.context_subscription:
            self.bus.signal_unsubscribe(self.context_subscription)
        self.release()
        self.quit()

    def do_shutdown(self):
        if self.running or self.connection:
            self.stop()
        Gtk.Application.do_shutdown(self)


if __name__ == '__main__':
    import sys
    raise SystemExit(Application().run(sys.argv))
