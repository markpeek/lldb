//===-- IRDynamicChecks.cpp -------------------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "lldb/Expression/IRDynamicChecks.h"

#include "lldb/Core/ConstString.h"
#include "lldb/Core/Log.h"
#include "lldb/Expression/ClangUtilityFunction.h"
#include "lldb/Target/ExecutionContext.h"
#include "lldb/Target/ObjCLanguageRuntime.h"
#include "lldb/Target/Process.h"
#include "lldb/Target/StackFrame.h"

#include "llvm/Support/raw_ostream.h"
#include "llvm/Constants.h"
#include "llvm/Function.h"
#include "llvm/Instructions.h"
#include "llvm/Module.h"
#include "llvm/Value.h"

using namespace llvm;
using namespace lldb_private;

static char ID;

#define VALID_POINTER_CHECK_NAME "$__lldb_valid_pointer_check"
#define VALID_OBJC_OBJECT_CHECK_NAME "$__lldb_objc_object_check"

static const char g_valid_pointer_check_text[] = 
"extern \"C\" void\n"
"$__lldb_valid_pointer_check (unsigned char *$__lldb_arg_ptr)\n"
"{\n"
"    unsigned char $__lldb_local_val = *$__lldb_arg_ptr;\n"
"}";

DynamicCheckerFunctions::DynamicCheckerFunctions ()
{
}

DynamicCheckerFunctions::~DynamicCheckerFunctions ()
{
}

bool
DynamicCheckerFunctions::Install(Stream &error_stream,
                                 ExecutionContext &exe_ctx)
{
    m_valid_pointer_check.reset(new ClangUtilityFunction(g_valid_pointer_check_text,
                                                         VALID_POINTER_CHECK_NAME));
    if (!m_valid_pointer_check->Install(error_stream, exe_ctx))
        return false;
    
    Process *process = exe_ctx.GetProcessPtr();

    if (process)
    {
        ObjCLanguageRuntime *objc_language_runtime = process->GetObjCLanguageRuntime();
        
        if (objc_language_runtime)
        {
            m_objc_object_check.reset(objc_language_runtime->CreateObjectChecker(VALID_OBJC_OBJECT_CHECK_NAME));
            
            if (!m_objc_object_check->Install(error_stream, exe_ctx))
                return false;
        }
    }
        
    return true;
}

bool
DynamicCheckerFunctions::DoCheckersExplainStop (lldb::addr_t addr, Stream &message)
{
    // FIXME: We have to get the checkers to know why they scotched the call in more detail,
    // so we can print a better message here.
    if (m_valid_pointer_check.get() != NULL && m_valid_pointer_check->ContainsAddress(addr))
    {
        message.Printf ("Attempted to dereference an invalid pointer.");
        return true;
    }
    else if (m_objc_object_check.get() != NULL && m_objc_object_check->ContainsAddress(addr))
    {
        message.Printf ("Attempted to dereference an invalid ObjC Object or send it an unrecognized selector");
        return true;
    }
    return false;
}


static std::string 
PrintValue(llvm::Value *V, bool truncate = false)
{
    std::string s;
    raw_string_ostream rso(s);
    V->print(rso);
    rso.flush();
    if (truncate)
        s.resize(s.length() - 1);
    return s;
}

