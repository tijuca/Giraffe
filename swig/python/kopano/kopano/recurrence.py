"""
Part of the high-level python bindings for Kopano

Copyright 2005 - 2016 Zarafa and its licensors (see LICENSE file for details)
Copyright 2016 - Kopano and its licensors (see LICENSE file for details)
"""

import datetime
import libfreebusy
import struct
import sys
import time

from MAPI import (
    MAPI_UNICODE, KEEP_OPEN_READWRITE, MODRECIP_ADD, MODRECIP_MODIFY,
    MSGFLAG_READ, MSGFLAG_UNSENT, ATTACH_EMBEDDED_MSG,
)

from MAPI.Tags import (
    PR_MESSAGE_CLASS_W, PR_ATTACH_NUM, PR_ATTACH_FLAGS, PR_ATTACHMENT_FLAGS,
    PR_ATTACHMENT_HIDDEN, PR_ATTACH_METHOD, PR_DISPLAY_NAME_W,
    PR_EXCEPTION_STARTTIME, PR_EXCEPTION_ENDTIME, PR_HASATTACH,
    PR_NORMALIZED_SUBJECT_W, PR_ATTACHMENT_LINKID, PR_ICON_INDEX,
    PR_MESSAGE_RECIPIENTS, IID_IMAPITable, PR_RECIPIENT_FLAGS,
    PR_MESSAGE_FLAGS, PR_RECIPIENT_TRACKSTATUS, recipSendable,
    recipExceptionalResponse, recipExceptionalDeleted, recipOrganizer,
    respOrganized, respDeclined,
)

from MAPI.Defs import (
    HrGetOneProp, PpropFindProp,
)

from MAPI.Struct import (
    SPropValue,
)

from MAPI.Time import (
    unixtime,
)

from dateutil.rrule import (
    DAILY, WEEKLY, MONTHLY, MO, TU, TH, FR, WE, SA, SU, rrule, rruleset
)
from datetime import timedelta

from .compat import repr as _repr
from .errors import NotSupportedError
from .defs import (
    ARO_SUBJECT, ARO_MEETINGTYPE, ARO_REMINDERDELTA, ARO_REMINDERSET,
    ARO_LOCATION, ARO_BUSYSTATUS, ARO_ATTACHMENT, ARO_SUBTYPE,
    ARO_APPTCOLOR
)

if sys.hexversion >= 0x03000000:
    try:
        from . import utils as _utils
    except ImportError:
        _utils = sys.modules[__package__+'.utils']
    from . import meetingrequest as _meetingrequest
else:
    import utils as _utils
    import meetingrequest as _meetingrequest

SHORT, LONG = 2, 4

PidLidSideEffects = "PT_LONG:common:0x8510"
PidLidSmartNoAttach = "PT_BOOLEAN:common:0x8514"
PidLidReminderDelta = 'PT_LONG:common:0x8501'
PidLidReminderSet = "PT_BOOLEAN:common:0x8503"
PidLidReminderSignalTime = "PT_SYSTIME:common:0x8560"
PidLidReminderDelta = "PT_LONG:common:0x8501"
PidLidBusyStatus = "PT_LONG:appointment:0x8205"
PidLidExceptionReplaceTime = "PT_SYSTIME:appointment:0x8228"
PidLidAppointmentSubType = "PT_BOOLEAN:appointment:0x8215"
PidLidResponseStatus = "PT_LONG:appointment:0x8218"
PidLidTimeZoneStruct = "PT_BINARY:PSETID_Appointment:0x8233"
PidLidAppointmentRecur = "PT_BINARY:PSETID_Appointment:0x8216"
PidLidLocation = "PT_UNICODE:PSETID_Appointment:0x8208" # XXX PT_STRING8 doesn't work?
PidLidAppointmentSubType = "PT_BOOLEAN:PSETID_Appointment:0x8215"
PidLidAppointmentColor = "PT_LONG:PSETID_Appointment:0x8214"
PidLidIntendedBusyStatus = "PT_LONG:PSETID_Appointment:0x8224"
PidLidAppointmentStartWhole = "PT_SYSTIME:PSETID_Appointment:0x820D"
PidLidAppointmentEndWhole = "PT_SYSTIME:PSETID_Appointment:0x820E"

