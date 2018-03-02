#include "basic.h"
#include "parser.hpp"
#include <llvm/IR/ValueSymbolTable.h>

using namespace basic;
using namespace llvm;

static int BASIC_INTERPRETER_VERSION = 0x06;

typedef struct yy_buffer_state *YY_BUFFER_STATE;
extern YY_BUFFER_STATE yy_scan_string(const char* strBuff);
extern void yy_delete_buffer(YY_BUFFER_STATE pbuff);

void yyerror(basic::interpreter* p, const char* msg)
{
    std::cerr << "error: " << msg << "\n";
}

extern interpreter* interp;
interpreter::interpreter(const char* modname)
{
    sp_mod = std::make_unique<Module>(modname, *this);
    m_fmain = std::unique_ptr<Function>(Function::Create(
            FunctionType::get(Type::getVoidTy(*this), false),
            Function::ExternalLinkage,
            "main",
            sp_mod.get()));
    m_entry = std::unique_ptr<BasicBlock>(BasicBlock::Create(*this, "entry", m_fmain.get()));

    m_blockStack.push_front(m_entry.get());
    // prepare the default variable map
    m_varMap[m_entry.get()] = VariableList();
    std::cerr << "basic::interpreter ready\n";
    interp = this;
}

interpreter::~interpreter()
{
    m_fmain->removeFromParent();
    m_fmain.release();
    std::cerr << "interpreter deleted\n";
    interp = nullptr;
}

int interpreter::eval(const std::string& strLine)
{
    YY_BUFFER_STATE state = yy_scan_string(strLine.c_str());
    int result = yyparse(this);
    yy_delete_buffer(state);
    return result;
}

void interpreter::print_version(std::ostream& os)
{
    os << "Basic Shell Interpreter Version "
        << ((BASIC_INTERPRETER_VERSION >> 16) & 0xff) << "."
        << ((BASIC_INTERPRETER_VERSION >> 8) & 0xff) << "."
        << (BASIC_INTERPRETER_VERSION & 0xff);
}

BasicBlock* interpreter::get_current_block()
{
    return m_blockStack.front();
}

void interpreter::map_variable_name(BasicBlock* bb, const char* pszName, AllocaInst* pa)
{
    /**
     * IMPORTANT:
     * we dont need to save anything, the name->Value pair
     * can be asked from BasicBlock->ValueSymbolTable,
     * using the following way:
     */

    ValueSymbolTable* vs = bb->getValueSymbolTable();
    if (vs)
    {
        Value* pv = vs->lookup(pszName);
        std::cerr << pszName << " -> " << pa << " -> " << pv << "\n";
    }
}

llvm::Value* interpreter::get_variable(llvm::BasicBlock* bb, const char* pszname)
{
    ValueSymbolTable* vs = bb->getValueSymbolTable();
    if (vs)
    {
        return vs->lookup(pszname);
    }
    return nullptr;
}

void interpreter::print_module(std::string& buffer)
{
    raw_string_ostream rso(buffer);
    sp_mod->print(rso, nullptr);
}

llvm::Type* interpreter::get_llvm_type(int nId)
{
    switch (nId)
    {
    case BYTE:
    case BOOLEAN:
        return Type::getInt8Ty(*this);
    case INTEGER:
        return Type::getInt32Ty(*this);
    case LONG:
        return Type::getInt64Ty(*this);
    case SINGLE:
        return Type::getFloatTy(*this);
    case DOUBLE:
        return Type::getDoubleTy(*this);
    case STRING:
        return Type::getInt8PtrTy(*this);
    default:
        std::cerr << "Invalid BASIC Type: " << nId << ", returning llvm::Void Type (Any)\n";
        return Type::getVoidTy(*this);
    }
}

llvm::BasicBlock* interpreter::get_entry()
{
    return m_entry.get();
}

