//===-- UnwindAssemblyInstEmulation.cpp --------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "UnwindAssemblyInstEmulation.h"

#include "llvm-c/EnhancedDisassembly.h"

#include "lldb/Core/Address.h"
#include "lldb/Core/ArchSpec.h"
#include "lldb/Core/DataBufferHeap.h"
#include "lldb/Core/Disassembler.h"
#include "lldb/Core/Error.h"
#include "lldb/Core/Log.h"
#include "lldb/Core/PluginManager.h"
#include "lldb/Core/StreamString.h"
#include "lldb/Target/ExecutionContext.h"
#include "lldb/Target/Process.h"
#include "lldb/Target/Thread.h"
#include "lldb/Target/Target.h"

using namespace lldb;
using namespace lldb_private;



//-----------------------------------------------------------------------------------------------
//  UnwindAssemblyParser_x86 method definitions 
//-----------------------------------------------------------------------------------------------

bool
UnwindAssemblyInstEmulation::GetNonCallSiteUnwindPlanFromAssembly (AddressRange& range, 
                                                                   Thread& thread, 
                                                                   UnwindPlan& unwind_plan)
{
    if (range.GetByteSize() > 0 && 
        range.GetBaseAddress().IsValid() &&
        m_inst_emulator_ap.get())
    {
     
        // The the instruction emulation subclass setup the unwind plan for the
        // first instruction.
        m_inst_emulator_ap->CreateFunctionEntryUnwind (unwind_plan);

        // CreateFunctionEntryUnwind should have created the first row. If it
        // doesn't, then we are done.
        if (unwind_plan.GetRowCount() == 0)
            return false;
        
        ExecutionContext exe_ctx;
        thread.CalculateExecutionContext(exe_ctx);
        DisassemblerSP disasm_sp (Disassembler::DisassembleRange (m_arch,
                                                                  NULL,
                                                                  exe_ctx,
                                                                  range));
        
        LogSP log(GetLogIfAllCategoriesSet (LIBLLDB_LOG_UNWIND));

        if (disasm_sp)
        {
            
            m_range_ptr = &range;
            m_thread_ptr = &thread;
            m_unwind_plan_ptr = &unwind_plan;

            const uint32_t addr_byte_size = m_arch.GetAddressByteSize();
            const bool show_address = true;
            const bool show_bytes = true;
            const bool raw = false;
            // Initialize the CFA with a known value. In the 32 bit case
            // it will be 0x80000000, and in the 64 bit case 0x8000000000000000.
            // We use the address byte size to be safe for any future addresss sizes
            m_inst_emulator_ap->GetRegisterInfo (unwind_plan.GetRegisterKind(), 
                                                 unwind_plan.GetInitialCFARegister(), 
                                                 m_cfa_reg_info);
            
            m_fp_is_cfa = false;
            m_register_values.clear();
            m_pushed_regs.clear();

            m_initial_sp = (1ull << ((addr_byte_size * 8) - 1));
            RegisterValue cfa_reg_value;
            cfa_reg_value.SetUInt (m_initial_sp, m_cfa_reg_info.byte_size);
            SetRegisterValue (m_cfa_reg_info, cfa_reg_value);
                
            const InstructionList &inst_list = disasm_sp->GetInstructionList ();
            const size_t num_instructions = inst_list.GetSize();
            if (num_instructions > 0)
            {
                Instruction *inst = inst_list.GetInstructionAtIndex (0).get();
                const addr_t base_addr = inst->GetAddress().GetFileAddress();
                // Initialize the current row with the one row that was created
                // from the CreateFunctionEntryUnwind call above...
                m_curr_row = unwind_plan.GetLastRow();

                for (size_t idx=0; idx<num_instructions; ++idx)
                {
                    inst = inst_list.GetInstructionAtIndex (idx).get();
                    if (inst)
                    {

                        if (log && log->GetVerbose ())
                        {
                            StreamString strm;
                            inst->Dump(&strm, inst_list.GetMaxOpcocdeByteSize (), show_address, show_bytes, &exe_ctx, raw);
                            log->PutCString (strm.GetData());
                        }

                        m_inst_emulator_ap->SetInstruction (inst->GetOpcode(), 
                                                            inst->GetAddress(), 
                                                            exe_ctx.GetTargetPtr());

                        m_inst_emulator_ap->EvaluateInstruction (eEmulateInstructionOptionIgnoreConditions);
                        
                        if (unwind_plan.GetLastRow() != m_curr_row)
                        {
                            // Be sure to not edit the offset unless our row has changed
                            // so that the "!=" call above doesn't trigger every time
                            m_curr_row.SetOffset (inst->GetAddress().GetFileAddress() + inst->GetOpcode().GetByteSize() - base_addr);
                            // Append the new row
                            unwind_plan.AppendRow (m_curr_row);
                        }
                    }
                }
            }
        }
        
        if (log && log->GetVerbose ())
        {
            StreamString strm;
            lldb::addr_t base_addr = range.GetBaseAddress().GetLoadAddress(&thread.GetProcess().GetTarget());
            strm.Printf ("Resulting unwind rows for [0x%llx - 0x%llx):", base_addr, base_addr + range.GetByteSize());
            unwind_plan.Dump(strm, &thread, base_addr);
            log->PutCString (strm.GetData());
        }
        return unwind_plan.GetRowCount() > 0;
    }
    return false;
}

