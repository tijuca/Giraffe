#!/usr/bin/python

import os
import unittest

import kopano

auth_user = os.environ['AUTH_USER']
auth_pass = os.environ['AUTH_PASS']


class TestBackup(unittest.TestCase):
    username = auth_user

    def cmd(self, c):
        self.assertEqual(os.system(c), 0)

    def setUp(self):
        self.user = kopano.user(self.username)
        self.backup_folder = self.user.folder('backup', create=True)
        self.backup_folder.empty()

    def testBackupRestoreUser(self):
        # backup to directory self.user
        self.cmd('rm -rf %s' % self.username)
        self.cmd('kopano-backup -u %s' % self.username)

        # restore to folder 'backup'
        self.cmd('kopano-backup --restore %s --restore-root=backup' % self.username)

        # check restored inbox
        inbox = self.user.folder('backup/Inbox')
        self.assertIn("the towers", [item.subject for item in inbox])
