import sys
import os
from unittest.mock import MagicMock

sys.path.append(os.path.join(os.path.dirname(__file__), "../"))

UNTESTED_MODULES = [
    'wabengine',
    'wabengine.common',
    'wabengine.common.exception',
    'wallixgenericnotifier',
    'wabconfig',
    'wallix.logger',
    'wallixconst',
    'wallixconst.protocol',
    'wallixconst.authentication',
    'wallixconst.account',
    'wallixconst.approval',
    'wallixconst.misc',
    'wallixconst.chgpasswd',
    'wallixconst.configuration',
    'wallixconst.trace',
    'wallixredis',
    'wallixutils',
    'wabengine.client',
    'wabengine.client.checker',
    'wallixauthentication',
    'wallixauthentication.api',
    'wabx509',
]
for module in UNTESTED_MODULES:
    sys.modules[module] = MagicMock()