bool
UnwindAssemblyInstEmulation::GetFastUnwindPlan (AddressRange& func, 
                                                Thread& thread, 
                                                UnwindPlan &unwind_plan)
{
    return false;
}

bool
UnwindAssemblyInstEmulation::FirstNonPrologueInsn (AddressRange& func, 
                                                   Target& target, 
                                                   Thread* thread, 
                                                   Address& first_non_prologue_insn)
{
    return false;
}

UnwindAssembly *
UnwindAssemblyInstEmulation::CreateInstance (const ArchSpec &arch)
{
    std::auto_ptr<EmulateInstruction> inst_emulator_ap (EmulateInstruction::FindPlugin (arch, eInstructionTypePrologueEpilogue, NULL));
    // Make sure that all prologue instructions are handled
    if (inst_emulator_ap.get())
        return new UnwindAssemblyInstEmulation (arch, inst_emulator_ap.release());
    return NULL;
}


//------------------------------------------------------------------
// PluginInterface protocol in UnwindAssemblyParser_x86
//------------------------------------------------------------------

const char *
UnwindAssemblyInstEmulation::GetPluginName()
{
    return "UnwindAssemblyInstEmulation";
}

const char *
UnwindAssemblyInstEmulation::GetShortPluginName()
{
    return "unwindassembly.inst-emulation";
}


uint32_t
UnwindAssemblyInstEmulation::GetPluginVersion()
{
    return 1;
}

void
UnwindAssemblyInstEmulation::Initialize()
{
    PluginManager::RegisterPlugin (GetPluginNameStatic(),
                                   GetPluginDescriptionStatic(),
                                   CreateInstance);
}

void
UnwindAssemblyInstEmulation::Terminate()
{
    PluginManager::UnregisterPlugin (CreateInstance);
}


const char *
UnwindAssemblyInstEmulation::GetPluginNameStatic()
{
    return "UnwindAssemblyInstEmulation";
}

const char *
UnwindAssemblyInstEmulation::GetPluginDescriptionStatic()
{
    return "Instruction emulation based unwind information.";
}


uint64_t 
UnwindAssemblyInstEmulation::MakeRegisterKindValuePair (const RegisterInfo &reg_info)
{
    uint32_t reg_kind, reg_num;
    if (EmulateInstruction::GetBestRegisterKindAndNumber (&reg_info, reg_kind, reg_num))
        return (uint64_t)reg_kind << 24 | reg_num;
    return 0ull;
}

