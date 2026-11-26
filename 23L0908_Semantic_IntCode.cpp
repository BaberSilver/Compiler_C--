#include<iostream>
#include<vector>
#include<algorithm>

#define SEMANTIC
#include "23L0908_Parser.cpp"
using namespace std;

const string TYPE_ERROR = "type_error";

struct Entry{
    string name,kind,type;
    vector<string> paramTypes;
    int line;
    int order = 0;
};

struct Scope{
    string name;
    unordered_map<string, Entry> symbols;
    Scope* Parent;
    vector<Scope*> children;
};

class SymbolTable{
        Scope* root;
        Scope* current;
        void print(ostream& out, Scope* scope, int depth);
    public:
        SymbolTable();
        bool insert(const Entry& e);
        bool insertIn(const string& scopeName, const Entry& e);
        Entry* lookup(const string& name);
        const Scope* globalScope() const { return root; }
        const Scope* functionScope(const string& name) const;
        static vector<const Entry*> entries(const Scope* scope, bool recursive);
        void openScope(const string& name);
        void closeScope();
        void print(ostream& out);
};

SymbolTable::SymbolTable(){
    root = new Scope{"file", {}, nullptr, {}};
    current = root;
}

bool SymbolTable::insert(const Entry& e){
    Entry copy = e;
    copy.order = (int)current->symbols.size();
    return current->symbols.insert({copy.name, copy}).second;
}

bool SymbolTable::insertIn(const string& scopeName, const Entry& e){
    Scope* target = root;
    for (Scope* child : root->children) {
        if (child->name == scopeName) { target = child; break; }
    }
    Entry copy = e;
    copy.order = (int)target->symbols.size();
    return target->symbols.insert({copy.name, copy}).second;
}

const Scope* SymbolTable::functionScope(const string& name) const{
    for (const Scope* child : root->children) {
        if (child->name == name) return child;
    }
    return nullptr;
}

vector<const Entry*> SymbolTable::entries(const Scope* scope, bool recursive){
    vector<const Entry*> result;
    for (const auto& s : scope->symbols) result.push_back(&s.second);
    sort(result.begin(), result.end(), [](const Entry* a, const Entry* b){
        return a->order < b->order;
    });
    if (recursive) {
        for (const Scope* child : scope->children) {
            vector<const Entry*> inner = entries(child, true);
            result.insert(result.end(), inner.begin(), inner.end());
        }
    }
    return result;
}

Entry* SymbolTable::lookup(const string& name){
    for (Scope* s = current; s != nullptr; s = s->Parent) {
        auto it = s->symbols.find(name);
        if (it != s->symbols.end()) 
            return &it->second;
    }
    return nullptr;
}

void SymbolTable::openScope(const string& name){
    Scope* s = new Scope{name, {}, current, {}};
    current->children.push_back(s);
    current = s;
}

void SymbolTable::closeScope(){
    current = current->Parent;
}

void SymbolTable::print(ostream& out){
    print(out, root, 0);
}

void SymbolTable::print(ostream& out, Scope* scope, int depth){
    string indent(depth * 2, ' ');
    out << indent << "Scope: " << scope->name << "\n";

    for (const Entry* e : entries(scope, false)) {
        out << indent << "  " << e->name << " : " << e->kind << ", " << e->type;
        if (e->kind == "func") {
            out << " (";
            for (size_t i = 0; i < e->paramTypes.size(); i++) {
                out << (i ? ", " : "") << e->paramTypes[i];
            }
            out << ")";
        }
        if (e->line >= 0) 
            out << ", line " << e->line;
        out << "\n";
    }
    for (Scope* child : scope->children) 
        print(out, child, depth + 1);
}

struct SyntaxNode{
    string val;
    SyntaxNode* left;
    SyntaxNode* middle;
    SyntaxNode* right;
    string type;
};

SyntaxNode* makeNode(const string& val, SyntaxNode* left = nullptr, SyntaxNode* middle = nullptr, SyntaxNode* right = nullptr){
    return new SyntaxNode{val, left, middle, right, ""};
}

SyntaxNode* seq(SyntaxNode* first, SyntaxNode* rest){
    if (first == nullptr) 
        return rest;
    if (rest == nullptr) 
        return first;
    return makeNode("seq", first, nullptr, rest);
}

