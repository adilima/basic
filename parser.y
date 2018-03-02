%{
#include "basic.h"
extern int yylex(basic_parser_types*);
extern void yyerror(basic::interpreter* interp, const char* msg);
%}

%output "parser.cpp"
%define api.value.type {basic_parser_types}
%define api.pure full
%parse-param {basic::interpreter* interp}

%token <llvmConstantPtr> BYTE BOOLEAN INTEGER LONG SINGLE DOUBLE STRING OBJECT
%token <typeID>          DIM FUNCTION SUB END AS TYPEID KEYWORD IF ELSE ELSEIF ENDIF THEN FOR EACH NEXT TO STEP
%token <llvmValuePtr>    VAR
%token <identifier>     ID FUNCTION_NAME CURRENT_FUNCTION_NAME
%type <llvmValuePtr>    exp func_call
%type <llvmConstantPtr> constant func_decl
%type <llvmTypeList> parameter_list
%type <llvmTypeList> variable_list
%type <llvmTypeList> declaration
%type <llvmValueList> argument_list
%type <forStatementPtr> for_stmt


%left '+' '-' '*' '/'
%right '^'


%%

input:
    %empty /* no inputs */
|   input line
;

line:
    '\n'
|   exp {
    std::string buff("=> ");
    llvm::raw_string_ostream rso(buff);
    $1->print(rso);
    std::cerr << buff << "\n";
}
|   declaration {
    if (!$1->empty())
    {
        std::string buff("declaration: allocating variables\n");
        llvm::raw_string_ostream rso(buff);
        llvm::IRBuilder<> builder(interp->get_current_block());
        for (auto [sname, stype]: *$1)
        {
            llvm::AllocaInst* allocation = builder.CreateAlloca(stype, nullptr, sname.c_str());
            if (allocation)
            {
                rso << " => ";
                allocation->print(rso);
                rso << "\n";
            }
        }
        std::cout << buff << "\n";
    }
}
|   func_decl {
    std::string strDecl("Function or Sub: ");
    llvm::raw_string_ostream rso(strDecl);
    $1->print(rso);
    rso << "\n";
    std::cerr << strDecl;
}
|   for_stmt {
    // FOR VAR = CONSTANT TO CONSTANT [ STEP CONSNTANT ]
    //      ...
    // NEXT
}
;

exp:
    constant {
    $$ = $1;
}
|   func_call {
    $$ = $1;
}
|   VAR {
    $$ = $1;
}
|   exp '+' exp {
    std::cerr << "parser: add operation\n";
    $$ = interp->make_add($1, $3);
}
|   exp '-' exp {
    $$ = interp->make_subtract($1, $3);
}
|   exp '*' exp { $$ = interp->make_multiply($1, $3); }
|   exp '/' exp { $$ = interp->make_divide($1, $3); }
|   exp '^' exp { $$ = interp->make_pow($1, $3); }
|   '(' exp ')' { $$ = $2; }
|   CURRENT_FUNCTION_NAME '=' exp {
    $$ = interp->make_return_value($3);
}
|   VAR '=' exp {
    $$ = interp->assign_value($1, $3);
}
;

declaration:
    DIM variable_list {
    /**
     * Karena baik DIM, atau variable_list, dan juga declaration,
     * kita deklarasikan sebagai llvmTypeList, maka kita bisa langsung
     * mengembalikan $1 dari sini.
     */
    $$ = $2;
}
;

func_decl:
    SUB ID {
    llvm::Function* lf = interp->make_sub($2);
    $$ = static_cast<llvm::Constant*>(lf);
}
|   FUNCTION ID {
    llvm::Function* lf = interp->make_sub($2);
    $$ = static_cast<llvm::Constant*>(lf);
}
|   FUNCTION ID AS TYPEID {
    llvm::Function* lf = interp->make_function($2, interp->get_llvm_type($4));
    $$ = static_cast<llvm::Constant*>(lf);
}
|   SUB ID '(' parameter_list ')' {
    llvm::Function* lf = interp->make_sub($2, $4);
    $$ = static_cast<llvm::Constant*>(lf);
}
|   FUNCTION ID '(' parameter_list ')' AS TYPEID {
    llvm::Function* lf = interp->make_function($2, interp->get_llvm_type($7), $4);
    $$ = static_cast<llvm::Constant*>(lf);
}
|   END SUB {
    $$ = interp->make_end_sub();
}
|   END FUNCTION {
    $$ = interp->make_end_function();
}
;

variable_list:
    ID AS TYPEID {
    $$ = new std::list<std::tuple<std::string, llvm::Type*>>();
    llvm::Type* tp = interp->get_llvm_type($3);
    $$->push_back(std::tuple<std::string, llvm::Type*>($1, tp));
}
|   variable_list ',' ID AS TYPEID {
    $1->push_back(std::tuple<std::string, llvm::Type*>($3, interp->get_llvm_type($5)));
    $$ = $1;
}
;

parameter_list:
    ID AS TYPEID {
    /* lets see if we can make it better */
    $$ = new std::list<std::tuple<std::string, llvm::Type*>>();
    llvm::Type* pType = interp->get_llvm_type($3);
    $$->push_back(std::tuple<std::string, llvm::Type*>($1, pType));
}
|   parameter_list ',' ID AS TYPEID {
    $1->push_back(std::tuple<std::string, llvm::Type*>($3, interp->get_llvm_type($5)));
    $$ = $1;
}
;

constant:
    BYTE { $$ = $1; }
|   BOOLEAN { $$ = $1; }
|   INTEGER { std::cerr << "Got integer\n"; $$ = $1; }
|   LONG { std::cerr << "Got a long\n"; $$ = $1; }
|   SINGLE { $$ = $1; }
|   DOUBLE { $$ = $1; }
|   STRING { $$ = $1; }
;

func_call:
    FUNCTION_NAME {
    $$ = interp->make_void_call($1);
}
|   FUNCTION_NAME argument_list {
    $$ = interp->make_void_call($1, *$2);
}
|   FUNCTION_NAME '(' argument_list ')' {
    $$ = interp->make_call($1, *$3);
}
;

argument_list:
    constant {
    $$ = new std::vector<llvm::Value*>();
    $$->push_back($1);
}
|   VAR {
    $$ = new std::vector<llvm::Value*>();
    $$->push_back($1);
}
|   argument_list ',' argument_list {
    for (auto& arg: *$3)
        $1->push_back(arg);
    $$ = $1;
}
;

for_stmt:
    FOR VAR '=' constant TO constant {
    $$ = interp->push_for_statement_block($2, $4, $6);
}
|   NEXT {
    // check the VAR control statement for condition
    // and exit the loop if true, or jump back to the above
    // label on false
    $$ = interp->next_block();
}
;

%%


