#!/usr/bin/python

import os
import unittest

import kopano

auth_user = os.environ['AUTH_USER']
auth_pass = os.environ['AUTH_PASS']

def cmd(c):
    assert os.system(c) == 0

class TestServer(unittest.TestCase):
    user = auth_user

    def setUp(self):
        self.user = kopano.user(self.user)
        self.backup_folder = self.user.folder('backup', create=True)
        self.backup_folder.empty()

    def testBackupRestoreUser(self):
        # backup to directory 'user1'
        cmd('rm user1 -rf')
        cmd('kopano-backup -u user1')

        # restore to folder 'backup'
        cmd('kopano-backup --restore user1 --restore-root=backup')

        # check restored inbox
        inbox = self.user.folder('backup/Inbox')
        self.assertIn("the towers", [item.subject for item in inbox])