void printSyntaxTree(SyntaxNode* n, ostream& out, const string& prefix = "", bool last = true, bool root = true){
    out << prefix << (root ? "" : (last ? "└── " : "├── ")) << n->val << "\n";
    string childPrefix = prefix + (root ? "" : (last ? "    " : "│   "));
    vector<SyntaxNode*> kids;
    for (SyntaxNode* k : {n->left, n->middle, n->right}) {
        if (k != nullptr) kids.push_back(k);
    }
    for (size_t i = 0; i < kids.size(); i++) {
        printSyntaxTree(kids[i], out, childPrefix, i + 1 == kids.size(), false);
    }
}

struct Attr {
    string type;
    SyntaxNode* node;
};

Attr typed(const string& type, SyntaxNode* node){
    node->type = type;
    return {type, node};
}

class Translator{
        string currentReturnType;

        void declare(const string& name, const string& kind, const string& type, int line);
        void error(const string& msg, int line);
        string checkOperands(const string& op, const string& left, const string& right, int line);
        void checkCondition(const Attr& c, int line);
        Attr call(const string& name, Node* argsNode, int line);

        void Items(Node* n, SyntaxNode*& tree);
        SyntaxNode* Item(Node* n);
        SyntaxNode* DeclTail(Node* n, const string& type, const string& name, int line);

        void Params(Node* n, vector<Entry>& params);
        void ParamsTail(Node* n, vector<Entry>& params);
        void Param(Node* n, vector<Entry>& params);

        SyntaxNode* Block(Node* n, bool newScope);
        SyntaxNode* Stmts(Node* n);
        SyntaxNode* Stmt(Node* n);
        SyntaxNode* Statement(Node* child);

        SyntaxNode* DeclStmt(Node* n);
        SyntaxNode* VarsTail(Node* n, const string& type);
        SyntaxNode* Init(Node* n, const string& type, const string& name, int line);

        SyntaxNode* AssignStmt(Node* n);
        SyntaxNode* AssignTail(Node* n, const string& name, int line);

        SyntaxNode* IF(Node* n);
        SyntaxNode* ElseTail(Node* n);
        SyntaxNode* WHILE(Node* n);

        SyntaxNode* ReturnStmt(Node* n);

        void Args(Node* n, vector<Attr>& args);
        void ArgsTail(Node* n, vector<Attr>& args);

        Attr Expr(Node* n);
        Attr OrTail(Node* n, const Attr& inh);
        Attr AndExpr(Node* n);
        Attr AndTail(Node* n, const Attr& inh);
        Attr RelExpr(Node* n);
        Attr RelTail(Node* n, const Attr& inh);
        Attr AddExpr(Node* n);
        Attr AddTail(Node* n, const Attr& inh);
        Attr MulExpr(Node* n);
        Attr MulTail(Node* n, const Attr& inh);
        Attr Unary(Node* n);
        Attr Primary(Node* n);
        Attr IdTail(Node* n, const string& name, int line);

    public:
        SymbolTable symtab;
        vector<string> errors;
        SyntaxNode* Program(Node* node);
};

void Translator::error(const string& msg, int line){
    errors.push_back("Error (line " + to_string(line) + "): " + msg);
}

void Translator::declare(const string& name, const string& kind, const string& type, int line){
    if (!symtab.insert({name, kind, type, {}, line})) {
        error("Redeclaration of '" + name + "'", line);
    }
}

string Translator::checkOperands(const string& op, const string& left, const string& right, int line){
    if (left == TYPE_ERROR || right == TYPE_ERROR) 
        return TYPE_ERROR;
    if (left != right) {
        error("Type mismatch: '" + left + " " + op + " " + right + "'", line);
        return TYPE_ERROR;
    }
    bool arithmetic = (op == "+" || op == "-" || op == "*" || op == "/" || op == "%");
    return arithmetic ? left : "int";
}

void Translator::checkCondition(const Attr& c, int line){
    if (c.type != "int" && c.type != TYPE_ERROR) {
        error("Condition must be int, found " + c.type, line);
    }
}

