//===-- ProcessLinux.h ------------------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef liblldb_ProcessLinux_H_
#define liblldb_ProcessLinux_H_

// C Includes

// C++ Includes
#include <queue>

// Other libraries and framework includes
#include "lldb/Target/Process.h"
#include "LinuxSignals.h"
#include "ProcessMessage.h"

class ProcessMonitor;

class ProcessLinux :
    public lldb_private::Process
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
    ProcessLinux(lldb_private::Target& target,
                 lldb_private::Listener &listener);

    virtual
    ~ProcessLinux();

    //------------------------------------------------------------------
    // Process protocol.
    //------------------------------------------------------------------
    virtual bool
    CanDebug(lldb_private::Target &target, bool plugin_specified_by_name);

    virtual lldb_private::Error
    WillLaunch(lldb_private::Module *module);

    virtual lldb_private::Error
    DoAttachToProcessWithID(lldb::pid_t pid);

    virtual lldb_private::Error
    DoLaunch (lldb_private::Module *exe_module, 
              const lldb_private::ProcessLaunchInfo &launch_info);

    virtual void
    DidLaunch();

    virtual lldb_private::Error
    DoResume();

    virtual lldb_private::Error
    DoHalt(bool &caused_stop);

    virtual lldb_private::Error
    DoDetach();

    virtual lldb_private::Error
    DoSignal(int signal);

    virtual lldb_private::Error
    DoDestroy();

    virtual void
    RefreshStateAfterStop();

    virtual bool
    IsAlive();

    virtual size_t
    DoReadMemory(lldb::addr_t vm_addr,
                 void *buf,
                 size_t size,
                 lldb_private::Error &error);

    virtual size_t
    DoWriteMemory(lldb::addr_t vm_addr, const void *buf, size_t size,
                  lldb_private::Error &error);

    virtual lldb::addr_t
    DoAllocateMemory(size_t size, uint32_t permissions,
                     lldb_private::Error &error);

    virtual lldb_private::Error
    DoDeallocateMemory(lldb::addr_t ptr);

    virtual size_t
    GetSoftwareBreakpointTrapOpcode(lldb_private::BreakpointSite* bp_site);

    virtual lldb_private::Error
    EnableBreakpoint(lldb_private::BreakpointSite *bp_site);

    virtual lldb_private::Error
    DisableBreakpoint(lldb_private::BreakpointSite *bp_site);

    virtual uint32_t
    UpdateThreadListIfNeeded();

    uint32_t
    UpdateThreadList(lldb_private::ThreadList &old_thread_list, 
                     lldb_private::ThreadList &new_thread_list);

    virtual lldb::ByteOrder
    GetByteOrder() const;

    virtual lldb::addr_t
    GetImageInfoAddress();

    virtual size_t
    PutSTDIN(const char *buf, size_t len, lldb_private::Error &error);

    virtual size_t
    GetSTDOUT(char *buf, size_t len, lldb_private::Error &error);

    virtual size_t
    GetSTDERR(char *buf, size_t len, lldb_private::Error &error);

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

    //--------------------------------------------------------------------------
    // ProcessLinux internal API.

    /// Registers the given message with this process.
    void SendMessage(const ProcessMessage &message);

    ProcessMonitor &
    GetMonitor() { assert(m_monitor); return *m_monitor; }

    lldb_private::UnixSignals &
    GetUnixSignals();

private:
    /// Target byte order.
    lldb::ByteOrder m_byte_order;

    /// Process monitor;
    ProcessMonitor *m_monitor;

    /// The module we are executing.
    lldb_private::Module *m_module;

    /// Message queue notifying this instance of inferior process state changes.
    lldb_private::Mutex m_message_mutex;
    std::queue<ProcessMessage> m_message_queue;

    /// True when the process has entered a state of "limbo".
    ///
    /// This flag qualifies eStateStopped.  It lets us know that when we
    /// continue from this state the process will exit.  Also, when true,
    /// Process::m_exit_status is set.
    bool m_in_limbo;

    /// Drive any exit events to completion.
    bool m_exit_now;

    /// Linux-specific signal set.
    LinuxSignals m_linux_signals;

    /// Updates the loaded sections provided by the executable.
    ///
    /// FIXME:  It would probably be better to delegate this task to the
    /// DynamicLoader plugin, when we have one.
    void UpdateLoadedSections();

    /// Returns true if the process has exited.
    bool HasExited();

    /// Returns true if the process is stopped.
    bool IsStopped();

    typedef std::map<lldb::addr_t, lldb::addr_t> MMapMap;
    MMapMap m_addr_to_mmap_size;
};

#endif  // liblldb_MacOSXProcess_H_