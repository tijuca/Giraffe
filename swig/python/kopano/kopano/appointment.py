# SPDX-License-Identifier: AGPL-3.0-only
"""
Part of the high-level python bindings for Kopano

Copyright 2005 - 2016 Zarafa and its licensors (see LICENSE file)
Copyright 2016 - Kopano and its licensors (see LICENSE file)
"""
import codecs
import datetime
import struct
import sys

from MAPI import (
    PT_SYSTIME, MNID_ID, PT_BOOLEAN, MODRECIP_ADD,
    KEEP_OPEN_READWRITE, PT_LONG
)

from MAPI.Tags import (
    PR_MESSAGE_RECIPIENTS, PR_RESPONSE_REQUESTED, PR_ENTRYID,
    PR_DISPLAY_NAME_W, PR_ADDRTYPE_W, PR_EMAIL_ADDRESS_W, PR_RECIPIENT_TYPE,
)

from MAPI.Struct import SPropValue

from .attendee import Attendee
from .errors import NotFoundError, ArgumentError
from .recurrence import Recurrence, Occurrence

from .compat import (
    benc as _benc, bdec as _bdec, fake_unicode as _unicode,
)
from .defs import (
    PSETID_Appointment, ASF_CANCELED, NR_COLOR, COLOR_NR, FB_STATUS, STATUS_FB,
)
from .pidlid import (
    PidLidReminderSet, PidLidReminderDelta, PidLidAppointmentSubType,
    PidLidBusyStatus, PidLidGlobalObjectId, PidLidRecurring,
    PidLidTimeZoneStruct, PidLidTimeZoneDescription, PidLidLocation,
    PidLidAppointmentStateFlags, PidLidAppointmentColor,
)
if sys.hexversion >= 0x03000000:
    try:
        from . import utils as _utils
    except ImportError: # pragma: no cover
        _utils = sys.modules[__package__+'.utils']

    from . import timezone as _timezone
else: # pragma: no cover
    import utils as _utils
    import timezone as _timezone

ALL_DAY_NAME = (PSETID_Appointment, MNID_ID, 0x8215)
START_NAME = (PSETID_Appointment, MNID_ID, 33293) # TODO use pidlid instead
END_NAME = (PSETID_Appointment, MNID_ID, 33294)
RECURRING_NAME = (PSETID_Appointment, MNID_ID, 33315)
BUSYSTATUS = (PSETID_Appointment, MNID_ID, 33285)

