//===-- MachException.cpp ---------------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//  Created by Greg Clayton on 6/18/07.
//
//===----------------------------------------------------------------------===//

#include "MachException.h"
#include "MachProcess.h"
#include "DNB.h"
#include "DNBError.h"
#include <sys/types.h>
#include "DNBLog.h"
#include "PThreadMutex.h"
#include "SysSignal.h"
#include <errno.h>
#include <sys/ptrace.h>

// Routine mach_exception_raise
extern "C"
kern_return_t catch_mach_exception_raise
(
    mach_port_t exception_port,
    mach_port_t thread,
    mach_port_t task,
    exception_type_t exception,
    mach_exception_data_t code,
    mach_msg_type_number_t codeCnt
);

extern "C"
kern_return_t catch_mach_exception_raise_state
(
    mach_port_t exception_port,
    exception_type_t exception,
    const mach_exception_data_t code,
    mach_msg_type_number_t codeCnt,
    int *flavor,
    const thread_state_t old_state,
    mach_msg_type_number_t old_stateCnt,
    thread_state_t new_state,
    mach_msg_type_number_t *new_stateCnt
);

// Routine mach_exception_raise_state_identity
extern "C"
kern_return_t catch_mach_exception_raise_state_identity
(
    mach_port_t exception_port,
    mach_port_t thread,
    mach_port_t task,
    exception_type_t exception,
    mach_exception_data_t code,
    mach_msg_type_number_t codeCnt,
    int *flavor,
    thread_state_t old_state,
    mach_msg_type_number_t old_stateCnt,
    thread_state_t new_state,
    mach_msg_type_number_t *new_stateCnt
);

extern "C" boolean_t mach_exc_server(
        mach_msg_header_t *InHeadP,
        mach_msg_header_t *OutHeadP);

// Any access to the g_message variable should be done by locking the
// g_message_mutex first, using the g_message variable, then unlocking
// the g_message_mutex. See MachException::Message::CatchExceptionRaise()
// for sample code.

static MachException::Data *g_message = NULL;
//static pthread_mutex_t g_message_mutex = PTHREAD_MUTEX_INITIALIZER;


extern "C"
kern_return_t
catch_mach_exception_raise_state
(
    mach_port_t                 exc_port,
    exception_type_t            exc_type,
    const mach_exception_data_t exc_data,
    mach_msg_type_number_t      exc_data_count,
    int *                       flavor,
    const thread_state_t        old_state,
    mach_msg_type_number_t      old_stateCnt,
    thread_state_t              new_state,
    mach_msg_type_number_t *    new_stateCnt
)
{
    if (DNBLogCheckLogBit(LOG_EXCEPTIONS))
    {
        DNBLogThreaded ("::%s ( exc_port = 0x%4.4x, exc_type = %d ( %s ), exc_data = 0x%llx, exc_data_count = %d)",
                        __FUNCTION__,
                        exc_port,
                        exc_type, MachException::Name(exc_type),
                        (uint64_t)exc_data,
                        exc_data_count);
    }
    return KERN_FAILURE;
}

extern "C"
kern_return_t
catch_mach_exception_raise_state_identity
(
    mach_port_t             exc_port,
    mach_port_t             thread_port,
    mach_port_t             task_port,
    exception_type_t        exc_type,
    mach_exception_data_t   exc_data,
    mach_msg_type_number_t  exc_data_count,
    int *                   flavor,
    thread_state_t          old_state,
    mach_msg_type_number_t  old_stateCnt,
    thread_state_t          new_state,
    mach_msg_type_number_t *new_stateCnt
)
{
    kern_return_t kret;
    if (DNBLogCheckLogBit(LOG_EXCEPTIONS))
    {
        DNBLogThreaded("::%s ( exc_port = 0x%4.4x, thd_port = 0x%4.4x, tsk_port = 0x%4.4x, exc_type = %d ( %s ), exc_data[%d] = { 0x%llx, 0x%llx })",
            __FUNCTION__,
            exc_port,
            thread_port,
            task_port,
            exc_type, MachException::Name(exc_type),
            exc_data_count,
            (uint64_t)(exc_data_count > 0 ? exc_data[0] : 0xBADDBADD),
            (uint64_t)(exc_data_count > 1 ? exc_data[1] : 0xBADDBADD));
    }
    kret = mach_port_deallocate (mach_task_self (), task_port);
    kret = mach_port_deallocate (mach_task_self (), thread_port);

    return KERN_FAILURE;
}

