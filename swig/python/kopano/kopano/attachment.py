"""
Part of the high-level python bindings for Kopano

Copyright 2005 - 2016 Zarafa and its licensors (see LICENSE file)
Copyright 2016 - Kopano and its licensors (see LICENSE file)
"""

import sys

from MAPI.Tags import (
    PR_EC_HIERARCHYID, PR_ATTACH_NUM, PR_ATTACH_MIME_TAG_W,
    PR_ATTACH_LONG_FILENAME_W, PR_ATTACH_SIZE, PR_ATTACH_DATA_BIN,
    IID_IAttachment
)
from MAPI.Defs import HrGetOneProp
from MAPI.Struct import MAPIErrorNotFound

from .properties import Properties

if sys.hexversion >= 0x03000000:
    try:
        from . import utils as _utils
    except ImportError:
        _utils = sys.modules[__package__+'.utils']
else:
    import utils as _utils

class Attachment(Properties):
    """Attachment class"""

    def __init__(self, mapiitem=None, entryid=None, mapiobj=None):
        self._mapiitem = mapiitem
        self._entryid = entryid
        self._mapiobj = mapiobj
        self._data = None

    @property
    def mapiobj(self):
        if self._mapiobj:
            return self._mapiobj

        self._mapiobj = self._mapiitem.OpenAttach(
            self._entryid, IID_IAttachment, 0
        )
        return self._mapiobj

    @mapiobj.setter
    def mapiobj(self, mapiobj):
        self._mapiobj = mapiobj

    @property
    def hierarchyid(self):
        return self.prop(PR_EC_HIERARCHYID).value

    @property
    def number(self):
        try:
            return HrGetOneProp(self.mapiobj, PR_ATTACH_NUM).Value
        except MAPIErrorNotFound:
            return 0

    @property
    def mimetype(self):
        """Mime-type"""
        try:
            return HrGetOneProp(self.mapiobj, PR_ATTACH_MIME_TAG_W).Value
        except MAPIErrorNotFound:
            return u''

    @property
    def filename(self):
        """Filename"""
        try:
            return HrGetOneProp(self.mapiobj, PR_ATTACH_LONG_FILENAME_W).Value
        except MAPIErrorNotFound:
            return u''

    @property
    def size(self):
        """Size"""
        # XXX size of the attachment object, so more than just the attachment
        #     data
        # XXX (useful when calculating store size, for example.. sounds
        #     interesting to fix here)
        try:
            return int(HrGetOneProp(self.mapiobj, PR_ATTACH_SIZE).Value)
        except MAPIErrorNotFound:
            return 0 # XXX

    def __len__(self):
        return self.size

    @property
    def data(self):
        """Binary data"""
        if self._data is None:
            self._data = _utils.stream(self.mapiobj, PR_ATTACH_DATA_BIN)
        return self._data

    # file-like behaviour
    def read(self):
        return self.data

    @property
    def name(self):
        return self.filename

    def __unicode__(self):
        return u'Attachment("%s")' % self.name
