#include "basic.h"
#include "parser.hpp"

using namespace basic;
using namespace llvm;

extern basic::interpreter* interp;

// Toplevel statement
statement::statement(int tok, const char* sname)
{
	m_type = tok;
	m_name = sname;
	m_next = nullptr;
	m_prev = nullptr;
	m_parent = nullptr;
}

statement::statement(statement* parent, int tok, const char* sname)
{
	m_type = tok;
	m_name = sname;
	m_parent = parent;
	m_next = m_prev = nullptr;
}

statement::~statement()
{
	if (m_next)
		m_next->m_prev = m_prev;
	if (m_prev)
		m_prev->m_next = m_next;
	if (m_parent)
	{
		if (m_parent->m_children == this)
			m_parent->m_children = nullptr;
		m_parent = nullptr;
	}

	// we are responsible for deleting all of our children,
	// if we gets deleted.
	if (m_children)
	{
		m_children->m_parent = nullptr;

		// recursively delete next nodes
		m_children->remove_next();
		delete m_children;
	}
	std::cerr << "statement " << m_name << " deleted\n";
}

void statement::remove_next()
{
	if (m_next)
	{
		m_next->remove_next();
		delete m_next;
		m_next = nullptr;
	}
}

statement* statement::insert_last(int tok, const char* sname)
{
	statement* s = new statement(m_parent, tok, sname);
	if (m_next == nullptr)
	{
		m_next = s;
		return s;
	}

	statement* last = m_next;
	while (last->m_next)
		last = last->m_next;
	last->m_next = s;
	s->m_prev = last;
	return s;
}

statement* statement::insert_before(int tok, const char* sname)
{
	statement* s = new statement(m_parent, tok, sname);
	m_prev->m_next = s;
	s->m_next = this;
	s->m_prev = m_prev;
	m_prev = s;
	return s;
}

statement* statement::insert_after(int tok, const char* sname)
{
	statement* s = new statement(m_parent, tok, sname);
	s->m_next = m_next;
	m_next->m_prev = s;
	m_next = s;
	return s;
}

/////////////////////////
// DIM
/////////////////////////
dim_stmt::dim_stmt()
	: statement(DIM, "Dim")
{
	m_parentBlock = interp->get_entry();
}

dim_stmt::dim_stmt(BasicBlock* parentBlock)
	: statement(DIM, "Dim")
{
	m_parentBlock = parentBlock;
}

dim_stmt::~dim_stmt()
{
	std::cerr << "dim_stmt deleted\n";
}

AllocaInst* dim_stmt::add_variable(int vType, const char* vname)
{
	IRBuilder<> builder(m_parentBlock);
	AllocaInst* inst = builder.CreateAlloca(interp->get_llvm_type(vType), nullptr, vname);
	m_varlist.push_back(inst);
	if (!m_children)
	{
		m_children = new statement(this, vType, vname);
		return inst;
	}
	m_children->insert_last(vType, vname);
	return inst;
}

void dim_stmt::print_debug()
{
	if (m_varlist.empty())
	{
		std::cerr << "dim_stmt: empty list\n";
		return;
	}
	std::string buff;
	raw_string_ostream rso(buff);
	for (auto pVal: m_varlist)
	{
		rso << "=> ";
		pVal->print(rso);
		rso << "\n";
	}
	std::cerr << buff << "\n";
}

