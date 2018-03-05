%{
#include "basic.h"
extern int yylex(basic_parser_types*);
extern void yyerror(basic::interpreter* interp, const char* msg);
%}

%output "parser.cpp"
%define api.value.type {basic_parser_types}
%define api.pure full
%parse-param {basic::interpreter* interp}

%token <llvmConstant> BYTE BOOLEAN INTEGER LONG SINGLE DOUBLE STRING OBJECT
%token <typeID>       DIM FUNCTION SUB END AS TYPEID KEYWORD IF ELSE ELSEIF ENDIF THEN FOR EACH NEXT TO STEP
%token <llvmValue>    VAR
%token <identifier>   ID FUNCTION_NAME CURRENT_FUNCTION_NAME
%type <llvmValue>     expr function_call
%type <llvmConstant> constant
%type <dim> dim_stmt
%type <ifStmt> if_stmt
%type <llvmValueList> argument_list
%type <forStmt> for_stmt


%left '+' '-' '*' '/'
%right '^'


%%

input:
	%empty /* no inputs */
|   input line
;

line:
	'\n'
|   expr {
    std::string buff("expr: ");
	llvm::raw_string_ostream rso(buff);
	$1->print(rso);
	rso << "\n";
	std::cerr << buff << "\n";
}
|   dim_stmt {
    std::cerr << "DIM" << "\n";
    $1->print_debug();
}
|   if_stmt {
    std::string buff("IF ");
	$1->get_debug_string(buff);
	std::cerr << buff << "\n";
}
|   for_stmt {
    std::string buff("FOR ");
	$1->get_debug_string(buff);
	std::cerr << buff << "\n";
}
|   function_call {
    std::string buff("CALL: ");
	llvm::raw_string_ostream rso(buff);
	$1->print(rso);
	std::cerr << buff << "\n";
}
;

expr:
    constant {
	$$ = $1;
}
|   ID {
	llvm::Value* pVar = interp->find_variable($1);
	if (!pVar)
	{
	    llvm::Constant* pfn = interp->find_function($1);
		if (!pfn)
		{
		    std::cerr << "Unrecognized identifier: " << $1 << "\n";
			YYERROR;
		}
		pVar = static_cast<llvm::Value*>(pfn);
	}
	$$ = pVar;
}
|   expr '+' expr { $$ = interp->make_add($1, $3); }
|   expr '-' expr { $$ = interp->make_subtract($1, $3); }
|   expr '*' expr { $$ = interp->make_mult($1, $3); }
|   expr '/' expr { $$ = interp->make_divide($1, $3); }
|   expr '^' expr { $$ = interp->make_pow($1, $3); }
|   expr '<' expr { $$ = interp->make_compare_less_than($1, $3); }
|   expr '>' expr { $$ = interp->make_compare_greater_than($1, $3); }
|   '(' expr ')' { $$ = $2; }
|   expr '=' expr {
    if (llvm::AllocaInst::classof($1))
	{
	    // we only allow assignment to a variable,
		// indicated by the given Value is derived from AllocaInst,
		// otherwise, it would be a constant
		// or FUNCTION
		std::cerr << "assigning variable\n";
		$$ = interp->assign_variable($1, $3);
	}
	else if (llvm::Function::classof($1))
	{
	    $$ = interp->make_return_value(static_cast<llvm::Constant*>($1), $3);
	}
	else
	{
	    // treat this action as comparison
		$$ = interp->make_equal_comparison($1, $3);
	}
}
;

dim_stmt:
	DIM ID AS TYPEID {
	basic::dim_stmt* pDim = new basic::dim_stmt();
	pDim->add_variable($4, $2);
	$$ = pDim;
}
|   dim_stmt ',' ID AS TYPEID {
    $1->add_variable($5, $3);
	$$ = $1;
}
;

constant:
	BOOLEAN { $$ = $1; }
|   LONG { $$ = $1; }
|   DOUBLE { $$ = $1; }
|   STRING {
    // All Strings must be put in Module level address space
	// and should be marked as constant.
	llvm::GlobalVariable *gv = new llvm::GlobalVariable(*(interp->get_module().get()),
	    $1->getType(),
		true,
		llvm::GlobalVariable::InternalLinkage,
		$1);
	llvm::IRBuilder<> builder(interp->get_current_block());
	llvm::Value* idx[] = { builder.getInt32(0), builder.getInt32(0) };
	llvm::Value* pResult = builder.CreateInBoundsGEP(gv, llvm::ArrayRef<llvm::Value*>(idx));
	$$ = static_cast<llvm::Constant*>(pResult);
}
;