SyntaxNode* Translator::Program(Node* node){
    SyntaxNode* tree = nullptr;
    Items(node->children[0], tree);
    return tree;
}

void Translator::Items(Node* n, SyntaxNode*& tree){
    if (n->children[0]->label == "e") 
        return;
    SyntaxNode* first = Item(n->children[0]);
    SyntaxNode* rest = nullptr;
    Items(n->children[1], rest);
    tree = seq(first, rest);
}

SyntaxNode* Translator::Item(Node* n){
    if (n->children[0]->label == "TYPE") {
        string type = n->children[0]->lexeme;
        string name = n->children[1]->lexeme;
        return DeclTail(n->children[2], type, name, n->children[1]->line);
    }
    return Statement(n->children[0]);
}

SyntaxNode* Translator::DeclTail(Node* n, const string& type, const string& name, int line){
    if (n->children[0]->label == "(") {
        vector<Entry> params;
        Params(n->children[1], params);

        Entry f{name, "func", type, {}, line};
        for (const Entry& p : params) 
            f.paramTypes.push_back(p.type);

        if (!symtab.insert(f)) 
            error("Redeclaration of '" + name + "'", line);

        symtab.openScope(name);
        for (const Entry& p : params) 
            declare(p.name, p.kind, p.type, p.line);
        
        currentReturnType = type;
        SyntaxNode* body = Block(n->children[3], false);
        currentReturnType = "";
        symtab.closeScope();

        return makeNode("func", makeNode(name), nullptr, body);
    }
    SyntaxNode* first = Init(n->children[0], type, name, line);
    return seq(first, VarsTail(n->children[1], type));
}

void Translator::Params(Node* n, vector<Entry>& params){
    if (n->children[0]->label == "e") 
        return;
    Param(n->children[0], params);
    ParamsTail(n->children[1], params);
}

void Translator::ParamsTail(Node* n, vector<Entry>& params){
    if (n->children[0]->label == "e") 
        return;
    Param(n->children[1], params);
    ParamsTail(n->children[2], params);
}

void Translator::Param(Node* n, vector<Entry>& params){
    params.push_back({
        n->children[1]->lexeme, "param", 
        n->children[0]->lexeme, 
        {}, 
        n->children[1]->line
    });
}

SyntaxNode* Translator::Block(Node* n, bool newScope){
    if (newScope) 
        symtab.openScope("block");
    SyntaxNode* body = Stmts(n->children[1]);
    if (newScope) 
        symtab.closeScope();
    return body;
}

SyntaxNode* Translator::Stmts(Node* n){
    if (n->children[0]->label == "e") 
        return nullptr;
    SyntaxNode* first = Stmt(n->children[0]);
    return seq(first, Stmts(n->children[1]));
}

SyntaxNode* Translator::Stmt(Node* n){
    return Statement(n->children[0]);
}

SyntaxNode* Translator::Statement(Node* child){
    if (child->label == "DeclStmt") 
        return DeclStmt(child);

    if (child->label == "AssignStmt") 
        return AssignStmt(child);
    
    if (child->label == "IF") 
        return IF(child);

    if (child->label == "WHILE") 
        return WHILE(child);

    if (child->label == "ReturnStmt") 
        return ReturnStmt(child);

    return Block(child, true);
}

SyntaxNode* Translator::DeclStmt(Node* n){
    string type = n->children[0]->lexeme;
    string name = n->children[1]->lexeme;
    SyntaxNode* first = Init(
        n->children[2], 
        type, 
        name, 
        n->children[1]->line
    );
    return seq(first, VarsTail(n->children[3], type));
}

SyntaxNode* Translator::VarsTail(Node* n, const string& type){
    if (n->children[0]->label == "e") 
        return nullptr;

    SyntaxNode* first = Init(
        n->children[2], 
        type, n->children[1]->lexeme, 
        n->children[1]->line
    );
    return seq(first, VarsTail(n->children[3], type));
}

