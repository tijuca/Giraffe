#!/usr/bin/env python
import binascii
import csv
import datetime
from contextlib import closing
import fcntl
try:
    import cPickle as pickle
except ImportError:
    import _pickle as pickle

try:
    import dbhash
except ImportError:
    import dbm as dbhash

import shutil
from multiprocessing import Queue
import os.path
import sys
import time
from xml.etree import ElementTree
import zlib

from MAPI.Util import *

import kopano
from kopano import log_exc

"""
kopano-backup - a MAPI-level backup/restore tool built on python-kopano.

backup is done incrementally using ICS and can be parallellized over stores.

restore is not parallelized.

items are serialized and maintained in per-folder key-value stores.

metadata such as webapp settings, rules, acls and delegation permissions are also stored per-folder.

basic commands (see --help for all options):

kopano-backup -u user1 -> backup (sync) data for user 'user1' in (new) directory 'user1'
kopano-backup -u user1 -f Inbox -> backup 'Inbox' folder for user 'user1' in (new) directory 'user1'

kopano-backup --restore user1 -> restore data from directory 'user1' to account called 'user1'
kopano-backup --restore -u user2 user1 -> same, but restore to account 'user2'

kopano-backup --stats user1 -> summarize contents of backup directory 'user1', in CSV format
kopano-backup --index user1 -> low-level overview of stored items, in CSV format

options can be combined when this makes sense, for example:

kopano-backup --index user1 -f Inbox/subfolder --recursive --period-begin 2014-01-01

"""

CACHE_SIZE = 64000000 # XXX make configurable

def dbopen(path): # XXX unfortunately dbhash.open doesn't seem to accept unicode
    return dbhash.open(path.encode(sys.stdout.encoding or 'utf8'), 'c')

def _decode(s):
    return s.decode(sys.stdout.encoding or 'utf8')

class BackupWorker(kopano.Worker):
    """ each worker takes stores from a queue, and backs them up to disk (or syncs them),
        according to the given command-line options; it also detects deleted folders """

    def main(self):
        config, server, options = self.service.config, self.service.server, self.service.options
        while True:
            stats = {'changes': 0, 'deletes': 0, 'errors': 0}
            self.service.stats = stats # XXX generalize
            with log_exc(self.log, stats):
                # get store from input queue
                (store_entryid, username, path) = self.iqueue.get()
                store = server.store(entryid=store_entryid)
                user = store.user

                # create main directory and lock it
                if not os.path.isdir(path):
                    os.makedirs(path)

                with open(path+'/lock', 'w') as lockfile:
                    fcntl.flock(lockfile.fileno(), fcntl.LOCK_EX)

                    # backup user and store properties
                    if not options.folders:
                        file(path+'/store', 'w').write(dump_props(store.props(), stats, self.log))
                        if user:
                            file(path+'/user', 'w').write(dump_props(user.props(), stats, self.log))
                            if not options.skip_meta:
                                file(path+'/delegates', 'w').write(dump_delegates(user, server, stats, self.log))

                    # check command-line options and backup folders
                    t0 = time.time()
                    self.log.info('backing up: %s', path)
                    paths = set()
                    folders = list(store.folders())
                    if options.recursive:
                        folders = sum([[f]+list(f.folders()) for f in folders], [])
                    for folder in folders:
                        if (not store.public and \
                            ((options.skip_junk and folder == store.junk) or \
                             (options.skip_deleted and folder == store.wastebasket))):
                            continue
                        paths.add(folder.path)
                        self.backup_folder(path, folder, store.subtree, config, options, stats, user, server)

                    # timestamp deleted folders
                    if not options.folders:
                        path_folder = folder_struct(path, options)
                        for fpath in set(path_folder) - paths:
                            with closing(dbopen(path_folder[fpath]+'/index')) as db_index:
                                idx = db_index.get('folder')
                                d = pickle.loads(idx) if idx else {}
                                if not d.get('backup_deleted'):
                                    self.log.info('deleted folder: %s', fpath)
                                    d['backup_deleted'] = self.service.timestamp
                                    db_index['folder'] = pickle.dumps(d)

                    changes = stats['changes'] + stats['deletes']
                    self.log.info('backing up %s took %.2f seconds (%d changes, ~%.2f/sec, %d errors)',
                        path, time.time()-t0, changes, changes/(time.time()-t0), stats['errors'])

            # return statistics in output queue
            self.oqueue.put(stats)

    def backup_folder(self, path, folder, subtree, config, options, stats, user, server):
        """ backup single folder """

        self.log.info('backing up folder: %s', folder.path)

        # create directory for subfolders
        data_path = path+'/'+folder_path(folder, subtree)
        if not os.path.isdir('%s/folders' % data_path):
            os.makedirs('%s/folders' % data_path)

        # backup folder properties, path, metadata
        file(data_path+'/path', 'w').write(folder.path.encode('utf8'))
        file(data_path+'/folder', 'w').write(dump_props(folder.props(), stats, self.log))
        if not options.skip_meta:
            file(data_path+'/acl', 'w').write(dump_acl(folder, user, server, stats, self.log))
            file(data_path+'/rules', 'w').write(dump_rules(folder, user, server, stats, self.log))
        if options.only_meta:
            return

        # sync over ICS, using stored 'state'
        importer = FolderImporter(folder, data_path, config, options, self.log, stats, self.service)
        statepath = '%s/state' % data_path
        state = None
        if os.path.exists(statepath):
            state = file(statepath).read()
            self.log.info('found previous folder sync state: %s', state)
        new_state = folder.sync(importer, state, log=self.log, stats=stats, begin=options.period_begin, end=options.period_end)
        if new_state != state:
            importer.commit()
            file(statepath, 'w').write(new_state)
            self.log.info('saved folder sync state: %s', new_state)