llvm::Value* interpreter::assign_value(Value* lhs, Value* pVal)
{
    llvm::IRBuilder<> builder(get_current_block());

    Type* dest = lhs->getType();
    Type* src = pVal->getType();

    Value* rhs = pVal;

    // Actually we MUST reject assignment to LHS that is not
    // a value of type AllocaInst, because only a VAR can be assigned
    // NOT a Constant.
    //
    // A user can make such call:
    // 12 = 5.8
    // and the like, and that should be interpreted
    // as a COMPARISON operation, NOT an assignment.
    //
    // BASIC has an ambiguious meaning for operator '=', in which
    // it could be interpreted as both, but a careful examination
    // should be able to determine which is which.
    if (AllocaInst::classof(lhs))
    {
        // this is how to do safety dynamic_cast
        AllocaInst* inst = static_cast<AllocaInst*>(lhs);
        dest = inst->getAllocatedType();
    }
    if (AllocaInst::classof(pVal))
    {
        std::cerr << "<==> RHS is a VAR\n";
        AllocaInst* inst = static_cast<AllocaInst*>(pVal);
        src = inst->getAllocatedType();
        rhs = builder.CreateLoad(pVal);
    }

    std::cerr << "LHS TypeID = " << dest->getTypeID()
        << " RHS TypeID = " << src->getTypeID() << "\n";

    if (src == dest)
    {
        llvm::StoreInst* store = builder.CreateStore(rhs, lhs);

        // output the store for debugging
        std::string strTest("store => ");
        raw_string_ostream rso(strTest);
        store->print(rso);
        std::cerr << strTest << "\n";
        return static_cast<Value*>(store);
    }

    // At least there are a few possibilities here:
    //
    // This var is LONG and the given value is
    //    - i32 (very common and we most likely will get this).
    //    - float (uncommon, but possible)
    //    - double (common, because the lexer using this type as default floating-point values).
    //    - i8* (string), should be marked as invalid operation.
    //    - other pointer types
    // So we have to cast those possible types into this using:
    //    - CastInst::CreateSExtOrBitCast - to upcast int32 to int64
    //    - CastInst::TruncOrBitCast - to truncate double
    // If we want to make everything simple, just cast all numbers into double,
    // before casting to this type.
    // But that would be odd, isn't it.
    // :-)
    // ------------------------------------------------------------------------------------------
    //
    Value* castedVal = nullptr;

    std::string strTest("cast => ");
    raw_string_ostream rs(strTest);

    if (dest->isIntegerTy())
    {
        if (src->isFloatingPointTy())
            castedVal = builder.CreateCast(Instruction::FPToSI, rhs, dest, "tmp");
        else if (src->isArrayTy())
        {
            std::cerr << "Could not store a String or Array in an Integer types.\n";
            return lhs;
        }
        else
            castedVal = builder.CreateIntCast(rhs, dest, true);

        castedVal->print(rs);
        std::cerr << strTest << "\n";
        strTest = "=> ";

        StoreInst* store = builder.CreateStore(castedVal, lhs);
        store->print(rs);
        std::cout << strTest << "\n";
        return static_cast<Value*>(store);
    }
    else if (dest->isFloatingPointTy())
    {
        if (src->isFloatingPointTy())
        {
            // if pVal is also floating-point, then we just upcast
            // or downcast the value.
            castedVal = builder.CreateFPCast(rhs, dest);
        }
        else if (src->isIntOrIntVectorTy())
        {
            castedVal = builder.CreateSIToFP(rhs, dest);
        }
        return static_cast<Value*>(builder.CreateStore(castedVal, lhs));
    }
    else if (dest->isPointerTy())
    {
        if (!src->isArrayTy())
        {
            StringRef destName = lhs->getName();
            strTest = "Could not assign ";
            pVal->print(rs);
            rs << " to " << destName;
            strTest += "\n";
            std::cerr << strTest << "\n";
            return lhs;
        }

        // Now we should create a GEP and assign it to LHS.
        // The string in the RHS must be put into GlobalVariable first,
        // otherwise, it is just cannot be loaded, unless there's some trick
        // I could not remember now.

        GlobalVariable* gv = new GlobalVariable(*sp_mod.get(), src, true,
                GlobalVariable::InternalLinkage,
                static_cast<Constant*>(pVal));

        // Now, create a { 0, 0 } array of Values for index information
        // It is like accessing array with &strTest[index]
        Value* zero = builder.getInt32(0);
        Value* idx[] = { zero, zero };
        Value* elementPtr = builder.CreateGEP(gv->getValueType(), gv, idx);
        StoreInst* si = builder.CreateStore(elementPtr, lhs);

        // for debugging, we echo this store action
        strTest = "=> ";
        gv->print(rs);
        rs << "\n=> ";
        si->print(rs, true);
        std::cerr << strTest << "\n";
        return static_cast<Value*>(si);
    }

    // assume we have to truncate the value
    castedVal = builder.CreateTruncOrBitCast(rhs, dest);
    return static_cast<Value*>(builder.CreateStore(castedVal, lhs));
}