SyntaxNode* Translator::Init(Node* n, const string& type, const string& name, int line){
    if (n->children[0]->label == "e") {
        declare(name, "var", type, line);
        return nullptr;
    }
    Attr r = Expr(n->children[1]);
    declare(name, "var", type, line);
    if (r.type != type && r.type != TYPE_ERROR) {
        error("Type mismatch: cannot initialize " + type + " '" + name + "' with " + r.type, line);
    }
    return makeNode("=", makeNode(name), nullptr, r.node);
}

SyntaxNode* Translator::AssignStmt(Node* n){
    return AssignTail(n->children[1], n->children[0]->lexeme, n->children[0]->line);
}

SyntaxNode* Translator::AssignTail(Node* n, const string& name, int line){
    if (n->children[0]->label == "(") {
        return makeNode("call-stmt", call(name, n->children[1], line).node);
    }
    Attr r = Expr(n->children[1]);
    Entry* e = symtab.lookup(name);
    if (e == nullptr) {
        error("Undeclared variable '" + name + "'", line);
    } else if (e->kind == "func") {
        error("Cannot assign to function '" + name + "'", line);
    } else if (r.type != e->type && r.type != TYPE_ERROR) {
        error("Type mismatch: cannot assign " + r.type + " to " + e->type + " '" + name + "'", line);
    }
    return makeNode("=", makeNode(name), nullptr, r.node);
}

SyntaxNode* Translator::IF(Node* n){
    Attr c = Expr(n->children[2]);
    checkCondition(c, n->children[0]->line);
    SyntaxNode* thenPart = Block(n->children[4], true);

    Node* els = n->children[5];
    if (els->children[0]->label == "e") {
        return makeNode("if", c.node, nullptr, thenPart);
    }
    return makeNode("if-else", c.node, thenPart, ElseTail(els->children[1]));
}

SyntaxNode* Translator::ElseTail(Node* n){
    Node* child = n->children[0];
    if (child->label == "Block") 
        return Block(child, true);
    return IF(child);
}

SyntaxNode* Translator::WHILE(Node* n){
    Attr c = Expr(n->children[2]);
    checkCondition(c, n->children[0]->line);
    return makeNode("while", c.node, nullptr, Block(n->children[4], true));
}

SyntaxNode* Translator::ReturnStmt(Node* n){
    int line = n->children[0]->line;
    Node* tail = n->children[1];

    Attr value{"", nullptr};
    if (tail->children[0]->label != ";") 
        value = Expr(tail->children[0]);

    if (currentReturnType.empty()) {
        error("'return' outside of a function", line);
    } else if (value.node == nullptr) {
        error("Function must return " + currentReturnType, line);
    } else if (value.type != currentReturnType && value.type != TYPE_ERROR) {
        error("Type mismatch: returning " + value.type + " from " + currentReturnType + " function", line);
    }
    return makeNode("return", value.node);
}

void Translator::Args(Node* n, vector<Attr>& args){
    if (n->children[0]->label == "e") 
        return;

    args.push_back(Expr(n->children[0]));
    ArgsTail(n->children[1], args);
}

void Translator::ArgsTail(Node* n, vector<Attr>& args){
    if (n->children[0]->label == "e") 
        return;

    args.push_back(Expr(n->children[1]));
    ArgsTail(n->children[2], args);
}

Attr Translator::call(const string& name, Node* argsNode, int line){
    vector<Attr> args;
    Args(argsNode, args);

    SyntaxNode* argList = nullptr;
    for (int i = (int)args.size() - 1; i >= 0; i--) {
        argList = makeNode("arg", args[i].node, nullptr, argList);
    }
    SyntaxNode* node = makeNode("call", makeNode(name), nullptr, argList);

    Entry* e = symtab.lookup(name);
    if (e == nullptr && name == "print") {
        if (args.size() != 1) {
            error("'print' expects 1 argument(s), got " + to_string(args.size()), line);
            return {TYPE_ERROR, node};
        }
        return typed("int", node);
    }
    if (e == nullptr) {
        error("Undeclared function '" + name + "'", line);
        return {TYPE_ERROR, node};
    }
    if (e->kind != "func") {
        error("'" + name + "' is not a function", line);
        return {TYPE_ERROR, node};
    }
    if (args.size() != e->paramTypes.size()) {
        error("'" + name + "' expects " + to_string(e->paramTypes.size()) +
              " argument(s), got " + to_string(args.size()), line);
        return {TYPE_ERROR, node};
    }
    for (size_t i = 0; i < args.size(); i++) {
        if (args[i].type != e->paramTypes[i] && args[i].type != TYPE_ERROR) {
            error("Argument " + to_string(i + 1) + " of '" + name + "' must be " +
                  e->paramTypes[i] + ", found " + args[i].type, line);
            return {TYPE_ERROR, node};
        }
    }
    return typed(e->type, node);
}

