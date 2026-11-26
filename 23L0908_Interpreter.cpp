#include<iostream>
#include<fstream>
#include<sstream>
#include<cstdint>

#define INTERPRETER
#include"23L0908_Semantic_IntCode.cpp"
using namespace std;

const int MAX_CALL_DEPTH = 10000;
const long long MAX_STEPS = 50000000;

bool getQuads(const string& fileName){
    ifstream quadFile(fileName);
    if (!quadFile) {
        cout << "Error: " << fileName << " could not be opened." << endl;
        return false;
    }

    string line;
    while (getline(quadFile, line)) {
        size_t open = line.find('(');
        size_t close = line.rfind(')');
        if (open == string::npos || close == string::npos || close < open)
            continue;

        string content = line.substr(open + 1, close - open - 1);
        vector<string> f;
        size_t start = 0, pos;
        while (f.size() < 3 && (pos = content.find(", ", start)) != string::npos) {
            f.push_back(content.substr(start, pos - start));
            start = pos + 2;
        }
        f.push_back(content.substr(start));
        while (f.size() < 4) f.push_back("");

        code.push_back({f[0], f[1], f[2], f[3]});
    }
    return true;
}

bool getSymbolTable(const string& fileName, SymbolTable& symtab){
    ifstream symFile(fileName);
    if (!symFile) {
        cout << "Error: " << fileName << " could not be opened." << endl;
        return false;
    }

    int depth = 0;
    string line;
    while (getline(symFile, line)) {
        size_t indent = line.find_first_not_of(' ');
        if (indent == string::npos) continue;
        int level = (int)indent / 2;
        line = line.substr(indent);

        if (line.compare(0, 7, "Scope: ") == 0) {
            if (level == 0) continue;
            while (depth >= level) { symtab.closeScope(); depth--; }
            symtab.openScope(line.substr(7));
            depth = level;
            continue;
        }

        size_t sep = line.find(" : ");
        if (sep == string::npos) continue;
        Entry e;
        e.name = line.substr(0, sep);
        string rest = line.substr(sep + 3);

        size_t comma = rest.find(',');
        e.kind = rest.substr(0, comma);
        rest = rest.substr(comma + 2);

        size_t lineAt = rest.rfind(", line ");
        e.line = 0;
        if (lineAt != string::npos) {
            e.line = atoi(rest.c_str() + lineAt + 7);
            rest = rest.substr(0, lineAt);
        }

        size_t paren = rest.find(" (");
        e.type = rest.substr(0, paren);
        if (paren != string::npos) {
            stringstream params(rest.substr(paren + 2, rest.size() - paren - 3));
            for (string p; getline(params, p, ','); ) {
                size_t b = p.find_first_not_of(' ');
                if (b != string::npos) e.paramTypes.push_back(p.substr(b));
            }
        }
        symtab.insert(e);
    }
    while (depth-- > 0) symtab.closeScope();
    return true;
}

struct Val {
    int v = 0;
    bool isChar = false;
};

struct Slot {
    string type;
    int value = 0;
};

struct ActivationRecord {
    string function;
    unordered_map<string, Slot> vars; 
    vector<string> varOrder;          
    int returnPc = 0;             
    string returnTarget;            
};

struct RuntimeError {
    string msg;
};

class Interpreter {
        const SymbolTable& symtab;
        const FunctionTable& funcs;
        unordered_map<string, int> labels;
        vector<ActivationRecord> stack;   
        vector<Val> pendingParams;
        int pc = 0;
        bool trace;
        ostream& outFile;
        struct Finished {
            ActivationRecord last;
            int calls = 0;
            int returned = 0;
        };
        vector<string> calledOrder;
        unordered_map<string, Finished> finished;
        int exitCode = 0;
        bool hasExit = false;

        ActivationRecord makeRecord(const string& function, const Scope* scope, bool recursive);
        string pad() const { return string(2 * (stack.size() - 1), ' '); }
        void showRecord(const ActivationRecord& ar, const string& indent) const;

                static int normalize(const string& type, long long v);
        Slot* findVar(const string& name);
        Val resolve(const string& operand);
        void store(const string& name, Val v);
        Val binary(const string& op, Val a, Val b);