llvm::Value* interpreter::make_add(Value* lhs, Value* rhs)
{
    IRBuilder<> builder(get_current_block());

    Type* lhsType = lhs->getType();
    Type* rhsType = rhs->getType();

    Value* pLHS = lhs;
    Value* pRHS = rhs;

    // If one of these values (or both) is AllocaInst
    // means that it is a VAR, not a constant.
    if (AllocaInst::classof(lhs))
    {
        AllocaInst* pInst = static_cast<AllocaInst*>(lhs);
        lhsType = pInst->getAllocatedType();
        pLHS = builder.CreateLoad(lhs);
    }
    if (AllocaInst::classof(rhs))
    {
        AllocaInst* pInst = static_cast<AllocaInst*>(rhs);
        rhsType = pInst->getAllocatedType();
        pRHS = builder.CreateLoad(rhs);
    }

    // If one of these values is a floating-point, then
    // we simply cast both of them into DOUBLE(s),
    // and return DOUBLE.

    if (lhsType->isFloatingPointTy() || rhsType->isFloatingPointTy())
    {
        if (lhsType->isDoubleTy() || rhsType->isDoubleTy())
        {
            // One of the operand is DOUBLE, and the other one
            // could even be NaN
            if (!rhsType->isFloatingPointTy())
            {
                if (!rhsType->isIntegerTy())
                {
                    std::cerr << "Invalid right-hand operand type, expecting a floating-point or at least integer\n";
                    return builder.getInt32(0);
                }
                // If this is an integer, then we upcast it to be
                // a DOUBLE
                pRHS = builder.CreateUIToFP(rhs, builder.getDoubleTy());
            }
            else
            {
                // It can only be a Float32, or could it be LONG DOUBLE?
                // We do not support LONG DOUBLE for now
                pRHS = builder.CreateFPExt(rhs, builder.getDoubleTy());
            }

            // If the above if got executed, then we won't...
            if (!lhsType->isFloatingPointTy())
            {
                // this clearly means that RHS is a DOUBLE
                if (!lhsType->isIntegerTy())
                {
                    // We do not support this operation
                    std::cerr << "Invalid left-hand operand type, expecting a floating-point or integer\n";
                    return builder.getInt32(0);
                }
                pLHS =builder.CreateUIToFP(lhs, builder.getDoubleTy());
            }
            else
            {
                // must be a Float32, extend it to double
                pLHS = builder.CreateFPExt(lhs, builder.getDoubleTy());
            }
        }

        Value* pRes = builder.CreateAdd(pLHS, pRHS);
        std::string strDebug("Add => ");
        raw_string_ostream rso(strDebug);
        pRes->print(rso);
        std::cout << strDebug << "\n";
    }
    else if (lhsType->isArrayTy())
    {
        /////////////////////////////////////////////////////////
        // A String is an Array of Byte
        // Or a pointer to i8 type.
        // If the user typed something like
        //
        // Dim str1 As String
        // str1 = "This is a test " + 3.14
        //
        // Then actually we can just make a strcat() here,
        // but that would involved Formatting the number, and
        // that is just NOT our job, should be delegated to
        // another function.
        //
        // As an interpreter, we should be able to do strcat()
        // with LHS and RHS both of type String, which is i8*
        //
        /////////////////////////////////////////////////////////

        std::cerr << "Concatenating Strings...\nNot yet implemented, returning integer value 0\n";
        return builder.getInt32(0);
    }

    if (lhsType->isIntegerTy())
    {
        // Get the biggest scalar size of both, and do upcast
        // as necessary.
        //
        // If the LHS is an Integer VALUE, and RHS is a POINTER,
        // then the operation is not supported, we return int32(0)
        if (!rhsType->isIntegerTy())
        {
            std::cerr << "Unsupported right-hand operand type for Integer operation.\n";
            return builder.getInt32(0);
        }

        if (lhsType->getScalarSizeInBits() < rhsType->getScalarSizeInBits())
        {
            // cast the LHS, leave RHS as is
            pLHS = builder.CreateIntCast(lhs, rhsType, true);
        }
        else if (lhsType->getScalarSizeInBits() > rhsType->getScalarSizeInBits())
        {
            pRHS = builder.CreateIntCast(rhs, lhsType, true);
        }
        return builder.CreateAdd(pLHS, pRHS);
    }

    std::cerr << "WARNING: Unfiltered Add operation\n";
    return builder.CreateAdd(lhs, rhs);
}