Attr Translator::Expr(Node* n){
    return OrTail(n->children[1], AndExpr(n->children[0]));
}

Attr Translator::OrTail(Node* n, const Attr& inh){
    if (n->children[0]->label == "e") 
        return inh;

    Attr r = AndExpr(n->children[1]);
    string type = checkOperands(
        "||", 
        inh.type, 
        r.type, 
        n->children[0]->line
    );
    return OrTail(
        n->children[2], 
        typed(type, makeNode("||", inh.node, nullptr, r.node))
    );
}

Attr Translator::AndExpr(Node* n){
    return AndTail(n->children[1], RelExpr(n->children[0]));
}

Attr Translator::AndTail(Node* n, const Attr& inh){
    if (n->children[0]->label == "e") 
    return inh;
    Attr r = RelExpr(n->children[1]);
    string type = checkOperands("&&", inh.type, r.type, n->children[0]->line);
    return AndTail(
        n->children[2], 
        typed(type, makeNode("&&", inh.node, nullptr, r.node))
    );
}

Attr Translator::RelExpr(Node* n){
    return RelTail(n->children[1], AddExpr(n->children[0]));
}

Attr Translator::RelTail(Node* n, const Attr& inh){
    if (n->children[0]->label == "e") return inh;
    Node* op = n->children[0]->children[0];
    Attr r = AddExpr(n->children[1]);
    string type = checkOperands(op->lexeme, inh.type, r.type, op->line);
    return typed(type, makeNode(op->lexeme, inh.node, nullptr, r.node));
}

Attr Translator::AddExpr(Node* n){
    return AddTail(n->children[1], MulExpr(n->children[0]));
}

Attr Translator::AddTail(Node* n, const Attr& inh){
    if (n->children[0]->label == "e") 
        return inh;
    Node* op = n->children[0];
    Attr r = MulExpr(n->children[1]);
    string type = checkOperands(op->lexeme, inh.type, r.type, op->line);
    return AddTail(
        n->children[2], 
        typed(type, makeNode(op->lexeme, inh.node, nullptr, r.node))
    );
}

Attr Translator::MulExpr(Node* n){
    return MulTail(n->children[1], Unary(n->children[0]));
}

Attr Translator::MulTail(Node* n, const Attr& inh){
    if (n->children[0]->label == "e") 
        return inh;
    Node* op = n->children[0];
    Attr r = Unary(n->children[1]);
    string type = checkOperands(op->lexeme, inh.type, r.type, op->line);
    return MulTail(
        n->children[2], 
        typed(type, makeNode(op->lexeme, inh.node, nullptr, r.node))
    );
}

Attr Translator::Unary(Node* n){
    Node* first = n->children[0];
    if (first->label == "Primary") 
        return Primary(first);

    Attr r = Unary(n->children[1]);
    string type = (first->label == "!" && r.type != TYPE_ERROR) ? "int" : r.type;
    return typed(type, makeNode(first->lexeme, r.node));
}

Attr Translator::Primary(Node* n){
    Node* child = n->children[0];
    if (child->label == "ID") 
        return IdTail(n->children[1], child->lexeme, child->line);
    if (child->label == "NUM") 
        return {"int", makeNode(child->lexeme)};
    if (child->label == "CHR") 
        return {"char", makeNode(child->lexeme)};
    return Expr(n->children[1]);
}

Attr Translator::IdTail(Node* n, const string& name, int line){
    if (n->children[0]->label == "(") 
        return call(name, n->children[1], line);

    Entry* e = symtab.lookup(name);
    if (e == nullptr) {
        error("Undeclared variable '" + name + "'", line);
        return {TYPE_ERROR, makeNode(name)};
    }
    if (e->kind == "func") {
        error("Function '" + name + "' used as a variable", line);
        return {TYPE_ERROR, makeNode(name)};
    }
    return {e->type, makeNode(name)};
}

