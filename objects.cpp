#include "basic.h"
#include "parser.hpp"
#include <llvm/IR/ValueSymbolTable.h>

using namespace basic;
using namespace llvm;

extern interpreter* interp;
/////////////////////////////////////////////////////////////////////////
// constant_value
// maybe replaced by expression
/////////////////////////////////////////////////////////////////////////
constant_value* interpreter::get_constant(int tok, long nValue)
{
    constant_value* p = new constant_value();
    p->typeId = tok;
    p->value = ConstantInt::get(Type::getInt64Ty(*this), nValue, true);
    return p;
}

constant_value* interpreter::get_constant(int tok, double fValue)
{
    constant_value* p = new constant_value();
    p->typeId = tok;
    p->value = ConstantFP::get(Type::getDoubleTy(*this), fValue);
    return p;
}

constant_value* interpreter::get_constant(const char* pszText)
{
    constant_value* p = new constant_value();
    p->typeId = STRING;
    p->value = ConstantDataArray::getString(*this, pszText);
    return p;
}

void constant_value::print(std::string& buffer)
{
    try
    {
        raw_string_ostream rso(buffer);
        llvm::Constant* pv = std::get<llvm::Constant*>(value);
        pv->print(rso);
    }
    catch (std::exception& e)
    {
        std::cerr << "Got exception: " << e.what() << "\n";
    }
}


/////////////////////////////////////////////////
// Current Basic Objects
/////////////////////////////////////////////////

variable::variable(int tok, const char* varname)
{
    typeId = tok;
    name = varname;

    Type* tp = nullptr;

    // Do codegen right here
    BasicBlock* bb = interp->get_current_block();
    IRBuilder<> builder(bb);
    switch (tok)
    {
    case BYTE:
    case BOOLEAN:
        tp = builder.getInt8Ty();
        break;
    case INTEGER:
        tp = builder.getInt32Ty();
        break;
    case LONG:
        tp = builder.getInt64Ty();
        break;
    case SINGLE:
        tp = builder.getFloatTy();
        break;
    case DOUBLE:
        tp = builder.getDoubleTy();
        break;
    case STRING:
        tp = builder.getInt8PtrTy();
        break;
    default:
        {
            std::cerr << "Unsupported type: " << tok << ", converted to Integer\n";
            tp = builder.getInt32Ty();
            break;
        }
    }

    allocation = builder.CreateAlloca(tp, nullptr, varname);
    // save the inst to our map, this decl is only temporary
    interp->map_variable_name(bb, varname, allocation);
}

variable::variable(llvm::Value* pVar)
{
    allocation = static_cast<AllocaInst*>(pVar);
    Type* tp = pVar->getType();
    if (tp->isDoubleTy())
        typeId = DOUBLE;
    else if (tp->isFloatTy())
        typeId = SINGLE;
    else if (tp->isIntegerTy(8))
        typeId = BYTE;
    else if (tp->isIntegerTy(32))
        typeId = INTEGER;
    else if (tp->isIntegerTy(64))
        typeId = LONG;
    else if (tp->isIntOrIntVectorTy(8))
        typeId = STRING;
    else
        typeId = -1;
}

variable::~variable()
{
    std::cerr << "basic::variable[" << name << "] deleted\n";
}

llvm::Value* variable::set_value(llvm::Constant* pVal)
{
    llvm::IRBuilder<> builder(interp->get_current_block());

    if (pVal->getType() == allocation->getAllocatedType())
    {
        llvm::StoreInst* store = builder.CreateStore(pVal, allocation);
        value = pVal;

        // output the store for debugging
        std::string strTest("store => ");
        raw_string_ostream rso(strTest);
        store->print(rso);
        std::cerr << strTest << "\n";
        return static_cast<Value*>(store);
    }

    Type* dest = allocation->getAllocatedType();


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
    Type* src = pVal->getType();
    Value* castedVal = nullptr;

    std::string strTest("cast => ");
    raw_string_ostream rs(strTest);


    value = pVal;

    if (dest->isIntOrIntVectorTy())
    {
        if (src->isFloatingPointTy())
            castedVal = builder.CreateCast(Instruction::FPToSI, pVal, dest, "tmp");
        else
        {
            castedVal = builder.CreateIntCast(pVal, dest, true);
        }
        castedVal->print(rs);
        std::cerr << strTest << "\n";
        strTest = "=> ";

        StoreInst* store = builder.CreateStore(castedVal, allocation);
        store->print(rs);
        std::cout << strTest << "\n";
        value = castedVal;
        return static_cast<Value*>(store);
    }
    else if (dest->isFloatingPointTy())
    {
        if (src->isFloatingPointTy())
        {
            // if pVal is also floating-point, then we just upcast
            // or downcast the value.
            castedVal = builder.CreateFPCast(pVal, dest);
        }
        else if (src->isIntOrIntVectorTy())
        {
            castedVal = builder.CreateSIToFP(pVal, dest);
        }
        value = castedVal;
        return static_cast<Value*>(builder.CreateStore(castedVal, allocation));
    }

    // assume we have to truncate the value
    castedVal = builder.CreateTruncOrBitCast(pVal, dest);
    return static_cast<Value*>(builder.CreateStore(castedVal, allocation));
}