//----------------------------------------------------------------------
/// @class Instrumenter IRDynamicChecks.cpp
/// @brief Finds and instruments individual LLVM IR instructions
///
/// When instrumenting LLVM IR, it is frequently desirable to first search
/// for instructions, and then later modify them.  This way iterators
/// remain intact, and multiple passes can look at the same code base without
/// treading on each other's toes.
///
/// The Instrumenter class implements this functionality.  A client first
/// calls Inspect on a function, which populates a list of instructions to
/// be instrumented.  Then, later, when all passes' Inspect functions have
/// been called, the client calls Instrument, which adds the desired
/// instrumentation.
///
/// A subclass of Instrumenter must override InstrumentInstruction, which
/// is responsible for adding whatever instrumentation is necessary.
///
/// A subclass of Instrumenter may override:
///
/// - InspectInstruction [default: does nothing]
///
/// - InspectBasicBlock [default: iterates through the instructions in a 
///   basic block calling InspectInstruction]
///
/// - InspectFunction [default: iterates through the basic blocks in a 
///   function calling InspectBasicBlock]
//----------------------------------------------------------------------
class Instrumenter {
public:
    //------------------------------------------------------------------
    /// Constructor
    ///
    /// @param[in] module
    ///     The module being instrumented.
    //------------------------------------------------------------------
    Instrumenter (llvm::Module &module,
                  DynamicCheckerFunctions &checker_functions) :
        m_module(module),
        m_checker_functions(checker_functions),
        m_i8ptr_ty(NULL)
    {
    }
    
    virtual~Instrumenter ()
    {
    }

    //------------------------------------------------------------------
    /// Inspect a function to find instructions to instrument
    ///
    /// @param[in] function
    ///     The function to inspect.
    ///
    /// @return
    ///     True on success; false on error.
    //------------------------------------------------------------------
    bool Inspect (llvm::Function &function)
    {
        return InspectFunction(function);
    }
    
    //------------------------------------------------------------------
    /// Instrument all the instructions found by Inspect()
    ///
    /// @return
    ///     True on success; false on error.
    //------------------------------------------------------------------
    bool Instrument ()
    {
        for (InstIterator ii = m_to_instrument.begin(), last_ii = m_to_instrument.end();
             ii != last_ii;
             ++ii)
        {
            if (!InstrumentInstruction(*ii))
                return false;
        }
        
        return true;
    }
protected:
    //------------------------------------------------------------------
    /// Add instrumentation to a single instruction
    ///
    /// @param[in] inst
    ///     The instruction to be instrumented. 
    ///
    /// @return
    ///     True on success; false otherwise.
    //------------------------------------------------------------------
    virtual bool InstrumentInstruction(llvm::Instruction *inst) = 0;
    
    //------------------------------------------------------------------
    /// Register a single instruction to be instrumented
    ///
    /// @param[in] inst
    ///     The instruction to be instrumented.
    //------------------------------------------------------------------
    void RegisterInstruction(llvm::Instruction &i)
    {
        m_to_instrument.push_back(&i);
    }
    
    //------------------------------------------------------------------
    /// Determine whether a single instruction is interesting to
    /// instrument, and, if so, call RegisterInstruction
    ///
    /// @param[in] i
    ///     The instruction to be inspected.
    ///
    /// @return
    ///     False if there was an error scanning; true otherwise.
    //------------------------------------------------------------------
    virtual bool InspectInstruction(llvm::Instruction &i)
    {
        return true;
    }
    
    //------------------------------------------------------------------
    /// Scan a basic block to see if any instructions are interesting
    ///
    /// @param[in] bb
    ///     The basic block to be inspected.
    ///
    /// @return
    ///     False if there was an error scanning; true otherwise.
    //------------------------------------------------------------------
    virtual bool InspectBasicBlock(llvm::BasicBlock &bb)
    {
        for (llvm::BasicBlock::iterator ii = bb.begin(), last_ii = bb.end();
             ii != last_ii;
             ++ii)
        {
            if (!InspectInstruction(*ii))
                return false;
        }
        
        return true;
    }
    
    //------------------------------------------------------------------
    /// Scan a function to see if any instructions are interesting
    ///
    /// @param[in] f
    ///     The function to be inspected. 
    ///
    /// @return
    ///     False if there was an error scanning; true otherwise.
    //------------------------------------------------------------------
    virtual bool InspectFunction(llvm::Function &f)
    {
        for (llvm::Function::iterator bbi = f.begin(), last_bbi = f.end();
             bbi != last_bbi;
             ++bbi)
        {
            if (!InspectBasicBlock(*bbi))
                return false;
        }
        
        return true;
    }
    