Value* interpreter::make_subtract(Value* lhs, Value* rhs)
{
    // just check new function cast_as_needed()
    auto [pLHS, pRHS] = cast_as_needed(lhs, rhs);
    std::string buff("make_subtract: ");
    raw_string_ostream rso(buff);

    pLHS->print(rso);
    rso << " <=> ";
    pRHS->print(rso);
    std::cerr << buff << "\n";

    IRBuilder<> builder(get_current_block());
    return builder.CreateSub(pLHS, pRHS);
}

Value* interpreter::make_multiply(Value* lhs, Value* rhs)
{
    auto [pLHS, pRHS] = cast_as_needed(lhs, rhs);

    IRBuilder<> builder(get_current_block());

    Value* pVal = nullptr;

    if (pLHS->getType()->isFloatingPointTy())
        pVal = builder.CreateFMul(pLHS, pRHS);
    else
        pVal = builder.CreateMul(pLHS, pRHS);

    std::string strTest("MUL: ");
    raw_string_ostream rso(strTest);
    pVal->print(rso);
    rso << "\n";
    std::cerr << strTest << "\n";
    return pVal;
}

bool interpreter::is_zero(Value* pValue)
{
    if (ConstantInt::classof(pValue))
    {
        // ConstantInt has isZero() function.
        return static_cast<ConstantInt*>(pValue)->isZero();
    }
    else if (ConstantFP::classof(pValue))
    {
        // ConstantFP has isZero()
        return static_cast<ConstantFP*>(pValue)->isZero();
    }
    return false;
}