        void call(const string& name, int argc, const string& target, int returnPc);
        void doReturn(Val v);
        void builtinPrint(int argc, const string& target);

    public:
        Interpreter(const SymbolTable& st, const FunctionTable& ft, bool trace, ostream& out);
        void run();
        void showGlobals(ostream& out) const;
};

Interpreter::Interpreter(const SymbolTable& st, const FunctionTable& ft, bool t, ostream& out)
    : symtab(st), funcs(ft), trace(t), outFile(out) {
    for (int i = 0; i < (int)code.size(); i++) {
        if (code[i].op == "label") labels[code[i].arg1] = i;
    }
}

ActivationRecord Interpreter::makeRecord(const string& function, const Scope* scope, bool recursive){
    ActivationRecord ar;
    ar.function = function;
    for (const Entry* e : SymbolTable::entries(scope, recursive)) {
        if (e->kind == "func") continue;
        if (ar.vars.insert({e->name, Slot{e->type, 0}}).second)
            ar.varOrder.push_back(e->name);
    }
    return ar;
}

int Interpreter::normalize(const string& type, long long v){
    return type == "char" ? (int)(int8_t)v : (int)(int32_t)v;
}

Slot* Interpreter::findVar(const string& name){
    auto it = stack.back().vars.find(name);
    if (it != stack.back().vars.end()) return &it->second;
    it = stack[0].vars.find(name);
    if (it != stack[0].vars.end()) return &it->second;
    return nullptr;
}

Val Interpreter::resolve(const string& s){
    if (s.empty()) throw RuntimeError{"missing operand"};
    if (s[0] == '\'' && s.size() >= 3) return {(int)(int8_t)s[1], true};
    if (isDigit(s[0])) return {normalize("int", atoll(s.c_str())), false};
    if (s == "true") return {1, false};
    if (s == "false") return {0, false};

    if (Slot* slot = findVar(s)) return {slot->value, slot->type == "char"};
    throw RuntimeError{"use of undefined name '" + s + "'"};
}

void Interpreter::store(const string& name, Val v){
    if (Slot* slot = findVar(name)) {
        slot->value = normalize(slot->type, v.v);
        return;
    }
    throw RuntimeError{"assignment to undefined name '" + name + "'"};
}

Val Interpreter::binary(const string& op, Val a, Val b){
    long long x = a.v, y = b.v, r = 0;
    bool arithmetic = true;
    if (op == "+") r = x + y;
    else if (op == "-") r = x - y;
    else if (op == "*") r = x * y;
    else if (op == "/" || op == "%") {
        if (y == 0) throw RuntimeError{"division by zero"};
        r = (op == "/") ? x / y : x % y;
    }
    else {
        arithmetic = false;
        if (op == "==") r = x == y;
        else if (op == "!=") r = x != y;
        else if (op == "<") r = x < y;
        else if (op == ">") r = x > y;
        else if (op == "<=") r = x <= y;
        else if (op == ">=") r = x >= y;
        else if (op == "&&") r = x && y;
        else if (op == "||") r = x || y;
        else throw RuntimeError{"unknown operator '" + op + "'"};
    }
    bool ch = arithmetic && a.isChar && b.isChar;
    return {normalize(ch ? "char" : "int", r), ch};
}

void Interpreter::showRecord(const ActivationRecord& ar, const string& indent) const{
    cout << indent << "[AR " << ar.function << "]";
    for (const string& n : ar.varOrder) {
        const Slot& s = ar.vars.at(n);
        cout << " " << n << "=" << s.value;
        if (s.type == "char") cout << "('" << (char)s.value << "')";
    }
    cout << "\n";
}