    //------------------------------------------------------------------
    /// Build a function pointer for a function with signature 
    /// void (*)(uint8_t*) with a given address
    ///
    /// @param[in] start_address
    ///     The address of the function.
    ///
    /// @return
    ///     The function pointer, for use in a CallInst.
    //------------------------------------------------------------------
    llvm::Value *BuildPointerValidatorFunc(lldb::addr_t start_address)
    {
        IntegerType *intptr_ty = llvm::Type::getIntNTy(m_module.getContext(),
                                                             (m_module.getPointerSize() == llvm::Module::Pointer64) ? 64 : 32);
        
        llvm::Type *param_array[1];
        
        param_array[0] = const_cast<llvm::PointerType*>(GetI8PtrTy());
        
        ArrayRef<llvm::Type*> params(param_array, 1);
        
        FunctionType *fun_ty = FunctionType::get(llvm::Type::getVoidTy(m_module.getContext()), params, true);
        PointerType *fun_ptr_ty = PointerType::getUnqual(fun_ty);
        Constant *fun_addr_int = ConstantInt::get(intptr_ty, start_address, false);
        return ConstantExpr::getIntToPtr(fun_addr_int, fun_ptr_ty);
    }
    
    //------------------------------------------------------------------
    /// Build a function pointer for a function with signature 
    /// void (*)(uint8_t*, uint8_t*) with a given address
    ///
    /// @param[in] start_address
    ///     The address of the function.
    ///
    /// @return
    ///     The function pointer, for use in a CallInst.
    //------------------------------------------------------------------
    llvm::Value *BuildObjectCheckerFunc(lldb::addr_t start_address)
    {
        IntegerType *intptr_ty = llvm::Type::getIntNTy(m_module.getContext(),
                                                       (m_module.getPointerSize() == llvm::Module::Pointer64) ? 64 : 32);
        
        llvm::Type *param_array[2];
        
        param_array[0] = const_cast<llvm::PointerType*>(GetI8PtrTy());
        param_array[1] = const_cast<llvm::PointerType*>(GetI8PtrTy());
        
        ArrayRef<llvm::Type*> params(param_array, 2);
        
        FunctionType *fun_ty = FunctionType::get(llvm::Type::getVoidTy(m_module.getContext()), params, true);
        PointerType *fun_ptr_ty = PointerType::getUnqual(fun_ty);
        Constant *fun_addr_int = ConstantInt::get(intptr_ty, start_address, false);
        return ConstantExpr::getIntToPtr(fun_addr_int, fun_ptr_ty);
    }
    
    PointerType *GetI8PtrTy()
    {
        if (!m_i8ptr_ty)
            m_i8ptr_ty = llvm::Type::getInt8PtrTy(m_module.getContext());
            
        return m_i8ptr_ty;
    }
    
    typedef std::vector <llvm::Instruction *>   InstVector;
    typedef InstVector::iterator                InstIterator;
    
    InstVector                  m_to_instrument;        ///< List of instructions the inspector found
    llvm::Module               &m_module;               ///< The module which is being instrumented
    DynamicCheckerFunctions    &m_checker_functions;    ///< The dynamic checker functions for the process
private:
    PointerType                *m_i8ptr_ty;
};

class ValidPointerChecker : public Instrumenter
{
public:
    ValidPointerChecker (llvm::Module &module,
                         DynamicCheckerFunctions &checker_functions) :
        Instrumenter(module, checker_functions),
        m_valid_pointer_check_func(NULL)
    {
    }
    