Value* interpreter::make_pow(Value* lhs, Value* rhs)
{
    // we can do repeated MUL of course, but I guess
    // LLVM has a better way to do it, without calling
    // C Library's pow()
    // But currently I have no idea about which instruction to use.
    IRBuilder<> builder(get_current_block());
    Type* tp = builder.getDoubleTy();
    Type* pp[2] = { tp, tp };
    Constant* fPow = sp_mod->getOrInsertFunction("pow",
            FunctionType::get(builder.getDoubleTy(), ArrayRef<Type*>(pp), false));
    // cast both into double, if necessary
    Value* pLHS = lhs;
    Value* pRHS = rhs;
    if (AllocaInst::classof(lhs))
    {
        AllocaInst* inst = static_cast<AllocaInst*>(lhs);
        Type* currentType = inst->getAllocatedType();
        if (currentType != tp)
        {
            if (currentType->isIntegerTy())
                pLHS = builder.CreateUIToFP(lhs, tp);
            else if (currentType->isFloatingPointTy())
                pLHS = builder.CreateFPExt(lhs, tp);
            else
            {
                std::cerr << "Invalid or unsupported type for pow()\n";
                return lhs;
            }
        }
    }

    if (AllocaInst::classof(rhs))
    {
        AllocaInst* inst = static_cast<AllocaInst*>(lhs);
        Type* currentType = inst->getAllocatedType();
        if (currentType != tp)
        {
            if (currentType->isIntegerTy())
                pRHS = builder.CreateUIToFP(rhs, tp);
            else if (currentType->isFloatingPointTy())
                pRHS = builder.CreateFPExt(rhs, tp);
            else
            {
                std::cerr << "Invalid or unsupported type for pow()\n";
                return rhs;
            }
        }
    }

    if (!pLHS->getType()->isDoubleTy())
    {
        Type* tcur = pLHS->getType();
        if (tcur->isFloatingPointTy())
            pLHS = builder.CreateFPExt(pLHS, tp);
        else if (tcur->isIntegerTy())
            pLHS = builder.CreateUIToFP(pLHS, tp);
        else
        {
            std::cerr << "Invalid left-hand operand type for pow() operation\n";
            return lhs;
        }
    }

    if (!pRHS->getType()->isDoubleTy())
    {
        Type* tcur = pRHS->getType();
        if (tcur->isFloatingPointTy())
            pRHS = builder.CreateFPExt(pRHS, tp);
        else if (tcur->isIntegerTy())
            pRHS = builder.CreateUIToFP(pRHS, tp);
        else
        {
            std::cerr << "Invalid left-hand operand type for pow() operation\n";
            return rhs;
        }
    }

    Value* ppargs[2] = { pLHS, pRHS };
    Value* pRes = builder.CreateCall(fPow, ArrayRef<Value*>(ppargs));

    return pRes;
}

Value* interpreter::make_divide(Value* lhs, Value* rhs)
{
    // Check the possibility of Division By Zero
    auto [pLHS, pRHS] = cast_as_needed(lhs, rhs);
    if (is_zero(pRHS))
    {
        std::cerr << "RHS Value cause Division By Zero\n";
        return lhs;
    }
    IRBuilder<> builder(get_current_block());
    Value* pRes = nullptr;
    if (pLHS->getType()->isIntegerTy())
        pRes = builder.CreateExactSDiv(pLHS, pRHS);
    else if (pLHS->getType()->isFloatingPointTy())
        pRes = builder.CreateFDiv(pLHS, pRHS);

    return pRes;
}

