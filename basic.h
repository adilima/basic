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
    struct variable
    {
        int typeId;
        std::string name;
        std::variant<llvm::Value*, llvm::Constant*> value;
        llvm::AllocaInst* allocation;

        /* should be the allocation, not the value */
        variable(llvm::Value* pVar);
        variable(int tok, const char* varname);
        ~variable();
        llvm::Value* set_value(llvm::Constant* pVal);
    };

    struct parameter
    {
        int typeId;
        std::string name;    // parameter should have names
        llvm::Type* llvmType;
        parameter(int tok, const char* pname);
        ~parameter();
    };

    struct expression
    {
        int typeId;
        int isConstant; // dont define bool, because we use bison/flex in C mode
        std::variant<variable*, llvm::Value*, llvm::Constant*> value;
        expression(variable* pvar);

        // If the token is BOOLEAN, then we will create i1 type,
        // otherwise we will create i8
        expression(int tok, char bValue);
        expression(int iVal);
        expression(long lval);
        expression(const char* psz);
        expression(float fVal);
        expression(double dblVal);
        ~expression();
        void print(std::string& buffer);
    };

    struct declaration
    {
        int typeId;
        std::string name;

        // use this if function_decl
        std::vector<parameter>* params;

        // only use this if variable_decl
        std::vector<variable>* vars;

        // only use return_type if this is a function_decl
        llvm::Type* return_type;

        declaration(int tok, const char* dname);
        ~declaration();

        void print_all(std::string& buffer);
    };

    struct constant_value
    {
        int typeId; // basic typeid, from token
        std::variant<llvm::Value*, llvm::Constant*> value;
        constant_value() {} // do nothing constructor
        ~constant_value() {}
        void print(std::string& buffer);
    };

    struct for_statement
    {
        int typeId;
        llvm::Value* control;
        llvm::Value* controlState;
        llvm::Value* stepValue;
        llvm::BasicBlock* blockHead;
        llvm::BasicBlock* blockTail;
        for_statement();
        for_statement(llvm::Function* _parent, const char* _name);
        ~for_statement();
    };

    class interpreter : public llvm::LLVMContext
    {
    public:
        interpreter(const char* modname);
        virtual ~interpreter();

        void print_version(std::ostream& os);
        int eval(const std::string& strLine);

        // these function will generate appropriate types
        // according to the specified token, although
        // the values givin in long and double
        constant_value* get_constant(int tok, long ival);
        constant_value* get_constant(int tok, double fval);

        // this will surely generate type STRING for the constant
        // and declare no name internal unnamed_addr field,
        // which most likely will be numbered by LLVM.
        constant_value* get_constant(const char* pszText);

        llvm::BasicBlock* get_entry();

        llvm::BasicBlock* get_current_block();
        void map_variable_name(llvm::BasicBlock* bb, const char* pszname, llvm::AllocaInst* inst);

        // return the AllocaInst as Value*, if available, or nullptr
        llvm::Value* get_variable(llvm::BasicBlock* bb, const char* pszName);

        void print_module(std::string& buffer);

        // returns LLVM type for the BASIC typeID
        llvm::Type* get_llvm_type(int nTypeID);

        bool is_zero(llvm::Value* pValue);

        // assign RHS into LHS, do cast if necessary
        llvm::Value* assign_value(llvm::Value* lhs, llvm::Value* rhs);

        llvm::Value* make_add(llvm::Value* lhs, llvm::Value* rhs);
        llvm::Value* make_subtract(llvm::Value* lhs, llvm::Value* rhs);
        llvm::Value* make_multiply(llvm::Value* lhs, llvm::Value* rhs);
        llvm::Value* make_divide(llvm::Value* lhs, llvm::Value* rhs);
        llvm::Value* make_pow(llvm::Value* lhs, llvm::Value* rhs);

        // declare a new BASIC Sub with the given name
        using ParameterList = std::list<std::tuple<std::string, llvm::Type*>>;
        llvm::Function* make_sub(const char* pszname);
        llvm::Function* make_sub(const char* pszname, ParameterList* plist);
        llvm::Function* make_function(const char* pszname, llvm::Type* return_type);
        llvm::Function* make_function(const char* pszname, llvm::Type* return_type, ParameterList* plist);


        // we have to be in a valid block to make this call.
        // it does not mean the end of the function/sub though
        // it can also return from a branch.
        llvm::Value* make_return_value(llvm::Value* pVal);

        // will pop the current function/stack and the current block
        // from the stack.
        // after this, the program will return to previous block
        // This function returns the last Function that was popped out.
        llvm::Function* make_end_sub();
        llvm::Function* make_end_function();

        bool is_function_name(const char* pszname);
        bool is_current_function_name(const char* pszname);

        // call function with no parameters, but with return value void
        // use for Sub
        //   JustDoIt <no args>
        //   DoSomething arg0, arg1, arg2
        // But if it is a Function (not Sub), then it can also be:
        //   var1 = ChangeForThis(arg0)
        //   var1 = JustGiveMeValue()
        //
        // Note that to take a value you must call a Function with
        // a pair of parenthesis, although it has no args, otherwise
        // the call will be treated as Sub call.
        //
        // But make_void_call() will only return a Void llvm::Value,
        // if you need a real value, use make_call()
        llvm::Value* make_void_call(const char* pszFunctionName);
        llvm::Value* make_void_call(const char* pszFunctionName, std::vector<llvm::Value*>& arglist);

        // call function that return a value
        // use for calling function and take the return value
        // i.e var1 = test1(1234, "testing") or
        //
        llvm::Value* make_call(const char* pszFunctionName, std::vector<llvm::Value*>& arglist);

        for_statement* push_for_statement_block(std::string& varControlName, llvm::Value* startValue, llvm::Value* endValue);
        for_statement* next_block();


    protected:
        std::unique_ptr<llvm::Module> sp_mod;
        std::unique_ptr<llvm::Function> m_fmain;
        std::unique_ptr<llvm::BasicBlock> m_entry;

        // string is the name of the function
        using VariableList = std::list<std::tuple<std::string, llvm::AllocaInst*>>;
        std::map<llvm::BasicBlock*, VariableList> m_varMap;
        std::list<llvm::BasicBlock*> m_blockStack;
        std::list<for_statement*> forStatementList;

        std::tuple<llvm::Value*, llvm::Value*> cast_as_needed(llvm::Value* l, llvm::Value* r);

        llvm::Value* cast_for_call(llvm::Value* p, llvm::Type* destType);
    };
}

typedef union basic_parser_types
{
    int typeID;
    char* identifier;
    basic::expression* expr;
    basic::variable* var;

    // this will be a function's parameter list
    std::vector<basic::parameter>* paramlist;

    // actually this is just the same as above,
    // but we need a place to hang the names
    // for DIM and the like.
    std::vector<basic::variable>* varlist;
    basic::declaration* decl;
    basic::for_statement* forStatementPtr;

    // no intermediate struct needed, we directly
    // use llvm types
    llvm::Value* llvmValuePtr;
    llvm::Constant* llvmConstantPtr;
    std::list<std::tuple<std::string, llvm::Type*>>* llvmTypeList;
    std::vector<llvm::Value*>* llvmValueList;
} basic_parser_types;

#endif /* BASIC_COMMON_H */