struct Quad {
    string op, arg1, arg2, result;
};
vector<Quad> code;
int tempCount = 0, labelCount = 0;

struct TempInfo {
    string name, type, scope;
};
vector<TempInfo> tempList;
string currentFunc;

struct FuncRange {
    int start, end;
};

class FunctionTable{
        unordered_map<string, FuncRange> functions;
        unordered_map<int, int> endOfStart;
    public:
        void build(const vector<Quad>& quads);
        bool lookup(const string& name, FuncRange& range) const;
        int skipDefinition(int pc) const;
};

void FunctionTable::build(const vector<Quad>& quads){
    functions.clear();
    endOfStart.clear();
    string openName;
    int openStart = -1;
    for (int i = 0; i < (int)quads.size(); i++) {
        if (quads[i].op == "func") {
            openName = quads[i].arg1;
            openStart = i;
        } else if (quads[i].op == "endfunc" && openStart != -1) {
            functions[openName] = {openStart, i};
            endOfStart[openStart] = i;
            openStart = -1;
        }
    }
}

bool FunctionTable::lookup(const string& name, FuncRange& range) const{
    auto it = functions.find(name);
    if (it == functions.end())
        return false;
    range = it->second;
    return true;
}

int FunctionTable::skipDefinition(int pc) const{
    auto it = endOfStart.find(pc);
    return it == endOfStart.end() ? pc : it->second + 1;
}

FunctionTable funcTable;

void buildFunctionTable(){
    funcTable.build(code);
}

const FunctionTable& getFunctionTable(){
    return funcTable;
}

string newTemp(const string& type){
    string name = "t" + to_string(++tempCount);
    tempList.push_back({name, type, currentFunc});
    return name;
}

string newLabel(){
    return "L" + to_string(++labelCount);
}

void gen(const string& op, const string& arg1 = "", const string& arg2 = "", const string& result = ""){
    code.push_back({op, arg1, arg2, result});
}

bool isBinaryOperator(const string& op){
    static const set<string> ops = {"+", "-", "*", "/", "%", "==", "!=", "<", ">", "<=", ">=", "&&", "||"};
    return ops.count(op) > 0;
}

string tac(SyntaxNode* n);

string tac_binary(SyntaxNode* n){
    string lval = tac(n->left);
    string rval = tac(n->right);
    string t = newTemp(n->type);
    gen(n->val, lval, rval, t);
    return t;
}

string tac_if(SyntaxNode* n){
    string r = tac(n->left);
    string l1 = newLabel();
    string l2 = newLabel();
    gen("if", r, "true", l1);
    gen("goto", l2);
    gen("label", l1);
    tac(n->right);
    gen("label", l2);
    return "";
}

string tac_if_else(SyntaxNode* n){
    string r = tac(n->left);
    string l1 = newLabel();
    string l2 = newLabel();
    string l3 = newLabel();
    gen("if", r, "true", l1);
    gen("goto", l2);
    gen("label", l1);
    tac(n->middle);
    gen("goto", l3);
    gen("label", l2);
    tac(n->right);
    gen("label", l3);
    return "";
}

string tac_while(SyntaxNode* n){
    string l1 = newLabel();
    string l2 = newLabel();
    string l3 = newLabel();
    gen("label", l1);
    string r = tac(n->left);
    gen("if", r, "true", l2);
    gen("goto", l3);
    gen("label", l2);
    tac(n->right);
    gen("goto", l1);
    gen("label", l3);
    return "";
}

string tac_call(SyntaxNode* n, string target, bool usesResult){
    string resultType = n->type.empty() ? "int" : n->type;
    vector<string> places;
    for (SyntaxNode* arg = n->right; arg != nullptr; arg = arg->right) {
        places.push_back(tac(arg->left));
    }
    for (const string& place : places) 
        gen("param", place);
    if (usesResult && target.empty()) 
        target = newTemp(resultType);
    gen("call", n->left->val, to_string(places.size()), target);
    return target;
}