extern "C"
kern_return_t
catch_mach_exception_raise
(
    mach_port_t             exc_port,
    mach_port_t             thread_port,
    mach_port_t             task_port,
    exception_type_t        exc_type,
    mach_exception_data_t   exc_data,
    mach_msg_type_number_t  exc_data_count)
{
    if (DNBLogCheckLogBit(LOG_EXCEPTIONS))
    {
        DNBLogThreaded ("::%s ( exc_port = 0x%4.4x, thd_port = 0x%4.4x, tsk_port = 0x%4.4x, exc_type = %d ( %s ), exc_data[%d] = { 0x%llx, 0x%llx })",
                        __FUNCTION__,
                        exc_port,
                        thread_port,
                        task_port,
                        exc_type, MachException::Name(exc_type),
                        exc_data_count,
                        (uint64_t)(exc_data_count > 0 ? exc_data[0] : 0xBADDBADD),
                        (uint64_t)(exc_data_count > 1 ? exc_data[1] : 0xBADDBADD));
    }

    g_message->task_port = task_port;
    g_message->thread_port = thread_port;
    g_message->exc_type = exc_type;
    g_message->exc_data.resize(exc_data_count);
    ::memcpy (&g_message->exc_data[0], exc_data, g_message->exc_data.size() * sizeof (mach_exception_data_type_t));
    return KERN_SUCCESS;
}


void
MachException::Message::Dump() const
{
    DNBLogThreadedIf(LOG_EXCEPTIONS,
        "  exc_msg { bits = 0x%8.8x size = 0x%8.8x remote-port = 0x%8.8x local-port = 0x%8.8x reserved = 0x%8.8x id = 0x%8.8x } ",
        exc_msg.hdr.msgh_bits,
        exc_msg.hdr.msgh_size,
        exc_msg.hdr.msgh_remote_port,
        exc_msg.hdr.msgh_local_port,
        exc_msg.hdr.msgh_reserved,
        exc_msg.hdr.msgh_id);

    DNBLogThreadedIf(LOG_EXCEPTIONS,
        "reply_msg { bits = 0x%8.8x size = 0x%8.8x remote-port = 0x%8.8x local-port = 0x%8.8x reserved = 0x%8.8x id = 0x%8.8x }",
        reply_msg.hdr.msgh_bits,
        reply_msg.hdr.msgh_size,
        reply_msg.hdr.msgh_remote_port,
        reply_msg.hdr.msgh_local_port,
        reply_msg.hdr.msgh_reserved,
        reply_msg.hdr.msgh_id);

    state.Dump();
}

bool
MachException::Data::GetStopInfo(struct DNBThreadStopInfo *stop_info) const
{
    // Zero out the structure.
    memset(stop_info, 0, sizeof(struct DNBThreadStopInfo));
    // We always stop with a mach exceptions
    stop_info->reason = eStopTypeException;
    // Save the EXC_XXXX exception type
    stop_info->details.exception.type = exc_type;

    // Fill in a text description
    const char * exc_name = MachException::Name(exc_type);
    char *desc = stop_info->description;
    const char *end_desc = desc + DNB_THREAD_STOP_INFO_MAX_DESC_LENGTH;
    if (exc_name)
        desc += snprintf(desc, DNB_THREAD_STOP_INFO_MAX_DESC_LENGTH, "%s", exc_name);
    else
        desc += snprintf(desc, DNB_THREAD_STOP_INFO_MAX_DESC_LENGTH, "%i", exc_type);

    stop_info->details.exception.data_count = exc_data.size();

    int soft_signal = SoftSignal();
    if (soft_signal)
    {
        if (desc < end_desc)
        {
            const char *sig_str = SysSignal::Name(soft_signal);
            desc += snprintf(desc, end_desc - desc, " EXC_SOFT_SIGNAL( %i ( %s ))", soft_signal, sig_str ? sig_str : "unknown signal");
        }
    }
    else
    {
        // No special disassembly for exception data, just
        size_t idx;
        if (desc < end_desc)
        {
            desc += snprintf(desc, end_desc - desc, " data[%zu] = {", stop_info->details.exception.data_count);

            for (idx = 0; desc < end_desc && idx < stop_info->details.exception.data_count; ++idx)
                desc += snprintf(desc, end_desc - desc, "0x%llx%c", (uint64_t)exc_data[idx], ((idx + 1 == stop_info->details.exception.data_count) ? '}' : ','));
        }
    }

    // Copy the exception data
    size_t i;
    for (i=0; i<stop_info->details.exception.data_count; i++)
        stop_info->details.exception.data[i] = exc_data[i];

    return true;
}