class Appointment(object):
    """Appointment mixin class"""

    @property
    def all_day(self):
        proptag = self.store._name_id(ALL_DAY_NAME) | PT_BOOLEAN
        return self._get_fast(proptag)

    @all_day.setter
    def all_day(self, value):
        self[PidLidAppointmentSubType] = value

    @property
    def show_as(self):
        try:
            return {
                0: u'free',
                1: u'tentative',
                2: u'busy',
                3: u'out_of_office',
                4: u'working_elsewhere'
            }[self[PidLidBusyStatus]]
        except NotFoundError:
            return u'unknown'

    @property
    def start(self):
        proptag = self.store._name_id(START_NAME) | PT_SYSTIME
        return self._get_fast(proptag)

    @start.setter
    def start(self, val): # TODO update/invalidate cache
        # XXX check if exists?
        self.create_prop('common:34070', val, PT_SYSTIME) # props are identical
        self.create_prop('appointment:33293', val, PT_SYSTIME)
        if self.recurring:
            self.recurrence._update_offsets()

    @property
    def end(self):
        proptag = self.store._name_id(END_NAME) | PT_SYSTIME
        return self._get_fast(proptag)

    @end.setter
    def end(self, val): # TODO update/invalidate cache
        # XXX check if exists?
        self.create_prop('common:34071', val, PT_SYSTIME) # props are identical
        self.create_prop('appointment:33294', val, PT_SYSTIME)
        if self.recurring:
            self.recurrence._update_offsets()

    @property
    def location(self):
        return self.get(PidLidLocation)

    @location.setter
    def location(self, value):
        self[PidLidLocation] = value

    @property
    def recurring(self):
        proptag = self.store._name_id(RECURRING_NAME) | PT_BOOLEAN
        return self._get_fast(proptag)

    @recurring.setter
    def recurring(self, value): # TODO update/invalidate cache
        # TODO cleanup on False?
        if value and not self.recurring:
            Recurrence._init(self)
        self[PidLidRecurring] = value

    @property
    def busystatus(self):
        proptag = self.store._name_id(BUSYSTATUS) | PT_LONG
        return FB_STATUS.get(self._get_fast(proptag))

    @busystatus.setter
    def busystatus(self, val):
        try:
            self.create_prop('appointment:33285', STATUS_FB[val], PT_LONG) # props are identical
        except KeyError:
            raise ArgumentError('invalid busy status: %s' % val)


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

    def occurrence(self, id_=None):
        if self.recurring:
            return self.recurrence.occurrence(id_)
        else:
            # TODO check if matches args
            return Occurrence(self)

    @property
    def reminder(self):
        """Is reminder set."""
        return self.get(PidLidReminderSet, False)

    @reminder.setter
    def reminder(self, value): # TODO move to item, because common?
        self[PidLidReminderSet] = value

    @property
    def reminder_minutes(self):
        """Reminder minutes before appointment."""
        return self.get(PidLidReminderDelta)

    @reminder_minutes.setter
    def reminder_minutes(self, value):
        self[PidLidReminderDelta] = value

    @property
    def rrule(self): # XXX including timezone!
        if self.recurring: # XXX rrule for non-recurring makes sense?
            return self.recurrence.recurrences

    # XXX rrule setter!

    # TODO merge with item.recipients?
    def attendees(self):
        for row in self.table(PR_MESSAGE_RECIPIENTS):
            yield Attendee(self.server, row)

    def create_attendee(self, type_, address):
        # TODO move to Attendee class

        reciptype = {
            'required': 1,
            'optional': 2,
            'resource': 3
        }[type_]

        table = self.table(PR_MESSAGE_RECIPIENTS)
        names = []
        pr_addrtype, pr_dispname, pr_email, pr_entryid = self._addr_props(address)
        names.append([
            SPropValue(PR_RECIPIENT_TYPE, reciptype),
            SPropValue(PR_DISPLAY_NAME_W, pr_dispname),
            SPropValue(PR_ADDRTYPE_W, _unicode(pr_addrtype)),
            SPropValue(PR_EMAIL_ADDRESS_W, _unicode(pr_email)),
            SPropValue(PR_ENTRYID, pr_entryid),
        ])
        self.mapiobj.ModifyRecipients(MODRECIP_ADD, names)
        _utils._save(self.mapiobj)

    @property
    def response_requested(self):
        return self.get(PR_RESPONSE_REQUESTED, False)

    @property
    def icaluid(self):
        try:
            return _benc(self[PidLidGlobalObjectId])
        except NotFoundError:
            pass

    @property
    def eventid(self):
        # msgraph has both appointments and expanded appointments under
        # /events, so we need an identier which can be used for both.
        eid = _bdec(self.entryid)
        return _benc(b'\x00' + _utils.pack_short(len(eid)) + eid)

    @property
    def tzinfo(self):
        tzdata = self.get(PidLidTimeZoneStruct)
        if tzdata:
            return _timezone.MAPITimezone(tzdata)

    @property
    def timezone(self):
        return self.get(PidLidTimeZoneDescription)

    @timezone.setter
    def timezone(self, value):
        self[PidLidTimeZoneDescription] = _unicode(value)
        self[PidLidTimeZoneStruct] = _timezone._timezone_struct(value)

    def accept(self, comment=None, tentative=False, respond=True):
        # TODO update appointment itself

        if respond:
            if tentative:
                message_class = 'IPM.Schedule.Meeting.Resp.Tent'
            else:
                message_class = 'IPM.Schedule.Meeting.Resp.Pos'
            self._respond('Accepted', message_class, comment)

    def decline(self, comment=None, respond=True):
        # TODO update appointment itself

        if respond:
            message_class = 'IPM.Schedule.Meeting.Resp.Neg'
            self._respond('Declined', message_class, comment)

    # TODO merge with meetingrequest version
    def _respond(self, subject_prefix, message_class, comment=None):
        response = self.copy(self.store.outbox)
        response.message_class = message_class

        response.subject = subject_prefix + ': ' + self.subject
        if comment:
            response.text = comment
        response.to = self.server.user(email=self.from_.email) # XXX
        response.from_ = self.store.user # XXX slow?

        response.send()

    def cancel(self):
        self[PidLidAppointmentStateFlags] |= ASF_CANCELED

    @property
    def canceled(self):
        return bool(self[PidLidAppointmentStateFlags] & ASF_CANCELED)

    @property
    def color(self): # property used by old clients
        nr = self.get(PidLidAppointmentColor, 0)
        if nr != 0:
            return NR_COLOR[nr]

    @color.setter
    def color(self, value):
        try:
            self[PidLidAppointmentColor] = COLOR_NR[value]
        except KeyError:
            raise ArgumentError('invalid color: %r' % value)
