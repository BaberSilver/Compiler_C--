#include <iostream>
#include <unordered_map>
#include <stack>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <set>
#include <cstdlib>

#define PARSER
#include "23L0908_Lexical.cpp"   

using namespace std;

const int EPSILON = -1;
const int TERMINAL_BASE = 1000;
const int NO_RULE = -1;


struct Production {
    int lhs;
    vector<int> rhs;
};

struct InputToken {
    int line;
    int terminal;
    string lexeme;
};

struct Node {
    string label;
    string lexeme;
    int line=0;
    vector<Node*> children;
};

class Parser {
  protected:
    vector<InputToken> input;
    int current;

    vector<string> nonTerminals;
    vector<string> terminals;
    unordered_map<string,int> symbolId;
    vector<Production> productions;

    vector<set<int>> first;
    vector<set<int>> follow;
    vector<vector<int>> parseTable;

    void next(int=1);
    void readGrammar(const string& fileName);
    void computeFirst();
    void computeFollow();
    void initTable();

    bool isTerminal(int symbol);
    string name(int symbol);
    int addTerminal(const string& name);
    set<int> firstOfSequence(const vector<int>& seq, size_t from = 0);
    string productionString(int p);
    void printTree(Node* node, const string& prefix, bool last, bool root);

  public:
    Parser(const string& grammarFile);
    void putFirstFollowInFile(ostream& out);
    void putTableInFile(ostream& out);
    InputToken makeToken(const int& line,const string& lexeme, const string& category);
    Node* parse(const vector<InputToken>& tokens);
    void printTree(Node* root);
};

Parser::Parser(const string& grammarFile){
    readGrammar(grammarFile);
    computeFirst();
    computeFollow();
    initTable();
}

void Parser::next(int s){
    current += s;
}

bool Parser::isTerminal(int symbol){
    return symbol >= TERMINAL_BASE;
}

string Parser::name(int symbol){
    if (symbol == EPSILON) return "e";
    if (isTerminal(symbol)) return terminals[symbol - TERMINAL_BASE];
    return nonTerminals[symbol];
}

int Parser::addTerminal(const string& name){
    if (!symbolId.count(name)) {
        symbolId[name] = TERMINAL_BASE + terminals.size();
        terminals.push_back(name);
    }
    return symbolId[name];
}

string Parser::productionString(int p){
    string s = nonTerminals[productions[p].lhs] + " ->";
    if (productions[p].rhs.empty()) s += " e";
    for (int sym : productions[p].rhs) s += " " + name(sym);
    return s;
}


void Parser::readGrammar(const string& fileName){
    ifstream file(fileName);
    if (!file) {
        cout << "Error: " << fileName << " could not be opened." << endl;
        exit(1);
    }

    vector<pair<int,string>> rules;
    for (string line; getline(file, line); ) {
        size_t arrow = line.find("->");
        if (arrow == string::npos) continue;
        string lhs;
        stringstream(line.substr(0, arrow)) >> lhs;
        if (!symbolId.count(lhs)) {
            symbolId[lhs] = nonTerminals.size();
            nonTerminals.push_back(lhs);
        }
        rules.push_back({symbolId[lhs], line.substr(arrow + 2)});
    }

    for (auto& rule : rules) {
        stringstream ss(rule.second + " |");
        vector<int> rhs;
        for (string sym; ss >> sym; ) {
            if (sym == "|") {
                productions.push_back({rule.first, rhs});
                rhs.clear();
            }
            else if (sym != "e") {
                rhs.push_back(symbolId.count(sym) ? symbolId[sym] : addTerminal(sym));
            }
        }
    }
    addTerminal("$");
}

set<int> Parser::firstOfSequence(const vector<int>& seq, size_t from){
    set<int> result;
    for (size_t i = from; i < seq.size(); i++) {
        if (isTerminal(seq[i])) {
            result.insert(seq[i]);
            return result;
        }
        for (int t : first[seq[i]]) {
            if (t != EPSILON) result.insert(t);
        }
        if (!first[seq[i]].count(EPSILON)) return result;
    }
    result.insert(EPSILON);
    return result;
}

void Parser::computeFirst(){
    first.assign(nonTerminals.size(), set<int>());
    bool changed = true;
    while (changed) {
        changed = false;
        for (auto& p : productions) {
            for (int t : firstOfSequence(p.rhs)) {
                if (first[p.lhs].insert(t).second) changed = true;
            }
        }
    }
}

void Parser::computeFollow(){
    follow.assign(nonTerminals.size(), set<int>());
    follow[0].insert(symbolId["$"]);
    bool changed = true;
    while (changed) {
        changed = false;
        for (auto& p : productions) {
            for (size_t i = 0; i < p.rhs.size(); i++) {
                int B = p.rhs[i];
                if (isTerminal(B)) continue;
                set<int> add = firstOfSequence(p.rhs, i + 1);
                if (add.erase(EPSILON)) {
                    add.insert(follow[p.lhs].begin(), follow[p.lhs].end());
                }
                for (int t : add) {
                    if (follow[B].insert(t).second) changed = true;
                }
            }
        }
    }
}