void
MachException::Data::DumpStopReason() const
{
    int soft_signal = SoftSignal();
    if (soft_signal)
    {
        const char *signal_str = SysSignal::Name(soft_signal);
        if (signal_str)
            DNBLog("signal(%s)", signal_str);
        else
            DNBLog("signal(%i)", soft_signal);
        return;
    }
    DNBLog("%s", Name(exc_type));
}

kern_return_t
MachException::Message::Receive(mach_port_t port, mach_msg_option_t options, mach_msg_timeout_t timeout, mach_port_t notify_port)
{
    DNBError err;
    const bool log_exceptions = DNBLogCheckLogBit(LOG_EXCEPTIONS);
    mach_msg_timeout_t mach_msg_timeout = options & MACH_RCV_TIMEOUT ? timeout : 0;
    if (log_exceptions && ((options & MACH_RCV_TIMEOUT) == 0))
    {
        // Dump this log message if we have no timeout in case it never returns
        DNBLogThreaded ("::mach_msg ( msg->{bits = %#x, size = %u remote_port = %#x, local_port = %#x, reserved = 0x%x, id = 0x%x}, option = %#x, send_size = 0, rcv_size = %zu, rcv_name = %#x, timeout = %u, notify = %#x)",
                        exc_msg.hdr.msgh_bits,
                        exc_msg.hdr.msgh_size,
                        exc_msg.hdr.msgh_remote_port,
                        exc_msg.hdr.msgh_local_port,
                        exc_msg.hdr.msgh_reserved,
                        exc_msg.hdr.msgh_id,
                        options,
                        sizeof (exc_msg.data),
                        port,
                        mach_msg_timeout,
                        notify_port);
    }

    err = ::mach_msg (&exc_msg.hdr,
                      options,                  // options
                      0,                        // Send size
                      sizeof (exc_msg.data),    // Receive size
                      port,                     // exception port to watch for exception on
                      mach_msg_timeout,         // timeout in msec (obeyed only if MACH_RCV_TIMEOUT is ORed into the options parameter)
                      notify_port);

    // Dump any errors we get
    if (log_exceptions)
    {
        err.LogThreaded("::mach_msg ( msg->{bits = %#x, size = %u remote_port = %#x, local_port = %#x, reserved = 0x%x, id = 0x%x}, option = %#x, send_size = %u, rcv_size = %u, rcv_name = %#x, timeout = %u, notify = %#x)",
            exc_msg.hdr.msgh_bits,
            exc_msg.hdr.msgh_size,
            exc_msg.hdr.msgh_remote_port,
            exc_msg.hdr.msgh_local_port,
            exc_msg.hdr.msgh_reserved,
            exc_msg.hdr.msgh_id,
            options,
            0,
            sizeof (exc_msg.data),
            port,
            mach_msg_timeout,
            notify_port);
    }
    return err.Error();
}

