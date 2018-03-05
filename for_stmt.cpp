#include "basic.h"
#include "parser.hpp"

using namespace llvm;
using namespace basic;

extern basic::interpreter* interp;

for_stmt::for_stmt()
	: statement(FOR, "For")
{
	m_parentBlock = interp->get_current_block();
}

for_stmt::for_stmt(BasicBlock* parentBlock, Value* vCounter)
	: statement(FOR, "For")
{
	m_parentBlock = parentBlock;
	m_varCounter = vCounter;
	if (!AllocaInst::classof(vCounter))
	{
		std::string buff("WARNING: FOR loop using something other than a variable could lead into undefined result.\n");
		raw_string_ostream rso(buff);
		vCounter->print(rso);
		rso << "\n";
		std::cerr << buff << "\n";
	}
}

// For created with another For buddy, must be a Next
for_stmt::for_stmt(for_stmt* prev)
	: statement(NEXT, "Next")
{
	//
}

for_stmt::~for_stmt()
{
	//
}

////////////////////////////////////////
// What if we got something like:
//
// For i = j To k
//    DoStuffs
// Next i
//
////////////////////////////////////////
// The parser have to make sure that
// we will get a CONSTANTs for both
// start and end value
//
bool for_stmt::set_condition(Value* vStart, Value* vEnd)
{
	// simply use int32(1) as default increment value
	return set_condition(vStart, vEnd,
			ConstantInt::get(Type::getInt32Ty(*interp), 1));
}

bool for_stmt::set_condition(Value* vStart, Value* vEnd, Value* vStep)
{
	m_startValue = vStart;
	m_endValue = vEnd;
	m_stepValue = vStep;

	Function* f = m_parentBlock->getParent();
	m_startBlock = BasicBlock::Create(*interp, "", f);
	m_loopBlock  = BasicBlock::Create(*interp, "", f);
	m_nextBlock  = BasicBlock::Create(*interp, "", f);
	m_exitBlock  = BasicBlock::Create(*interp, "", f);

	// before we jump, set the variable to the value of vStart
	IRBuilder<> builder(m_parentBlock);

	// adjust the Type as required
	AllocaInst* inst = static_cast<AllocaInst*>(m_varCounter);
	Type* t = inst->getAllocatedType();
	Value* pVal = interp->cast_for_assignment(vStart, t);
	// assign the value
	builder.CreateStore(pVal, m_varCounter);
	
	if (t->isIntegerTy())
		m_stepValue = ConstantInt::get(t, 1);
	else
		m_stepValue = ConstantFP::get(t, 1.0);

	// ready to jump
	builder.CreateBr(m_startBlock);

	// the startBlock jobdesc is only checking if the counter
	// reached the maximum value refering to vEnd
	builder.SetInsertPoint(m_startBlock);

	// needed by cast_as_needed to work
	interp->set_current_block(m_startBlock);

	auto [p1, p2] = interp->cast_as_needed(m_varCounter, vEnd);
	t = p1->getType();

	Value* cond = nullptr;
	if (t->isFloatingPointTy())
		cond = builder.CreateFCmpOGT(p1, p2);
	else
		cond = builder.CreateICmpSGT(p1, p2);

	builder.CreateCondBr(cond, m_exitBlock, m_loopBlock);

	// we will write the definition for Next, when the user
	// writing next in the command-line
	// then the parser will be called
	interp->push_context(this);
	interp->set_current_block(m_loopBlock);
	return true;
}

void for_stmt::get_debug_string(std::string& buffer)
{
	raw_string_ostream rso(buffer);
	rso << "For: startBlock = ";
	m_startBlock->print(rso);
}

bool for_stmt::is_equal(const char* strId)
{
	if (m_varCounter->getName() == strId)
		return true;
	return false;
}

void for_stmt::write_next()
{
	IRBuilder<> builder(interp->get_current_block());
	builder.CreateBr(m_nextBlock);
	interp->set_current_block(m_nextBlock);

	builder.SetInsertPoint(m_nextBlock);

	// we simply increment the varCounter using value of m_stepValue
	auto [p1, p2] = interp->cast_as_needed(m_varCounter, m_stepValue);

	// p1 = m_varCounter::value
	// p2 = m_stepValue (casted to matched the type, if needed)
	Value* pRes = builder.CreateAdd(p1, p2);
	// store the value
	builder.CreateStore(pRes, m_varCounter);

	// Now, after finishing our work, we should make a branch to m_startBlock
	builder.CreateBr(m_startBlock);

	// but because user write this at the end of the block,
	// then we must set the current interpreter insert point to
	// our exit point.
	// And then we will also have to pop the current for_stmt out from the list
	
	interp->pop_context();
	interp->set_current_block(m_exitBlock);

	// everything should be okay...
}