std::tuple<llvm::Value*, llvm::Value*> interpreter::cast_as_needed(Value* lhs, Value* rhs)
{
    IRBuilder<> builder(get_current_block());

    Value* pLHS = lhs;
    Value* pRHS = rhs;
    Type* lhsType = pLHS->getType();
    Type* rhsType = pRHS->getType();

    if (AllocaInst::classof(lhs))
    {
        AllocaInst* pInst = static_cast<AllocaInst*>(lhs);
        lhsType = pInst->getAllocatedType();
        pLHS = builder.CreateLoad(lhs);
    }

    if (AllocaInst::classof(rhs))
    {
        AllocaInst* pInst = static_cast<AllocaInst*>(rhs);
        pRHS = builder.CreateLoad(rhs);
        rhsType = pInst->getAllocatedType();
    }

    if (lhsType == rhsType)
    {
        // we must return a suitable Value for binary operations
        // the pointer Value must be loaded.
        return std::tuple<Value*, Value*>(pLHS, pRHS);
    }

    // start with LHS
    if (lhsType->isIntegerTy())
    {
        if (rhsType->isFloatingPointTy())
        {
            // cast the LHS into RHS float type
            pLHS = builder.CreateUIToFP(pLHS, rhsType);
            return std::tuple<Value*, Value*>(pLHS, pRHS);
        }

        // done with RHS == Floating-point
        // now, check if the bits is differ
        if (rhsType->isPointerTy())
        {
            // currently we don't support this operation,
            // we may, in the future.
            std::cerr << "basic: Casting Right Hand Side operand to Integer is not currently supported.\n"
                << "Returning uncasted values.\n";
            return std::tuple<Value*, Value*>(lhs, rhs);
        }

        // If we got here, then RHS is also integer
        // Test the 'true' sign flags first
        // If we got something weird, then we change the following.
        if (lhsType->getScalarSizeInBits() < rhsType->getScalarSizeInBits())
            pLHS = builder.CreateIntCast(pLHS, rhsType, true);
        else if (lhsType->getScalarSizeInBits() > rhsType->getScalarSizeInBits())
            pRHS = builder.CreateIntCast(pRHS, lhsType, true);

        return std::tuple<Value*, Value*>(pLHS, pRHS);
    }
    else if (lhsType->isFloatingPointTy())
    {
        if (rhsType->isIntegerTy())
        {
            // upcast the integer into floating-point of LHS type
            pRHS = builder.CreateUIToFP(pRHS, lhsType);
            return std::tuple<Value*, Value*>(pLHS, pRHS);
        }

        // Otherwise, we just upcast one of them into appropriate Value
        // We currently only support Float and Double (32 and 64 bits),
        // and no 128 bits LONG DOUBLE
        //
        // The possibility of these two being the same type has been filtered
        // by the above ---> if (lhsType == rhsType)
        // So one of them must be bigger.
        //
        if (!lhsType->isDoubleTy())
        {
            std::cerr << "cast LHS type info double\n";
            pLHS = builder.CreateFPExt(pLHS, rhsType);
        }
        else if (!rhsType->isDoubleTy())
        {
            std::cerr << "cast RHS type info double\n";
            pRHS = builder.CreateFPExt(pRHS, lhsType);
        }

        std::string strCast("floating-point CAST\n=> ");
        raw_string_ostream rso(strCast);
        pLHS->print(rso);
        rso << "\n=> ";
        pRHS->print(rso);
        std::cerr << strCast << "\n";
    }

    return std::tuple<Value*, Value*>(pLHS, pRHS);
}

llvm::Function* interpreter::make_sub(const char* pszname)
{
    Function* pf = Function::Create(
            FunctionType::get(Type::getVoidTy(*this), false),
            Function::ExternalLinkage,
            pszname,
            sp_mod.get());
    BasicBlock* bb = BasicBlock::Create(*this, "", pf);
    m_blockStack.push_front(bb);
    return pf;
}

llvm::Function* interpreter::make_function(const char* pszname, Type* return_type)
{
    Function* pf = Function::Create(
            FunctionType::get(return_type, false),
            Function::ExternalLinkage,
            pszname,
            sp_mod.get());
    BasicBlock* bb = BasicBlock::Create(*this, "", pf);
    m_blockStack.push_front(bb);
    return pf;
}

llvm::Value* interpreter::make_return_value(Value* pVal)
{
    IRBuilder<> builder(get_current_block());

    if (AllocaInst::classof(pVal))
    {
        // this is a variable, not the value
        Value* pVarValue = builder.CreateLoad(pVal);
        return builder.CreateRet(pVarValue);
    }

    return builder.CreateRet(pVal);
}

bool interpreter::is_current_function_name(const char* pszname)
{
    BasicBlock* bb = get_current_block();
    Function* f = bb->getParent();
    if (!f)
    {
        std::cerr << "Warning: Current block returning NULL pointer for the parent.\n"
            << "is_current_function_name will return FALSE.\n";
        return false;
    }
    if (f->getName() == pszname)
        return true;
    return false;
}

bool interpreter::is_function_name(const char* pszname)
{
    Constant* pfn = sp_mod->getFunction(pszname);
    return (pfn == nullptr) ? false : true;
}