void Parser::initTable(){
    parseTable.assign(nonTerminals.size(), vector<int>(terminals.size(), NO_RULE));

    for (size_t p = 0; p < productions.size(); p++) {
        int A = productions[p].lhs;
        set<int> lookaheads = firstOfSequence(productions[p].rhs);
        if (lookaheads.erase(EPSILON)) {
            lookaheads.insert(follow[A].begin(), follow[A].end());
        }
        for (int t : lookaheads) {
            int& cell = parseTable[A][t - TERMINAL_BASE];
            if (cell != NO_RULE) {
                cout << "LL(1) conflict at M[" << name(A) << ", " << name(t) << "]" << endl;
            }
            cell = p;
        }
    }
}

void Parser::putFirstFollowInFile(ostream& out){
    out << "FIRST and FOLLOW sets:\n";
    for (size_t A = 0; A < nonTerminals.size(); A++) {
        out << nonTerminals[A] << "\n  FIRST  = { ";
        for (int t : first[A]) out << name(t) << " ";
        out << "}\n  FOLLOW = { ";
        for (int t : follow[A]) out << name(t) << " ";
        out << "}\n";
    }
    out << endl;
}

void Parser::putTableInFile(ostream& out){
    out << "Parse table entries:\n";
    for (size_t A = 0; A < nonTerminals.size(); A++) {
        for (size_t t = 0; t < terminals.size(); t++) {
            if (parseTable[A][t] == NO_RULE) continue;
            out << "M[" << nonTerminals[A] << ", " << terminals[t] << "] = "
                << productionString(parseTable[A][t]) << "\n";
        }
    }
    out << endl;
}

InputToken Parser::makeToken(const int& line,const string& lexeme, const string& category){
    if (category == "ERROR") return {line, -1, lexeme};

    string symbol = lexeme;
    if (category == "IDENTIFIER")        symbol = "ID";
    else if (category == "INT_LITERAL")  symbol = "NUM";
    else if (category == "CHAR_LITERAL") symbol = "CHR";
    else if (category == "DATATYPE")     symbol = "TYPE";

    auto it = symbolId.find(symbol);
    if (it == symbolId.end() || !isTerminal(it->second)) return {line, -1, lexeme};
    return {line, it->second, lexeme};
}

Node* Parser::parse(const vector<InputToken>& tokens){
    input = tokens;
    int lastLine = input.empty() ? 1 : input.back().line;
    input.push_back({lastLine, symbolId["$"], "$"});
    current = 0;

    Node* root = new Node{nonTerminals[0], {}};
    stack<pair<int,Node*>> stack1;
    stack1.push({symbolId["$"], nullptr});
    stack1.push({0, root});

    while (!stack1.empty()) {
        int X = stack1.top().first;
        Node* node = stack1.top().second;
        InputToken& a = input[current];

        if (a.terminal == -1) {
            cout << "Error: unknown token '" << a.lexeme << "'" << endl;
            return nullptr;
        }
        if (isTerminal(X)) {
            if (X != a.terminal) {
                cout << "Error: expected '" << name(X) << "' but found '" << a.lexeme << "'" << endl;
                return nullptr;
            }
            if (node != nullptr) {
                node->lexeme = a.lexeme;
                node->line = a.line;
            }
            stack1.pop();
            next();
        }
        else {
            int p = parseTable[X][a.terminal - TERMINAL_BASE];
            if (p == NO_RULE) {
                cout << "Error: unexpected '" << a.lexeme << "' while parsing " << name(X) << endl;
                return nullptr;
            }
            stack1.pop();

            vector<int>& rhs = productions[p].rhs;
            if (rhs.empty()) {
                node->children.push_back(new Node{"e", {}});
            }
            for (int sym : rhs) {
                node->children.push_back(new Node{name(sym), {}});
            }
            for (int i = (int)rhs.size() - 1; i >= 0; i--) {
                stack1.push({rhs[i], node->children[i]});
            }
        }
    }
    return root;
}

void Parser::printTree(Node* root){
    printTree(root, "", true, true);
}

void Parser::printTree(Node* node, const string& prefix, bool last, bool root){
    cout << prefix << (root ? "" : (last ? "└── " : "├── ")) << node->label;
    if (!node->lexeme.empty() && node->lexeme != node->label) {
        cout << " (" << node->lexeme << ")";
    }
    cout << "\n";
    string childPrefix = prefix + (root ? "" : (last ? "    " : "│   "));
    for (size_t i = 0; i < node->children.size(); i++) {
        printTree(node->children[i], childPrefix, i + 1 == node->children.size(), false);
    }
}

Node* parser_analysis() {
    lexical_analysis();

    Parser p("ProductionRules.txt");

    ofstream tableFile("ParseTable.txt");
    p.putFirstFollowInFile(tableFile);
    p.putTableInFile(tableFile);
    cout << "FIRST/FOLLOW sets and parse table written to ParseTable.txt\n\n";

    vector<InputToken> input;
    for (const Token& tok : tokens) {
        input.push_back(p.makeToken(tok.line, tok.lexeme, tok.token));
    }

    Node* tree = p.parse(input);
    if (tree == nullptr) {
        cout << "error" << endl;
        return nullptr;
    }
    cout << "parse successful\n\nParse tree:\n";
    p.printTree(tree);
    return tree;
}

#ifndef SEMANTIC
int main(){
    parser_analysis();
    return 0;
}
#endif