void
UnwindAssemblyInstEmulation::SetRegisterValue (const RegisterInfo &reg_info, const RegisterValue &reg_value)
{
    m_register_values[MakeRegisterKindValuePair (reg_info)] = reg_value;
}

bool
UnwindAssemblyInstEmulation::GetRegisterValue (const RegisterInfo &reg_info, RegisterValue &reg_value)
{
    const uint64_t reg_id = MakeRegisterKindValuePair (reg_info);
    RegisterValueMap::const_iterator pos = m_register_values.find(reg_id);
    if (pos != m_register_values.end())
    {
        reg_value = pos->second;
        return true; // We had a real value that comes from an opcode that wrote
                     // to it...
    }
    // We are making up a value that is recognizable...
    reg_value.SetUInt(reg_id, reg_info.byte_size);
    return false;
}


size_t
UnwindAssemblyInstEmulation::ReadMemory (EmulateInstruction *instruction,
                                         void *baton,
                                         const EmulateInstruction::Context &context, 
                                         lldb::addr_t addr, 
                                         void *dst,
                                         size_t dst_len)
{
    LogSP log(GetLogIfAllCategoriesSet (LIBLLDB_LOG_UNWIND));

    if (log && log->GetVerbose ())
    {
        StreamString strm;
        strm.Printf ("UnwindAssemblyInstEmulation::ReadMemory    (addr = 0x%16.16llx, dst = %p, dst_len = %zu, context = ", 
                     addr,
                     dst,
                     dst_len);
        context.Dump(strm, instruction);
        log->PutCString (strm.GetData ());
    }
    return dst_len;
}

size_t
UnwindAssemblyInstEmulation::WriteMemory (EmulateInstruction *instruction,
                                          void *baton,
                                          const EmulateInstruction::Context &context, 
                                          lldb::addr_t addr, 
                                          const void *dst,
                                          size_t dst_len)
{
    if (baton && dst && dst_len)
        return ((UnwindAssemblyInstEmulation *)baton)->WriteMemory (instruction, context, addr, dst, dst_len);
    return 0;
}

