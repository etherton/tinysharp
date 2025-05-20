%union {
};

%token T_var
%token T_if
%token T_while
%token T_do
%token T_in
%token T_case
%token T_switch
%token T_INTLIT
%token T_for
%token T_foreach
%token T_break
%token T_continue
%token T_ref
%token T_INCR
%token T_DECR
%token T_QQ // ??
%token T_LE // <=
%token T_GE // >=
%token T_EQEQ // ==
%token T_NE // !=
%token T_PLUSEQ // +=
%token T_MINUSEQ // -=
%token T_DIVEQ // /=
%token T_MULEQ // *=
%token T_MODEQ // %=
%token T_ANDEQ // %=
%token T_OREQ // |=
%token T_XOREQ // ^=
%token T_LSHEQ // <<=
%token T_SYMBOL

%%

/* https://learn.microsoft.com/en-us/dotnet/csharp/language-reference/language-specification/grammar */

// Source: §12.21.1 General
assignment
    : unary_expression assignment_operator expression
    ;

assignment_operator
    : '=' opt_ref | T_PLUSEQ | T_MINUSEQ | T_MULEQ | T_DIVEQ | T_MODEQ | T_ANDEQ | T_OREQ | T_XOREQ | T_LSHEQ
    | right_shift_assignment
    ;

opt_ref
    :
    | T_ref
    ;

right_shift
    : '>' '>'
    ;

right_shift_assignment
    : '>' T_GE
    ;

// Source: §12.22 Expression
expression
    : non_assignment_expression
    | assignment
    ;

non_assignment_expression
    : declaration_expression
    | conditional_expression
    //| lambda_expression
    //| query_expression
    ;

// Source: §12.23 Constant expressions
constant_expression
    : expression
    ;

// Source: §12.24 Boolean expressions
boolean_expression
    : expression
    ;

statement
    : labeled_statement
    | declaration_statement
    | embedded_statement
    ;

embedded_statement
    : block
    | empty_statement
    | selection_statement
    | iteration_statement
    | jump_statement
    ;

block  
    : '{' opt_statement_list '}'
    ;

opt_statement_list
    :
    | statement_list
    ;

statement_list
    : statement
    | statement_list statement
    ;

// Source: §13.5 Labeled statements
labeled_statement
    : identifier ':' statement
    ;

identifier
    : T_SYMBOL
    ;

empty_statement
    : ';'
    ;

declaration_statement
    : local_variable_declaration
    | local_constant_declaration
 /*   | local_function_declaration */
    ;

// Source: §13.6.2.1 General
local_variable_declaration
    : implicitly_typed_local_variable_declaration
    | explicitly_typed_local_variable_declaration
    | explicitly_typed_ref_local_variable_declaration
    ;

// Source: §13.6.2.2 Implicitly typed local variable declarations
implicitly_typed_local_variable_declaration
    : 'var' implicitly_typed_local_variable_declarator
    | ref_kind 'var' ref_local_variable_declarator
    ;

implicitly_typed_local_variable_declarator
    : identifier '=' expression
    ;

// Source: §13.6.2.3 Explicitly typed local variable declarations
explicitly_typed_local_variable_declaration
    : type explicitly_typed_local_variable_declarators
    ;

explicitly_typed_local_variable_declarators
    : explicitly_typed_local_variable_declarator
      (',' explicitly_typed_local_variable_declarator)*
    ;

explicitly_typed_local_variable_declarator
    : identifier ('=' local_variable_initializer)?
    ;

local_variable_initializer
    : expression
    | array_initializer
    ;

// Source: §13.6.2.4 Explicitly typed ref local variable declarations
explicitly_typed_ref_local_variable_declaration
    : ref_kind type ref_local_variable_declarators
    ;

ref_local_variable_declarators
    : ref_local_variable_declarator (',' ref_local_variable_declarator)*
    ;

ref_local_variable_declarator
    : identifier '=' 'ref' variable_reference
    ;

// Source: §13.6.3 Local constant declarations
local_constant_declaration
    : 'const' type constant_declarators
    ;

constant_declarators
    : constant_declarator (',' constant_declarator)*
    ;

constant_declarator
    : identifier '=' constant_expression
    ;

// Source: §13.6.4 Local function declarations
local_function_declaration
    : local_function_modifier* return_type local_function_header
      local_function_body
    | ref_local_function_modifier* ref_kind ref_return_type
      local_function_header ref_local_function_body
    ;

local_function_header
    : identifier '(' parameter_list? ')'
    | identifier type_parameter_list '(' parameter_list? ')'
      type_parameter_constraints_clause*
    ;

local_function_modifier
    : ref_local_function_modifier
    | 'async'
    ;

ref_local_function_modifier
    : 'static'
    | unsafe_modifier   // unsafe code support
    ;

local_function_body
    : block
    | '=>' null_conditional_invocation_expression ';'
    | '=>' expression ';'
    ;

ref_local_function_body
    : block
    | '=>' 'ref' variable_reference ';'
    ;

// Source: §13.7 Expression statements
expression_statement
    : statement_expression ';'
    ;

statement_expression
    : null_conditional_invocation_expression
    | invocation_expression
    | object_creation_expression
    | assignment
    | post_increment_expression
    | post_decrement_expression
    | pre_increment_expression
    | pre_decrement_expression
    ;

selection_statement
    : if_statement
    | switch_statement
    ;

