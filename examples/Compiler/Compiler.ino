/**
 * @file FVM/Compiler.ino
 * @version 1.0
 *
 * @section License
 * Copyright (C) 2016, Mikael Patel
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place, Suite 330,
 * Boston, MA  02111-1307  USA
 *
 * @section Description
 * Basic interactive compiler for the Forth Virtual Machine (FVM).
 * Compiles forth definitions, statements and generates virtual
 * machine code (C++).
 */

#include "FVM.h"

// Dictionary entry prefix (C++)
#define PREFIX "WORD"

FVM_COLON(0, FORWARD_MARK, "mark>")
// : mark> ( -- addr ) here 0 c, ;
  FVM_OP(HERE),
  FVM_OP(ZERO),
  FVM_OP(C_COMMA),
  FVM_OP(EXIT)
};

FVM_COLON(1, FORWARD_RESOLVE, "resolve>")
// : resolve> ( addr -- ) here over - swap c! ;
  FVM_OP(HERE),
  FVM_OP(OVER),
  FVM_OP(MINUS),
  FVM_OP(SWAP),
  FVM_OP(C_STORE),
  FVM_OP(EXIT)
};

FVM_COLON(2, BACKWARD_MARK, "<mark")
// : <mark ( -- addr ) here ;
  FVM_OP(HERE),
  FVM_OP(EXIT)
};

FVM_COLON(3, BACKWARD_RESOLVE, "<resolve")
// : <resolve ( addr -- ) here - c, ;
  FVM_OP(HERE),
  FVM_OP(MINUS),
  FVM_OP(C_COMMA),
  FVM_OP(EXIT)
};

FVM_COLON(4, IF, "if")
// : if ( -- addr ) compile (0branch) mark> ; immediate
  FVM_OP(COMPILE),
  FVM_OP(ZERO_BRANCH),
  FVM_CALL(FORWARD_MARK),
  FVM_OP(EXIT)
};

FVM_COLON(5, THEN, "then")
// : then ( addr -- ) resolve> ; immediate
  FVM_CALL(FORWARD_RESOLVE),
  FVM_OP(EXIT)
};

FVM_COLON(6, ELSE, "else")
// : else ( addr1 -- addr2 ) compile (branch) mark> swap resolve> ; immediate
  FVM_OP(COMPILE),
  FVM_OP(BRANCH),
  FVM_CALL(FORWARD_MARK),
  FVM_OP(SWAP),
  FVM_CALL(FORWARD_RESOLVE),
  FVM_OP(EXIT)
};

FVM_COLON(7, BEGIN, "begin")
// : begin ( -- addr ) <mark ; immediate
  FVM_CALL(BACKWARD_MARK),
  FVM_OP(EXIT)
};

FVM_COLON(8, AGAIN, "again")
// : again ( addr -- ) compile (branch) <resolve ; immediate
  FVM_OP(COMPILE),
  FVM_OP(BRANCH),
  FVM_CALL(BACKWARD_RESOLVE),
  FVM_OP(EXIT)
};

FVM_COLON(9, UNTIL, "until")
// : until ( addr -- ) compile (0branch) <resolve ; immediate
  FVM_OP(COMPILE),
  FVM_OP(ZERO_BRANCH),
  FVM_CALL(BACKWARD_RESOLVE),
  FVM_OP(EXIT)
};

#if 0
FVM_COLON(10, WHILE, "while")
// : while ( addr1 -- addr1 addr2 ) compile (0branch) mark> ; immediate
  FVM_OP(COMPILE),
  FVM_OP(ZERO_BRANCH),
  FVM_CALL(FORWARD_MARK),
  FVM_OP(EXIT)
};
#else
const int WHILE = 10;
const char WHILE_PSTR[] PROGMEM = "while";
# define WHILE_CODE IF_CODE
#endif

FVM_COLON(11, REPEAT, "repeat")
// : repeat (addr1 addr2 -- ) swap [compile] again resolve> ; immediate
  FVM_OP(SWAP),
  FVM_CALL(AGAIN),
  FVM_CALL(FORWARD_RESOLVE),
  FVM_OP(EXIT)
};

const FVM::code_P FVM::fntab[] PROGMEM = {
  FORWARD_MARK_CODE,
  FORWARD_RESOLVE_CODE,
  BACKWARD_MARK_CODE,
  BACKWARD_RESOLVE_CODE,
  IF_CODE,
  THEN_CODE,
  ELSE_CODE,
  BEGIN_CODE,
  AGAIN_CODE,
  UNTIL_CODE,
  WHILE_CODE,
  REPEAT_CODE
};

