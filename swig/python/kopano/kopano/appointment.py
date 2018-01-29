"""
Part of the high-level python bindings for Kopano

Copyright 2005 - 2016 Zarafa and its licensors (see LICENSE file)
Copyright 2016 - Kopano and its licensors (see LICENSE file)
"""

from MAPI import (
    PT_SYSTIME,
)

from .errors import NotFoundError
from .recurrence import Recurrence, Occurrence

class Appointment(object):
    """Appointment mixin class"""

    @property
    def start(self): # XXX optimize, guid
        return self.prop('common:34070').value

    @start.setter
    def start(self, val):
        # XXX check if exists?
        self.create_prop('common:34070', val, PT_SYSTIME)
        self.create_prop('appointment:33293', val, PT_SYSTIME)

    @property
    def end(self): # XXX optimize, guid
        return self.prop('common:34071').value

    @end.setter
    def end(self, val):
        # XXX check if exists?
        self.create_prop('common:34071', val, PT_SYSTIME)
        self.create_prop('appointment:33294', val, PT_SYSTIME)

    @property
    def location(self):
        try:
            return self.prop('appointment:33288').value
        except NotFoundError:
            pass

    @property
    def recurring(self):
        return self.prop('appointment:33315').value

    @property
    def recurrence(self):
        return Recurrence(self)

    def occurrences(self, start=None, end=None):
        if self.recurring:
            for occ in self.recurrence.occurrences(start=start, end=end):
                yield occ
        else:
            if (not start or self.end > start) and \
               (not end or self.start < end):
                start = max(self.start, start) if start else self.start
                end = min(self.end, end) if end else self.end
                yield Occurrence(self, start, end)

    @property
    def rrule(self): # XXX including timezone!
        if self.recurring: # XXX rrule for non-recurring makes sense?
            return self.recurrence.recurrences

    # XXX rrule setter!
