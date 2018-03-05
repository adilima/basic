#ifndef BASIC_COMMON_H
#define BASIC_COMMON_H

#include <llvm/IR/Module.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/Support/raw_ostream.h>

#include <iostream>
#include <string>
#include <vector>
#include <variant>
#include <fstream>
#include <list>


namespace basic
{
	class statement
	{
	public:
		statement(int tok, const char* sname);
		statement(statement* parent, int tok, const char* sname);
		virtual ~statement();

		int type() const { return m_type; }
		int type() { return m_type; }
		const std::string& name() const
		{
			return m_name;
		}

		statement* get_parent() { return m_parent; }
		statement* get_next() { return m_next; }
		statement* get_prev() { return m_prev; }

		statement* insert_last(int tok, const char* sname);
		statement* insert_before(int tok, const char* sname);
		statement* insert_after(int tok, const char* sname);

	protected:
		int m_type;
		std::string m_name;
		statement* m_parent;
		statement* m_next;
		statement* m_prev;
	    statement* m_children;
		void remove_next();
	};

	class dim_stmt : public statement
	{
	public:
		dim_stmt();
		dim_stmt(llvm::BasicBlock* parentBlock);
		~dim_stmt();

		const std::list<llvm::AllocaInst*>& get_variable_list() const
		{ return m_varlist; }
		llvm::AllocaInst* add_variable(int nType, const char* vName);

		void print_debug();

	private:
		llvm::BasicBlock* m_parentBlock;
		std::list<llvm::AllocaInst*> m_varlist;
	};

	class if_stmt : public statement
	{
	public:
		if_stmt();
		if_stmt(if_stmt* topIf, int tok, const char* _ifname);
		if_stmt(llvm::BasicBlock* bb);
		if_stmt(llvm::BasicBlock* bb, llvm::Value* cond);
		~if_stmt();

		llvm::BasicBlock* true_block();
		llvm::BasicBlock* false_block();
		llvm::BasicBlock* exit_block();
		llvm::BasicBlock* get_parent();

		void get_debug_string(std::string& buffer);

		// make the branch with the given condition,
		// if the branch already existed, it will do nothing.
		// The return value is the branch.
		llvm::Value* set_branch(llvm::Value* cond);
		llvm::Value* set_branch();
		llvm::Value* make_end_if();

	private:
		llvm::BasicBlock* m_parentBlock;
		llvm::Value* m_cond;
		llvm::Value* m_branch;
		llvm::BasicBlock* m_trueBlock;
		llvm::BasicBlock* m_exitBlock;
		llvm::BasicBlock* m_falseBlock;
	};

	class interpreter : public llvm::LLVMContext
	{
	public:
		interpreter(const char* src);
		~interpreter();

		std::unique_ptr<llvm::Module>& get_module()
		{ return *(std::unique_ptr<llvm::Module>*)&module; }
		llvm::BasicBlock* get_entry()
		{ return m_entryBlock; }
		llvm::BasicBlock* get_exit()
		{ return m_exitBlock; }

		// return function that's being edited
		llvm::Function* get_current_function();

		// exit the interpreter session
		// make ret void as necessary for main()
		void quit();

		void print_module(std::string& buffer);
		void print_version(std::ostream& os);
		int eval(const std::string& strCode);

		// BASIC TypeID to llvm::Type
		llvm::Type* get_llvm_type(int nType);
		llvm::Value* get_variable(llvm::BasicBlock* bb, const char* pszVarName);

		// Constant Values
		llvm::Constant* get_constant_long(long nValue);
		llvm::Constant* get_constant_double(double d);

		llvm::Value* find_variable(const char* pszname);
		llvm::Constant* find_function(const char* pszname);

		llvm::BasicBlock* get_current_block();
		void set_current_block(llvm::BasicBlock* bb);

		llvm::Value* make_return_value(llvm::Constant* fn, llvm::Value* pVal);
		llvm::Value* make_equal_comparison(llvm::Value* lhs, llvm::Value* rhs);
		llvm::Value* assign_variable(llvm::Value* pVar, llvm::Value* pVal);


		llvm::Value* make_add(llvm::Value* lhs, llvm::Value* rhs);
		llvm::Value* make_subtract(llvm::Value* lhs, llvm::Value* rhs);
		llvm::Value* make_mult(llvm::Value* lhs, llvm::Value* rhs);
		llvm::Value* make_divide(llvm::Value* lhs, llvm::Value* rhs);
		llvm::Value* make_compare_less_than(llvm::Value* lhs, llvm::Value* rhs);
		llvm::Value* make_compare_greater_than(llvm::Value* lhs, llvm::Value* rhs);
		llvm::Value* make_pow(llvm::Value* lhs, llvm::Value* rhs);

		// Utilities to cast values
		std::tuple<llvm::Value*, llvm::Value*> cast_as_needed(llvm::Value* lhs, llvm::Value* rhs);
		llvm::Value* cast_for_assignment(llvm::Value* pVal, llvm::Type* pType);
		llvm::Value* cast_to_double(llvm::Value* pValue);

		void push_context(statement* pNew);
		// pop and remove the statement from the list
		statement* pop_context();
		// does not pop
		statement* last_context();

	private:
		std::unique_ptr<llvm::Module> module;
		llvm::BasicBlock* m_entryBlock;
		llvm::BasicBlock* m_exitBlock;
		std::list<llvm::Function*> m_functions;
		llvm::BasicBlock* m_activeBlock;
		std::list<statement*> m_statementList;
	};
}

typedef union basic_parser_types
{
	int typeID;
	char* identifier;
	llvm::Value* llvmValue;
	llvm::Constant* llvmConstant;
	std::list<std::tuple<std::string, llvm::Type*>>* llvmTypeList;
	std::vector<llvm::Value*>* llvmValueList;
	basic::dim_stmt* dim;
	basic::if_stmt* ifStmt;
} basic_parser_types;

#endif /* BASIC_COMMON_H */


