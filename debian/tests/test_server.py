#!/usr/bin/python

import os
import unittest

import kopano

auth_user = os.environ['AUTH_USER']
auth_pass = os.environ['AUTH_PASS']


class TestServer(unittest.TestCase):
    user = auth_user

    def setUp(self):

        self.server = kopano.Server(auth_user=auth_user,
                                    auth_pass=auth_pass)

    def testFindItemBySubject(self):
        user = self.server.user(self.user)
        inbox = user.store.folder("Inbox")
        self.assertIn("the towers", [item.subject for item in inbox.items()])