Function* interpreter::make_end_sub()
{
    // first, check if this is a valid statement,
    // a SUB never return a value, but in LLVM
    // we should always create a ret void.
    BasicBlock* bb = get_current_block();
    Function* f = bb->getParent();
    if (!f->getReturnType()->isVoidTy())
    {
        std::cerr << "Invalid END SUB, current FUNCTION has return value.\n";
        return f;
    }
    IRBuilder<> builder(bb);
    Value* pVal = builder.CreateRetVoid();

    // pop the stack
    m_blockStack.pop_front();

    std::string strTest("=> ");
    raw_string_ostream rso(strTest);
    pVal->print(rso);
    std::cerr << strTest << "\n";
    return f;
}

Function* interpreter::make_end_function()
{
    // This is tricky, because BASIC uses
    //
    // function_name = value
    //
    // to return the value from a function.
    //
    // So if we define a return value here, then
    // it will most likely be multiple ret statement.
    BasicBlock* bb = get_current_block();
    Function* f = bb->getParent();
    m_blockStack.pop_front();
    return f;
}

Value* interpreter::make_void_call(const char* pszFunctionName)
{
    // we will get the Function using module->getFunction(name)
    // and will issue WARNING if it is not found, then we will
    // return Value representing NULL.
    Constant* pFunction = sp_mod->getFunction(pszFunctionName);
    if (!pFunction)
    {
        std::cerr << "WARNING: Could not find Function/Sub: \'"
            << pszFunctionName << "\'\n";
        return ConstantInt::get(Type::getInt32Ty(*this), 0);
    }
    IRBuilder<> builder(get_current_block());
    Value* pRes = builder.CreateCall(pFunction);
    return pRes;
}

Function* interpreter::make_sub(const char* pszName, ParameterList* paramList)
{
    /* There is a simple trick to preserve argument names, but I just dont like it,
     * it uses alloca to create variables for each parameters.
     * Clang using exactly that method for C/C++ functions.
     *
     * It looks like I just could not find a better way, but to use the same
     * method as the rest.
     */
    std::vector<Type*> ptypes;
    std::vector<std::string> spname;
    for (auto [sname, stype]: *paramList)
    {
        ptypes.push_back(stype);
        spname.push_back(sname);
    }

    Function* f = Function::Create(
            FunctionType::get(Type::getVoidTy(*this), ArrayRef<Type*>(ptypes), false),
            Function::ExternalLinkage,
            pszName,
            sp_mod.get());

    std::vector<std::string>::iterator itnames = spname.begin();
    for (Argument* iter = f->arg_begin(); iter != f->arg_end(); iter++, itnames++)
    {
        iter->setName(itnames->c_str());
    }

    BasicBlock* bb = BasicBlock::Create(*this, "", f);
    m_blockStack.push_front(bb);
    return f;
}

Function* interpreter::make_function(const char* pszName, Type* return_type, ParameterList* paramList)
{
    std::vector<std::string> argname;
    std::vector<Type*> ptypes;
    for (auto [sname, stype]: *paramList)
    {
        ptypes.push_back(stype);
        argname.push_back(sname);
    }

    Function* f = Function::Create(
            FunctionType::get(return_type, ArrayRef<Type*>(ptypes), false),
            Function::ExternalLinkage,
            pszName,
            sp_mod.get());

    std::vector<std::string>::iterator itnames = argname.begin();
    for (Argument* iter = f->arg_begin(); iter != f->arg_end(); iter++, itnames++)
    {
        iter->setName(itnames->c_str());
    }

    BasicBlock* bb = BasicBlock::Create(*this, "", f);
    m_blockStack.push_front(bb);
    return f;
}