//////////////////////
//  parameter
/////////////////////
parameter::parameter(int tok, const char* pname)
    : typeId(tok), name(pname)
{
    switch (tok)
    {
    case BYTE:
    case BOOLEAN:
        llvmType = Type::getInt8Ty(*interp);
        break;
    case INTEGER:
        llvmType = Type::getInt32Ty(*interp);
        break;
    case LONG:
        llvmType = Type::getInt64Ty(*interp);
        break;
    case SINGLE:
        llvmType = Type::getFloatTy(*interp);
        break;
    case DOUBLE:
        llvmType = Type::getDoubleTy(*interp);
        break;
    case STRING:
        llvmType = Type::getInt8PtrTy(*interp);
        break;
    default:
        {
            std::cerr << "Unsupported type: " << tok << ", converted to Integer\n";
            llvmType = Type::getInt32Ty(*interp);
            break;
        }
    }
}

parameter::~parameter()
{
    // do nothing
}

//////////////////////////////////
// expression
//////////////////////////////////
expression::expression(variable* pvar)
    : isConstant(0)
{
    typeId = pvar->typeId;

    // this way, expression is just a container for variable
    value = pvar;
}

expression::~expression()
{
    //
}

expression::expression(int tok, char bValue)
    : typeId(tok), isConstant(1)
{
    // if tok == BOOLEAN, and bValue is non-zero
    // we create boolean 'True', otherwise
    // obviously 'False'.
    // but if this is a 'Byte', then we just copy the value
    // into llvm::Value.
    if (tok == BOOLEAN)
    {
        if (bValue)
            value = ConstantInt::getTrue(Type::getInt8Ty(*interp));
        else
            value = ConstantInt::getFalse(Type::getInt8Ty(*interp));
    }
    else if (tok == BYTE)
    {
        value = ConstantInt::get(Type::getInt8Ty(*interp), static_cast<uint64_t>(bValue));
    }
}

expression::expression(int iValue)
    : typeId(INTEGER), isConstant(1)
{
    value = ConstantInt::get(Type::getInt32Ty(*interp), iValue, true);
}

expression::expression(long lValue)
    : typeId(LONG), isConstant(1)
{
    value = ConstantInt::get(Type::getInt64Ty(*interp), lValue, true);
}

expression::expression(const char* pszText)
    : typeId(STRING), isConstant(1)
{
    value = ConstantDataArray::getString(*interp, pszText);
}

expression::expression(float fVal)
    : typeId(SINGLE), isConstant(1)
{
    value = ConstantFP::get(Type::getFloatTy(*interp), (double)fVal);
}

expression::expression(double dblVal)
    : typeId(DOUBLE), isConstant(1)
{
    value = ConstantFP::get(Type::getDoubleTy(*interp), dblVal);
}

void expression::print(std::string& buffer)
{
    try
    {
        raw_string_ostream rso(buffer);
        if (isConstant)
        {
            llvm::Constant* pv = std::get<llvm::Constant*>(value);
            pv->print(rso);
        }
        else
        {
            // is variable
            variable* pv = std::get<variable*>(value);
            Constant* pc = std::get<Constant*>(pv->value);
            pc->print(rso);
        }
    }
    catch (std::exception& e)
    {
        std::cerr << "Got exception: " << e.what() << "\n";
    }
}


/////////////////////////////////////////////
// Declaration
////////////////////////////////////////////
declaration::declaration(int tok, const char* dname)
    : typeId(tok), name(dname)
{
    // the type should be one of Dim, Private, or Public
    // refer to VB6 declaration style.
    // No Protected supported, as we dont (or not yet)
    // support inheritance.
}

declaration::~declaration()
{
    //
}

void declaration::print_all(std::string& buffer)
{
    if (vars->empty())
    {
        std::cerr << "No declared variables.\n";
    }

    raw_string_ostream rso(buffer);

    for (auto& v: *vars)
    {
        rso << " => ";
        v.allocation->print(rso);
        rso << "\n";
    }
}

///////////////////////////////
// For Statement
///////////////////////////////

for_statement::for_statement() : typeId(FOR)
{
    //
}

for_statement::for_statement(llvm::Function* fn, const char* pszName)
    : typeId(FOR)
{
    blockHead = BasicBlock::Create(*interp, pszName, fn);

}

for_statement::~for_statement()
{
    //
}

