//===-- OptionGroupWatchpoint.cpp -------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "lldb/Interpreter/OptionGroupWatchpoint.h"

// C Includes
// C++ Includes
// Other libraries and framework includes
// Project includes
#include "lldb/lldb-enumerations.h"
#include "lldb/Interpreter/Args.h"
#include "lldb/Utility/Utils.h"

using namespace lldb;
using namespace lldb_private;

static OptionEnumValueElement g_watch_type[] =
{
    { OptionGroupWatchpoint::eWatchRead,      "read",       "Watch for read"},
    { OptionGroupWatchpoint::eWatchWrite,     "write",      "Watch for write"},
    { OptionGroupWatchpoint::eWatchReadWrite, "read_write", "Watch for read/write"},
    { 0, NULL, NULL }
};

static OptionEnumValueElement g_watch_size[] =
{
    { 1, "1", "Watch for byte size of 1"},
    { 2, "2", "Watch for byte size of 2"},
    { 4, "4", "Watch for byte size of 4"},
    { 8, "8", "Watch for byte size of 8"},
    { 0, NULL, NULL }
};

static OptionDefinition
g_option_table[] =
{
    { LLDB_OPT_SET_1, false, "watch", 'w', required_argument, g_watch_type, 0, eArgTypeWatchType, "Determine how to watch a variable; or, with -x option, its pointee."},
    { LLDB_OPT_SET_1, false, "xsize", 'x', required_argument, g_watch_size, 0, eArgTypeByteSize, "Number of bytes to use to watch the pointee."}
};


OptionGroupWatchpoint::OptionGroupWatchpoint () :
    OptionGroup()
{
}

OptionGroupWatchpoint::~OptionGroupWatchpoint ()
{
}

Error
OptionGroupWatchpoint::SetOptionValue (CommandInterpreter &interpreter,
                                       uint32_t option_idx, 
                                       const char *option_arg)
{
    Error error;
    char short_option = (char) g_option_table[option_idx].short_option;
    switch (short_option)
    {
        case 'w':
            watch_type = (WatchType) Args::StringToOptionEnum(option_arg, g_option_table[option_idx].enum_values, 0, error);
            if (error.Success())
                watch_variable = true;
            break;

        case 'x':
            watch_size = (WatchType) Args::StringToOptionEnum(option_arg, g_option_table[option_idx].enum_values, 0, error);
            break;

        default:
            error.SetErrorStringWithFormat("unrecognized short option '%c'", short_option);
            break;
    }
    
    return error;
}

void
OptionGroupWatchpoint::OptionParsingStarting (CommandInterpreter &interpreter)
{
    watch_variable = false;
    watch_type = eWatchInvalid;
    watch_size = 0;
}


const OptionDefinition*
OptionGroupWatchpoint::GetDefinitions ()
{
    return g_option_table;
}

uint32_t
OptionGroupWatchpoint::GetNumDefinitions ()
{
    return arraysize(g_option_table);
}
