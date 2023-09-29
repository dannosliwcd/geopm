import os
import sys

from colorama import Style, Fore
from geopmtools import configuration, __version__ as version
from prompt_toolkit import print_formatted_text, HTML, prompt, PromptSession
from prompt_toolkit.application.current import get_app
from prompt_toolkit.auto_suggest import AutoSuggestFromHistory
from prompt_toolkit.completion import FuzzyWordCompleter
from prompt_toolkit.history import FileHistory
from prompt_toolkit.validation import Validator, ValidationError
from struct import pack, unpack
import argparse
import colorama
import functools
import re
import signal
import time

try:
    import geopmdpy.pio as pio
    import geopmdpy.topo as topo
except OSError as e:
    print(f'{Style.BRIGHT}{Fore.RED}ERROR:{Style.RESET_ALL} '
          f'Unable to import geopm modules.\nReason: {e}',
          file=sys.stderr)
    exit(1)


@functools.lru_cache(128)  # Not much impact unless we are getting ALL signals under all domain indices
def get_units(signal_name):
    """Given a signal name, return the units abbreviation"""
    if signal_name.endswith('#'):
        return ''

    full_description = pio.signal_description(signal_name)
    for line in full_description.splitlines():
        line_parts = line.split()
        if len(line_parts) == 2 and line_parts[0] == 'units:':
            if line_parts[1] == 'seconds':
                return ' s'
            elif line_parts[1] == 'hertz':
                return ' Hz'
            elif line_parts[1] == 'watts':
                return ' W'
            elif line_parts[1] == 'joules':
                return ' j'
            elif line_parts[1] == 'celsius':
                return ' °C'
            else:
                return ''
    return ''


def get_signals_from_name_pattern(name_pattern):
    """Get a collection of signal names that match a given pattern. Patterns
    include at least one asterisk (*) as a wildcard, and one or more negative
    patterns prefixed by a dash (-)."""
    available_signals = pio.signal_names()
    signals = list()
    # See if any signal name wildcards were specified
    if '*' in name_pattern:
        if '-' in name_pattern:
            pattern, *anti_patterns = name_pattern.split('-')
        else:
            pattern = name_pattern
            anti_patterns = [r'^$']
        pattern = '.*'.join([re.escape(part) for part in pattern.split('*')])
        # see if there's any subtracted pattern
        anti_patterns = ['.*'.join([re.escape(part) for part in anti_pattern.split('*')])
                         for anti_pattern in anti_patterns]

        for available_signal in available_signals:
            if re.match(pattern, available_signal) and not any(re.match(anti_pattern, available_signal)
                                                               for anti_pattern in anti_patterns):
                signals.append(available_signal)
    else:
        if name_pattern not in available_signals:
            print(f'ERROR: Signal "{name_pattern}" is not available', file=sys.stderr)
            exit(1)
        signals.append(name_pattern)
    return signals


def interactively_generate_name_patterns():
    """Get a collection of signal names that match an interactively-given
    pattern. The interactive name selection provides visual overlays that
    show signal information to the user while the pattern is being
    constructed."""
    available_signals = pio.signal_names()

    short_meta_generators = {
        signal_name: (lambda s=signal_name: topo.domain_name(pio.signal_domain_type(s)))
        for signal_name in available_signals
    }

    def bottom_toolbar():
        """Show signal information in the bottom toolbar. The displayed
        information describes a signal with a name that starts similar to
        whatever is currently being typed by the user."""
        buffer_signal = get_app().current_buffer.document.get_word_under_cursor(WORD=True)
        buffer_signal = buffer_signal.split('@', 1)[0]

        nearby_existing_signal = None

        if buffer_signal:
            try:
                nearby_existing_signal = next(signal_name for signal_name in available_signals
                                              if signal_name.lower().startswith(buffer_signal.lower()))
            except StopIteration:
                pass

        if nearby_existing_signal:
            return f'{nearby_existing_signal}\n' + pio.signal_description(nearby_existing_signal)
        else:
            return ''

    completer = FuzzyWordCompleter(available_signals, WORD=True, meta_dict=short_meta_generators)
    session = PromptSession(
        completer=completer,
        complete_while_typing=True,
        auto_suggest=AutoSuggestFromHistory(),
        history=FileHistory(configuration.get_history_path()),
        bottom_toolbar=bottom_toolbar,
        vi_mode=configuration.is_vi_mode()
    )

    print('Enter a space-separated list of signals.')
    print('Each signal may optionally be followed by a domain specification. E.g.,')
    print_formatted_text(HTML('<i>CPU_FREQUENCY_STATUS@core[0,3-4] CPU_FREQUENCY_CONTROL@package POWER_PACKAGE</i>'))
    signals = session.prompt('signals> ', vi_mode=configuration.is_vi_mode()).split()
    return signals


class DelayTimeValidator(Validator):
    """A prompt_toolkit user-input validator for time delays."""

    def validate(self, document):
        text = document.text
        bad_input = False

        if text == '':
            return

        try:
            value = float(text)
        except ValueError:
            bad_input = True
        else:
            if value < 0:
                bad_input = True
        if bad_input:
            raise ValidationError(message='Must enter nothing or a non-negative float.')