const str_P FVM::fnstr[] PROGMEM = {
  (str_P) FORWARD_MARK_PSTR,
  (str_P) FORWARD_RESOLVE_PSTR,
  (str_P) BACKWARD_MARK_PSTR,
  (str_P) BACKWARD_RESOLVE_PSTR,
  (str_P) IF_PSTR,
  (str_P) THEN_PSTR,
  (str_P) ELSE_PSTR,
  (str_P) BEGIN_PSTR,
  (str_P) AGAIN_PSTR,
  (str_P) UNTIL_PSTR,
  (str_P) WHILE_PSTR,
  (str_P) REPEAT_PSTR,
  0
};

// Data area for the shell
const int DATA_MAX = 1024;
uint8_t data[DATA_MAX];
bool compiling = false;
uint8_t* latest = data;

// Forth virtual machine and task
FVM fvm(data);
FVM::task_t task(Serial);

void setup()
{
  Serial.begin(57600);
  while (!Serial);
  Serial.println(F("FVM/Compiler: started [Newline]"));
}

void loop()
{
  char buffer[32];
  int op;
  char c;

  // Scan buffer for a single word or number
  c = fvm.scan(buffer, task);
  op = fvm.lookup(buffer);

  // Check for special words or literals
  if (op < 0) {

    // Check for start of definition
    if (!strcmp_P(buffer, PSTR(":")) && !compiling) {
      fvm.scan(buffer, task);
      fvm.compile((FVM::code_t) 0);
      fvm.compile(buffer);
      compiling = true;
    }

    // Check for variable definition
    else if (!strcmp_P(buffer, PSTR("variable")) && !compiling) {
      fvm.scan(buffer, task);
      fvm.compile((FVM::code_t) 0);
      fvm.compile(buffer);
      fvm.compile(FVM::OP_VAR);
      fvm.compile((FVM::code_t) 0);
      fvm.compile((FVM::code_t) 0);
      *latest = fvm.dp() - latest - 1;
      latest = fvm.dp();
    }

    // Check for variable definition
    else if (!strcmp_P(buffer, PSTR("constant")) && !compiling) {
      int val = task.pop();
      fvm.scan(buffer, task);
      fvm.compile((FVM::code_t) 0);
      fvm.compile(buffer);
      fvm.compile(FVM::OP_CONST);
      fvm.compile(val);
      fvm.compile(val >> 8);
      *latest = fvm.dp() - latest - 1;
      latest = fvm.dp();
    }

    // Check for comment
    else if (!strcmp_P(buffer, PSTR("("))) {
      while (Serial.read() != ')') yield();
    }

    // Check for print string literal
    else if (!strcmp_P(buffer, PSTR(".\"")) && compiling) {
      fvm.compile(FVM::OP_DOT_QUOTE);
      while (1) {
	while (!Serial.available()) yield();
	c = Serial.read();
	if (c == '\"') break;
	fvm.compile(c);
      }
      fvm.compile((FVM::code_t) 0);
    }

    // Check for room; available memory
    else if (!strcmp_P(buffer, PSTR("room")) && !compiling) {
      task.push(DATA_MAX - (fvm.dp() - data));
    }

    // Check for list compiled words
    else if (!strcmp_P(buffer, PSTR("compiled-words"))) {
      words(Serial);
    }

    // Check for code generation
    else if (!strcmp_P(buffer, PSTR("generate-code")) && !compiling) {
      codegen(Serial);
      fvm.dp(data);
      latest = data;
      *latest = 0;
    }

    // Check for help
    else if (!strcmp_P(buffer, PSTR("help"))) {
      Serial.println(F(": NAME BODY ;          - declare function"));
      Serial.println(F("( COMMENT)             - comment"));
      Serial.println(F("variable NAME          - create variable"));
      Serial.println(F("VALUE constant NAME    - create constant"));
      Serial.println(F("compiled-words         - list compiled words"));
      Serial.println(F("generate-code          - print source code"));
      Serial.println(F("room                   - available memory"));
      Serial.println();
    }

    // Check for function call (defined word)
    else if ((op = lookup(buffer)) < 0) {
      fvm.compile(op);
    }

    // Check for end of definition
    else if (!strcmp_P(buffer, PSTR(";")) && compiling) {
      fvm.compile(FVM::OP_EXIT);
      *latest = fvm.dp() - latest - 1;
      latest = fvm.dp();
      compiling = false;
    }

    // Assume number (should check)
    else {
      int value = atoi(buffer);
      if (compiling) {
	if (value < -128 || value > 127) {
	  fvm.compile(FVM::OP_LIT);
	  fvm.compile(value);
	  fvm.compile(value >> 8);
	}
	else {
	  fvm.compile(FVM::OP_CLIT);
	  fvm.compile(value);
	}
      }
      else {
	task.push(value);
      }
    }
  }

  // Compile operation
  else if (op < 128 && compiling) {
    fvm.compile(op);
  }
  else {
    fvm.execute(op, task);
  }

  // Prompt on end of line
  if (c == '\n' && !compiling) Serial.println(F(" ok"));
}