string tac_assign(SyntaxNode* n){
    string target = n->left->val;
    if (n->right->val == "call" && 
        n->right->left != nullptr) {
        tac_call(n->right, target, true);
    } else {
        gen("=", tac(n->right), "", target);
    }
    return target;
}

string tac(SyntaxNode* n){
    if (n == nullptr) return "";

    if (n->val == "return") {
        gen("return", tac(n->left));
        return "";
    }
    if (n->left == nullptr && 
        n->middle == nullptr && 
        n->right == nullptr) {
        return n->val;
    }

    if (n->val == "seq") {
        tac(n->left);
        tac(n->right);
        return "";
    }
    if (n->val == "=") 
        return tac_assign(n);
    if (n->val == "if") 
        return tac_if(n);
    if (n->val == "if-else") 
        return tac_if_else(n);
    if (n->val == "while") 
        return tac_while(n);
    if (n->val == "call") 
        return tac_call(n, "", true); 
    if (n->val == "call-stmt") 
        return tac_call(n->left, "", false); 
    if (n->val == "func") {
        gen("func", n->left->val);
        currentFunc = n->left->val;
        tac(n->right);
        currentFunc = "";
        gen("endfunc");
        return "";
    }
    if (n->right == nullptr) {  
        string r = tac(n->left);
        string t = newTemp(n->type);
        gen(n->val, r, "", t);
        return t;
    }
    return tac_binary(n);
}

void printTAC(ostream& out){
    for (const Quad& q : code) {
        if (q.op == "label")
            out << q.arg1 << ":\n";
        else if (q.op == "func") 
            out << q.arg1 << ":\n";
        else if (q.op == "endfunc") 
            out << "endfunc\n";
        else if (q.op == "if") 
            out << "  if " << q.arg1 << " == " << q.arg2 << " goto " << q.result << "\n";
        else if (q.op == "goto") 
            out << "  goto " << q.arg1 << "\n";
        else if (q.op == "param") 
            out << "  param " << q.arg1 << "\n";
        else if (q.op == "return") 
            out << "  return " << q.arg1 << "\n";
        else if (q.op == "call") {
            out << "  ";
            if (!q.result.empty()) 
                out << q.result << " = ";
            out << "call " << q.arg1 << ", " << q.arg2 << "\n";
        }
        else if (q.op == "=") 
            out << "  " << q.result << " = " << q.arg1 << "\n";
        else if (q.arg2.empty()) 
            out << "  " << q.result << " = " << q.op << " " << q.arg1 << "\n";
        else 
            out << "  " << q.result << " = " << q.arg1 << " " << q.op << " " << q.arg2 << "\n";
    }
}

void printQuad(ostream& out){
    for (const Quad& q : code) {
        out << "(" << q.op << ", " << q.arg1 << ", " << q.arg2 << ", " << q.result << ")\n";
    }
}

bool semantic_analysis(){
    Node* tree = parser_analysis();
    if (tree == nullptr) return false;

    Translator tr;
    SyntaxNode* syntaxTree = tr.Program(tree);
    if (!tr.errors.empty()) {
        cout << "\nSemantic analysis failed with " << tr.errors.size() << " error(s):\n";
        
        for (const string& e : tr.errors) 
            cout << e << "\n";
        
        return false;
    }

    cout << "\nSyntax tree:\n";
    if (syntaxTree != nullptr) printSyntaxTree(syntaxTree, cout);

    tac(syntaxTree);
    for (const TempInfo& t : tempList) {
        tr.symtab.insertIn(t.scope, {t.name, "temp", t.type, {}, -1});
    }
    buildFunctionTable();

    cout << "\nThree address code:\n";
    printTAC(cout);

    cout << "\nSymbol table:\n";
    tr.symtab.print(cout);

    ofstream symFile("SymbolTable.txt");
    tr.symtab.print(symFile);
    ofstream tacFile("TAC.txt");
    printTAC(tacFile);
    ofstream QuadruplesFile("Quadruples.txt");
    printQuad(QuadruplesFile);

    symFile.close();
    tacFile.close();
    QuadruplesFile.close();
    return true;
}

#ifndef INTERPRETER
int main(){
    semantic_analysis();
    return 0;
}
#endif