if_stmt:
	IF ID '=' expr THEN {
	llvm::Value* pVar = interp->find_variable($2);
	if (!pVar)
	{
	    yyerror(interp, "Unrecognized identifier, expecting variable name");
		YYERROR;
	}
	llvm::Value* cond = interp->make_equal_comparison(pVar, $4);
	basic::if_stmt* pObj = new basic::if_stmt(interp->get_current_block(), cond);
	// The interpreter has to define a way to hang this data until we have END IF
	// because the ELSEIF and ELSE will use it, and END IF will have to pop out
	// the context, so the control will be returned to the current Function's
	// previous context (if any), or the function itself.
	$$ = pObj;
}
|   ELSEIF ID '=' expr THEN {
	llvm::Value* pVar = interp->find_variable($2);
	if (!pVar)
	{
	    yyerror(interp, "Unrecognized identifier, expecting variable name");
		YYERROR;
	}
	llvm::Value* cond = interp->make_equal_comparison(pVar, $4);
	// it must be there
	basic::if_stmt* prev_if = static_cast<basic::if_stmt*>(interp->pop_context());
	basic::if_stmt* pObj = new basic::if_stmt(prev_if, ELSEIF, "ElseIf");
	// this new ElseIf must be pushed as the new context
	// and the following call will do that, after creating the branch
	llvm::Value* pResult = pObj->set_branch(cond);
	$$ = pObj;
	std::string buff("ElseIf: ");
	llvm::raw_string_ostream rso(buff);
	pResult->print(rso);
	std::cerr << buff << "\n";
}
|  IF expr THEN {
   // We will never know unless we try, this is the most conflicting RULE,
   // as the expr also define ID as standalone entity, then define expr = expr
   // the if_stmt MUST check if the condition given returning something
   // other than a boolean value, and if it is, then it will have
   // to be converted.
   basic::if_stmt* pVal = new basic::if_stmt(interp->get_current_block(), $2);
   $$ = pVal;
}
|  ELSEIF expr THEN {
   basic::if_stmt* prev_if = static_cast<basic::if_stmt*>(interp->pop_context());
   basic::if_stmt* pObj = new basic::if_stmt(prev_if, ELSEIF, "ElseIf");
   pObj->set_branch($2);
   $$ = pObj;
}
|  ELSE {
   // Else must incorporate and implement the previous if's false block
   // and does not have any condition.
   basic::if_stmt* prev_if = static_cast<basic::if_stmt*>(interp->pop_context());
   basic::if_stmt* pElse = new basic::if_stmt(prev_if, ELSE, "Else");
   // we will define set_branch with empty argument
   pElse->set_branch();
   $$ = pElse;
}
|  END IF {
   basic::if_stmt* prev_if = static_cast<basic::if_stmt*>(interp->pop_context());
   // we jump to prev_if::false/exitBlock
   // and signal the interpreter to set the insert point to it's exit block
   // the whole if_stmt context was already popped out above.
   prev_if->make_end_if();
   $$ = prev_if;
}
;

argument_list:
	constant {
	std::vector<llvm::Value*>* pObj = new std::vector<llvm::Value*>();
	pObj->push_back($1);
	$$ = pObj;
}
|   ID {
    llvm::Value* pVar = interp->find_variable($1);
	if (!pVar)
	{
	    std::string strError("Unrecognized identifier \'");
		strError += $1;
		strError += "\', expecting variable name or constant.";
		yyerror(interp, strError.c_str());
		YYERROR;
	}
	std::vector<llvm::Value*>* pObj = new std::vector<llvm::Value*>();
	pObj->push_back(pVar);
	$$ = pObj;
}
|   argument_list ',' argument_list {
    for (auto pObj: *$3)
	    $1->push_back(pObj);
	$$ = $1;
}
;

function_call:
	ID argument_list {
	llvm::Constant* pfn = interp->find_function($1);
	if (!pfn)
	{
	    std::string strErr("No such Function/Sub: ");
		strErr += $1;
		yyerror(interp, strErr.c_str());
		YYERROR;
	}
	llvm::IRBuilder<> builder(interp->get_current_block());
	llvm::Value* pResult = builder.CreateCall(pfn, llvm::ArrayRef<llvm::Value*>(*$2));
	$$ = pResult;
}
|   ID '(' argument_list ')' {
}
;

for_stmt:
	FOR ID '=' constant TO constant {
	llvm::Value* pVar = interp->find_variable($2);
	if (!pVar)
	{
	    std::string buff("Undefined identifier: ");
		buff += $2;
		yyerror(interp, buff.c_str());
		YYERROR;
	}
	basic::for_stmt* pObj = new basic::for_stmt(interp->get_current_block(), pVar);
	pObj->set_condition($4, $6);
	$$ = pObj;
}
|   FOR ID '=' constant TO constant STEP constant {
    // the only different is the STEP value
	llvm::Value* pVar = interp->find_variable($2);
	if (!pVar)
	{
	    std::string buff("Undefined identifier: ");
		buff += $2;
		yyerror(interp, buff.c_str());
		YYERROR;
	}
	basic::for_stmt* pObj = new basic::for_stmt(interp->get_current_block(), pVar);
	pObj->set_condition($4, $6, $8);
	$$ = pObj;
}
|   NEXT ID {
    // lookup the previous context to find the for with varCounter == ID
	basic::for_stmt* pObj = interp->find_last_for($2);
	if (!pObj)
	{
	    std::string strErr("Invalid Next control for ");
		strErr += $2;
		yyerror(interp, strErr.c_str());
		YYERROR;
	}
	pObj->write_next();
	$$ = pObj;
}
;

%%


