#include "basic.h"
#include "parser.hpp"
#include <llvm/IR/ValueSymbolTable.h>

using namespace basic;
using namespace llvm;

static int BASIC_INTERPRETER_VERSION = 0x08;

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
	interp = this;
	module = std::make_unique<Module>(modname, *this);

	// an interpreter MUST have at least a 'main' function
	// as its default, along with the default entry, and,
	// if necessary, an exit block
	Function* fmain = Function::Create(
			FunctionType::get(Type::getVoidTy(*this), false),
			Function::ExternalLinkage, "main", module.get());
	m_entryBlock = BasicBlock::Create(*this, "entry", fmain);
	m_exitBlock = BasicBlock::Create(*this, "exit", fmain);

	// the activeBlock can change overtime,
	// depending on who's currently using it
	m_activeBlock = m_entryBlock;

	// dont push the main function into function list
	// we must maintain it internally
	Constant* pfn = module->getOrInsertFunction("puts",
			FunctionType::get(Type::getInt32Ty(*this), ArrayRef<Type*>(Type::getInt8PtrTy(*this)), false));
}

interpreter::~interpreter()
{
	//module.release();
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
	module->print(rso, nullptr);
}

llvm::Type* interpreter::get_llvm_type(int nId)
{
	switch (nId)
	{
	case BOOLEAN:
		return Type::getInt1Ty(*this);
	case BYTE:
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

void interpreter::quit()
{
	IRBuilder<> builder(m_activeBlock);
	
	// jump to exit block on the last
	// instruction in this block
	builder.CreateBr(m_exitBlock);

	// and make return void
	builder.SetInsertPoint(m_exitBlock);
	builder.CreateRetVoid();
}

Function* interpreter::get_current_function()
{
	if (m_functions.empty())
		return m_activeBlock->getParent();
	return m_functions.front();
}

Constant* interpreter::get_constant_long(long nValue)
{
	return ConstantInt::get(Type::getInt64Ty(*this), nValue, true);
}

Constant* interpreter::get_constant_double(double d)
{
	return ConstantFP::get(Type::getDoubleTy(*this), d);
}

Value* interpreter::find_variable(const char* pszname)
{
	// first, lookup the name in current block
	Function* f = get_current_function();
	if (!f->empty())
	{
		for (auto iter = f->begin(); iter != f->end(); iter++)
		{
			ValueSymbolTable* sym = iter->getValueSymbolTable();
			if (sym)
			{
				Value* pVal = sym->lookup(pszname);
				if (pVal)
					return pVal;
			}
		}
	}

	// either f is empty, or the variable is in module level,
	// or simply wasn't there
	Value* testValue = module->getGlobalVariable(pszname, true);
	return testValue;
}

Constant* interpreter::find_function(const char* pszname)
{
	return static_cast<Constant*>(module->getFunction(pszname));
}

Value* interpreter::make_equal_comparison(Value* lhs, Value* rhs)
{
	IRBuilder<> builder(m_activeBlock);

	Value* v1 = lhs;
	Value* v2 = rhs;
	Type* t1 = lhs->getType();
	Type* t2 = rhs->getType();

	if (AllocaInst::classof(lhs))
	{
		AllocaInst* p = static_cast<AllocaInst*>(lhs);
		t1 = p->getAllocatedType();
		v1 = builder.CreateLoad(lhs);
	}

	if (AllocaInst::classof(rhs))
	{
		AllocaInst* p = static_cast<AllocaInst*>(rhs);
		t2 = p->getAllocatedType();
		v2 = builder.CreateLoad(rhs);
	}

	if (t1 != t2)
	{
		// dilemma here...
		// we should check if any of them is constant
		// and try to compare against the constant type ???
		auto [p1, p2] = cast_as_needed(v1, v2);
		if (p1->getType()->isFloatingPointTy())
			return builder.CreateFCmpOEQ(p1, p2);
		return builder.CreateICmpEQ(p1, p2);
	}

	if (t1->isFloatingPointTy())
		return builder.CreateFCmpOEQ(v1, v2);
	else if (t1->isIntegerTy())
		return builder.CreateICmpEQ(v1, v2);

	return builder.getInt1(false);
}

Value* interpreter::cast_for_assignment(Value* pVal, Type* pType)
{
	IRBuilder<> builder(m_activeBlock);
	Type* t = pVal->getType();
	if (t == pType)
		return pVal;
	if (pType->isFloatingPointTy() && t->isIntegerTy())
		return builder.CreateSIToFP(pVal, pType);
	else if (pType->isIntegerTy() && t->isFloatingPointTy())
		return builder.CreateFPToSI(pVal, pType);
	
	if (t->isFloatingPointTy())
	{
		// this is basically cast between 2 floating-point of different size
		return builder.CreateFPCast(pVal, pType);
	}
	else if (t->isIntegerTy())
	{
		// otherwise, we just extent/truncate the value
		if (t->getScalarSizeInBits() > pType->getScalarSizeInBits())
			return builder.CreateTrunc(pVal, pType);
		return builder.CreateZExt(pVal, pType);
	}
	else if (t->isArrayTy())
	{
		std::cerr << "Warning: casting array type is not currently supported.\n"
			<< "Treating the array as an array of characters (String)\n";
		// try to take the address of array[0][0]
		// and return it
		// The string must be in Global space, but it can be internal
		GlobalVariable* gv = new GlobalVariable(*(module.get()), t, true, 
				GlobalVariable::InternalLinkage, static_cast<Constant*>(pVal));
		Value* zero = builder.getInt32(0);
		Value* idx[] = { zero, zero };
		Value* retVal = builder.CreateInBoundsGEP(t, gv, ArrayRef<Value*>(idx));
		std::string strDebug("GlobalString => ");
		raw_string_ostream rso(strDebug);
		gv->print(rso);
		std::cerr << strDebug << "\n";
		return retVal;
	}
	return pVal;
}

Value* interpreter::assign_variable(Value* pVar, Value* pVal)
{
	IRBuilder<> builder(m_activeBlock);

	// we filter out the possibilities of getting error here
	if (AllocaInst::classof(pVar))
	{
		Value* rhs = pVal;
		if (AllocaInst::classof(pVal))
			rhs = builder.CreateLoad(pVal);
		
		Type* allocatedType = static_cast<AllocaInst*>(pVar)->getAllocatedType();
		rhs = cast_for_assignment(rhs, allocatedType);
		return builder.CreateStore(rhs, pVar);
	}

	// if the LHS is anything other than the above types,
	// then we simply return the value of RHS,
	// but echoing a WARNING.
	std::cerr << "WARNING: (assign_variable) The left hand operand is not a variable.\n";
	return pVal;
}

Value* interpreter::make_return_value(Constant* fn, Value* pVal)
{
	Function* f = static_cast<Function*>(fn);
	Type* ret = f->getReturnType();
	Value* rhs = pVal;

	BasicBlock* bb = nullptr;
	if (f->empty())
	{
		// it is possible
		bb = BasicBlock::Create(*this, "", f);
	}
	else
		bb = &(f->getEntryBlock());

	IRBuilder<> builder(bb);
	if (AllocaInst::classof(pVal))
		rhs = builder.CreateLoad(pVal);
	return builder.CreateRet(rhs);
}

Value* interpreter::make_add(Value* lhs, Value* rhs)
{
	// have to change this style as soon as we mess with IF
	IRBuilder<> builder(m_activeBlock);
	if (lhs->getType() == rhs->getType())
		return builder.CreateAdd(lhs, rhs);
	auto [p1, p2] = cast_as_needed(lhs, rhs);
	return builder.CreateAdd(p1, p2);
}

// maintain compatibility with earlier version
BasicBlock* interpreter::get_current_block()
{
	return m_activeBlock;
}

void interpreter::set_current_block(BasicBlock* bb)
{
	m_activeBlock = bb;
}

Value* interpreter::make_subtract(Value* lhs, Value* rhs)
{
	IRBuilder<> builder(m_activeBlock);
	if (lhs->getType() == rhs->getType())
		return builder.CreateSub(lhs, rhs);

	auto [p1, p2] = cast_as_needed(lhs, rhs);
	return builder.CreateSub(p1, p2);
}

Value* interpreter::make_mult(Value* lhs, Value* rhs)
{
	IRBuilder<> builder(m_activeBlock);
	auto [p1, p2] = cast_as_needed(lhs, rhs);
	Type* t = p1->getType();
	if (t->isFloatingPointTy())
		return builder.CreateFMul(p1, p2);
	return builder.CreateMul(p1, p2);
}

Value* interpreter::make_divide(Value* lhs, Value* rhs)
{
	IRBuilder<> builder(m_activeBlock);
	auto [p1, p2] = cast_as_needed(lhs, rhs);
	Type* t = p1->getType();
	if (t->isFloatingPointTy())
		return builder.CreateFDiv(p1, p2);
	return builder.CreateSDiv(p1, p2);
}

Value* interpreter::make_compare_less_than(Value* lhs, Value* rhs)
{
	IRBuilder<> builder(m_activeBlock);
	auto [p1, p2] = cast_as_needed(lhs, rhs);
	Type* t = p1->getType();
	if (t->isFloatingPointTy())
	{
		// slightly different
		return builder.CreateFCmpOLT(p1, p2);
	}
	return builder.CreateICmpSLT(p1, p2);
}

Value* interpreter::make_compare_greater_than(Value* lhs, Value* rhs)
{
	IRBuilder<> builder(m_activeBlock);
	auto [p1, p2] = cast_as_needed(lhs, rhs);
	Type* t = p1->getType();
	if (t->isFloatingPointTy())
	{
		// slightly different
		return builder.CreateFCmpOGT(p1, p2);
	}
	return builder.CreateICmpSGT(p1, p2);
}

Value* interpreter::cast_to_double(Value* pVal)
{
	Type* t = pVal->getType();
	if (t->isDoubleTy())
		return pVal;

	IRBuilder<> builder(m_activeBlock);

	Value* pRes = pVal;
	if (AllocaInst::classof(pVal))
	{
		AllocaInst* inst = static_cast<AllocaInst*>(pVal);
		t = inst->getAllocatedType();
		pRes = builder.CreateLoad(pVal);
	}
	if (t->isDoubleTy())
		return pRes;
	if (t->isFloatingPointTy())
		return builder.CreateFPCast(pRes, builder.getDoubleTy());
	else if (t->isIntegerTy())
		return builder.CreateUIToFP(pRes, builder.getDoubleTy());
	std::cerr << "WARNING: dont know how to cast the value to double.\n";
	return pRes;
}

Value* interpreter::make_pow(Value* lhs, Value* rhs)
{
	IRBuilder<> builder(m_activeBlock);
	Type* dt = builder.getDoubleTy();
	Type* argTypes[] = { dt, dt };
	Constant* fn = module->getOrInsertFunction("pow",
			FunctionType::get(builder.getDoubleTy(), ArrayRef<Type*>(argTypes), false));
	Value* p1 = cast_to_double(lhs);
	Value* p2 = cast_to_double(rhs);
	Value* args[] = { p1, p2 };
	return builder.CreateCall(fn, ArrayRef<Value*>(args));
}

std::tuple<llvm::Value*, llvm::Value*> interpreter::cast_as_needed(Value* lhs, Value* rhs)
{
	IRBuilder<> builder(m_activeBlock);

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

void interpreter::push_context(statement* ps)
{
	m_statementList.push_front(ps);
}

statement* interpreter::pop_context()
{
	if (!m_statementList.empty())
	{
		statement* p = m_statementList.front();
		m_statementList.pop_front();
		return p;
	}
	std::cerr << "pop_context: WARNING: empty context list, returning NULL\n";
	return nullptr;
}

statement* interpreter::last_context()
{
	if (!m_statementList.empty())
		return m_statementList.front();
	return nullptr;
}

