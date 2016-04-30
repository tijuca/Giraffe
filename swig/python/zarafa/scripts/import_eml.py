#!/usr/bin/env python

# import .eml file

# usage: ./import-eml.py username filename

import sys
import zarafa

zarafa.User(sys.argv[1]).store.inbox.create_item(eml=open(sys.argv[2]).read())
