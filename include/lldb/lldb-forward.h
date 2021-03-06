//===-- lldb-forward.h ------------------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef LLDB_lldb_forward_h_
#define LLDB_lldb_forward_h_

#if defined(__cplusplus)

//----------------------------------------------------------------------
// lldb forward declarations
//----------------------------------------------------------------------
namespace lldb_private {

class   ABI;
class   Address;
class   AddressImpl;
class   AddressRange;
class   AddressResolver;
class   ArchSpec;
class   Args;
class   ASTResultSynthesizer;
class   Baton;
class   Block;
class   Breakpoint;
class   BreakpointID;
class   BreakpointIDList;
class   BreakpointList;
class   BreakpointLocation;
class   BreakpointLocationCollection;
class   BreakpointLocationList;
class   BreakpointOptions;
class   BreakpointResolver;
class   BreakpointSite;
class   BreakpointSiteList;
class   Broadcaster;
class   CPPLanguageRuntime;
class   ClangASTContext;
class   ClangASTImporter;
class   ClangASTSource;
class   ClangASTType;
class   ClangNamespaceDecl;
class   ClangExpression;
class   ClangExpressionDeclMap;
class   ClangExpressionParser;
class   ClangExpressionVariable;
class   ClangExpressionVariableList;
class   ClangExpressionVariableList;
class   ClangExpressionVariables;
class   ClangFunction;
class   ClangPersistentVariables;
class   ClangUserExpression;
class   ClangUtilityFunction;
class   CommandInterpreter;
class   CommandObject;
class   CommandReturnObject;
class   Communication;
class   CompileUnit;
class   Condition;
class   Connection;
class   ConnectionFileDescriptor;
class   ConstString;
class   DWARFCallFrameInfo;
class   DWARFExpression;
class   DataBuffer;
class   DataEncoder;
class   DataExtractor;
class   Debugger;
class   Declaration;
class   Disassembler;
class   DynamicLoader;
class   EmulateInstruction;
class   Error;
class   Event;
class   EventData;
class   ExecutionContext;
class   ExecutionContextScope;
class   FileSpec;
class   FileSpecList;
class   Flags;
class   FormatCategory;
class   FormatManager;
class   FuncUnwinders;
class   Function;
class   FunctionInfo;
class   InlineFunctionInfo;
class   InputReader;
class   InstanceSettings;
class   Instruction;
class   LanguageRuntime;
class   LineTable;
class   Listener;
class   Log;
class   LogChannel;
class   Mangled;
class   Module;
class   ModuleList;
class   Mutex;
struct  NameSearchContext;
class   ObjCLanguageRuntime;
class   ObjectContainer;
class   OptionGroup;
class   OptionGroupPlatform;
class   ObjectFile;
class   OperatingSystem;
class   Options;
class   OptionValue;
class   NamedOption;
class   PathMappingList;
class   Platform;
class   Process;
class   ProcessAttachInfo;
class   ProcessModID;
class   ProcessInfo;
class   ProcessInstanceInfo;
class   ProcessInstanceInfoList;
class   ProcessInstanceInfoMatch;
class   ProcessLaunchInfo;
class   RegisterContext;
class   RegisterLocation;
class   RegisterLocationList;
class   RegisterValue;
class   RegularExpression;
class   Scalar;
class   ScriptInterpreter;
#ifndef LLDB_DISABLE_PYTHON
class   ScriptInterpreterPython;
struct  ScriptSummaryFormat;
#endif
class   SearchFilter;
class   Section;
class   SectionImpl;
class   SectionList;
class   SourceManager;
class   SourceManagerImpl;
class   StackFrame;
class   StackFrameList;
class   StackID;
class   StopInfo;
class   Stoppoint;
class   StoppointCallbackContext;
class   StoppointLocation;
class   Stream;
template <unsigned N> class StreamBuffer;
class   StreamFile;
class   StreamString;
class   StringList;
struct  StringSummaryFormat;
struct  SummaryFormat;
class   Symbol;
class   SymbolContext;
class   SymbolContextList;
class   SymbolContextScope;
class   SymbolContextSpecifier;
class   SymbolFile;
class   SymbolFileType;
class   SymbolVendor;
class   Symtab;
class   SyntheticChildren;
class   SyntheticChildrenFrontEnd;
#ifndef LLDB_DISABLE_PYTHON
class   SyntheticScriptProvider;
#endif
class   Target;
class   TargetList;
class   Thread;
class   ThreadList;
class   ThreadPlan;
class   ThreadPlanBase;
class   ThreadPlanRunToAddress;
class   ThreadPlanStepInstruction;
class   ThreadPlanStepOut;
class   ThreadPlanStepOverBreakpoint;
class   ThreadPlanStepRange;
class   ThreadPlanStepThrough;
class   ThreadPlanTracer;
class   ThreadSpec;
class   TimeValue;
class   Type;
class   TypeImpl;
class   TypeAndOrName;
class   TypeList;
class   TypeListImpl;
class   TypeMemberImpl;    
class   UUID;
class   Unwind;
class   UnwindAssembly;
class   UnwindPlan;
class   UnwindTable;
class   UserSettingsController;
class   VMRange;
class   Value;
struct  ValueFormat;
class   ValueList;
class   ValueObject;
class   ValueObjectChild;
class   ValueObjectConstResult;
class   ValueObjectConstResultChild;
class   ValueObjectConstResultImpl;
class   ValueObjectList;
class   Variable;
class   VariableList;
class   Watchpoint;
class   WatchpointList;
struct  LineEntry;

} // namespace lldb_private

#endif  // #if defined(__cplusplus)
#endif  // LLDB_lldb_forward_h_