// Source: §13.8.2 The if statement
if_statement
    : T_if '(' boolean_expression ')' embedded_statement
    | T_if '(' boolean_expression ')' embedded_statement
      T_else embedded_statement
    ;

// Source: §13.8.3 The switch statement
switch_statement
    : T_switch '(' expression ')' switch_block
    ;

switch_block
    : '{' switch_section '}'
    ;

switch_section
    : switch_label_list statement_list
    ;

switch_label_list
    : switch_label
    | switch_label_list switch_label
    ;

switch_label
    : T_case T_INTLIT ':'
    | T_default ':'
    ;

// Source: §13.9.1 General
iteration_statement
    : while_statement
    | do_statement
    | for_statement
    | foreach_statement
    ;

// Source: §13.9.2 The while statement
while_statement
    : T_while '(' boolean_expression ')' embedded_statement
    ;

// Source: §13.9.3 The do statement
do_statement
    : T_do embedded_statement T_while '(' boolean_expression ')' ';'
    ;

// Source: §13.9.4 The for statement
for_statement
    : T_for '(' opt_for_initializer ';' opt_for_condition ';' opt_for_iterator ')'
      embedded_statement
    ;

opt_for_initializer
    :
    | for_initializer
    ;

for_initializer
    : local_variable_declaration
    | statement_expression_list
    ;

for_condition
    : boolean_expression
    ;

for_iterator
    : statement_expression_list
    ;

statement_expression_list
    : statement_expression
    | statement_expression_list ',' statement_expression
    ;

// Source: §13.9.5 The foreach statement
foreach_statement
    : T_foreach '(' opt_ref_kind local_variable_type identifier T_in
      expression ')' embedded_statement
    ;

// Source: §13.10.1 General
jump_statement
    : break_statement
    | continue_statement
    | goto_statement
    | return_statement
    | throw_statement
    ;

// Source: §13.10.2 The break statement
break_statement
    : T_break ';'
    ;

// Source: §13.10.3 The continue statement
continue_statement
    : T_continue ';'
    ;

// Source: §13.10.4 The goto statement
goto_statement
    : T_goto identifier ';'
    | T_goto T_case constant_expression ';'
    | T_goto T_default ';'
    ;

// Source: §13.10.5 The return statement
return_statement
    : T_return ';'
    | T_return expression ';'
    | T_return T_ref variable_reference ';'
    ;

// Source: §12.9.1 General
unary_expression
    : primary_expression
    | '+' unary_expression
    | '-' unary_expression
    | logical_negation_operator unary_expression
    | '~' unary_expression
    | pre_increment_expression
    | pre_decrement_expression
    | cast_expression
//    | await_expression
//    | pointer_indirection_expression    // unsafe code support
//    | addressof_expression              // unsafe code support
    ;

// Source: §12.9.6 Prefix increment and decrement operators
pre_increment_expression
    : T_INCR unary_expression
    ;

pre_decrement_expression
    : T_DECR unary_expression
    ;

// Source: §12.9.7 Cast expressions
cast_expression
    : '(' type ')' unary_expression
    ;

// Source: §12.9.8.1 General
await_expression
    : T_await unary_expression
    ;

// Source: §12.10.1 General
multiplicative_expression
    : unary_expression
    | multiplicative_expression '*' unary_expression
    | multiplicative_expression '/' unary_expression
    | multiplicative_expression '%' unary_expression
    ;

additive_expression
    : multiplicative_expression
    | additive_expression '+' multiplicative_expression
    | additive_expression '-' multiplicative_expression
    ;

// Source: §12.11 Shift operators
shift_expression
    : additive_expression
    | shift_expression T_LSH additive_expression
    | shift_expression right_shift additive_expression
    ;

// Source: §12.12.1 General
relational_expression
    : shift_expression
    | relational_expression '<' shift_expression
    | relational_expression '>' shift_expression
    | relational_expression T_LE shift_expression
    | relational_expression T_GE shift_expression
    | relational_expression T_is type
    | relational_expression T_is pattern
    | relational_expression T_as type
    ;

equality_expression
    : relational_expression
    | equality_expression T_EQEQ relational_expression
    | equality_expression T_NE relational_expression
    ;

// Source: §12.13.1 General
and_expression
    : equality_expression
    | and_expression '&' equality_expression
    ;

exclusive_or_expression
    : and_expression
    | exclusive_or_expression '^' and_expression
    ;

inclusive_or_expression
    : exclusive_or_expression
    | inclusive_or_expression '|' exclusive_or_expression
    ;

// Source: §12.14.1 General
conditional_and_expression
    : inclusive_or_expression
    | conditional_and_expression T_ANDAND inclusive_or_expression
    ;

conditional_or_expression
    : conditional_and_expression
    | conditional_or_expression T_OROR conditional_and_expression
    ;

// Source: §12.15 The null coalescing operator
null_coalescing_expression
    : conditional_or_expression
    | conditional_or_expression T_QQ null_coalescing_expression
    | throw_expression
    ;

// Source: §12.16 The throw expression operator
throw_expression
    : 'throw' null_coalescing_expression
    ;

// Source: §12.17 Declaration expressions
declaration_expression
    : local_variable_type identifier
    ;

local_variable_type
    : type
    | T_var
    ;

// Source: §12.18 Conditional operator
conditional_expression
    : null_coalescing_expression
    | null_coalescing_expression '?' expression ':' expression
    | null_coalescing_expression '?' T_ref variable_reference ':'
      T_ref variable_reference
    ;
