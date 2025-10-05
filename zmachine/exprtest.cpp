#include <iostream>
#include <map>
#include <tuple>
#include <cctype>

using namespace std;

// arity is -1 for (
// arity is 0 for functions
// arity is 1 for unary operators
// else arity is 2 for binary operators
struct operator_def { 
	operator_def(int a,int p,string f) : arity(a), precedence(p), func(f) { }
	int arity, precedence; string func; 
};

map<string,operator_def*> operators;

map<string,tuple<int,bool>> commands;

vector<operator_def*> opstack;

#define LP "("
#define RP ")"

// https://www.chris-j.co.uk/parsing.php
void shunting_yard() {
	string token;
	while (cin >> token) {
		auto o1 = operators.find(token);
		if (o1 != operators.end()) {
			// ( or functions or unary prefix operators
			if (o1->second->arity <= 1)
				opstack.push_back(o1->second);
			else {
				if (opstack.size()) {
					auto o2 = opstack.back();
					int p1 = o1->second->precedence;
					// <= becomes < if o1 is right-associative
					while (o2->arity != -1 && o2->precedence <= p1) {
						cout << o2->func << " ";
						opstack.pop_back();
						if (opstack.size() == 0)
							break;
						o2 = opstack.back();
					}
				}
				opstack.push_back(o1->second);
			}
		}
		else if (token==",") {
			while (opstack.size() && opstack.back()->arity!=-1) {
				cout << opstack.back()->func << " ";
				opstack.pop_back();
			}
		}
		else if (token==RP) {
			while (opstack.back()->arity != -1) {
				cout << opstack.back()->func << " ";
				opstack.pop_back();
			}
			opstack.pop_back();
			if (opstack.size() && opstack.back()->arity == 0) {
				cout << opstack.back()->func << " ";
				opstack.pop_back();
			}
		}
		else
			cout << token << " ";
	}
	while (opstack.size()) {
		cout << opstack.back()->func << " ";
		opstack.pop_back();
	}
	cout << "\n";
}

int main(int,char**) {
	operators[LP] = new operator_def(-1,0,LP);

	operators["!"] = new operator_def(1,2,"not");
	operators["~"] = new operator_def(1,2,"bitnot");

	operators["++>"] = new operator_def(2,3,"inc_chk");
	operators["--<"] = new operator_def(2,3,"dec_chk");
	operators["has"] = new operator_def(2,4,"test_attr");
	operators["*"] = new operator_def(2,5,"mul");
	operators["/"] = new operator_def(2,5,"div");
	operators["%"] = new operator_def(2,5,"mod");
	operators["+"] = new operator_def(2,6,"add");
	operators["-"] = new operator_def(2,6,"sub");
	operators["<"] = new operator_def(2,9,"lt");
	operators["<="] = new operator_def(2,9,"le");
	operators[">"] = new operator_def(2,9,"gt");
	operators[">="] = new operator_def(2,9,"ge");
	operators["=="] = new operator_def(2,10,"eq");
	operators["!="] = new operator_def(2,10,"ne");
	operators["&"] = new operator_def(2,11,"bitand");
	operators["|"] = new operator_def(2,11,"bitor");
	operators["and"] = new operator_def(2,14,"logand");
	operators["or"] = new operator_def(2,15,"logor");

	operators["test_attr"] = new operator_def(0,1,"test_attr");
	operators["get_sibling"] = new operator_def(0,1,"get_sibling");
	operators["get_child"] = new operator_def(0,1,"get_child");

	commands["set_attr"] = make_tuple(2,false);
	commands["insert_obj"] = make_tuple(2,false);
	commands["++"] = make_tuple(2,true);
	commands["--"] = make_tuple(2,true);

	shunting_yard();
}
