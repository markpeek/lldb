"""
Test some lldb command abbreviations to make sure the common short spellings of
many commands remain available even after we add/delte commands in the future.
"""

import os, time
import unittest2
import lldb
from lldbtest import *

class CommonShortSpellingsTestCase(TestBase):
    
    mydir = os.path.join("functionalities", "abbreviation")

    @unittest2.skipUnless(sys.platform.startswith("darwin"), "requires Darwin")
    def test_with_dsym (self):
        self.buildDsym ()
        self.run_abbrevs2 ()

    def test_with_dwarf (self):
        self.buildDwarf ()
        self.run_abbrevs2 ()

    def run_abbrevs2 (self):
        exe = os.path.join (os.getcwd(), "a.out")
        self.expect("file " + exe,
                    patterns = [ "Current executable set to .*a.out.*" ])

        # br s -> breakpoint set
        self.expect("br s -n sum",
            startstr = "Breakpoint created: 1: name = 'sum', locations = 1")

        # disp -> display
        self.expect("disp a",
            startstr = "target stop-hook add -o")
        self.expect("disp b",
            startstr = "target stop-hook add -o")

        # di/dis -> disassemble
        self.expect("help di",
            substrs = ["disassemble"])
        self.expect("help dis",
            substrs = ["disassemble"])

        # ta st a -> target stop-hook add
        self.expect("help ta st a",
            substrs = ["target stop-hook add"])

        # fr v -> frame variable
        self.expect("help fr v",
            substrs = ["frame variable"])

        # ta st li -> target stop-hook list
        self.expect("ta st li",
            substrs = ["Hook: 1", "Hook: 2"])


if __name__ == '__main__':
    import atexit
    lldb.SBDebugger.Initialize()
    atexit.register(lambda: lldb.SBDebugger.Terminate())
    unittest2.main()

