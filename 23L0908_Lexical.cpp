#include <iostream>
#include <fstream>
#include <string>
#include <unordered_set>
#include <vector>
#include <iterator>

using namespace std;

int error_count = 0;

struct Token{
    int line;
    string lexeme;
    string token;
};

vector<Token> tokens;

const unordered_set<string> keywords = {
    "if", "else", "while", "return"
};

const unordered_set<string> datatype = {
    "int", "char"
};

const unordered_set<string> two_char_ops = {
    "==", "!=", "<=", ">=", "&&", "||"
};

const string one_char_ops = "+-*/%=<>!";

const string punctuation = ";,(){}";

void report_error(int line, const string &msg) {
    cout << "<ERROR, line " << line << ": " << msg << ">" << '\n';
    error_count++;
}


void getFileData(string &str) {
    string filename ;
    cout << "Enter the filename: ";
    cin >> filename;
    ifstream file(filename);

    if (!file) {
        cout << "Error: File could not be opened." << '\n';
        return;
    }

    str.assign(istreambuf_iterator<char>(file), istreambuf_iterator<char>());
}



bool isAlpha(char c) {
    return ((c >= 'A' && c <= 'Z') ||
            (c >= 'a' && c <= 'z'));
}



bool isDigit(char c) {
    return (c >= '0' && c <= '9');
}


bool isAlphaNumeric(char c) {
    return isAlpha(c) || isDigit(c);
}


string scanCharLiteral(const string &str, int &i, bool &closed) {

    int n = str.length();

    string lexeme = "";
    closed = false;


    lexeme += str[i++];


    if (i < n && str[i] != '\n') {

        lexeme += str[i++];


        if (i < n && str[i] == '\'') {
            lexeme += str[i++];
            closed = true;
        }
    }

    return lexeme;
}


void processFileData(const string &str) {

    int i = 0;
    int n = str.length();
    int line = 1;


    while (i < n) {

        char c = str[i];



        if (c == '\n') {
            line++;
            i++;
            continue;
        }



        if (c == ' ' || c == '\t' || c == '\r') {
            i++;
            continue;
        }



        if (c == '/' &&
            i + 1 < n &&
            str[i + 1] == '/') {

            while (i < n && str[i] != '\n') {
                i++;
            }

            continue;
        }



        if (c == '/' &&
            i + 1 < n &&
            str[i + 1] == '*') {

            int start_line = line;
            i += 2;

            bool closed = false;

            while (i < n) {

                if (str[i] == '*' &&
                    i + 1 < n &&
                    str[i + 1] == '/') {

                    i += 2;
                    closed = true;
                    break;
                }

                if (str[i] == '\n') {
                    line++;
                }

                i++;
            }

            if (!closed) {
                report_error(
                    start_line,
                    "unterminated comment"
                );
            }

            continue;
        }

        if (isAlpha(c) || c == '_') {

            string lexeme = "";

            while (i < n &&
                   (isAlphaNumeric(str[i]) ||
                    str[i] == '_')) {

                lexeme += str[i++];
            }

            const char* category = keywords.count(lexeme) ? "KEYWORD"
                                 : datatype.count(lexeme) ? "DATATYPE"
                                 : "IDENTIFIER";
            cout << "<" << category << ", " << lexeme << ">\n";
            tokens.push_back({line, lexeme, category});

            continue;
        }



        if (isDigit(c)) {

            string lexeme = "";

            while (i < n && isDigit(str[i])) {
                lexeme += str[i++];
            }


            if (i < n &&
                (isAlpha(str[i]) ||
                 str[i] == '_')) {

                while (i < n &&
                       (isAlphaNumeric(str[i]) ||
                        str[i] == '_')) {

                    lexeme += str[i++];
                }

                report_error(
                    line,
                    "invalid number '" +
                    lexeme + "'"
                );
                tokens.push_back({line,lexeme, "ERROR"});
            }
            else {

                cout << "<INT_LITERAL, "
                     << lexeme
                     << ">" << '\n';
                tokens.push_back({line,lexeme, "INT_LITERAL"});
            }

            continue;
        }



        if (c == '\'') {

            bool closed;

            string lexeme =
                scanCharLiteral(str, i, closed);

            if (!closed) {

                report_error(
                    line,
                    "invalid character literal " +
                    lexeme
                );
                tokens.push_back({line,lexeme, "ERROR"});
            }
            else {

                cout << "<CHAR_LITERAL, "
                     << lexeme
                     << ">" << '\n';
                tokens.push_back({line,lexeme, "CHAR_LITERAL"});
            }

            continue;
        }


        if (i + 1 < n) {
            string two = str.substr(i, 2);
            if (two_char_ops.count(two)) {
                cout << "<OPERATOR, " << two << ">" << '\n';
                tokens.push_back({line, two, "OPERATOR"});
                i += 2;
                continue;
            }
        }


        if (one_char_ops.find(c) != string::npos) {

            cout << "<OPERATOR, "
                 << c
                 << ">" << '\n';
            tokens.push_back({line,string(1, c), "OPERATOR"});
            i++;
            continue;
        }



        if (punctuation.find(c) != string::npos) {

            cout << "<PUNCTUATION, "
                 << c
                 << ">" << '\n';
            tokens.push_back({line,string(1, c), "PUNCTUATION"});

            i++;
            continue;
        }



        report_error(
            line,
            string("unknown character '") +
            c + "'"
        );
        tokens.push_back({line,string(1, c), "ERROR"});

        i++;
    }


    cout << "\nLexical analysis finished with "
         << error_count
         << " error(s)." << '\n';
}

void putTokensInFile() {
    ofstream outfile("tokens.txt");

    if (!outfile) {
        cout << "Error: Could not open output file." << '\n';
        return;
    }

    for (const Token &t : tokens) {
        outfile << "<" << t.token << ", " << t.lexeme << ">" << '\n';
    }

    outfile.close();
}

void lexical_analysis() {
    string str;

    getFileData(str);

    if (!str.empty()) {
        processFileData(str);
    }

    putTokensInFile();
    if(error_count>0){
        cout<<"Lexical analysis failed with "<<error_count<<" error(s)."<<endl;
    }
}
#ifndef PARSER
int main() {
    lexical_analysis();
    return 0;
}
#endif