class FolderImporter:
    """ tracks changes for a given folder """

    def __init__(self, *args):
        self.folder, self.folder_path, self.config, self.options, self.log, self.stats, self.service = args
        self.reset_cache()

    def reset_cache(self):
        self.item_updates = []
        self.index_updates = []
        self.cache_size = 0

    def update(self, item, flags):
        """ store updated item in 'items' database, and subject and date in 'index' database """

        with log_exc(self.log, self.stats):
            self.log.debug('folder %s: new/updated document with entryid %s, sourcekey %s', self.folder.sourcekey, item.entryid, item.sourcekey)

            data = zlib.compress(item.dumps(attachments=not self.options.skip_attachments, archiver=False, skip_broken=True))
            self.item_updates.append((item.sourcekey, data))

            orig_prop = item.get_prop(PR_EC_BACKUP_SOURCE_KEY)
            if orig_prop:
                orig_prop = orig_prop.value.encode('hex').upper()
            idx = pickle.dumps({
                'subject': item.subject,
                'orig_sourcekey': orig_prop,
                'last_modified': item.last_modified,
                'backup_updated': self.service.timestamp,
            })
            self.index_updates.append((item.sourcekey, idx))

            self.stats['changes'] += 1

            self.cache_size += len(data) + len(idx)
            if self.cache_size > CACHE_SIZE:
                self.commit()

    def delete(self, item, flags): # XXX batch as well, 'updating' cache?
        """ deleted item from 'items' and 'index' databases """

        with log_exc(self.log, self.stats):
            with closing(dbopen(self.folder_path+'/items')) as db_items:
                with closing(dbopen(self.folder_path+'/index')) as db_index:
                    if item.sourcekey in db_items: # ICS can generate delete events without update events..
                        self.log.debug('folder %s: deleted document with sourcekey %s', self.folder.sourcekey, item.sourcekey)
                        if self.options.deletes in (None, 'yes'):
                            idx = pickle.loads(db_index[item.sourcekey])
                            idx['backup_deleted'] = self.service.timestamp
                            db_index[item.sourcekey] = pickle.dumps(idx)
                        else:
                            del db_items[item.sourcekey]
                            del db_index[item.sourcekey]
                        self.stats['deletes'] += 1

    def commit(self):
        """ commit data to storage """

        t0 = time.time()
        with closing(dbopen(self.folder_path+'/items')) as item_db:
            with closing(dbopen(self.folder_path+'/index')) as index_db:
                for sourcekey, data in self.item_updates:
                    item_db[sourcekey] = data
                for sourcekey, idx in self.index_updates:
                    index_db[sourcekey] = idx

        self.log.debug('commit took %.2f seconds (%d items)', time.time()-t0, len(self.item_updates))
        self.reset_cache()

