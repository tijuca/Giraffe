from MAPI.Util import kc_session_save, kc_session_restore, GetDefaultStore
import json
try:
    import urlparse
except ImportError:
    import urllib.parse as urlparse
import types

import falcon
import kopano

def _server(req):
    userid = req.get_header('X-Kopano-UserEntryID', required=True)
    if userid in userid_sessiondata:
        sessiondata = userid_sessiondata[userid]
        mapisession = kc_session_restore(sessiondata)
        server = kopano.Server(mapisession=mapisession, parse_args=False)
    else:
        username = admin_server.user(userid=userid).name
        server = kopano.Server(auth_user=username, auth_pass='',
                               parse_args=False, store_cache=False)
        sessiondata = kc_session_save(server.mapisession)
        userid_sessiondata[userid] = sessiondata
    return server

class Resource(object):
    def get_fields(self, obj, fields):
        return {f: self.fields[f](obj) for f in fields}

    def json(self, obj, fields):
        return json.dumps(self.get_fields(obj, fields),
            indent=4, separators=(',', ': ')
        )

    def json_multi(self, obj, fields):
        # TODO itertools magic?
        yield b'[\n'
        first = True
        for o in obj:
            if not first:
                yield b',\n'
            first = False
            yield self.json(o, fields).encode('utf-8')
        yield b'\n]'

    def respond(self, req, resp, obj):
        # determine fields (default all)
        args = urlparse.parse_qs(req.query_string)
        if 'fields' in args:
            fields = args['fields'][0].split(',') # TODO 0?
        else:
            fields = self.fields.keys()

        # jsonify result (as stream)
        resp.content_type = "application/json"
        if isinstance(obj, types.GeneratorType):
            resp.stream = self.json_multi(obj, fields)
        else:
            resp.body = self.json(obj, fields)

    def generator(self, req, generator):
        # determine pagination and ordering
        args = urlparse.parse_qs(req.query_string)
        start = int(args['start'][0]) if 'start' in args else None
        limit = int(args['limit'][0]) if 'limit' in args else None
        order = tuple(args['order'][0].split(',')) if 'order' in args else None

        return generator(page_start=start, page_limit=limit, order=order)

class UserResource(Resource):
    fields = {
        'userid': lambda user: user.userid,
        'name': lambda user: user.name,
        'fullname': lambda user: user.fullname,
        'store': lambda user: user.store and user.store.entryid or None,
    }

    def on_get(self, req, resp, userid=None):
        server = _server(req)
        if userid:
            data = server.user(userid=userid)
        else:
            data = server.users()
        self.respond(req, resp, data)

class StoreResource(Resource):
    fields = {
        'entryid': lambda store: store.entryid,
        'public': lambda store: store.public,
        'user': lambda store: store.user.entryid if store.user else None,
    }

    def on_get(self, req, resp, storeid=None):
        server = _server(req)
        if storeid:
            data = server.store(entryid=storeid)
        else:
            data = server.stores()
        self.respond(req, resp, data)

class FolderResource(Resource):
    fields = {
        'entryid': lambda folder: folder.entryid,
        'parent': lambda folder: folder.parent.entryid,
        'name': lambda folder: folder.name,
        'modified': lambda folder: folder.last_modified.isoformat(),
        'unread': lambda folder: folder.unread,
    }

    def on_get(self, req, resp, storeid=None, folderid=None):
        server = _server(req)
        if storeid:
            store = server.store(entryid=storeid)
        else:
            store = kopano.Store(server=server,
                mapiobj = GetDefaultStore(server.mapisession))
        if folderid:
            data = store.folder(entryid=folderid)
        else:
            data = self.generator(req, store.folders)
        self.respond(req, resp, data)

    def on_post(self, req, resp, storeid, folderid):
        server = _server(req)
        store = server.store(entryid=storeid)
        folder = store.folder(entryid=folderid)

        fields = json.loads(req.stream.read())
        item = folder.create_item(**fields) # TODO conversion

        resp.status = falcon.HTTP_201
        resp.location = req.path+'/items/'+item.entryid

    def on_put(self, req, resp, storeid, folderid):
        server = _server(req)
        store = server.store(entryid=storeid)
        folder = store.folder(entryid=folderid)

        data = json.loads(req.stream.read())
        if 'action' in data:
            action = data['action']
            items = [store.item(entryid=entryid) for entryid in data['items']]

            if action == 'send':
                for item in items:
                    item.send()
            elif action == 'delete':
                folder.delete(items)
            elif action == 'copy':
                target = store.folder(entryid=data['target'])
                folder.copy(items, target)
            elif action == 'move':
                target = store.folder(entryid=data['target'])
                folder.move(items, target)

class ItemResource(Resource):
    fields = {
        'entryid': lambda item: item.entryid,
        'subject': lambda item: item.subject,
        'to': lambda item: ['%s <%s>' % (to.name, to.email) for to in item.to],
        'text': lambda item: item.text,
        'modified': lambda item: item.last_modified.isoformat(),
        'received': lambda item: item.received.isoformat()
    }

    def on_get(self, req, resp, storeid=None, folderid=None, itemid=None):
        server = _server(req)
        if storeid:
            store = server.store(entryid=storeid)
        else:
            store = kopano.Store(server=server,
                mapiobj = GetDefaultStore(server.mapisession))
        if itemid:
            data = store.item(itemid)
        else:
            folder = store.folder(entryid=folderid)
            data = self.generator(req, folder.items)
        self.respond(req, resp, data)

admin_server = kopano.Server(parse_args=False, store_cache=False)
userid_sessiondata = {}

app = falcon.API()
users = UserResource()
stores = StoreResource()
items = ItemResource()
folders = FolderResource()

app.add_route('/users', users)
app.add_route('/users/{userid}', users)
app.add_route('/stores', stores)
app.add_route('/stores/{storeid}', stores)

for (route, resource) in (
    ('/folders', folders),
    ('/folders/{folderid}', folders),
    ('/folders/{folderid}/items', items),
    ('/folders/{folderid}/items/{itemid}', items),
    ):
    app.add_route(route, resource)
    app.add_route('/stores/{storeid}'+route, resource)
