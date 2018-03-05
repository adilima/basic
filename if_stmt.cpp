#include "basic.h"
#include "parser.hpp"

using namespace basic;
using namespace llvm;

extern basic::interpreter* interp;

if_stmt::if_stmt() : statement(IF, "If")
{
	m_parentBlock = interp->get_current_block();
	m_cond = nullptr;
	m_trueBlock = m_exitBlock = m_falseBlock = nullptr;
}

if_stmt::if_stmt(BasicBlock* parent) : statement(IF, "If")
{
	m_parentBlock = parent;
	m_cond = nullptr;
	m_trueBlock = m_exitBlock = m_falseBlock = nullptr;
}

if_stmt::if_stmt(BasicBlock* parent, Value* cond) : statement(IF, "If")
{
	m_parentBlock = parent;
	m_cond = cond;
	Function* f = parent->getParent();
	m_trueBlock = BasicBlock::Create(*interp, "", f);
	m_falseBlock = BasicBlock::Create(*interp, "", f);
	m_exitBlock = m_exitBlock;
	IRBuilder<> builder(parent);
	m_branch = builder.CreateCondBr(cond, m_trueBlock, m_falseBlock);
	interp->set_current_block(m_trueBlock);
	interp->push_context(this);
}

if_stmt::~if_stmt()
{
	//
}

// This can be use to create ElseIf, Else, or EndIf
if_stmt::if_stmt(if_stmt* topIf, int tok, const char* iname)
	: statement(tok, iname)
{
	topIf->m_next = this;
	m_prev = topIf;
	m_parentBlock = topIf->m_trueBlock;
	m_cond = nullptr;
	m_trueBlock = topIf->m_falseBlock;
	m_falseBlock = BasicBlock::Create(*interp, "", m_parentBlock->getParent());
	topIf->m_exitBlock = m_falseBlock;
	m_exitBlock = m_falseBlock;

	// we do not have a condition, so we cannot make the branch
	m_branch = nullptr;
	m_cond = nullptr;
}

BasicBlock* if_stmt::true_block()
{
	return m_trueBlock;
}

BasicBlock* if_stmt::false_block()
{
	return m_falseBlock;
}

BasicBlock* if_stmt::exit_block()
{
	return m_exitBlock;
}

BasicBlock* if_stmt::get_parent()
{
	return m_parentBlock;
}

void if_stmt::get_debug_string(std::string& buffer)
{
	if (m_branch == nullptr)
	{
		raw_string_ostream rso(buffer);
		rso << this << " <empty_if>";
		return;
	}
	raw_string_ostream rso(buffer);
	m_branch->print(rso);
}

Value* if_stmt::set_branch(Value* cond)
{
	if (m_branch)
	{
		std::string buff("Warning: if_stmt::set_branch\nThe condition already set: ");
		raw_string_ostream rso(buff);
		m_cond->print(rso);
		rso << "\n\tCould not alter if condition into: ";
		cond->print(rso);
		std::cerr << buff << "\nIgnoring the new conditional statement\n";
		return m_branch;
	}
	m_cond = cond;
	
	// The insert point should be set to the parentBlock
	// So if we've been created using another if_stmt as the first argument,
	// the m_parentBlock should be set to topIf->true_block(),
	// otherwise the resulting branch will be in a wrong place.
	IRBuilder<> builder(m_parentBlock);
	m_branch = builder.CreateCondBr(cond, m_trueBlock, m_falseBlock);
	interp->push_context(this);
	interp->set_current_block(m_trueBlock);
	return m_branch;
}

Value* if_stmt::set_branch()
{
	// used by ELSE
	// we have no conditions, so jump directly
	std::cerr << m_name << "::set_branch()\n";
	IRBuilder<> builder(m_parentBlock);
	m_branch = builder.CreateBr(m_trueBlock);
	interp->push_context(this);
	interp->set_current_block(m_trueBlock);
	return m_branch;
}

Value* if_stmt::make_end_if()
{
	// Used by END IF
	IRBuilder<> builder(m_trueBlock);
	m_branch = builder.CreateBr(m_falseBlock);
	interp->set_current_block(m_exitBlock);
	return m_branch;
}

