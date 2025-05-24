%token T_if T_while T_do T_for

%%

prog
	: definition_list
	;

definition_list
	: definition_list definition
	| definition
	;

statement
	: 

multiplicativeExpr
	: 
additiveExpr
	: unaryExpr
	: multiplicativeExpr '+' multiplicativeExpr
	| multiplicativeExpr '-' multiplicativeExpr
	;