void Interpreter::showGlobals(ostream& out) const{
    out << "\nGlobal activation record:\n";
    if (stack[0].varOrder.empty()) {
        out << "  (no global variables)\n";
    }
    for (const string& n : stack[0].varOrder) {
        const Slot& s = stack[0].vars.at(n);
        out << "  " << s.type << " " << n << " = " << s.value;
        if (s.type == "char") out << " ('" << (char)s.value << "')";
        out << "\n";
    }
    for (const string& f : calledOrder) {
        const Finished& fin = finished.at(f);
        out << "\nActivation record of " << f << " (last of " << fin.calls << " call(s)):\n";
        for (const string& n : fin.last.varOrder) {
            const Slot& sl = fin.last.vars.at(n);
            out << "  " << sl.type << " " << n << " = " << sl.value;
            if (sl.type == "char") out << " ('" << (char)sl.value << "')";
            out << "\n";
        }
        out << "  return value = " << fin.returned << "\n";
    }
    if (hasExit) out << "main returned " << exitCode << "\n";
}

void Interpreter::builtinPrint(int argc, const string& target){
    if (argc != 1 || (int)pendingParams.size() < 1)
        throw RuntimeError{"print expects 1 argument"};
    Val v = pendingParams.back();
    pendingParams.pop_back();
    if (v.isChar) { cout << (char)v.v << "\n"; outFile << (char)v.v << "\n"; }
    else { cout << v.v << "\n"; outFile << v.v << "\n"; }
    if (!target.empty()) store(target, {0, false});
}

void Interpreter::call(const string& name, int argc, const string& target, int returnPc){
    FuncRange range;
    const Entry* f = nullptr;
    for (const Entry* e : SymbolTable::entries(symtab.globalScope(), false)) {
        if (e->name == name && e->kind == "func") f = e;
    }
    if (f == nullptr || !funcs.lookup(name, range)) {
        if (name == "print") { builtinPrint(argc, target); return; }
        throw RuntimeError{"call to undefined function '" + name + "'"};
    }
    if ((int)stack.size() >= MAX_CALL_DEPTH)
        throw RuntimeError{"call stack overflow (depth " + to_string(MAX_CALL_DEPTH) + ")"};
    if ((int)pendingParams.size() < argc)
        throw RuntimeError{"missing arguments for call to '" + name + "'"};

    const Scope* scope = symtab.functionScope(name);
    ActivationRecord ar = makeRecord(name, scope, true);
    ar.returnPc = returnPc;
    ar.returnTarget = target;

    vector<const Entry*> params;
    for (const Entry* e : SymbolTable::entries(scope, false)) {
        if (e->kind == "param") params.push_back(e);
    }
    if ((int)params.size() != argc)
        throw RuntimeError{"'" + name + "' expects " + to_string(params.size()) +
                           " argument(s), got " + to_string(argc)};

    size_t first = pendingParams.size() - argc;
    for (int i = 0; i < argc; i++) {
        Slot& slot = ar.vars[params[i]->name];
        slot.value = normalize(slot.type, pendingParams[first + i].v);
    }
    pendingParams.resize(first);

    if (trace) {
        cout << pad() << "-> call " << name << "(";
        for (int i = 0; i < argc; i++) {
            cout << (i ? ", " : "") << params[i]->name << "=" << ar.vars[params[i]->name].value;
        }
        cout << ")  [depth " << stack.size() << "]\n";
    }
    stack.push_back(std::move(ar));
    pc = range.start + 1;
}

void Interpreter::doReturn(Val v){
    if (stack.size() < 2) throw RuntimeError{"'return' outside of a function"};

    ActivationRecord ar = std::move(stack.back());
    stack.pop_back();

    const Entry* f = nullptr;
    for (const Entry* e : SymbolTable::entries(symtab.globalScope(), false)) {
        if (e->name == ar.function && e->kind == "func") f = e;
    }
    string type = f ? f->type : "int";
    v = {normalize(type, v.v), type == "char"};

    if (trace) {
        showRecord(ar, pad() + "   ");
        cout << pad() << "<- return " << v.v << " from " << ar.function << "\n";
    }
    Finished& fin = finished[ar.function];
    if (fin.calls == 0) calledOrder.push_back(ar.function);
    fin.calls++;
    fin.last = ar;
    fin.returned = v.v;
    pc = ar.returnPc;
    if (ar.function == "main" && stack.size() == 1) { exitCode = v.v; hasExit = true; }
    if (!ar.returnTarget.empty()) store(ar.returnTarget, v);
}