class Service(kopano.Service):
    """ main backup process """

    def main(self):
        self.timestamp = datetime.datetime.now()

        if self.options.restore or self.options.purge:
            data_path = _decode(self.args[0].rstrip('/'))
            with open(data_path+'/lock', 'w') as lockfile:
                fcntl.flock(lockfile.fileno(), fcntl.LOCK_EX)

                if self.options.restore:
                    self.restore(data_path)
                elif self.options.purge:
                    self.purge(data_path)
        else:
            self.backup()

    def backup(self):
        """ create backup workers, determine which stores to queue, start the workers, display statistics """

        self.iqueue, self.oqueue = Queue(), Queue()
        workers = [BackupWorker(self, 'backup%d'%i, nr=i, iqueue=self.iqueue, oqueue=self.oqueue)
                       for i in range(self.config['worker_processes'])]
        for worker in workers:
            worker.start()
        jobs = self.create_jobs()
        for job in jobs:
            self.iqueue.put(job)
        self.log.info('queued %d store(s) for parallel backup (%s processes)', len(jobs), len(workers))
        t0 = time.time()
        stats = [self.oqueue.get() for i in range(len(jobs))] # blocking
        changes = sum(s['changes'] + s['deletes'] for s in stats)
        errors = sum(s['errors'] for s in stats)
        self.log.info('queue processed in %.2f seconds (%d changes, ~%.2f/sec, %d errors)',
            (time.time()-t0), changes, changes/(time.time()-t0), errors)

    def restore(self, data_path):
        """ restore data from backup """

        # determine store to restore to
        self.data_path = data_path # XXX remove var
        self.log.info('starting restore of %s', self.data_path)
        username = os.path.split(self.data_path)[1]
        if self.options.users:
            store = self._store(self.options.users[0])
        elif self.options.stores:
            store = self.server.store(self.options.stores[0])
        else:
            store = self._store(username)
        user = store.user

        # start restore
        self.log.info('restoring to store %s', store.entryid)
        t0 = time.time()
        stats = {'changes': 0, 'errors': 0}

        # restore metadata (webapp/mapi settings)
        if user and not self.options.folders and not self.options.skip_meta:
            if os.path.exists('%s/store' % self.data_path):
                storeprops = pickle.loads(file('%s/store' % self.data_path).read())
                for proptag in (PR_EC_WEBACCESS_SETTINGS_JSON, PR_EC_OUTOFOFFICE_SUBJECT, PR_EC_OUTOFOFFICE_MSG,
                                PR_EC_OUTOFOFFICE, PR_EC_OUTOFOFFICE_FROM, PR_EC_OUTOFOFFICE_UNTIL):
                    if PROP_TYPE(proptag) == PT_TSTRING:
                        proptag = CHANGE_PROP_TYPE(proptag, PT_UNICODE)
                    value = storeprops.get(proptag)
                    if value:
                        store.mapiobj.SetProps([SPropValue(proptag, value)])
                store.mapiobj.SaveChanges(KEEP_OPEN_READWRITE)
            if os.path.exists('%s/delegates' % self.data_path):
                load_delegates(user, self.server, file('%s/delegates' % self.data_path).read(), stats, self.log)

        # determine stored and specified folders
        path_folder = folder_struct(self.data_path, self.options)
        paths = self.options.folders or sorted(path_folder.keys())
        if self.options.recursive:
            paths = [path2 for path2 in path_folder for path in paths if (path2+'//').startswith(path+'/')]

        # restore specified folders
        restored = []
        for path in paths:
            if path not in path_folder:
                self.log.error('no such folder: %s', path)
                stats['errors'] += 1
            else:
                # handle --restore-root, filter and start restore
                restore_path = _decode(self.options.restore_root)+'/'+path if self.options.restore_root else path
                folder = store.subtree.folder(restore_path, create=True)
                if (not store.public and \
                    ((self.options.skip_junk and folder == store.junk) or \
                    (self.options.skip_deleted and folder == store.wastebasket))):
                        continue
                data_path = path_folder[path]

                if self.options.deletes in (None, 'no') and folder_deleted(data_path):
                    continue

                if not self.options.only_meta:
                    self.restore_folder(folder, path, data_path, store, store.subtree, stats, user, self.server)
                restored.append((folder, data_path))

        # restore metadata
        if not (self.options.sourcekeys or self.options.skip_meta):
            self.log.info('restoring metadata')
            for (folder, data_path) in restored:
                load_acl(folder, user, self.server, file(data_path+'/acl').read(), stats, self.log)
                load_rules(folder, user, self.server, file(data_path+'/rules').read(), stats, self.log)

        self.log.info('restore completed in %.2f seconds (%d changes, ~%.2f/sec, %d errors)',
            time.time()-t0, stats['changes'], stats['changes']/(time.time()-t0), stats['errors'])

    def purge(self, data_path):
        """ permanently delete old folders/items from backup """

        assert not self.options.folders, 'cannot combine --folder with --purge'

        stats = {'folders': 0, 'items': 0}
        self.data_path = data_path # XXX remove var
        path_folder = folder_struct(self.data_path, self.options)

        for path, data_path in path_folder.items():
            # check if folder was deleted
            self.log.info('checking folder: %s', path)
            if folder_deleted(data_path):
                if (self.timestamp - folder_deleted(data_path)).days > self.options.purge:
                    self.log.debug('purging folder')
                    shutil.rmtree(data_path)
                    stats['folders'] += 1
            else: # check all items for deletion
                with closing(dbopen(data_path+'/items')) as db_items:
                    with closing(dbopen(data_path+'/index')) as db_index:
                        for item, idx in db_index.items():
                            d = pickle.loads(idx)
                            backup_deleted = d.get('backup_deleted')
                            if backup_deleted and (self.timestamp - backup_deleted).days > self.options.purge:
                                self.log.debug('purging item: %s', item)
                                stats['items'] += 1
                                del db_items[item]
                                del db_index[item]

        self.log.info('purged %d folders and %d items', stats['folders'], stats['items'])

    def create_jobs(self):
        """ check command-line options and determine which stores should be backed up """

        output_dir = self.options.output_dir or u''
        jobs = []

        # specified companies/all users
        if self.options.companies or not (self.options.users or self.options.stores):
            for company in self.server.companies():
                companyname = company.name if company.name != 'Default' else ''
                for user in company.users():
                    if user.store:
                        jobs.append((user.store, user.name, os.path.join(output_dir, companyname, user.name)))
                if company.public_store and not self.options.skip_public:
                    target = 'public@'+companyname if companyname else 'public'
                    jobs.append((company.public_store, None, os.path.join(output_dir, companyname, target)))

        # specified users
        if self.options.users:
            for user in self.server.users():
                if user.store:
                    jobs.append((user.store, user.name, os.path.join(output_dir, user.name)))

        # specified stores
        if self.options.stores:
            for store in self.server.stores():
                if store.public:
                    target = 'public' + ('@'+store.company.name if store.company.name != 'Default' else '')
                else:
                    target = store.entryid
                jobs.append((store, None, os.path.join(output_dir, target)))

        return [(job[0].entryid,)+job[1:] for job in sorted(jobs, reverse=True, key=lambda x: x[0].size)]

    def restore_folder(self, folder, path, data_path, store, subtree, stats, user, server):
        """ restore single folder (or item in folder) """

        # check --sourcekey option (only restore specified item if it exists)
        if self.options.sourcekeys:
            with closing(dbopen(data_path+'/items')) as db:
                if not [sk for sk in self.options.sourcekeys if sk in db]:
                    return
        else:
            self.log.info('restoring folder %s', path)

            # restore container class
            folderprops = pickle.loads(file('%s/folder' % data_path).read())
            container_class = folderprops.get(long(PR_CONTAINER_CLASS_W))
            if container_class:
                folder.container_class = container_class

        # load existing sourcekeys in folder, to check for duplicates
        existing = set()
        table = folder.mapiobj.GetContentsTable(0)
        table.SetColumns([PR_SOURCE_KEY, PR_EC_BACKUP_SOURCE_KEY], 0)
        for row in table.QueryRows(-1, 0):
            if PROP_TYPE(row[1].ulPropTag) != PT_ERROR:
                existing.add(row[1].Value.encode('hex').upper())
            else:
                existing.add(row[0].Value.encode('hex').upper())

        # load entry from 'index', so we don't have to unpickle everything
        with closing(dbopen(data_path+'/index')) as db:
            index = dict((a, pickle.loads(b)) for (a,b) in db.iteritems())

        # now dive into 'items', and restore desired items
        with closing(dbopen(data_path+'/items')) as db:
            # determine sourcekey(s) to restore
            sourcekeys = db.keys()
            if self.options.sourcekeys:
                sourcekeys = [sk for sk in sourcekeys if sk in self.options.sourcekeys]

            for sourcekey2 in sourcekeys:
                with log_exc(self.log, stats):
                    # date check against 'index'
                    last_modified = index[sourcekey2]['last_modified']
                    if ((self.options.period_begin and last_modified < self.options.period_begin) or
                        (self.options.period_end and last_modified >= self.options.period_end) or
                        (index[sourcekey2].get('backup_deleted') and self.options.deletes in (None, 'no'))):
                        continue

                    # check for duplicates
                    if sourcekey2 in existing or index[sourcekey2]['orig_sourcekey'] in existing:
                        self.log.warning('skipping duplicate item with sourcekey %s', sourcekey2)
                    else:
                        # actually restore item
                        self.log.debug('restoring item with sourcekey %s', sourcekey2)
                        item = folder.create_item(loads=zlib.decompress(db[sourcekey2]), attachments=not self.options.skip_attachments)

                        # store original sourcekey or it is lost
                        try:
                            item.prop(PR_EC_BACKUP_SOURCE_KEY)
                        except MAPIErrorNotFound:
                            item.mapiobj.SetProps([SPropValue(PR_EC_BACKUP_SOURCE_KEY, sourcekey2.decode('hex'))])
                            item.mapiobj.SaveChanges(0)

                        stats['changes'] += 1

    def _store(self, username):
        """ lookup store for username """

        if '@' in username:
            u, c = username.split('@')
            if u == 'public' or u == c:
                return self.server.company(c).public_store
            else:
                return self.server.user(username).store
        elif username == 'public':
            return self.server.public_store
        else:
            return self.server.user(username).store

