import os
import configparser

try:
    from functools import cache
except ImportError:
    # Workaround for old Pythons
    from functools import lru_cache

    def cache(func):
        return lru_cache(None)(func)

config_root = os.getenv('XDG_CONFIG_HOME',
                        default=os.path.expanduser('~/.config/geopmtools'))
os.makedirs(config_root, exist_ok=True)


def get_history_path():
    return os.path.join(config_root, 'batch_reader_history')


def get_configuration_path():
    return os.path.join(config_root, 'configuration.ini')


@cache
def read_configuration():
    config = configparser.ConfigParser()
    config.read(get_configuration_path())
    return config


def is_vi_mode():
    return read_configuration().getboolean('editor', 'vi_mode', fallback='no')