    virtual ~ValidPointerChecker ()
    {
    }
private:
    bool InstrumentInstruction(llvm::Instruction *inst)
    {
        lldb::LogSP log(lldb_private::GetLogIfAllCategoriesSet (LIBLLDB_LOG_EXPRESSIONS));

        if (log)
            log->Printf("Instrumenting load/store instruction: %s\n", 
                        PrintValue(inst).c_str());
        
        if (!m_valid_pointer_check_func)
            m_valid_pointer_check_func = BuildPointerValidatorFunc(m_checker_functions.m_valid_pointer_check->StartAddress());
        
        llvm::Value *dereferenced_ptr = NULL;
        
        if (llvm::LoadInst *li = dyn_cast<llvm::LoadInst> (inst))
            dereferenced_ptr = li->getPointerOperand();
        else if (llvm::StoreInst *si = dyn_cast<llvm::StoreInst> (inst))
            dereferenced_ptr = si->getPointerOperand();
        else
            return false;
        
        // Insert an instruction to cast the loaded value to int8_t*
        
        BitCastInst *bit_cast = new BitCastInst(dereferenced_ptr,
                                                GetI8PtrTy(),
                                                "",
                                                inst);
        
        // Insert an instruction to call the helper with the result
        
        llvm::Value *arg_array[1];
        
        arg_array[0] = bit_cast;
        
        llvm::ArrayRef<llvm::Value *> args(arg_array, 1);
        
        CallInst::Create(m_valid_pointer_check_func, 
                         args,
                         "",
                         inst);
            
        return true;
    }
    
    bool InspectInstruction(llvm::Instruction &i)
    {
        if (dyn_cast<llvm::LoadInst> (&i) ||
            dyn_cast<llvm::StoreInst> (&i))
            RegisterInstruction(i);
        
        return true;
    }
    
    llvm::Value         *m_valid_pointer_check_func;
};

class ObjcObjectChecker : public Instrumenter
{
public:
    ObjcObjectChecker(llvm::Module &module,
                        DynamicCheckerFunctions &checker_functions) :
        Instrumenter(module, checker_functions),
        m_objc_object_check_func(NULL)
    {
    }
    
    virtual
    ~ObjcObjectChecker ()
    {
    }
    
    enum msgSend_type
    {
        eMsgSend = 0,
        eMsgSendSuper,
        eMsgSendSuper_stret,
        eMsgSend_fpret,
        eMsgSend_stret
    };
    
    std::map <llvm::Instruction *, msgSend_type> msgSend_types;

private:
    bool InstrumentInstruction(llvm::Instruction *inst)
    {
        CallInst *call_inst = dyn_cast<CallInst>(inst);
        
        if (!call_inst)
            return false; // call_inst really shouldn't be NULL, because otherwise InspectInstruction wouldn't have registered it
        
        if (!m_objc_object_check_func)
            m_objc_object_check_func = BuildObjectCheckerFunc(m_checker_functions.m_objc_object_check->StartAddress());
        
        // id objc_msgSend(id theReceiver, SEL theSelector, ...)
        
        llvm::Value *target_object;
        llvm::Value *selector;
        
        switch (msgSend_types[inst])
        {
        case eMsgSend:
        case eMsgSend_fpret:
            target_object = call_inst->getArgOperand(0);
            selector = call_inst->getArgOperand(1);
            break;
        case eMsgSend_stret:
            target_object = call_inst->getArgOperand(1);
            selector = call_inst->getArgOperand(2);
        case eMsgSendSuper:
        case eMsgSendSuper_stret:
            return true;
        }
                
        // Insert an instruction to cast the receiver id to int8_t*
        
        BitCastInst *bit_cast = new BitCastInst(target_object,
                                                GetI8PtrTy(),
                                                "",
                                                inst);
        
        // Insert an instruction to call the helper with the result
        
        llvm::Value *arg_array[2];
        
        arg_array[0] = bit_cast;
        arg_array[1] = selector;
        
        ArrayRef<llvm::Value*> args(arg_array, 2);
        
        CallInst::Create(m_objc_object_check_func, 
                         args,
                         "",
                         inst);
        
        return true;
    }
    
