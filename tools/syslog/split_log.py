#!/usr/bin/env python3
import re
import sys
import argparse
import itertools
from collections import defaultdict
from pathlib import Path
from dataclasses import dataclass
from typing import Iterable
import operator


class TimeRangeAction(argparse.Action):
    r"""
    Parse time range:
        HH:MM:SS-HH:MM:SS
        HH:MM:SS-
        -HH:MM:SS
        HH:MM:SS

        with HH:MM:SS = \d+(:\d+(:\d+))
               ~~~~~~ optional -> assume range of HH:00:00-HH:59:59
                  ~~~ optional -> assume range of HH:MM:00-HH:MM:59

        Create a pair of duration in seconds
    """
    def __call__(self, parser, namespace, values, option_string=None):
        if not values:
            return

        m = re.match(r'^(?:(\d++):?+(\d*+):?+(\d*+))?+(-)?+(\d*+):?+(\d*+):?+(\d*+)$',
                     values)
        if not m:
            raise ValueError("invalid date range format")

        (start_h, start_m, start_s,
        sep,
        end_h, end_m, end_s) = m.group(1,2,3, 4, 5,6,7)

        start = (
            int(start_h) * 3600 +
            (int(start_m) * 60 if start_m else 0) +
            (int(start_s) if start_s else 0)
            ) if start_h else 0
        if not sep:
            end_h, end_m, end_s = start_h, start_m, start_s
        end = (
            int(end_h) * 3600 +
            (int(end_m) * 60 if end_m else 3599) +
            (int(end_s) if end_s else 59)
            ) if end_h else 2**64
        setattr(namespace, self.dest, (start, end))


parser = argparse.ArgumentParser(description='Log splitter')
parser.add_argument('-t', '--time', action=TimeRangeAction,
                    help='Only log in the specified date (HH:MM:SS-HH:MM:SS with optional left or right part and optional MM and SS).')
parser.add_argument('-s', '--suffix', help='Specify suffix for output filenames.', default='.log')
parser.add_argument('-p', '--prefix', help='Specify prefix for output filenames.')
parser.add_argument('-m', '--min-ignore', type=int, default=18, metavar='N',
                    help='Ignore log with number of line <= N. Default is 18.')
parser.add_argument('filenames', nargs='*', help='Log files. Use \'-\' as filename for stdin. When no filename, stdin is used.')

args = parser.parse_args()


month_priority = {
    'Jan': 0,
    'janv': 0,
    'jan': 0,
    'Feb': 1,
    'févr': 1,
    'fév': 1,
    'Mar': 2,
    'mars': 2,
    'mar': 2,
    'Apr': 3,
    'avr': 3,
    'May': 4,
    'mai': 4,
    'Jun': 5,
    'jun': 5,
    'Jul': 6,
    'juill': 6,
    'jul': 6,
    'Aug': 7,
    'aoû': 7,
    'aou': 7,
    'Sep': 8,
    'Sept': 8,
    'sept': 8,
    'sep': 8,
    'Oct': 9,
    'oct': 9,
    'Nov': 10,
    'nov': 10,
    'Dec': 11,
    'déc': 11,
}

LineInfo = tuple[int,  # month + time
                 str,  # progname
                 str,  # pid
                 str]  # line

# Log sample:
# Dec  8 15:51:22 HOSTNAME rdpproxy[211075]: INFO (211075/211075) -- ReDemPtion 10.4.31 starting
# Dec  8 15:51:22 HOSTNAME rdpproxy[211075]: INFO (211075/211075) -- src=10.10.8.87 sport=51109 dst=10.94.0.13 dport=3389
# Dec  8 15:51:22 HOSTNAME rdpproxy[211075]: INFO (20770/20770) -- connecting to /var/lib/redemption/auth/sesman.sck
# Dec  8 15:51:22 HOSTNAME rdpproxy[211075]: INFO (20770/20770) -- connection to /var/lib/redemption/auth/sesman.sck succeeded : socket 6
# Dec  8 15:51:22 HOSTNAME WAB(root)[8790]: Bastion user 'Interactive@abcdef:RDP:XYZ:abc.def' authentication from 10.10.8.87 failed [4 tries remains]
progname_reg = re.compile(r' (rdpproxy|WAB\(root\))\[(\d+)\]: ')
sesman_start_reg = re.compile(r" Bastion user '[^']+' authentication from ([^ ]+) failed")
rdpproxy_start_reg = re.compile(r' -- Redemption \d+\.\d+\.\d+$')
rdpproxy_src_addr_reg = re.compile(r' -- src=([^ ]+)')
time_reg = re.compile(r'(\d++):(\d++):(\d++)')
date_reg = re.compile(r'^(\w+) (\d+) (\d+):(\d+):(\d+)')