Value* interpreter::cast_for_call(Value* pVal, Type* pType)
{
    IRBuilder<> builder(get_current_block());
    Type* t = pVal->getType();
    Value* pRes = pVal;
    if (AllocaInst::classof(pVal))
    {
        AllocaInst* inst = static_cast<AllocaInst*>(pVal);
        t = inst->getAllocatedType();
        pRes = builder.CreateLoad(pVal);
    }
    if (t == pType)
        return pRes;

    if (pType->isIntegerTy())
    {
        if (t->isIntegerTy())
        {
            pRes = builder.CreateIntCast(pRes, pType, true);
            return pRes;
        }
        else if (t->isFloatingPointTy())
        {
            pRes = builder.CreateFPToUI(pRes, pType);
            return pRes;
        }
        else
        {
            std::cerr << "WARNING: Returning uncasted Value.\n";
            return pRes;
        }
    }
    else if (pType->isFloatingPointTy())
    {
        if (t->isFloatingPointTy())
        {
            if (pType->getScalarSizeInBits() < t->getScalarSizeInBits())
                pRes = builder.CreateFPTrunc(pRes, pType);
            else
                pRes = builder.CreateFPExt(pRes, pType);
            return pRes;
        }
        else if (t->isIntegerTy())
        {
            pRes = builder.CreateUIToFP(pRes, pType);
            return pRes;
        }
    }
    std::cerr << "Warning: returning uncasted value.\n";
    return pRes;
}

Value* interpreter::make_void_call(const char* pszFunctionName, std::vector<Value*>& arglist)
{
    // we will get the Function using module->getFunction(name)
    // and will issue WARNING if it is not found, then we will
    // return Value representing NULL.
    Constant* pFunction = sp_mod->getFunction(pszFunctionName);
    if (!pFunction)
    {
        std::cerr << "WARNING: Could not find Function/Sub: \'"
            << pszFunctionName << "\'\n";
        return ConstantInt::get(Type::getInt32Ty(*this), 0);
    }

    // actually we have to validate the argument list here,
    // but currently I'd like to see how it works
    IRBuilder<> builder(get_current_block());

    // Fix the values that most likely being truncated by the lexer
    // to match each of the params defined by the function
    FunctionType* ft = static_cast<Function*>(pFunction)->getFunctionType();
    unsigned i = 0;

    std::vector<Value*> casted;
    for (auto& arg: arglist)
    {
        // arg is a Value*
        Type* neededType = ft->getParamType(i);
        Value* pVal = cast_for_call(arg, neededType);
        casted.push_back(pVal);
        ++i;
    }

    Value* pRes = builder.CreateCall(pFunction, ArrayRef<Value*>(casted));
    return pRes;
}

Value* interpreter::make_call(const char* pszFunctionName, std::vector<Value*>& arglist)
{
    // we will get the Function using module->getFunction(name)
    // and will issue WARNING if it is not found, then we will
    // return Value representing NULL.
    Constant* pFunction = sp_mod->getFunction(pszFunctionName);
    if (!pFunction)
    {
        std::cerr << "WARNING: Could not find Function/Sub: \'"
            << pszFunctionName << "\'\n";
        return ConstantInt::get(Type::getInt32Ty(*this), 0);
    }
    IRBuilder<> builder(get_current_block());
    FunctionType* ft = static_cast<Function*>(pFunction)->getFunctionType();
    unsigned i = 0;

    std::vector<Value*> casted;
    for (auto& arg: arglist)
    {
        // arg is a Value*
        Type* neededType = ft->getParamType(i);
        Value* pVal = cast_for_call(arg, neededType);
        casted.push_back(pVal);
        ++i;
    }

    Value* pRes = builder.CreateCall(pFunction, ArrayRef<Value*>(casted));
    return pRes;
}

for_statement* interpreter::push_for_statement_block(std::string& varControlName, Value* startValue, Value* endValue)
{
    std::string strName = varControlName + "_start";
    for_statement* stmt = new for_statement(get_current_block()->getParent(), strName);
    forStatementList.push_front(stmt);
    return stmt;
}

for_statement* interpreter::next_block()
{
    if (forStatementList.emty())
    {
        std::cerr << "basic: Next without For\n";
        return nullptr;
    }
    return forStatementList.front();
}


