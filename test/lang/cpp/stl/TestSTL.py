"""
Test some expressions involving STL data types.
"""

import os, time
import unittest2
import lldb
from lldbtest import *

class STLTestCase(TestBase):

    mydir = os.path.join("lang", "cpp", "stl")

    # rdar://problem/10400981
    @unittest2.expectedFailure
    @unittest2.skipUnless(sys.platform.startswith("darwin"), "requires Darwin")
    def test_with_dsym(self):
        """Test some expressions involving STL data types."""
        self.buildDsym()
        self.step_stl_exprs()

    # rdar://problem/10400981
    @unittest2.expectedFailure
    def test_with_dwarf(self):
        """Test some expressions involving STL data types."""
        self.buildDwarf()
        self.step_stl_exprs()

    def setUp(self):
        # Call super's setUp().
        TestBase.setUp(self)
        # Find the line number to break inside main().
        self.line = line_number('main.cpp', '// Set break point at this line.')

    def step_stl_exprs(self):
        """Test some expressions involving STL data types."""
        exe = os.path.join(os.getcwd(), "a.out")

        # The following two lines, if uncommented, will enable loggings.
        #self.ci.HandleCommand("log enable -f /tmp/lldb.log lldb default", res)
        #self.assertTrue(res.Succeeded())

        self.runCmd("file " + exe, CURRENT_EXECUTABLE_SET)

        # rdar://problem/8543077
        # test/stl: clang built binaries results in the breakpoint locations = 3,
        # is this a problem with clang generated debug info?
        self.expect("breakpoint set -f main.cpp -l %d" % self.line,
                    BREAKPOINT_CREATED,
            startstr = "Breakpoint created: 1: file ='main.cpp', line = %d" %
                        self.line)

        self.runCmd("run", RUN_SUCCEEDED)

        # Stop at 'std::string hello_world ("Hello World!");'.
        self.expect("thread list", STOPPED_DUE_TO_BREAKPOINT,
            substrs = ['main.cpp:%d' % self.line,
                       'stop reason = breakpoint'])

        # The breakpoint should have a hit count of 1.
        self.expect("breakpoint list -f", BREAKPOINT_HIT_ONCE,
            substrs = [' resolved, hit count = 1'])

        # Now try some expressions....

        self.runCmd('expr for (int i = 0; i < hello_world.length(); ++i) { (void)printf("%c\\n", hello_world[i]); }')

        # rdar://problem/10373783
        # rdar://problem/10400981
        self.expect('expr associative_array.size()',
            substrs = [' = 3'])
        self.expect('expr associative_array.count(hello_world)',
            substrs = [' = 1'])
        self.expect('expr associative_array[hello_world]',
            substrs = [' = 1'])
        self.expect('expr associative_array["hello"]',
            substrs = [' = 2'])


if __name__ == '__main__':
    import atexit
    lldb.SBDebugger.Initialize()
    atexit.register(lambda: lldb.SBDebugger.Terminate())
    unittest2.main()
