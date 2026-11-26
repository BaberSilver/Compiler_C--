# C-- Compiler and Interpreter

A small compiler front end and interpreter for **C--**, a subset of C, written in C++. A program goes through lexical analysis, LL(1) parsing, semantic analysis and three-address code generation, and is then interpreted from the generated quadruples using activation records.

```
source file
    │
    ▼
Lexer ──────────► tokens.txt
    │
    ▼
LL(1) Parser ───► ParseTable.txt
    │  parse tree
    ▼
Translator ─────► symbol table + syntax tree
    │
    ▼
Code generator ─► TAC.txt, Quadruples.txt, SymbolTable.txt
    │
    ▼
Interpreter ────► output.txt, ActivationRecords.txt
```

## Build and run

Requires `g++`. Each stage includes the one before it, so compiling the interpreter builds the whole pipeline:

```bash
g++ 23L0908_Interpreter.cpp -o 23L0908_Interpreter
./23L0908_Interpreter
```

The program asks for a source file:

```
Enter the filename: input1.cpp
```

Run it from the project folder: the parser reads `ProductionRules.txt` from the current directory, and all output files are written there.

| Command | What it does |
|---|---|
| `./23L0908_Interpreter` | Runs the full pipeline on a source file |
| `./23L0908_Interpreter -t` | Same, and also lists the quadruples and traces every call and return |
| `./23L0908_Interpreter Quadruples.txt SymbolTable.txt` | Skips the front end and runs existing files |

Each earlier stage can also be built on its own:

```bash
g++ 23L0908_Lexical.cpp -o 23L0908_Lexical
g++ 23L0908_Parser.cpp -o 23L0908_Parser
g++ 23L0908_Semantic_IntCode.cpp -o 23L0908_Semantic_IntCode
```

## Files

| File | Contents |
|---|---|
| `23L0908_Lexical.cpp` | Lexer |
| `23L0908_Parser.cpp` | Table-driven LL(1) parser: FIRST and FOLLOW sets, parse table, parse tree |
| `23L0908_Semantic_IntCode.cpp` | Symbol table, type checking, syntax tree, three-address code, function table |
| `23L0908_Interpreter.cpp` | Loads quadruples and the symbol table, then executes them |
| `ProductionRules.txt` | The grammar the parser loads at run time |
| `23L0908_Grammar.txt` | Grammar write-up. `ProductionRules.txt` is the version the code uses |
| `input1.cpp` to `input3.cpp` | Sample programs |

## The C-- language

| Feature | Supported |
|---|---|
| Types | `int` (32-bit), `char` (signed 8-bit) |
| Literals | Integers, single characters such as `'a'` |
| Statements | Declaration, assignment, call, `if` / `else if` / `else`, `while`, `return`, `{ }` block |
| Functions | Parameters, return values, recursion |
| Operators | `+ - * / %`, `== != < > <= >=`, `&& \|\| !`, unary `-` |
| Comments | `//` and `/* */` |
| Built-in | `print(value)` prints one value on its own line |

Rules to know:

- **`main` is optional.** Top-level statements run in order. If a `main` function exists, it runs after them.
- **No implicit conversion.** Both operands of an operator must have the same type, and so must both sides of an assignment.
- **Conditions must be `int`.** Comparisons produce `int`.
- **Define before use.** A function can call itself, but not a function defined later.
- **`if` and `while` bodies need braces.**

## Example

```c
int square(int n){
    return n * n;
}

int x = square(3) + 1;
print(x);
```

Program output:

```
10
```

Three-address code (`TAC.txt`):

```
square:
  t1 = n * n
  return t1
endfunc
  param 3
  t2 = call square, 1
  t3 = t2 + 1
  x = t3
  param x
  call print, 1
```

Symbol table (`SymbolTable.txt`):

```
Scope: file
  square : func, int (int), line 1
  x : var, int, line 5
  t2 : temp, int
  t3 : temp, int
  Scope: square
    n : param, int, line 1
    t1 : temp, int
```

Activation records (`ActivationRecords.txt`):

```
Global activation record:
  int x = 10
  int t2 = 9
  int t3 = 10

Activation record of square (last of 1 call(s)):
  int n = 3
  int t1 = 9
  return value = 9
```

## How it works

### Symbol table

A tree of scopes: the file scope, one child per function, and a `block` scope for each nested `{ }`. Each entry records a name, a kind (`var`, `param`, `func` or `temp`), a type and, except for temporaries, a source line. Lookup walks from the current scope up through its parents.

### Semantic analysis

The `Translator` walks the parse tree with one method per grammar construct. It fills the symbol table, checks types and builds the syntax tree. It reports undeclared names, redeclarations, type mismatches, non-`int` conditions, wrong return types and wrong call arguments. No code is generated if there are errors.

### Intermediate code

`tac()` walks the syntax tree and emits quadruples `(op, arg1, arg2, result)`. Every temporary is typed and added to the symbol table, in the scope of the function that uses it, or in the file scope for top-level code.

### Interpreter

- A function table maps each function to the positions of its `func` and `endfunc` quadruples, so definitions are skipped during top-level execution.
- Each call pushes an activation record built from that function's scope. It holds slots for parameters, local variables and temporaries, plus the return position and the variable that receives the result.
- Names are looked up in the current record first, then in the global record.
- `ActivationRecords.txt` shows the global record and the record of the last completed call of each function.
- Execution stops with a runtime error on division by zero, more than 10,000 stacked records or more than 50,000,000 steps.

## Known limitations

- **Shadowing does not work at run time.** The symbol table keeps scopes separate, but quadruples refer to variables by name only. A variable redeclared in an inner block shares storage with the outer one, so changes remain after the block ends.
- **Avoid names such as `t1` or `t2`.** They collide with compiler temporaries and give wrong results.
- **`&&` and `||` do not short-circuit.** Both sides are always evaluated.
- **Comparisons cannot be chained.** `a < b < c` is a syntax error.
- **No arrays, strings, `for` loops or escape sequences** such as `'\n'`.
- **A file that cannot be opened** is treated as an empty program instead of stopping the run.
- **Output files can be stale.** After a failed run, some may still hold results from an earlier one.
- **No optimization**, such as constant folding or common subexpression elimination with a DAG.