int lookup(const char* s)
{
  int op = -1;
  uint8_t* dp = data;
  while (dp < fvm.dp()) {
    if (!strcmp(s, (const char*) dp + 1)) return (op);
    dp += ((uint8_t) *dp) + 1;
    op -= 1;
  }
  return (0);
}

void words(Stream& ios)
{
  uint8_t* dp = data;
  int nr = 0;
  while (dp < fvm.dp()) {
    ios.print(nr++);
    ios.print(':');
    ios.print(dp - data);
    ios.print(':');
    ios.print(*dp);
    ios.print(':');
    ios.println((const char*) dp + 1);
    dp += ((uint8_t) *dp) + 1;
  }
}

void codegen(Stream& ios)
{
  int nr, last;
  uint8_t* dp;
  char* name;
  int val;

  // Generate function name strings and code
  dp = data;
  nr = 0;
  while (dp < fvm.dp()) {
    uint8_t length = *dp++;
    ios.print(F("const char " PREFIX));
    ios.print(nr);
    ios.print(F("_PSTR[] PROGMEM = \""));
    name = (char*) dp;
    ios.print(name);
    ios.println(F("\";"));
    uint8_t n = strlen(name) + 1;
    dp += n;
    switch (*dp) {
    case FVM::OP_VAR:
      ios.print(F("FVM::data_t " PREFIX));
      ios.print(nr);
      ios.println(';');
      ios.print(F("const FVM::var_t " PREFIX));
      ios.print(nr);
      ios.println(F("_VAR[] PROGMEM = {"));
      ios.print(F("  FVM::OP_VAR, &" PREFIX));
      ios.println(nr);
      ios.println(F("};"));
      dp += sizeof(FVM::var_t);
      break;
    case FVM::OP_CONST:
      val = dp[2] << 8 | dp[1];
      ios.print(F("const FVM::const_t " PREFIX));
      ios.print(nr);
      ios.println(F("_CONST[] PROGMEM = {"));
      ios.print(F("  FVM::OP_CONST, "));
      ios.println(val);
      ios.println(F("};"));
      dp += sizeof(FVM::const_t);
      break;
    default:
      ios.print(F("const FVM::code_t " PREFIX));
      ios.print(nr);
      ios.print(F("_CODE[] PROGMEM = {\n  "));
      while (n < length) {
	int8_t code = (int8_t) *dp++;
	ios.print(code);
	if (++n < length) ios.print(F(", "));
      }
      ios.println();
      ios.println(F("};"));
    }
    nr += 1;
  }
  last = nr;

  // Generate function code table
  nr = 0;
  dp = data;
  ios.println(F("const FVM::code_P FVM::fntab[] PROGMEM = {"));
  while (dp < fvm.dp()) {
    uint8_t length = *dp++;
    name = (char*) dp;
    uint8_t n = strlen(name) + 1;
    switch (dp[n]) {
    case FVM::OP_VAR:
      ios.print(F("  (code_P) &" PREFIX));
      ios.print(nr++);
      ios.print(F("_VAR"));
      break;
    case FVM::OP_CONST:
      ios.print(F("  (code_P) &" PREFIX));
      ios.print(nr++);
      ios.print(F("_CONST"));
      break;
    default:
      ios.print(F("  " PREFIX));
      ios.print(nr++);
      ios.print(F("_CODE"));
    }
    if (nr != last) ios.println(','); else ios.println();
    dp += length;
  }
  ios.println(F("};"));

  // Generate function string table
  nr = 0;
  dp = data;
  ios.println(F("const str_P FVM::fnstr[] PROGMEM = {"));
  while (dp < fvm.dp()) {
    uint8_t length = *dp++;
    ios.print(F("  (str_P) " PREFIX));
    ios.print(nr++);
    ios.println(F("_PSTR,"));
    dp += length;
  }
  ios.println(F("  0"));
  ios.println(F("};"));
}