bool
MachException::Message::CatchExceptionRaise()
{
    bool success = false;
    // locker will keep a mutex locked until it goes out of scope
//    PThreadMutex::Locker locker(&g_message_mutex);
    //    DNBLogThreaded("calling  mach_exc_server");
    g_message = &state;
    // The exc_server function is the MIG generated server handling function
    // to handle messages from the kernel relating to the occurrence of an
    // exception in a thread. Such messages are delivered to the exception port
    // set via thread_set_exception_ports or task_set_exception_ports. When an
    // exception occurs in a thread, the thread sends an exception message to
    // its exception port, blocking in the kernel waiting for the receipt of a
    // reply. The exc_server function performs all necessary argument handling
    // for this kernel message and calls catch_exception_raise,
    // catch_exception_raise_state or catch_exception_raise_state_identity,
    // which should handle the exception. If the called routine returns
    // KERN_SUCCESS, a reply message will be sent, allowing the thread to
    // continue from the point of the exception; otherwise, no reply message
    // is sent and the called routine must have dealt with the exception
    // thread directly.
    if (mach_exc_server (&exc_msg.hdr, &reply_msg.hdr))
    {
        success = true;
    }
    else if (DNBLogCheckLogBit(LOG_EXCEPTIONS))
    {
        DNBLogThreaded("mach_exc_server returned zero...");
    }
    g_message = NULL;
    return success;
}



kern_return_t
MachException::Message::Reply(MachProcess *process, int signal)
{
    // Reply to the exception...
    DNBError err;

    // If we had a soft signal, we need to update the thread first so it can
    // continue without signaling
    int soft_signal = state.SoftSignal();
    if (soft_signal)
    {
        int state_pid = -1;
        if (process->Task().TaskPort() == state.task_port)
        {
            // This is our task, so we can update the signal to send to it
            state_pid = process->ProcessID();
            soft_signal = signal;
        }
        else
        {
            err = ::pid_for_task(state.task_port, &state_pid);
        }

        assert (state_pid != -1);
        if (state_pid != -1)
        {
            errno = 0;
            if (::ptrace (PT_THUPDATE, state_pid, (caddr_t)state.thread_port, soft_signal) != 0)
                err.SetError(errno, DNBError::POSIX);
            else
                err.Clear();

            if (DNBLogCheckLogBit(LOG_EXCEPTIONS) || err.Fail())
                err.LogThreaded("::ptrace (request = PT_THUPDATE, pid = 0x%4.4x, tid = 0x%4.4x, signal = %i)", state_pid, state.thread_port, soft_signal);
        }
    }

    DNBLogThreadedIf(LOG_EXCEPTIONS, "::mach_msg ( msg->{bits = %#x, size = %u, remote_port = %#x, local_port = %#x, reserved = 0x%x, id = 0x%x}, option = %#x, send_size = %u, rcv_size = %u, rcv_name = %#x, timeout = %u, notify = %#x)",
        reply_msg.hdr.msgh_bits,
        reply_msg.hdr.msgh_size,
        reply_msg.hdr.msgh_remote_port,
        reply_msg.hdr.msgh_local_port,
        reply_msg.hdr.msgh_reserved,
        reply_msg.hdr.msgh_id,
        MACH_SEND_MSG | MACH_SEND_INTERRUPT,
        reply_msg.hdr.msgh_size,
        0,
        MACH_PORT_NULL,
        MACH_MSG_TIMEOUT_NONE,
        MACH_PORT_NULL);

    err = ::mach_msg (  &reply_msg.hdr,
                        MACH_SEND_MSG | MACH_SEND_INTERRUPT,
                        reply_msg.hdr.msgh_size,
                        0,
                        MACH_PORT_NULL,
                        MACH_MSG_TIMEOUT_NONE,
                        MACH_PORT_NULL);

    if (err.Fail())
    {
        if (err.Error() == MACH_SEND_INTERRUPTED)
        {
            if (DNBLogCheckLogBit(LOG_EXCEPTIONS))
                err.LogThreaded("::mach_msg() - send interrupted");
            // TODO: keep retrying to reply???
        }
        else
        {
            if (state.task_port == process->Task().TaskPort())
            {
                if (DNBLogCheckLogBit(LOG_EXCEPTIONS))
                    err.LogThreaded("::mach_msg() - failed (task)");
                abort ();
            }
            else
            {
                if (DNBLogCheckLogBit(LOG_EXCEPTIONS))
                    err.LogThreaded("::mach_msg() - failed (child of task)");
            }
        }
    }

    return err.Error();
}


