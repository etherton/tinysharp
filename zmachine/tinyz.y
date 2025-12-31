/* tinyz.y */

%{
	#include "opcodes.h"
%}

%token ATTRIBUTE PROPERTY DIRECTION GLOBAL OBJECT LOCATION ROUTINE ARTICLE PLACEHOLDER ACTION HAS HASNT
%token BYTE_ARRAY WORD_ARRAY CALL
%token DICT
%token ANAME
%token PNAME
%token LNAME
%token GNAME
%token RNAME
%token INTLIT
%token STRLIT
%token FWDREF // Can be NEWSYM or existing sym
%token NEWSYM
%token WHILE "while" REPEAT "repeat" IF "if" ELSE "else"
%token LE "<=" GE ">=" EQ "==" NE "!="
%token LSH "<<" RSH ">>"
%token RFALSE "rfalse" RTRUE "rtrue" RETURN "return"
%token OR "or" AND "and" NOT "not"

%right '~' NOT
%left '*' '/' '%'
%left '+' '-'
%left LSH RSH
%left '&'
%left '|'
%left AND
%left OR

%%

prog
	: decl_list;

decl_list
	: decl_list decl
	| decl;

decl
	: attribute_def
	| property_def
	| direction_def
	| global_def
	| object_def
	| location_def
	| routine_def
	| article_def
	| placeholder_def
	| action_def
	;

attribute_def
	: ATTRIBUTE scope NEWSYM ';'
	;

scope
	: GLOBAL
	| LOCATION
	| OBJECT
	;

property_def
	: PROPERTY scope NEWSYM ';'
	;

direction_def
	: DIRECTION NEWSYM dict_list ';'
	;

dict_list
	: dict_list DICT
	| DICT
	;


global_def
	: GLOBAL NEWSYM ';'
	| BYTE_ARRAY '(' INTLIT ')' NEWSYM opt_array_def ';'
	| WORD_ARRAY '(' INTLIT ')' NEWSYM opt_array_def ';'
	;

opt_array_def
	:
	| array_def
	;

array_def
	: '{' init_list '}'
	;

init_list
	: init
	| init_list ',' init
	;

init
	: INTLIT
	;

object_def
	: OBJECT { is_object = true; } object_or_location_def
	;

location_def
	: LOCATION { is_object = false; } object_or_location_def
	;

object_or_location_def
	: NEWSYM STRLIT opt_parent '{' property_or_attribute_list '}' ';'
	;

opt_parent
	:
	| '(' FWDREF ')'
	;

property_or_attribute_list
	: property_or_attribute_list property_or_attribute
	| property_or_attribute
	;

property_or_attribute
	: PNAME ':' pvalue ';'
	| ANAME ';'
	;

pvalue
	: FWDREF
	| STRLIT
	| INTLIT
	| routine_body
	| dict_list
	;

routine_def
	: ROUTINE NEWSYM routine_body
	;

article_def
	: ARTICLE dict_list ';'
	;

placeholder_def
	: PLACEHOLDER NEWSYM ';'
	;

action_def
	: ACTION NEWSYM '{' '}'
	;

routine_body
	: '[' { open_scope(); } opt_params_list opt_locals_list ']' '{' stmts '}' { close_scope(); }
	;

opt_params_list
	: 
	| params_list
	;

params_list
	: params_list param
	| param
	;

param
	:  NEWSYM
	;

opt_locals_list
	: 
	| ';' locals_list
	;

locals_list
	: locals_list local
	| local
	;

local
	: NEWSYM
	;

stmts
	: stmts stmt
	| stmt
	;

stmt
	: "if" cond_expr stmt opt_else ';'
	| "repeat" stmt "while" cond_expr ';'
	| "while" cond_expr stmt ';'
	| '{' stmts '}'
	| LNAME '=' expr
	| GNAME '=' expr ';'
	| RETURN expr ';'
	| RFALSE ';'
	| RTRUE ';'
	| CALL expr opt_call_args ';'
	| RNAME opt_call_args ';'
	; 
	
opt_else
	:
	| "else" stmt
	;

cond_expr
	: '(' rel_expr ')'
	;

opt_call_args
	:
	| STRLIT
	| '(' arg_list ')'
	;

arg_list
	: arg_list ',' arg
	| arg
	;

arg
	: expr
	;

expr
	: expr '+' expr { $$ = new expr_binary($1,_2op::add,$3); }
	| expr '-' expr { $$ = new expr_binary($1,_2op::sub,$3); }
	| expr '*' expr { $$ = new expr_binary($1,_2op::mul,$3); }
	| expr '/' expr { $$ = new expr_binary($1,_2op::div,$3); }
	| expr '%' expr { $$ = new expr_binary($1,_2op::mod,$3); }
	| '~' expr      { $$ = new expr_unary(_1op::not_); }
	| expr '&' expr { $$ = new expr_binary($1,_2op::and_,$3); }
	| expr '|' expr { $$ = new expr_binary($1,_2op::or_,$3); }
	| '(' expr ')'  { $$ = $2; }
	| primary       { $$ = $1; }
	| INTLIT        { $$ = new expr_literal($1); }
	;

primary
	: ANAME
	| LNAME
	| GNAME
	| PNAME
	;

aname
	: ANAME
	| LNAME
	| GNAME
	;

rel_expr
	: expr '<' expr             { $$ = new expr_binary_branch($1,_2op::jl,false,$3); }
	| expr "<=" expr			{ $$ = new expr_binary_branch($1,_2op::jg,true,$3); }
	| expr '>' expr				{ $$ = new expr_binary_branch($1,_2op::jg,false,$3); }
	| expr ">=" expr			{ $$ = new expr_binary_branch($1,_2op::jl,true,$3); }
	| expr "==" exprs			{ $$ = new expr_binary_branch($1,_2op::je,false,$3); }
	| expr "!=" exprs			{ $$ = new expr_binary_branch($1,_2op::je,true,$3); }
	| NOT rel_expr				{ $$ = $2; /* FIXME */ }
	| rel_expr AND rel_expr		
	| rel_expr OR rel_expr
	| primary HAS aname			{ $$ = new expr_binary_branch($1,_2op::test_attr,false,$3); }
	| primary HASNT aname		{ $$ = new expr_binary_branch($1,_2op::test_attr,true,$3); }
	| expr /* same as expr != 0 */ { $$ = new expr_unary_branch($1,_1op::jz,true); }
	;

exprs
	: expr
	| INTLIT ',' INTLIT
	| INTLIT ',' INTLIT ',' INTLIT
	;

