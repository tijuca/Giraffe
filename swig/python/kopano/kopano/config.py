"""
Part of the high-level python bindings for Kopano

Copyright 2005 - 2016 Zarafa and its licensors (see LICENSE file for details)
Copyright 2016 - Kopano and its licensors (see LICENSE file for details)
"""

import os

from .compat import decode as _decode
from .errors import ConfigError
from .utils import human_to_bytes as _human_to_bytes

class ConfigOption:
    def __init__(self, type_, **kwargs):
        self.type_ = type_
        self.kwargs = kwargs

    def parse(self, key, value):
        return getattr(self, 'parse_' + self.type_)(key, value)

    def parse_string(self, key, value):
        if self.kwargs.get('multiple'):
            values = value.split()
        else:
            values = [value]
        for value in values:
            if self.kwargs.get('check_path') and not os.path.exists(value): # XXX moved to parse_path
                raise ConfigError("%s: path '%s' does not exist" % (key, value))
            if self.kwargs.get('options') is not None and value not in self.kwargs.get('options'):
                raise ConfigError("%s: '%s' is not a legal value" % (key, value))
        if self.kwargs.get('multiple'):
            return values
        else:
            return values[0]

    def parse_path(self, key, value):
        if self.kwargs.get('check', True) and not os.path.exists(value):
            raise ConfigError("%s: path '%s' does not exist" % (key, value))
        return value

    def parse_integer(self, key, value):
        if self.kwargs.get('options') is not None and int(value) not in self.kwargs.get('options'):
            raise ConfigError("%s: '%s' is not a legal value" % (key, value))
        if self.kwargs.get('multiple'):
            return [int(x, base=self.kwargs.get('base', 10)) for x in value.split()]
        return int(value, base=self.kwargs.get('base', 10))

    def parse_boolean(self, key, value):
        return {'no': False, 'yes': True, '0': False, '1': True, 'false': False, 'true': True}[value]

    def parse_size(self, key, value):
        return _human_to_bytes(value)

class Config:
    """
Configuration class

:param config: dictionary describing configuration options. TODO describe available options

Example::

    config = Config({
        'some_str': Config.String(default='blah'),
        'number': Config.Integer(),
        'filesize': Config.size(), # understands '5MB' etc
    })

"""
    def __init__(self, config, service=None, options=None, filename=None, log=None):
        self.config = config
        self.service = service
        self.warnings = []
        self.errors = []
        if filename:
            pass
        elif options and getattr(options, 'config_file', None):
            filename = options.config_file
        elif service:
            filename = '/etc/kopano/%s.cfg' % service
        self.data = {}
        if self.config is not None:
            for key, val in self.config.items():
                if 'default' in val.kwargs:
                    self.data[key] = val.kwargs.get('default')
        for line in open(filename):
            line = _decode(line.strip())
            if not line.startswith('#'):
                pos = line.find('=')
                if pos != -1:
                    key = line[:pos].strip()
                    value = line[pos + 1:].strip()
                    if self.config is None:
                        self.data[key] = value
                    elif key in self.config:
                        if self.config[key].type_ == 'ignore':
                            self.data[key] = None
                            self.warnings.append('%s: config option ignored' % key)
                        else:
                            try:
                                self.data[key] = self.config[key].parse(key, value)
                            except ConfigError as e:
                                if service:
                                    self.errors.append(e.message)
                                else:
                                    raise
                    else:
                        msg = "%s: unknown config option" % key
                        if service:
                            self.warnings.append(msg)
                        else:
                            raise ConfigError(msg)
        if self.config is not None:
            for key, val in self.config.items():
                if key not in self.data and val.type_ != 'ignore':
                    msg = "%s: missing in config file" % key
                    if service: # XXX merge
                        self.errors.append(msg)
                    else:
                        raise ConfigError(msg)

    @staticmethod
    def string(**kwargs):
        return ConfigOption(type_='string', **kwargs)

    @staticmethod
    def path(**kwargs):
        return ConfigOption(type_='path', **kwargs)

    @staticmethod
    def boolean(**kwargs):
        return ConfigOption(type_='boolean', **kwargs)

    @staticmethod
    def integer(**kwargs):
        return ConfigOption(type_='integer', **kwargs)

    @staticmethod
    def size(**kwargs):
        return ConfigOption(type_='size', **kwargs)

    @staticmethod
    def ignore(**kwargs):
        return ConfigOption(type_='ignore', **kwargs)

    def get(self, x, default=None):
        return self.data.get(x, default)

    def __getitem__(self, x):
        return self.data[x]

CONFIG = {
    'log_method': Config.string(options=['file', 'syslog'], default='file'),
    'log_level': Config.string(options=[str(i) for i in range(7)] + ['info', 'debug', 'warning', 'error', 'critical'], default='info'),
    'log_file': Config.string(default=None),
    'log_timestamp': Config.integer(options=[0, 1], default=1),
    'pid_file': Config.string(default=None),
    'run_as_user': Config.string(default=None),
    'run_as_group': Config.string(default=None),
    'running_path': Config.string(check_path=True, default='/var/lib/kopano'),
    'server_socket': Config.string(default=None),
    'sslkey_file': Config.string(default=None),
    'sslkey_pass': Config.string(default=None),
    'worker_processes': Config.integer(default=1),
}