class UserPrinter():
    def __init__(self, signal_descriptions):
        self.formatted_signal_descriptions = [
            f'{Style.BRIGHT}{Fore.BLUE}{signal_name}{Style.RESET_ALL}@{domain}{Style.RESET_ALL}[{domain_idx}]'
            for (signal_name, domain, domain_idx) in signal_descriptions
        ]
        self.max_signal_length = max(len(description) for description in self.formatted_signal_descriptions)
        self.signal_units = [get_units(signal_name) for (signal_name, _, _) in signal_descriptions]

    def print(self, signal_values):
        for description, value, signal_unit in zip(self.formatted_signal_descriptions, signal_values, self.signal_units):
            print(f'{description:>{self.max_signal_length}} '
                  f'{Style.BRIGHT}{value}{Style.RESET_ALL}{signal_unit}')
        print()


class CSVPrinter():
    def __init__(self, signal_descriptions):
        self.formatted_signal_descriptions = [
            f'{signal_name}@{domain}[{domain_idx}]'
            for (signal_name, domain, domain_idx) in signal_descriptions
        ]
        print(','.join(self.formatted_signal_descriptions))

    def print(self, signal_values):
        print(','.join([str(v) for v in signal_values]))


def main():
    parser = argparse.ArgumentParser(
        description='Repeatedly perform batch-reads of GEOPM signals. Run with '
                    'no list of signals to interactively choose from available signals.')
    parser.add_argument('-V', '--version',
                        action='store_true',
                        help='Print the version of this tool')
    parser.add_argument('--no-input', action='store_true',
                        help='Force non-interactive operation')
    parser.add_argument('--color', choices=['auto', 'never', 'always'],
                        default='auto',
                        help='Force enable or disable color in output')

    parser.add_argument('signals',
                        nargs='*',
                        help='List of signals to read. A "*" character matches anything. '
                             'Signals are selected interactively if no signals are provided here.')
    parser.add_argument('--diff',
                        action='store_true',
                        help='Display the signals differenced values instead of their raw values.')
    parser.add_argument('--csv',
                        action='store_true',
                        help='Display the printed values in CSV format.')
    delay_group = parser.add_mutually_exclusive_group()
    delay_group.add_argument('--watch-delay',
                             type=float,
                             default=1.0,
                             help='Re-read after watch-delay seconds. Default: %(default)s')
    delay_group.add_argument('-1', '--one-shot',
                             action='store_true',
                             help='Read once and exit')

    args = parser.parse_args()

    if args.version:
        print('geopmtrace', version)
        sys.exit(0)

    is_interactive = (sys.stdin.isatty() if not args.no_input else False)
    is_output_colored = (sys.stdout.isatty() if args.color == 'auto'
                         else True if args.color == 'always'
                         else False)

    colorama.init(strip=not is_output_colored)

    signals_domains_indices = list()
    name_pattern = args.signals
    watch_delay = None if args.one_shot else args.watch_delay

    if not name_pattern:
        if not is_interactive:
            print("Error: Running non-interactively, but no signal name pattern has been specified",
                  file=sys.stderr)
            exit(1)
        try:
            name_pattern = interactively_generate_name_patterns()
        except KeyboardInterrupt:
            exit(0)

    for signal_name in name_pattern:
        signal_name, *domain_spec = signal_name.split('@', 1)
        signal_names = get_signals_from_name_pattern(signal_name)

        if len(domain_spec) == 1:
            match = re.match(r'^([^\s\[]+)(\[([0-9,\-]+)\])?$', domain_spec[0])
            #                   |         `-domain index spec (optional list of one or more comma-separated integers)
            #                   `-domain name  (non-whitespace, non-index-spec)
            if not match:
                print(f'{Fore.RED}ERROR{Style.RESET_ALL}: Domain specification "{domain_spec}" is not valid',
                      file=sys.stderr)
                exit(1)
        else:
            match = ()

        for signal_name in signal_names:
            if match:
                domain_name, _, domain_idx_spec = match.groups()
            else:
                domain_name = topo.domain_name(pio.signal_domain_type(signal_name))
                domain_idx_spec = None

            # See if specific domain indices were specified, as in SIGNAL@DOMAIN[0-3,8]
            domain_indices = set()
            if domain_idx_spec is not None:
                for idx_range in domain_idx_spec.split(','):
                    if '-' in idx_range:
                        min_idx, max_idx = [int(i) for i in idx_range.split('-', 1)]
                        domain_indices.update(range(min_idx, max_idx + 1))
                    else:
                        domain_indices.add(int(idx_range))
            else:
                domain_indices.update(range(0, topo.num_domain(domain_name)))

            for domain_idx in domain_indices:
                signals_domains_indices.append((signal_name, domain_name, domain_idx))

    if len(signals_domains_indices) == 0:
        print('No valid signals were requested.', file=sys.stderr)
        exit(1)

    signal_indices = {signal_domain_idx: pio.push_signal(*signal_domain_idx)
                      for signal_domain_idx in signals_domains_indices}
    last_values = {idx: float('nan') for idx in signal_indices.values()}

    def sigint_handler(sig, frame):
        if is_interactive:
            print('\nDone running interactively using the following signals:', ' '.join(name_pattern), file=sys.stderr)
        exit(0)
    signal.signal(signal.SIGINT, sigint_handler)

    if args.csv:
        printer = CSVPrinter(signal_indices.keys())
    else:
        printer = UserPrinter(signal_indices.keys())

    while(True):
        pio.read_batch()
        print_values = list()
        for pio_idx in signal_indices.values():
            value = pio.sample(pio_idx)
            diff = value - last_values[pio_idx]
            last_values[pio_idx] = value
            if args.diff:
                value = diff
            if signal_name.endswith('#'):
                value = hex(unpack('q', pack('d', value))[0])
            print_values.append(value)
        printer.print(print_values)
        if watch_delay is None:
            break
        else:
            time.sleep(watch_delay)