PATTERN_DAILY = 0
PATTERN_WEEKLY = 1
PATTERN_MONTHLY = 2
PATTERN_MONTHNTH = 3

# see MS-OXOCAL, section 2.2.1.44.5, "AppointmentRecurrencePattern Structure"

class Recurrence(object):
    def __init__(self, item):
        # XXX add check if we actually have a recurrence, otherwise we throw a mapi exception which might not be desirable

        self.item = item
        self.tz = item.get_value(PidLidTimeZoneStruct)
        self._parse()

    def _parse(self):
        # AppointmentRecurrencePattern
        value = self.item.prop(PidLidAppointmentRecur).value

        # RecurrencePattern
        self.reader_version = _utils.unpack_short(value, 0)
        self.writer_version = _utils.unpack_short(value, SHORT)

        self.recur_frequency = _utils.unpack_short(value, 2 * SHORT)
        self.pattern_type = _utils.unpack_short(value, 3 * SHORT)
        self.calendar_type = _utils.unpack_short(value, 4 * SHORT)
        self.first_datetime = _utils.unpack_long(value, 5 * SHORT)
        self.period = _utils.unpack_long(value, 5 * SHORT + LONG)
        self.sliding_flag = _utils.unpack_long(value, 5 * SHORT + 2 * LONG)

        pos = 5 * SHORT + 3 * LONG

        self.pattern_type_specific = []
        if self.pattern_type != 0:
            self.pattern_type_specific.append(_utils.unpack_long(value, pos))
            pos += LONG
            if self.pattern_type in (3, 0xB):
                self.pattern_type_specific.append(_utils.unpack_long(value, pos))
                pos += LONG

        self.end_type = _utils.unpack_long(value, pos)
        pos += LONG
        self.occurrence_count = _utils.unpack_long(value, pos)
        pos += LONG
        self.first_dow = _utils.unpack_long(value, pos)
        pos += LONG

        self.deleted_instance_count = _utils.unpack_long(value, pos)
        pos += LONG
        self.deleted_instance_dates = []
        for _ in range(0, self.deleted_instance_count):
            self.deleted_instance_dates.append(_utils.unpack_long(value, pos))
            pos += LONG

        self.modified_instance_count = _utils.unpack_long(value, pos)
        pos += LONG

        self.modified_instance_dates = []
        for _ in range(0, self.modified_instance_count):
            self.modified_instance_dates.append(_utils.unpack_long(value, pos))
            pos += LONG

        self.start_date = _utils.unpack_long(value, pos)
        pos += LONG
        self.end_date = _utils.unpack_long(value, pos)
        pos += LONG

        # AppointmentRecurrencePattern
        self.reader_version2 = _utils.unpack_long(value, pos)
        pos += LONG
        self.writer_version2 = _utils.unpack_long(value, pos)
        pos += LONG
        self.starttime_offset = _utils.unpack_long(value, pos)
        pos += LONG
        self.endtime_offset = _utils.unpack_long(value, pos)
        pos += LONG
        self.exception_count = _utils.unpack_short(value, pos)
        pos += SHORT

        # ExceptionInfo
        self.exceptions = []
        for i in range(0, self.modified_instance_count): # using modcount, as PHP seems to not update exception_count? equal according to docs
            exception = {}

            val = _utils.unpack_long(value, pos)
            exception['start_datetime'] = val
            pos += LONG
            val = _utils.unpack_long(value, pos)
            exception['end_datetime'] = val
            pos += LONG
            val = _utils.unpack_long(value, pos)
            exception['original_start_date'] = val
            pos += LONG
            exception['override_flags'] = _utils.unpack_short(value, pos)
            pos += SHORT

            # We have modified the subject
            if exception['override_flags'] & ARO_SUBJECT:
                subject_length = _utils.unpack_short(value, pos) # XXX: unused?
                pos += SHORT
                subject_length2 = _utils.unpack_short(value, pos)
                pos += SHORT
                exception['subject'] = value[pos:pos+subject_length2]
                pos += subject_length2

            if exception['override_flags'] & ARO_MEETINGTYPE:
                exception['meeting_type'] = _utils.unpack_long(value, pos)
                pos += LONG

            if exception['override_flags'] & ARO_REMINDERDELTA:
                exception['reminder_delta'] = _utils.unpack_long(value, pos) # XXX: datetime?
                pos += LONG

            if exception['override_flags'] & ARO_REMINDERSET:
                exception['reminder_set'] = _utils.unpack_long(value, pos) # XXX: bool?
                pos += LONG

            if exception['override_flags'] & ARO_LOCATION:
                location_length = _utils.unpack_short(value, pos) # XXX: unused?
                pos += SHORT
                location_length2 = _utils.unpack_short(value, pos)
                pos += SHORT
                exception['location'] = value[pos:pos+location_length2]
                pos += location_length2

            if exception['override_flags'] & ARO_BUSYSTATUS:
                exception['busy_status'] = _utils.unpack_long(value, pos)
                pos += LONG

            if exception['override_flags'] & ARO_ATTACHMENT:
                exception['attachment'] = _utils.unpack_long(value, pos)
                pos += LONG

            if exception['override_flags'] & ARO_SUBTYPE:
                exception['sub_type'] = _utils.unpack_long(value, pos)
                pos += LONG

            if exception['override_flags'] & ARO_APPTCOLOR:
                exception['appointment_color'] = _utils.unpack_long(value, pos)
                pos += LONG

            self.exceptions.append(exception)

        # ReservedBlock1Size
        pos += _utils.unpack_long(value, pos) + LONG

        # ExtendedException
        self.extended_exceptions = []
        for exception in self.exceptions:
            extended_exception = {}

            if self.writer_version2 >= 0x3009:
                change_highlight_size = _utils.unpack_long(value, pos)
                pos += LONG
                change_highlight_value = _utils.unpack_long(value, pos)
                extended_exception['change_highlight'] = change_highlight_value
                pos += change_highlight_size

            # Reserved
            pos += LONG

            if exception['override_flags'] & ARO_SUBJECT or exception['override_flags'] & ARO_LOCATION:
                extended_exception['start_datetime'] = _utils.unpack_long(value, pos)
                pos += LONG
                extended_exception['end_datetime'] = _utils.unpack_long(value, pos)
                pos += LONG
                extended_exception['original_start_date'] = _utils.unpack_long(value, pos)
                pos += LONG

            if exception['override_flags'] & ARO_SUBJECT:
                length = _utils.unpack_short(value, pos)
                pos += SHORT
                extended_exception['subject'] = value[pos:pos+2*length].decode('utf-16-le')
                pos += 2*length

            if exception['override_flags'] & ARO_LOCATION:
                length = _utils.unpack_short(value, pos)
                pos += SHORT
                extended_exception['location'] = value[pos:pos+2*length].decode('utf-16-le')
                pos += 2*length

            self.extended_exceptions.append(extended_exception)

    def _save(self):
        # AppointmentRecurrencePattern

        # RecurrencePattern
        data = struct.pack('<HHHHH', self.reader_version, self.writer_version,
            self.recur_frequency, self.pattern_type, self.calendar_type)

        data += struct.pack('<III', self.first_datetime, self.period, self.sliding_flag)

        for i in self.pattern_type_specific:
            data += struct.pack('<I', i)

        data += struct.pack('<I', self.end_type)

        # XXX check specs, php seems wrong
        if self.end_type == 0x2021: # stop after date
            occurrence_count = 0xa
        elif self.end_type == 0x2022: # stop after N occurrences
            occurrence_count = self.occurrence_count
        else:
            occurrence_count = 0
        data += struct.pack('<I', occurrence_count)

        data += struct.pack('<I', self.first_dow)

        data += struct.pack('<I', self.deleted_instance_count)
        for val in self.deleted_instance_dates:
            data += struct.pack('<I', val)

        data += struct.pack('<I', self.modified_instance_count)
        for val in self.modified_instance_dates:
            data += struct.pack('<I', val)

        data += struct.pack('<II', self.start_date, self.end_date)

        data += struct.pack('<II', 0x3006, 0x3008)
        data += struct.pack('<II', self.starttime_offset, self.endtime_offset)

        # ExceptionInfo
        data += struct.pack('<H', self.modified_instance_count)

        for exception in self.exceptions:
            data += struct.pack('<I', exception['start_datetime'])
            data += struct.pack('<I', exception['end_datetime'])
            data += struct.pack('<I', exception['original_start_date'])
            data += struct.pack('<H', exception['override_flags'])

            if exception['override_flags'] & ARO_SUBJECT:
                subject = exception['subject']
                data += struct.pack('<H', len(subject)+1)
                data += struct.pack('<H', len(subject))
                data += subject

            if exception['override_flags'] & ARO_MEETINGTYPE:
                data += struct.pack('<I', exception['meeting_type'])

            if exception['override_flags'] & ARO_REMINDERDELTA:
                data += struct.pack('<I', exception['reminder_delta'])

            if exception['override_flags'] & ARO_REMINDERSET:
                data += struct.pack('<I', exception['reminder_set'])

            if exception['override_flags'] & ARO_LOCATION:
                location = exception['location']
                data += struct.pack('<H', len(location)+1)
                data += struct.pack('<H', len(location))
                data += location

            if exception['override_flags'] & ARO_BUSYSTATUS:
                data += struct.pack('<I', exception['busy_status'])

            if exception['override_flags'] & ARO_ATTACHMENT:
                data += struct.pack('<I', exception['attachment'])

            if exception['override_flags'] & ARO_SUBTYPE:
                data += struct.pack('<I', exception['sub_type'])

            if exception['override_flags'] & ARO_APPTCOLOR:
                data += struct.pack('<I', exception['appointment_color'])

        # ReservedBlock1Size
        data += struct.pack('<I', 0)

        # ExtendedException
        for exception, extended_exception in zip(self.exceptions, self.extended_exceptions):
            data += struct.pack('<I', 0)

            overrideflags = exception['override_flags']

            if overrideflags & ARO_SUBJECT or overrideflags & ARO_LOCATION:
                data += struct.pack('<I', extended_exception['start_datetime'])
                data += struct.pack('<I', extended_exception['end_datetime'])
                data += struct.pack('<I', extended_exception['original_start_date'])

            if overrideflags & ARO_SUBJECT:
                subject = extended_exception['subject']
                data += struct.pack('<H', len(subject))
                data += subject.encode('utf-16-le')

            if overrideflags & ARO_LOCATION:
                location = extended_exception['location']
                data += struct.pack('<H', len(location))
                data += location.encode('utf-16-le')

            if overrideflags & ARO_SUBJECT or overrideflags & ARO_LOCATION:
                data += struct.pack('<I', 0)

        # ReservedBlock2Size
        data += struct.pack('<I', 0)

        self.item.set_value(PidLidAppointmentRecur, data)

    @property
    def _start(self):
        return datetime.datetime.fromtimestamp(_utils.rectime_to_unixtime(self.start_date)) + datetime.timedelta(minutes=self.starttime_offset) # XXX local time..

    @property
    def _end(self):
        return datetime.datetime.fromtimestamp(_utils.rectime_to_unixtime(self.end_date)) + datetime.timedelta(minutes=self.endtime_offset)# XXX local time..

    @property
    def _del_recurrences(self): # XXX local time..
        return [datetime.datetime.fromtimestamp(_utils.rectime_to_unixtime(val)) \
            for val in self.deleted_instance_dates]

    @property
    def _mod_recurrences(self): # XXX local time..
        return [datetime.datetime.fromtimestamp(_utils.rectime_to_unixtime(val)) \
            for val in self.modified_instance_dates]

    @property
    def recurrences(self):
        rrule_weekdays = {0: SU, 1: MO, 2: TU, 3: WE, 4: TH, 5: FR, 6: SA}
        rule = rruleset()

        if self.pattern_type == PATTERN_DAILY:
            rule.rrule(rrule(DAILY, dtstart=self._start, until=self._end, interval=self.period/(24*60)))

        if self.pattern_type == PATTERN_WEEKLY:
            byweekday = () # Set
            for index, week in rrule_weekdays.items():
                if (self.pattern_type_specific[0] >> index ) & 1:
                    byweekday += (week,)
            # FIXME: add one day, so that we don't miss the last recurrence, since the end date is for example 11-3-2015 on 1:00
            # But the recurrence is on 8:00 that day and we should include it.
            rule.rrule(rrule(WEEKLY, wkst = self._start.weekday(), dtstart=self._start, until=self._end + timedelta(days=1), byweekday=byweekday, interval=self.period))

        elif self.pattern_type == PATTERN_MONTHLY:
            # X Day of every Y month(s)
            # The Xnd Y (day) of every Z Month(s)
            rule.rrule(rrule(MONTHLY, dtstart=self._start, until=self._end, bymonthday=self.pattern_type_specific[0], interval=self.period))
            # self.pattern_type_specific[0] is either day of month or

        elif self.pattern_type == PATTERN_MONTHNTH:
            byweekday = () # Set
            for index, week in rrule_weekdays.items():
                if (self.pattern_type_specific[0] >> index ) & 1:
                    if self.pattern_type_specific[1] == 5:
                        byweekday += (week(-1),) # last week of month
                    else:
                        byweekday += (week(self.pattern_type_specific[1]),)
            # Yearly, the last XX of YY
            rule.rrule(rrule(MONTHLY, dtstart=self._start, until=self._end, interval=self.period, byweekday=byweekday))

        elif self.pattern_type != 0: # XXX check 0
            raise NotSupportedError('Unsupported recurrence pattern: %d' % self.pattern_type)

        # Remove deleted ocurrences
        for del_date in self._del_recurrences:
            del_date = datetime.datetime(del_date.year, del_date.month, del_date.day, self._start.hour, self._start.minute)
            if del_date not in self._mod_recurrences:
                rule.exdate(del_date)

        # add exceptions
        for exception in self.exceptions:
            rule.rdate(datetime.datetime.fromtimestamp(_utils.rectime_to_unixtime(exception['start_datetime'])))

        return rule

    def exception_message(self, basedate):
        for message in self.item.items():
            replacetime = message.get_value(PidLidExceptionReplaceTime)
            if replacetime and replacetime.date() == basedate.date():
                return message

    def is_exception(self, basedate):
        return self.exception_message(basedate) is not None

    def _update_exceptions(self, cal_item, item, startdate_val, enddate_val, basedate_val, exception, extended_exception, copytags, create=False): # XXX kill copytags, create args, just pass all properties as in php
        exception['start_datetime'] = startdate_val
        exception['end_datetime'] = enddate_val
        exception['original_start_date'] = basedate_val
        exception['override_flags'] = 0

        extended = False
        subject = item.get_value(PR_NORMALIZED_SUBJECT_W)
        if subject is not None:
            exception['override_flags'] |= ARO_SUBJECT
            exception['subject'] = subject.encode('cp1252', 'replace')
            extended = True
            extended_exception['subject'] = subject

        # skip ARO_MEETINGTYPE (like php)

        reminder_delta = item.get_value(PidLidReminderDelta)
        if reminder_delta is not None:
            exception['override_flags'] |= ARO_REMINDERDELTA
            exception['reminder_delta'] = reminder_delta

        reminder_set = item.get_value(PidLidReminderSet)
        if reminder_set is not None:
            exception['override_flags'] |= ARO_REMINDERSET
            exception['reminder_set'] = reminder_set

        location = item.get_value(PidLidLocation)
        if location is not None:
            exception['override_flags'] |= ARO_LOCATION
            exception['location'] = location.encode('cp1252', 'replace')
            extended = True
            extended_exception['location'] = location

        busy_status = item.get_value(PidLidBusyStatus)
        if busy_status is not None:
            exception['override_flags'] |= ARO_BUSYSTATUS
            exception['busy_status'] = busy_status

        # skip ARO_ATTACHMENT (like php)

        # XXX php doesn't set the following by accident? ("alldayevent" not in copytags..)
        if not copytags or not create:
            sub_type = item.get_value(PidLidAppointmentSubType)
            if sub_type is not None:
                exception['override_flags'] |= ARO_SUBTYPE
                exception['sub_type'] = sub_type

        appointment_color = item.get_value(PidLidAppointmentColor)
        if appointment_color is not None:
            exception['override_flags'] |= ARO_APPTCOLOR
            exception['appointment_color'] = appointment_color

        if extended:
            extended_exception['start_datetime'] = startdate_val
            extended_exception['end_datetime'] = enddate_val
            extended_exception['original_start_date'] = basedate_val

    def _update_calitem(self, item):
        cal_item = self.item

        cal_item.set_value(PidLidSideEffects, 3441) # XXX spec, check php
        cal_item.set_value(PidLidSmartNoAttach, True)

        # reminder
        if cal_item.get_value(PidLidReminderSet) and cal_item.get_value(PidLidReminderDelta):
            occs = list(cal_item.occurrences(datetime.datetime.now(), datetime.datetime(2038,1,1))) # XXX slow for daily?
            occs.sort(key=lambda occ: occ.start)
            for occ in occs: # XXX check default/reminder props
                dueby = occ.start - datetime.timedelta(minutes=cal_item.get_value(PidLidReminderDelta))
                if dueby > datetime.datetime.now():
                    cal_item.set_value(PidLidReminderSignalTime, dueby)
                    break
            else:
                cal_item.prop(PidLidReminderSet).value = False
                cal_item.prop(PidLidReminderSignalTime).value = datetime.datetime.fromtimestamp(0x7ff00000)

    def _update_embedded(self, basedate, message, item, copytags=None, create=False):
        basetime = basedate + datetime.timedelta(minutes=self.starttime_offset)
        cal_item = self.item

        if copytags:
            props = item.mapiobj.GetProps(copytags, 0)
        else: # XXX remove?
            props = [p.mapiobj for p in item.props() if p.proptag != PR_ICON_INDEX]

        message.mapiobj.SetProps(props)

        message.set_value(PR_MESSAGE_CLASS_W, u'IPM.OLE.CLASS.{00061055-0000-0000-C000-000000000046}')
        message.set_value(PidLidExceptionReplaceTime, basetime)

        intended_busystatus = cal_item.get_value(PidLidIntendedBusyStatus) # XXX tentative? merge with modify_exc?
        if intended_busystatus is not None:
            message.set_value(PidLidBusyStatus, intended_busystatus)

        sub_type = cal_item.get_value(PidLidAppointmentSubType)
        if sub_type is not None:
            message.set_value(PidLidAppointmentSubType, sub_type)

        props = [
            SPropValue(PR_ATTACHMENT_FLAGS, 2), # XXX cannot find spec
            SPropValue(PR_ATTACHMENT_HIDDEN, True),
            SPropValue(PR_ATTACHMENT_LINKID, 0),
            SPropValue(PR_ATTACH_FLAGS, 0),
            SPropValue(PR_ATTACH_METHOD, ATTACH_EMBEDDED_MSG),
            SPropValue(PR_DISPLAY_NAME_W, u'Exception'),
        ]

        start = message.get_value(PidLidAppointmentStartWhole)
        if start is not None:
            start_local = unixtime(time.mktime(_utils._from_gmt(start, self.tz).timetuple())) # XXX why local??
            props.append(SPropValue(PR_EXCEPTION_STARTTIME, start_local))

        end = message.prop(PidLidAppointmentEndWhole).value
        if end is not None:
            end_local = unixtime(time.mktime(_utils._from_gmt(end, self.tz).timetuple())) # XXX why local??
            props.append(SPropValue(PR_EXCEPTION_ENDTIME, end_local))

        message._attobj.SetProps(props)
        if not create: # XXX php bug?
            props = props[:-2]
        message.mapiobj.SetProps(props)

        message.mapiobj.SaveChanges(KEEP_OPEN_READWRITE)
        message._attobj.SaveChanges(KEEP_OPEN_READWRITE)

    def create_exception(self, basedate, item, copytags=None, merge=False):
        tz = item.get_value(PidLidTimeZoneStruct)
        cal_item = self.item

        # create embedded item
        message_flags = MSGFLAG_READ
        if item.get_value(PR_MESSAGE_FLAGS) == 0: # XXX wut/php compat
            message_flags |= MSGFLAG_UNSENT
        message = cal_item.create_item(message_flags)

        self._update_embedded(basedate, message, item, copytags, create=True)

        message.set_value(PidLidResponseStatus, respDeclined | respOrganized) # XXX php bug for merge case?
        if copytags:
            message.set_value(PidLidBusyStatus, 0)

        # copy over recipients (XXX check php delta stuff..)
        table = item.mapiobj.OpenProperty(PR_MESSAGE_RECIPIENTS, IID_IMAPITable, MAPI_UNICODE, 0)
        table.SetColumns(_meetingrequest.RECIP_PROPS, 0)
        recips = list(table.QueryRows(-1, 0))

        for recip in recips:
            flags = PpropFindProp(recip, PR_RECIPIENT_FLAGS)
            if not flags:
                recip.append(SPropValue(PR_RECIPIENT_FLAGS, recipExceptionalResponse | recipSendable))

        if copytags:
            for recip in recips:
                recip.append(SPropValue(PR_RECIPIENT_FLAGS, recipExceptionalDeleted | recipSendable))
                recip.append(SPropValue(PR_RECIPIENT_TRACKSTATUS, 0))

        organiser = _meetingrequest._organizer_props(message, item)
        if organiser and not merge: # XXX merge -> initialize?
            recips.insert(0, organiser)

        message.mapiobj.ModifyRecipients(MODRECIP_ADD, recips)

        message.mapiobj.SaveChanges(KEEP_OPEN_READWRITE)
        message._attobj.SaveChanges(KEEP_OPEN_READWRITE)
        cal_item.mapiobj.SaveChanges(KEEP_OPEN_READWRITE)

        # XXX attachments?

        # update blob
        self.deleted_instance_count += 1
        deldate = _utils._from_gmt(basedate, tz)
        deldate_val = _utils.unixtime_to_rectime(time.mktime(deldate.timetuple()))
        self.deleted_instance_dates.append(deldate_val)
        self.deleted_instance_dates.sort()

        self.modified_instance_count += 1
        moddate = message.prop(PidLidAppointmentStartWhole).value
        daystart = moddate - datetime.timedelta(hours=moddate.hour, minutes=moddate.minute) # XXX different approach in php? seconds?
        localdaystart = _utils._from_gmt(daystart, tz)
        moddate_val = _utils.unixtime_to_rectime(time.mktime(localdaystart.timetuple()))
        self.modified_instance_dates.append(moddate_val)
        self.modified_instance_dates.sort()

        startdate = _utils._from_gmt(message.prop(PidLidAppointmentStartWhole).value, tz)
        startdate_val = _utils.unixtime_to_rectime(time.mktime(startdate.timetuple()))

        enddate = _utils._from_gmt(message.prop(PidLidAppointmentEndWhole).value, tz)
        enddate_val = _utils.unixtime_to_rectime(time.mktime(enddate.timetuple()))

        exception = {}
        extended_exception = {}
        self._update_exceptions(cal_item, message, startdate_val, enddate_val, deldate_val, exception, extended_exception, copytags, create=True)
        self.exceptions.append(exception) # no evidence of sorting
        self.extended_exceptions.append(extended_exception)

        self._save()

        # update calitem
        self._update_calitem(item)

    def modify_exception(self, basedate, item, copytags=None): # XXX 'item' too MR specific
        tz = item.get_value(PidLidTimeZoneStruct)
        cal_item = self.item

        # update embedded item
        for message in cal_item.items(): # XXX no cal_item? to helper
            replacetime = message.get_value(PidLidExceptionReplaceTime)
            if replacetime and replacetime.date() == basedate.date():
                self._update_embedded(basedate, message, item, copytags)

                icon_index = item.get_value(PR_ICON_INDEX)
                if not copytags and icon_index is not None:
                    message.set_value(PR_ICON_INDEX, icon_index)

                message._attobj.SaveChanges(KEEP_OPEN_READWRITE)
                break
        else:
            return # XXX exception

        if copytags: # XXX bug in php code? (setallrecipients, !empty..)
            message.set_value(PidLidBusyStatus, 0)

            table = message.mapiobj.OpenProperty(PR_MESSAGE_RECIPIENTS, IID_IMAPITable, MAPI_UNICODE, 0)
            table.SetColumns(_meetingrequest.RECIP_PROPS, 0)

            recips = list(table.QueryRows(-1, 0))
            for recip in recips:
                flags = PpropFindProp(recip, PR_RECIPIENT_FLAGS)
                if flags and flags.Value != (recipOrganizer | recipSendable):
                    flags.Value = recipExceptionalDeleted | recipSendable
                    trackstatus = PpropFindProp(recip, PR_RECIPIENT_TRACKSTATUS)
                    recip.append(SPropValue(PR_RECIPIENT_TRACKSTATUS, 0))

            message.mapiobj.ModifyRecipients(MODRECIP_MODIFY, recips)

            message.mapiobj.SaveChanges(KEEP_OPEN_READWRITE)
            message._attobj.SaveChanges(KEEP_OPEN_READWRITE)
            cal_item.mapiobj.SaveChanges(KEEP_OPEN_READWRITE)

        # update blob
        basedate_val = _utils.unixtime_to_rectime(time.mktime(_utils._from_gmt(basedate, tz).timetuple()))

        startdate = _utils._from_gmt(message.prop(PidLidAppointmentStartWhole).value, tz)
        startdate_val = _utils.unixtime_to_rectime(time.mktime(startdate.timetuple()))

        enddate = _utils._from_gmt(message.prop(PidLidAppointmentEndWhole).value, tz)
        enddate_val = _utils.unixtime_to_rectime(time.mktime(enddate.timetuple()))

        for i, exception in enumerate(self.exceptions):
            if exception['original_start_date'] == basedate_val:
                current_startdate_val = exception['start_datetime'] - self.starttime_offset

                for j, val in enumerate(self.modified_instance_dates):
                    if val == current_startdate_val:
                        self.modified_instance_dates[j] = startdate_val - self.starttime_offset
                        self.modified_instance_dates.sort()
                        break

                extended_exception = self.extended_exceptions[i]
                self._update_exceptions(cal_item, message, startdate_val, enddate_val, basedate_val, exception, extended_exception, copytags, create=False)

        self._save()

        # update calitem
        self._update_calitem(item)

    def delete_exception(self, basedate, item, copytags):
        tz = item.get_value(PidLidTimeZoneStruct)

        basedate2 = _utils._from_gmt(basedate, tz)
        basedate_val = _utils.unixtime_to_rectime(time.mktime(basedate2.timetuple()))

        if self.is_exception(basedate):
            self.modify_exception(basedate, item, copytags)

            for i, exc in enumerate(self.exceptions):
                if exc['original_start_date'] == basedate_val:
                    break

            self.modified_instance_count -= 1
            self.modified_instance_dates = [m for m in self.modified_instance_dates if m != exc['start_datetime']]

            del self.exceptions[i]
            del self.extended_exceptions[i]

        else:
            self.deleted_instance_count += 1

            self.deleted_instance_dates.append(basedate_val)
            self.deleted_instance_dates.sort()

        self._save()
        self._update_calitem(item)

    def occurrences(self, start=None, end=None): # XXX fit-to-period
        tz = self.item.get_value(PidLidTimeZoneStruct)

        recurrences = self.recurrences
        if start and end:
            recurrences = recurrences.between(_utils._from_gmt(start, tz), _utils._from_gmt(end, tz))

        start_end = {}
        for exc in self.exceptions:
            start_end[exc['start_datetime']] = exc['end_datetime']

        for d in recurrences:
            startdatetime_val = _utils.unixtime_to_rectime(time.mktime(d.timetuple()))

            if startdatetime_val in start_end:
                minutes = start_end[startdatetime_val] - startdatetime_val
            else:
                minutes = self.endtime_offset - self.starttime_offset

            d = _utils._to_gmt(d, tz)

            occ = Occurrence(self.item, d, d + datetime.timedelta(minutes=minutes))
            if (not start or occ.end > start) and (not end or occ.start < end):
                yield occ

    def __unicode__(self):
        return u'Recurrence(start=%s - end=%s)' % (self.start, self.end)

    def __repr__(self):
        return _repr(self)


class Occurrence(object):
    def __init__(self, item, start, end): # XXX make sure all GMT?
        self.item = item
        self.start = start
        self.end = end

    def __getattr__(self, x):
        return getattr(self.item, x)

    def __unicode__(self):
        return u'Occurrence(%s)' % self.subject

    def __repr__(self):
        return _repr(self)