size_t
UnwindAssemblyInstEmulation::WriteMemory (EmulateInstruction *instruction,
                                          const EmulateInstruction::Context &context, 
                                          lldb::addr_t addr, 
                                          const void *dst,
                                          size_t dst_len)
{
    DataExtractor data (dst, 
                        dst_len, 
                        instruction->GetArchitecture ().GetByteOrder(), 
                        instruction->GetArchitecture ().GetAddressByteSize());

    LogSP log(GetLogIfAllCategoriesSet (LIBLLDB_LOG_UNWIND));

    if (log && log->GetVerbose ())
    {
        StreamString strm;

        strm.PutCString ("UnwindAssemblyInstEmulation::WriteMemory   (");
        data.Dump(&strm, 0, eFormatBytes, 1, dst_len, UINT32_MAX, addr, 0, 0);
        strm.PutCString (", context = ");
        context.Dump(strm, instruction);
        log->PutCString (strm.GetData());
    }
    
    const bool can_replace = true;
    const bool cant_replace = false;

    switch (context.type)
    {
        default:
        case EmulateInstruction::eContextInvalid:
        case EmulateInstruction::eContextReadOpcode:
        case EmulateInstruction::eContextImmediate:
        case EmulateInstruction::eContextAdjustBaseRegister:
        case EmulateInstruction::eContextRegisterPlusOffset:
        case EmulateInstruction::eContextAdjustPC:
        case EmulateInstruction::eContextRegisterStore:
        case EmulateInstruction::eContextRegisterLoad:  
        case EmulateInstruction::eContextRelativeBranchImmediate:
        case EmulateInstruction::eContextAbsoluteBranchRegister:
        case EmulateInstruction::eContextSupervisorCall:
        case EmulateInstruction::eContextTableBranchReadMemory:
        case EmulateInstruction::eContextWriteRegisterRandomBits:
        case EmulateInstruction::eContextWriteMemoryRandomBits:
        case EmulateInstruction::eContextArithmetic:
        case EmulateInstruction::eContextAdvancePC:    
        case EmulateInstruction::eContextReturnFromException:
        case EmulateInstruction::eContextPopRegisterOffStack:
        case EmulateInstruction::eContextAdjustStackPointer:
            break;
            
        case EmulateInstruction::eContextPushRegisterOnStack:
            {
                uint32_t reg_num = LLDB_INVALID_REGNUM;
                bool is_return_address_reg = false;
                const uint32_t unwind_reg_kind = m_unwind_plan_ptr->GetRegisterKind();
                if (context.info_type == EmulateInstruction::eInfoTypeRegisterToRegisterPlusOffset)
                {
                    reg_num = context.info.RegisterToRegisterPlusOffset.data_reg.kinds[unwind_reg_kind];
                    if (context.info.RegisterToRegisterPlusOffset.data_reg.kinds[eRegisterKindGeneric] == LLDB_REGNUM_GENERIC_RA)
                        is_return_address_reg = true;
                }
                else
                {
                    assert (!"unhandled case, add code to handle this!");
                }
                
                if (reg_num != LLDB_INVALID_REGNUM)
                {
                    if (m_pushed_regs.find (reg_num) == m_pushed_regs.end())
                    {
                        m_pushed_regs[reg_num] = addr;
                        const int32_t offset = addr - m_initial_sp;
                        m_curr_row.SetRegisterLocationToAtCFAPlusOffset (reg_num, offset, cant_replace);
                        if (is_return_address_reg)
                        {
                            // This push was pushing the return address register,
                            // so this is also how we will unwind the PC...
                            RegisterInfo pc_reg_info;
                            if (instruction->GetRegisterInfo (eRegisterKindGeneric, LLDB_REGNUM_GENERIC_PC, pc_reg_info))
                            {
                                uint32_t pc_reg_num = pc_reg_info.kinds[unwind_reg_kind];
                                if (pc_reg_num != LLDB_INVALID_REGNUM)
                                    m_curr_row.SetRegisterLocationToAtCFAPlusOffset (pc_reg_num, offset, can_replace);
                            }
                        }
                    }
                }
            }
            break;
            
    }

    return dst_len;
}

bool
UnwindAssemblyInstEmulation::ReadRegister (EmulateInstruction *instruction,
                                           void *baton,
                                           const RegisterInfo *reg_info,
                                           RegisterValue &reg_value)
{
    
    if (baton && reg_info)
        return ((UnwindAssemblyInstEmulation *)baton)->ReadRegister (instruction, reg_info, reg_value);
    return false;
}
bool
UnwindAssemblyInstEmulation::ReadRegister (EmulateInstruction *instruction,
                                           const RegisterInfo *reg_info,
                                           RegisterValue &reg_value)
{
    bool synthetic = GetRegisterValue (*reg_info, reg_value);

    LogSP log(GetLogIfAllCategoriesSet (LIBLLDB_LOG_UNWIND));
    
    if (log && log->GetVerbose ())
    {
        
        StreamString strm;
        strm.Printf ("UnwindAssemblyInstEmulation::ReadRegister  (name = \"%s\") => synthetic_value = %i, value = ", reg_info->name, synthetic);
        reg_value.Dump(&strm, reg_info, false, false, eFormatDefault);
        log->PutCString(strm.GetData());
    }
    return true;
}

