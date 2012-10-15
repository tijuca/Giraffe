import sys
from MAPICore import *

MAPIInitialize(None)

unicode = False

mod_struct = sys.modules.get('MAPI.Struct')
if mod_struct:
    mod_struct.MAPIError._initialize_errors()
