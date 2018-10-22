# SPDX-License-Identifier: AGPL-3.0-or-later
"""
Part of the high-level python bindings for Kopano

Copyright 2018 - Kopano and its licensors (see LICENSE file for details)
"""

# two-way conversion of timezones between MAPI and python
#
# note that pyko always outputs/expects naive system-local datetimes.
# (except for recurrence.{start,end}, which are timezone-local dates)
# this may have been a mistake, but OTOH, it does seem nice for interactive use.
# anyway this is not easy to change now. we intend to add a global flag
# to python-kopano, to make all datetimes timezone-aware.

import calendar
import datetime
import struct

import dateutil.tz
from dateutil.relativedelta import (
    relativedelta, MO, TU, TH, FR, WE, SA, SU
)
import pytz

RRULE_WEEKDAYS = {0: SU, 1: MO, 2: TU, 3: WE, 4: TH, 5: FR, 6: SA}
UTC = dateutil.tz.tzutc()
LOCAL = dateutil.tz.tzlocal()
TZFMT = '<lll H HHHHHHHH H HHHHHHHH'

# convert MAPI timezone struct to datetime-compatible tzinfo class

class MAPITimezone(datetime.tzinfo):
    def __init__(self, tzdata):
        # TODO more thoroughly check specs (MS-OXOCAL)
        self.timezone, _, self.timezonedst, \
        _, \
        _, self.dstendmonth, self.dstendweek, self.dstendday, self.dstendhour, _, _, _, \
        _, \
        _, self.dststartmonth, self.dststartweek, self.dststartday, self.dststarthour, _, _, _ = \
            struct.unpack(TZFMT, tzdata)

    def _date(self, dt, dstmonth, dstweek, dstday, dsthour):
        d = datetime.datetime(dt.year, dstmonth, 1, dsthour)
        if dstday == 5: # last weekday of month
            d += relativedelta(months=1, days=-1, weekday=RRULE_WEEKDAYS[dstweek](-1))
        else:
            d += relativedelta(weekday=RRULE_WEEKDAYS[dstweek](dstday))
        return d

    def dst(self, dt):
        if self.dststartmonth == 0: # no DST
            return datetime.timedelta(0)

        start = self._date(dt, self.dststartmonth, self.dststartweek, self.dststartday, self.dststarthour)
        end = self._date(dt, self.dstendmonth, self.dstendweek, self.dstendday, self.dstendhour)

        # Can't compare naive to aware objects, so strip the timezone from
        # dt first.
        # TODO end < start case!
        dt = dt.replace(tzinfo=None)

        if ((start < end and start < dt < end) or \
            (start > end and not end < dt < start)):
            return datetime.timedelta(minutes=-self.timezone)
        else:
            return datetime.timedelta(minutes=0)

    def utcoffset(self, dt):
        return datetime.timedelta(minutes=-self.timezone) + self.dst(dt)

    def tzname(self, dt):
        return 'MAPITimezone()'

    def __repr__(self):
        return 'MAPITimezone()'

# convert Olson timezone name to MAPI timezone struct

def _timezone_struct(name):
    # this is a bit painful. we need to generate a MAPI timezone structure
    # (rule) from an 'olson' name. on the one hand the olson db (as in pytz)
    # does not expose the actual DST rules, if any, and just expands all
    # transitions. this means we have to do some guess work.
    # on the other hand, the MAPI struct/rule is quite limited (transition
    # always on "Nth weekday in month", and not historical). ideally of course
    # we move away from the MAPI struct altogether, and just use the olson name..
    # but that would probably break other clients at this point.

    tz = pytz.timezone(name)

    now = datetime.datetime.now()
    year = now.year

    yearstart = datetime.datetime(year,1,1)

    runningdst = tz.dst(yearstart)

    UTCBIAS = tz.utcoffset(yearstart)
    DSTBIAS = None
    DSTSTART = None
    DSTEND = None

    ZERO = datetime.timedelta(0)

    days = 366 if calendar.isleap(year) else 365

    for day in range(days): # TODO find matching transitions
        dt = datetime.datetime(year,1,1) + datetime.timedelta(days=day)

        datedst = tz.dst(dt)

        if runningdst == ZERO and datedst != ZERO:
            runningdst = datedst
            UTCBIAS = tz.utcoffset(dt) - datedst
            DSTBIAS = datedst
            DSTSTART = dt

        elif runningdst != ZERO and datedst == ZERO:
            runningdst = datedst
            DSTEND = dt

    utcbias = -UTCBIAS.seconds//60

    if DSTBIAS:
        dstbias = -DSTBIAS.seconds//60
        startmonth = DSTSTART.month
        endmonth = DSTEND.month
        weekday = DSTSTART.weekday() # TODO don't assume start/end are same weekday and last-in-month

        return struct.pack(TZFMT, utcbias, 0, dstbias, 0, 0, endmonth, weekday, 5, 0, 0, 0, 0, 0, 0, startmonth, weekday, 5, 0, 0, 0, 0)
    else:
        return struct.pack(TZFMT, utcbias, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0)

# conversion helpers

def _from_utc(date, tzinfo):
    return date.replace(tzinfo=UTC).astimezone(tzinfo).replace(tzinfo=None)

def _to_utc(date, tzinfo): # TODO local??
    return date.replace(tzinfo=tzinfo).astimezone(LOCAL).replace(tzinfo=None)

def _tz2(date, tz1, tz2):
    return date.replace(tzinfo=tz1).astimezone(tz2).replace(tzinfo=None)
