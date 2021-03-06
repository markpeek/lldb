//===-- ProcessFreeBSD.h ------------------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef liblldb_ProcessFreeBSD_H_
#define liblldb_ProcessFreeBSD_H_

// C Includes

// C++ Includes
#include <queue>

// Other libraries and framework includes
#include "lldb/Target/Process.h"
#include "lldb/Target/ThreadList.h"
#include "ProcessMessage.h"
#include "ProcessPOSIX.h"

class ProcessMonitor;

class ProcessFreeBSD :
    public ProcessPOSIX
{

public:
    //------------------------------------------------------------------
    // Static functions.
    //------------------------------------------------------------------
    static Process*
    CreateInstance(lldb_private::Target& target,
                   lldb_private::Listener &listener);

    static void
    Initialize();

    static void
    Terminate();

    static const char *
    GetPluginNameStatic();

    static const char *
    GetPluginDescriptionStatic();

    //------------------------------------------------------------------
    // Constructors and destructors
    //------------------------------------------------------------------
    ProcessFreeBSD(lldb_private::Target& target,
                 lldb_private::Listener &listener);

    virtual uint32_t
    UpdateThreadList(lldb_private::ThreadList &old_thread_list, lldb_private::ThreadList &new_thread_list);

    //------------------------------------------------------------------
    // PluginInterface protocol
    //------------------------------------------------------------------
    virtual const char *
    GetPluginName();

    virtual const char *
    GetShortPluginName();

    virtual uint32_t
    GetPluginVersion();

    virtual void
    GetPluginCommandHelp(const char *command, lldb_private::Stream *strm);

    virtual lldb_private::Error
    ExecutePluginCommand(lldb_private::Args &command,
                         lldb_private::Stream *strm);

    virtual lldb_private::Log *
    EnablePluginLogging(lldb_private::Stream *strm,
                        lldb_private::Args &command);

};

#endif  // liblldb_MacOSXProcess_H_