def folder_struct(data_path, options, mapper=None):
    """ determine all folders in backup directory """

    if mapper is None:
        mapper = {}
    if os.path.exists(data_path+'/path'):
        path = file(data_path+'/path').read().decode('utf8')
        mapper[path] = data_path
    if os.path.exists(data_path+'/folders'):
        for f in os.listdir(data_path+'/folders'):
            d = data_path+'/folders/'+f
            if os.path.isdir(d):
                folder_struct(d, options, mapper)
    return mapper

def folder_path(folder, subtree):
    """ determine path to folder in backup directory """

    path = ''
    parent = folder
    while parent and parent != subtree:
        path = '/folders/'+parent.sourcekey+path
        parent = parent.parent
    return path[1:]

def folder_deleted(data_path):
    if os.path.exists(data_path+'/index'):
        with closing(dbhash.open(data_path+'/index')) as db:
           idx = db.get('folder')
           if idx and pickle.loads(idx).get('backup_deleted'):
               return pickle.loads(idx).get('backup_deleted')
    return None

def show_contents(data_path, options):
    """ summary of contents of backup directory, at the item or folder level, in CSV format """

    # setup CSV writer, perform basic checks
    writer = csv.writer(sys.stdout)
    path_folder = folder_struct(data_path, options)
    paths = options.folders or sorted(path_folder)
    for path in paths:
        if path not in path_folder:
            print('no such folder:', path)
            sys.exit(-1)
    if options.recursive:
        paths = [p for p in path_folder if [f for f in paths if p.startswith(f)]]

    # loop over folders
    for path in paths:
        data_path = path_folder[path]
        items = []

        if options.deletes == 'no' and folder_deleted(data_path):
            continue

        # filter items on date using 'index' database
        if os.path.exists(data_path+'/index'):
            with closing(dbhash.open(data_path+'/index')) as db:
                for key, value in db.iteritems():
                    d = pickle.loads(value)
                    if ((key == 'folder') or
                        (options.period_begin and d['last_modified'] < options.period_begin) or
                        (options.period_end and d['last_modified'] >= options.period_end) or
                        (options.deletes == 'no' and d.get('backup_deleted'))):
                        continue
                    items.append((key, d))

        # --stats: one entry per folder
        if options.stats:
            writer.writerow([path.encode(sys.stdout.encoding or 'utf8'), len(items)])

        # --index: one entry per item
        elif options.index:
            items = sorted(items, key=lambda item: item[1]['last_modified'])
            for key, d in items:
                writer.writerow([key, path.encode(sys.stdout.encoding or 'utf8'), d['last_modified'], d['subject'].encode(sys.stdout.encoding or 'utf8')])

