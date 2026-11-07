"""Validated user settings; no desktop keyboard settings are modified."""
import json
import os
from pathlib import Path

KEYS = {'Q': 16, 'W': 17, 'E': 18, 'R': 19, 'T': 20,
        'A': 30, 'S': 31, 'D': 32, 'F': 33, 'G': 34}
DEFAULT = {'threshold_ms': 250, 'diagnostics': False, 'protect_input': True, 'bindings': [
    {'key': 'Q', 'layout': 'us'}, {'key': 'W', 'layout': 'ru'},
    {'key': 'F', 'layout': 'ua'}]}


def validate(data):
    if not isinstance(data, dict):
        raise ValueError('Settings must be an object.')
    threshold = data.get('threshold_ms')
    bindings = data.get('bindings')
    if type(threshold) is not int or not 100 <= threshold <= 1000:
        raise ValueError('Double-tap interval must be 100-1000 ms.')
    if not isinstance(bindings, list) or not 1 <= len(bindings) <= 5:
        raise ValueError('Choose 1-5 hotkeys.')
    result, used = [], set()
    for binding in bindings:
        if not isinstance(binding, dict):
            raise ValueError('Invalid binding.')
        key, layout = binding.get('key'), binding.get('layout')
        if not isinstance(key, str) or key not in KEYS or key in used:
            raise ValueError('Choose distinct hotkeys.')
        if not isinstance(layout, str) or not 1 <= len(layout) <= 128:
            raise ValueError('Choose a layout.')
        if any(not (c.isascii() and (c.isalnum() or c in '+_-')) for c in layout):
            raise ValueError('Invalid layout ID.')
        used.add(key)
        result.append({'key': key, 'layout': layout})
    options = {}
    for name in ('diagnostics', 'protect_input'):
        value = data.get(name, DEFAULT[name])
        if type(value) is not bool:
            raise ValueError(f'{name} must be true or false.')
        options[name] = value
    return {'threshold_ms': threshold, 'bindings': result, **options}


def config_path():
    return Path(os.environ.get('XDG_CONFIG_HOME', Path.home() / '.config')) / 'double-tap-switcher/settings.json'


def load():
    path = config_path()
    return validate(json.loads(path.read_text()) if path.exists() else DEFAULT)


def save(data):
    data = validate(data)
    path = config_path()
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_suffix('.tmp')
    temporary.write_text(json.dumps(data, indent=2) + '\n')
    temporary.chmod(0o600)
    temporary.replace(path)