def extract_info(line: str) -> None | LineInfo:
    if m := progname_reg.search(line):
        progname, pid = m.group(1, 2)
        if m := date_reg.match(line):
            month, day, h, m, s = m.group(1,2,3,4,5)
            imonth = month_priority.get(month, -1)
            t = imonth * 2851200 + int(day) * 86400 + int(h) * 3600 + int(m) * 60 + int(s)
        else:
            t = -1
        return (t, progname, pid, line)


def parse_time(s: str) -> int:
    """
    HH:MM:SS format to duration in seconds
    """
    if m := time_reg.search(s):
        h, m, s = m.group(1, 2, 3)
        return int(h) * 3600 + int(m) * 60 + int(s)
    return 0


@dataclass
class SessionData:
    pid: str
    lines: list[str]
    scr_addr: str = ''
    sesman_pid: str = ''


class Splitter:
    finished_log: list[SessionData]
    # ignore short log (probably watchdog or mstsc pre-connect)
    min_ignore: int
    time_range: None | tuple[int, int] = None
    data_by_pid: dict[str, SessionData]
    sesman_map: dict[str, str]  # pid -> pid
    src_map: dict[str, str]  # scr_addr -> pid

    def __init__(self, min_ignore: int, time_range: None | tuple[int, int]):
        self.min_ignore = min_ignore
        self.time_range = time_range
        self.finished_log = []
        self.data_by_pid = {}
        self.sesman_map = {}
        self.src_map = {}

    def process(self, lines_info: Iterable[LineInfo]):
        """
        Read LineInfo and initialize finished_log
        """
        for _, progname, pid, line in lines_info:
            if m := progname_reg.search(line):
                progname, pid = m.group(1, 2)

                # rdpproxy
                if progname.startswith('r'):
                    data = self.data_by_pid.get(pid)
                    # start a new rdp session
                    if rdpproxy_start_reg.search(line):
                        if data:
                            self.clean_session(data)
                            if self.filter_data(data):
                                self.finished_log.append(data)
                        data = SessionData(pid, [line])
                        self.data_by_pid[pid] = data
                    # add line
                    elif data:
                        # second line, extract source addr for sesman mapping
                        if len(data.lines) == 1 and (m := rdpproxy_src_addr_reg.search(line)):
                            data.scr_addr = m.group(1)
                            self.src_map[data.scr_addr] = pid
                        data.lines.append(line)

                # WAB
                elif proxy_pid := self.sesman_map.get(pid):
                    self.data_by_pid[proxy_pid].lines.append(line)
                # new sesman session
                elif m := sesman_start_reg.search(line):
                    addr = m.group(1)
                    if proxy_pid := self.src_map.get(addr):
                        if data := self.data_by_pid[proxy_pid]:
                            self.sesman_map[pid] = proxy_pid
                            self.src_map.pop(addr)
                            data.sesman_pid = pid
                            data.lines.append(line)

        self.finished_log.extend(self.clean_session(data)
                                 for data in self.data_by_pid.values()
                                 if self.filter_data(data))
        self.data_by_pid.clear()

    def clean_session(self, data: SessionData) -> SessionData:
        self.src_map.pop(data.scr_addr, None)
        self.sesman_map.pop(data.sesman_pid, None)
        return data

    def filter_data(self, data: SessionData) -> bool:
        # too short log
        if len(data.lines) <= self.min_ignore:
            return False

        # not date configured
        if not self.time_range:
            return True

        t1 = parse_time(data.lines[0], 0)
        t2 = parse_time(data.lines[-1], 0)
        if not t1 or not t2:  # bad log
            return True

        start = self.time_range[0]
        end = self.time_range[1]
        return start <= t1 <= end or start <= t2 <= end


#
# Read files
#

if not args.filenames:
    lines = sys.stdin
else:
    # files are not closed...
    lines = itertools.chain.from_iterable(
        (open(filename, encoding='utf-8') if filename != '-' else sys.stdin)
        for filename in args.filenames
    )

#
# Parse log
#

splitter = Splitter(min_ignore=args.min_ignore,
                    time_range=args.time)
splitter.process(sorted(filter(None, map(extract_info, lines)),
                        key=operator.itemgetter(0)))

#
# Create log by session
#

default_prefix: str = args.prefix or ''
prefix_counter = defaultdict(int)
suffix: str = args.suffix

total_line = 0
data: SessionData
for data in splitter.finished_log:
    total_line += len(data.lines)
    content = ''.join(data.lines)
    date = f"{content[:content.index(' rdpproxy')]}."
    prefix = f'{default_prefix}{date}{data.pid}'
    n = prefix_counter[prefix]
    prefix_counter[prefix] += 1
    suffix_counter = f'-{n}' if n else ''
    filename = f'{prefix}{suffix_counter}{suffix}'
    print(f'new file: \x1b[34m{filename}\x1b[m (lines={len(data.lines)})')
    Path(filename).write_text(content, encoding='utf-8')
print(f'total line: {total_line}\ntotal file: {len(splitter.finished_log)}')