def dump_props(props, stats, log):
    """ dump given MAPI properties """

    data = {}
    with log_exc(log, stats):
        data = dict((prop.proptag, prop.mapiobj.Value) for prop in props)
    return pickle.dumps(data)

def dump_acl(folder, user, server, stats, log):
    """ dump acl for given folder """

    rows = []
    with log_exc(log, stats):
        acl_table = folder.mapiobj.OpenProperty(PR_ACL_TABLE, IID_IExchangeModifyTable, 0, 0)
        table = acl_table.GetTable(0)
        for row in table.QueryRows(-1,0):
            entryid = row[1].Value
            try:
                row[1].Value = ('user', server.sa.GetUser(entryid, MAPI_UNICODE).Username)
            except MAPIErrorNotFound:
                try:
                    row[1].Value = ('group', server.sa.GetGroup(entryid, MAPI_UNICODE).Groupname)
                except MAPIErrorNotFound:
                    log.warning("skipping access control entry for unknown user/group %s", entryid.encode('hex').upper())
                    continue
            rows.append(row)
    return pickle.dumps(rows)

def load_acl(folder, user, server, data, stats, log):
    """ load acl for given folder """

    with log_exc(log, stats):
        data = pickle.loads(data)
        rows = []
        for row in data:
            try:
                member_type, value = row[1].Value
                if member_type == 'user':
                    entryid = server.user(value).userid
                else:
                    entryid = server.group(value).groupid
                row[1].Value = entryid.decode('hex')
                rows.append(row)
            except kopano.NotFoundError:
                log.warning("skipping access control entry for unknown user/group '%s'", value)
        acltab = folder.mapiobj.OpenProperty(PR_ACL_TABLE, IID_IExchangeModifyTable, 0, MAPI_MODIFY)
        acltab.ModifyTable(0, [ROWENTRY(ROW_ADD, row) for row in rows])