void Interpreter::run(){
    stack.push_back(makeRecord("global", symtab.globalScope(), false));
    for (const Scope* child : symtab.globalScope()->children) {
        if (child->name != "block") continue;
        for (const Entry* e : SymbolTable::entries(child, true)) {
            if (stack[0].vars.insert({e->name, Slot{e->type, 0}}).second)
                stack[0].varOrder.push_back(e->name);
        }
    }

    FuncRange mainRange;
    bool mainPending = funcs.lookup("main", mainRange);
    long long steps = 0;

    while (true) {
        if (pc >= (int)code.size()) {
            if (!mainPending) break;
            mainPending = false;
            call("main", 0, "", (int)code.size());
            continue;
        }
        if (++steps > MAX_STEPS)
            throw RuntimeError{"step limit exceeded (possible infinite loop)"};

        const Quad& q = code[pc];
        const string& op = q.op;

        if (op == "func") {
            pc = stack.size() == 1 ? funcs.skipDefinition(pc) : pc + 1;
        }
        else if (op == "endfunc") {
            doReturn({0, false});
        }
        else if (op == "label") {
            pc++;
        }
        else if (op == "goto") {
            auto it = labels.find(q.arg1);
            if (it == labels.end()) throw RuntimeError{"unknown label " + q.arg1};
            pc = it->second;
        }
        else if (op == "if") {
            bool cond = resolve(q.arg1).v != 0;
            bool jump = (q.arg2 == "false") ? !cond : cond;
            auto it = labels.find(q.result);
            if (it == labels.end()) throw RuntimeError{"unknown label " + q.result};
            pc = jump ? it->second : pc + 1;
        }
        else if (op == "param") {
            pendingParams.push_back(resolve(q.arg1));
            pc++;
        }
        else if (op == "call") {
            int returnPc = pc + 1;
            pc = returnPc;
            call(q.arg1, atoi(q.arg2.c_str()), q.result, returnPc);
        }
        else if (op == "return") {
            doReturn(q.arg1.empty() ? Val{0, false} : resolve(q.arg1));
        }
        else if (op == "=") {
            store(q.result, resolve(q.arg1));
            pc++;
        }
        else if (q.arg2.empty()) {
            Val a = resolve(q.arg1);
            if (op == "-") store(q.result, {normalize(a.isChar ? "char" : "int", -(long long)a.v), a.isChar});
            else if (op == "!") store(q.result, {a.v == 0, false});
            else throw RuntimeError{"unknown unary operator '" + op + "'"};
            pc++;
        }
        else {
            store(q.result, binary(op, resolve(q.arg1), resolve(q.arg2)));
            pc++;
        }
    }
}

void printQuads(){
    cout << "\nThree-Address Code (quadruples):\n";
    for (size_t i = 0; i < code.size(); i++) {
        cout << i << ": (" << code[i].op << ", " << code[i].arg1 << ", "
             << code[i].arg2 << ", " << code[i].result << ")\n";
    }
}

int main(int argc, char** argv){
    bool trace = false;
    vector<string> files;
    for (int i = 1; i < argc; i++) {
        string a = argv[i];
        if (a == "-t") trace = true;
        else files.push_back(a);
    }
    if (files.empty() && !semantic_analysis())
        return 1;
    code.clear();

    string quadFile = files.size() > 0 ? files[0] : "Quadruples.txt";
    string symFile = files.size() > 1 ? files[1] : "SymbolTable.txt";

    SymbolTable symtab;
    if (!getQuads(quadFile) || !getSymbolTable(symFile, symtab))
        return 1;
    buildFunctionTable();

    ofstream outFile("output.txt");
    int status = 0;

    if (trace) printQuads();

    Interpreter in(symtab, getFunctionTable(), trace, outFile);
    cout << "\nProgram output:\n";
    try {
        in.run();
    } catch (const RuntimeError& e) {
        cout << "Runtime error: " << e.msg << "\n";
        status = 1;
    }
    in.showGlobals(cout);
    ofstream arFile("ActivationRecords.txt");
    in.showGlobals(arFile);

    return status;
}