bool
UnwindAssemblyInstEmulation::WriteRegister (EmulateInstruction *instruction,
                                            void *baton,
                                            const EmulateInstruction::Context &context, 
                                            const RegisterInfo *reg_info,
                                            const RegisterValue &reg_value)
{
    if (baton && reg_info)
        return ((UnwindAssemblyInstEmulation *)baton)->WriteRegister (instruction, context, reg_info, reg_value);
    return false;
}
bool
UnwindAssemblyInstEmulation::WriteRegister (EmulateInstruction *instruction,
                                            const EmulateInstruction::Context &context, 
                                            const RegisterInfo *reg_info,
                                            const RegisterValue &reg_value)
{
    LogSP log(GetLogIfAllCategoriesSet (LIBLLDB_LOG_UNWIND));

    if (log && log->GetVerbose ())
    {
        
        StreamString strm;
        strm.Printf ("UnwindAssemblyInstEmulation::WriteRegister (name = \"%s\", value = ", reg_info->name);
        reg_value.Dump(&strm, reg_info, false, false, eFormatDefault);
        strm.PutCString (", context = ");
        context.Dump(strm, instruction);
        log->PutCString(strm.GetData());
    }

    const bool must_replace = true;
    SetRegisterValue (*reg_info, reg_value);

    switch (context.type)
    {
        default:
        case EmulateInstruction::eContextInvalid:
        case EmulateInstruction::eContextReadOpcode:
        case EmulateInstruction::eContextImmediate:
        case EmulateInstruction::eContextAdjustBaseRegister:
        case EmulateInstruction::eContextRegisterPlusOffset:
        case EmulateInstruction::eContextAdjustPC:
        case EmulateInstruction::eContextRegisterStore:
        case EmulateInstruction::eContextRegisterLoad:  
        case EmulateInstruction::eContextRelativeBranchImmediate:
        case EmulateInstruction::eContextAbsoluteBranchRegister:
        case EmulateInstruction::eContextSupervisorCall:
        case EmulateInstruction::eContextTableBranchReadMemory:
        case EmulateInstruction::eContextWriteRegisterRandomBits:
        case EmulateInstruction::eContextWriteMemoryRandomBits:
        case EmulateInstruction::eContextArithmetic:
        case EmulateInstruction::eContextAdvancePC:    
        case EmulateInstruction::eContextReturnFromException:
        case EmulateInstruction::eContextPushRegisterOnStack:
//            {
//                const uint32_t reg_num = reg_info->kinds[m_unwind_plan_ptr->GetRegisterKind()];
//                if (reg_num != LLDB_INVALID_REGNUM)
//                {
//                    const bool can_replace_only_if_unspecified = true;
//
//                    m_curr_row.SetRegisterLocationToUndefined (reg_num, 
//                                                               can_replace_only_if_unspecified, 
//                                                               can_replace_only_if_unspecified);
//                }
//            }
            break;

        case EmulateInstruction::eContextPopRegisterOffStack:
            {
                const uint32_t reg_num = reg_info->kinds[m_unwind_plan_ptr->GetRegisterKind()];
                if (reg_num != LLDB_INVALID_REGNUM)
                {
                    m_curr_row.SetRegisterLocationToSame (reg_num, must_replace);
                }
            }
            break;

        case EmulateInstruction::eContextSetFramePointer:
            if (!m_fp_is_cfa)
            {
                m_fp_is_cfa = true;
                m_cfa_reg_info = *reg_info;
                const uint32_t cfa_reg_num = reg_info->kinds[m_unwind_plan_ptr->GetRegisterKind()];
                assert (cfa_reg_num != LLDB_INVALID_REGNUM);
                m_curr_row.SetCFARegister(cfa_reg_num);
                m_curr_row.SetCFAOffset(m_initial_sp - reg_value.GetAsUInt64());
            }
            break;

        case EmulateInstruction::eContextAdjustStackPointer:
            // If we have created a frame using the frame pointer, don't follow
            // subsequent adjustments to the stack pointer.
            if (!m_fp_is_cfa)
            {
                m_curr_row.SetCFAOffset (m_initial_sp - reg_value.GetAsUInt64());
            }
            break;
    }
    return true;
}