def dump_rules(folder, user, server, stats, log):
    """ dump rules for given folder """

    ruledata = None
    with log_exc(log, stats):
        try:
            ruledata = folder.prop(PR_RULES_DATA).value
        except MAPIErrorNotFound:
            pass
        else:
            etxml = ElementTree.fromstring(ruledata)
            for actions in etxml.findall('./item/item/actions'):
                for movecopy in actions.findall('.//moveCopy'):
                    try:
                        s = movecopy.findall('store')[0]
                        store = server.mapisession.OpenMsgStore(0, s.text.decode('base64'), None, 0)
                        guid = HrGetOneProp(store, PR_STORE_RECORD_KEY).Value.encode('hex')
                        store = server.store(guid) # XXX guid doesn't work for multiserver?
                        if store.public:
                            s.text = 'public'
                        else:
                            s.text = store.user.name if store != user.store else ''
                        f = movecopy.findall('folder')[0]
                        path = store.folder(entryid=f.text.decode('base64').encode('hex')).path
                        f.text = path
                    except (kopano.NotFoundError, MAPIErrorNotFound, binascii.Error):
                        log.warning("cannot serialize rule for unknown store/folder")
            ruledata = ElementTree.tostring(etxml)
    return pickle.dumps(ruledata)

def load_rules(folder, user, server, data, stats, log):
    """ load rules for given folder """

    with log_exc(log, stats):
        data = pickle.loads(data)
        if data:
            etxml = ElementTree.fromstring(data)
            for actions in etxml.findall('./item/item/actions'):
                for movecopy in actions.findall('.//moveCopy'):
                    try:
                        s = movecopy.findall('store')[0]
                        if s.text == 'public':
                            store = server.public_store
                        else:
                            store = server.user(s.text).store if s.text else user.store
                        s.text = store.entryid.decode('hex').encode('base64').strip()
                        f = movecopy.findall('folder')[0]
                        f.text = store.folder(f.text).entryid.decode('hex').encode('base64').strip()
                    except kopano.NotFoundError:
                        log.warning("skipping rule for unknown store/folder")
            etxml = ElementTree.tostring(etxml)
            folder.create_prop(PR_RULES_DATA, etxml)

