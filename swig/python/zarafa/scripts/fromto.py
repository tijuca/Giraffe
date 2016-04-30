#!/usr/bin/env python

# print names and email addresses of sender/recipients

# usage: ./fromto.py (change USER into username)

import zarafa

USER = 'user1'

server = zarafa.Server()

for item in server.user(USER).inbox:
    print item
    print 'from:', repr(item.sender.name), repr(item.sender.email),
    for rec in item.recipients():
        print 'to:', repr(rec.name), repr(rec.email),
    print