void
MachException::Data::Dump() const
{
    const char *exc_type_name = MachException::Name(exc_type);
    DNBLogThreadedIf(LOG_EXCEPTIONS, "    state { task_port = 0x%4.4x, thread_port =  0x%4.4x, exc_type = %i (%s) ...", task_port, thread_port, exc_type, exc_type_name ? exc_type_name : "???");

    const size_t exc_data_count = exc_data.size();
    // Dump any special exception data contents
    int soft_signal = SoftSignal();
    if (soft_signal != 0)
    {
        const char *sig_str = SysSignal::Name(soft_signal);
        DNBLogThreadedIf(LOG_EXCEPTIONS, "            exc_data: EXC_SOFT_SIGNAL (%i (%s))", soft_signal, sig_str ? sig_str : "unknown signal");
    }
    else
    {
        // No special disassembly for this data, just dump the data
        size_t idx;
        for (idx = 0; idx < exc_data_count; ++idx)
        {
            DNBLogThreadedIf(LOG_EXCEPTIONS, "            exc_data[%zu]: 0x%llx", idx, (uint64_t)exc_data[idx]);
        }
    }
}


kern_return_t
MachException::PortInfo::Save (task_t task)
{
    count = (sizeof (ports) / sizeof (ports[0]));
    DNBLogThreadedIf(LOG_EXCEPTIONS | LOG_VERBOSE, "MachException::PortInfo::Save ( task = 0x%4.4x )", task);
    DNBError err;
    err = ::task_get_exception_ports (task, EXC_MASK_ALL, masks, &count, ports, behaviors, flavors);
    if (DNBLogCheckLogBit(LOG_EXCEPTIONS) || err.Fail())
        err.LogThreaded("::task_get_exception_ports ( task = 0x%4.4x, mask = 0x%x, maskCnt => %u, ports, behaviors, flavors )", task, EXC_MASK_ALL, count);
    if (err.Fail())
        count = 0;
    return err.Error();
}

kern_return_t
MachException::PortInfo::Restore (task_t task)
{
    DNBLogThreadedIf(LOG_EXCEPTIONS | LOG_VERBOSE, "MachException::PortInfo::Restore( task = 0x%4.4x )", task);
    uint32_t i = 0;
    DNBError err;
    if (count > 0)
    {
        for (i = 0; i < count; i++)
        {
            err = ::task_set_exception_ports (task, masks[i], ports[i], behaviors[i], flavors[i]);
            if (DNBLogCheckLogBit(LOG_EXCEPTIONS) || err.Fail())
            {
                err.LogThreaded("::task_set_exception_ports ( task = 0x%4.4x, exception_mask = 0x%8.8x, new_port = 0x%4.4x, behavior = 0x%8.8x, new_flavor = 0x%8.8x )", task, masks[i], ports[i], behaviors[i], flavors[i]);
                // Bail if we encounter any errors
            }

            if (err.Fail())
                break;
        }
    }
    count = 0;
    return err.Error();
}

const char *
MachException::Name(exception_type_t exc_type)
{
    switch (exc_type)
    {
    case EXC_BAD_ACCESS:        return "EXC_BAD_ACCESS";
    case EXC_BAD_INSTRUCTION:   return "EXC_BAD_INSTRUCTION";
    case EXC_ARITHMETIC:        return "EXC_ARITHMETIC";
    case EXC_EMULATION:         return "EXC_EMULATION";
    case EXC_SOFTWARE:          return "EXC_SOFTWARE";
    case EXC_BREAKPOINT:        return "EXC_BREAKPOINT";
    case EXC_SYSCALL:           return "EXC_SYSCALL";
    case EXC_MACH_SYSCALL:      return "EXC_MACH_SYSCALL";
    case EXC_RPC_ALERT:         return "EXC_RPC_ALERT";
#ifdef EXC_CRASH
    case EXC_CRASH:             return "EXC_CRASH";
#endif
    default:
        break;
    }
    return NULL;
}