def _get_fbf(user, flags, log):
    try:
        fbeid = user.root.prop(PR_FREEBUSY_ENTRYIDS).value[1]
        return user.store.mapiobj.OpenEntry(fbeid, None, flags)
    except MAPIErrorNotFound:
        log.warning("skipping delegation because of missing freebusy data")

def dump_delegates(user, server, stats, log):
    """ dump delegate users for given user """

    usernames = []
    with log_exc(log, stats):
        fbf = _get_fbf(user, 0, log)
        delegate_uids = []
        try:
            if fbf:
                delegate_uids = HrGetOneProp(fbf, PR_SCHDINFO_DELEGATE_ENTRYIDS).Value
        except MAPIErrorNotFound:
            pass

        for uid in delegate_uids:
            try:
                usernames.append(server.sa.GetUser(uid, MAPI_UNICODE).Username)
            except MAPIErrorNotFound:
                log.warning("skipping delegate user for unknown userid")

    return pickle.dumps(usernames)

def load_delegates(user, server, data, stats, log):
    """ load delegate users for given user """

    with log_exc(log, stats):
        userids = []
        for name in pickle.loads(data):
            try:
                userids.append(server.user(name).userid.decode('hex'))
            except kopano.NotFoundError:
                log.warning("skipping delegation for unknown user '%s'", name)

        fbf = _get_fbf(user, MAPI_MODIFY, log)
        if fbf:
            fbf.SetProps([SPropValue(PR_SCHDINFO_DELEGATE_ENTRYIDS, userids)])
            fbf.SaveChanges(0)

def main():
    # select common options
    parser = kopano.parser('ckpsufwUPCSlObe', usage='kopano-backup [PATH] [options]')

    # add custom options
    parser.add_option('', '--skip-junk', dest='skip_junk', action='store_true', help='skip junk folder')
    parser.add_option('', '--skip-deleted', dest='skip_deleted', action='store_true', help='skip deleted items folder')
    parser.add_option('', '--skip-public', dest='skip_public', action='store_true', help='skip public store')
    parser.add_option('', '--skip-attachments', dest='skip_attachments', action='store_true', help='skip attachments')
    parser.add_option('', '--skip-meta', dest='skip_meta', action='store_true', help='skip metadata')
    parser.add_option('', '--only-meta', dest='only_meta', action='store_true', help='only backup/restore metadata')
    parser.add_option('', '--deletes', dest='deletes', help='store/restore deleted items/folders', metavar='YESNO')
    parser.add_option('', '--purge', dest='purge', type='int', help='purge items/folders deleted more than N days ago', metavar='N')
    parser.add_option('', '--restore', dest='restore', action='store_true', help='restore from backup')
    parser.add_option('', '--restore-root', dest='restore_root', help='restore under specific folder', metavar='PATH')
    parser.add_option('', '--stats', dest='stats', action='store_true', help='list folders for PATH')
    parser.add_option('', '--index', dest='index', action='store_true', help='list items for PATH')
    parser.add_option('', '--sourcekey', dest='sourcekeys', action='append', help='restore specific sourcekey', metavar='SOURCEKEY')
    parser.add_option('', '--recursive', dest='recursive', action='store_true', help='backup/restore folders recursively')

    # parse and check command-line options
    options, args = parser.parse_args()
    options.service = False
    if options.restore or options.stats or options.index or options.purge:
        assert len(args) == 1 and os.path.isdir(args[0]), 'please specify path to backup data'
    else:
        assert len(args) == 0, 'too many arguments'
    if options.deletes and options.deletes not in ('yes', 'no'):
        raise Exception("--deletes option takes 'yes' or 'no'")

    if options.stats or options.index:
        # handle --stats/--index
        show_contents(args[0], options)
    else:
        # start backup/restore
        Service('backup', options=options, args=args).start()

if __name__ == '__main__':
    main()