    bool InspectInstruction(llvm::Instruction &i)
    {
        lldb::LogSP log(lldb_private::GetLogIfAllCategoriesSet (LIBLLDB_LOG_EXPRESSIONS));

        CallInst *call_inst = dyn_cast<CallInst>(&i);
        
        if (call_inst)
        {
            // This metadata is set by IRForTarget::MaybeHandleCall().
            
            MDNode *metadata = call_inst->getMetadata("lldb.call.realName");
                        
            if (!metadata)
                return true;
            
            if (metadata->getNumOperands() != 1)
            {
                if (log)
                    log->Printf("Function call metadata has %d operands for [%p] %s", metadata->getNumOperands(), call_inst, PrintValue(call_inst).c_str());
                return false;
            }
            
            ConstantArray *real_name = dyn_cast<ConstantArray>(metadata->getOperand(0));
            
            if (!real_name)
            {
                if (log)
                    log->Printf("Function call metadata is not a ConstantArray for [%p] %s", call_inst, PrintValue(call_inst).c_str());
                return false;
            }
            
            if (!real_name->isString())
            {
                if (log)
                    log->Printf("Function call metadata is not a string for [%p] %s", call_inst, PrintValue(call_inst).c_str());
                return false;
            }
            
            if (log)
                log->Printf("Found call to %s: %s\n", real_name->getAsString().c_str(), PrintValue(call_inst).c_str());
            
            std::string name_str = real_name->getAsString();
            const char* name_cstr = name_str.c_str();
            
            if (name_str.find("objc_msgSend") == std::string::npos)
                return true;
            
            if (!strcmp(name_cstr, "objc_msgSend"))
            {
                RegisterInstruction(i);
                msgSend_types[&i] = eMsgSend;
                return true;
            }
            
            if (!strcmp(name_cstr, "objc_msgSend_stret"))
            {
                RegisterInstruction(i);
                msgSend_types[&i] = eMsgSend_stret;
                return true;
            }
            
            if (!strcmp(name_cstr, "objc_msgSend_fpret"))
            {
                RegisterInstruction(i);
                msgSend_types[&i] = eMsgSend_fpret;
                return true;
            }
            
            if (!strcmp(name_cstr, "objc_msgSendSuper"))
            {
                RegisterInstruction(i);
                msgSend_types[&i] = eMsgSendSuper;
                return true;
            }
            
            if (!strcmp(name_cstr, "objc_msgSendSuper_stret"))
            {
                RegisterInstruction(i);
                msgSend_types[&i] = eMsgSendSuper_stret;
                return true;
            }
            
            if (log)
                log->Printf("Function name '%s' contains 'objc_msgSend' but is not handled", name_str.c_str());
            
            return true;
        }
        
        return true;
    }
    
    llvm::Value         *m_objc_object_check_func;
};

IRDynamicChecks::IRDynamicChecks(DynamicCheckerFunctions &checker_functions,
                                 const char *func_name) :
    ModulePass(ID),
    m_func_name(func_name),
    m_checker_functions(checker_functions)
{
}

IRDynamicChecks::~IRDynamicChecks()
{
}

bool
IRDynamicChecks::runOnModule(llvm::Module &M)
{
    lldb::LogSP log(lldb_private::GetLogIfAllCategoriesSet (LIBLLDB_LOG_EXPRESSIONS));
    
    llvm::Function* function = M.getFunction(StringRef(m_func_name.c_str()));
    
    if (!function)
    {
        if (log)
            log->Printf("Couldn't find %s() in the module", m_func_name.c_str());
        
        return false;
    }

    ValidPointerChecker vpc(M, m_checker_functions);
    
    if (!vpc.Inspect(*function))
        return false;
    
    if (!vpc.Instrument())
        return false;
    
    ObjcObjectChecker ooc(M, m_checker_functions);
    
    if (!ooc.Inspect(*function))
        return false;
    
    if (!ooc.Instrument())
        return false;

    if (log && log->GetVerbose())
    {
        std::string s;
        raw_string_ostream oss(s);
        
        M.print(oss, NULL);
        
        oss.flush();
        
        log->Printf ("Module after dynamic checks: \n%s", s.c_str());
    }
    
    return true;    
}

void
IRDynamicChecks::assignPassManager(PMStack &PMS,
                                   PassManagerType T)
{
}

PassManagerType
IRDynamicChecks::getPotentialPassManagerType() const
{
    return PMT_ModulePassManager;
}