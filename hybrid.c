/******************************************************************************
* Original work Copyright (C) 1994-2017 Lua.org, PUC-Rio.
* Modified work Copyright (C)      2017 Hugo Musso Gualandi
* Modified work Copyright (C)      2017 Matheus Coelho Ambrozio
*
* Permission is hereby granted, free of charge, to any person obtaining
* a copy of this software and associated documentation files (the
* "Software"), to deal in the Software without restriction, including
* without limitation the rights to use, copy, modify, merge, publish,
* distribute, sublicense, and/or sell copies of the Software, and to
* permit persons to whom the Software is furnished to do so, subject to
* the following conditions:
*
* The above copyright notice and this permission notice shall be
* included in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
******************************************************************************/

// for strdup:
#define _XOPEN_SOURCE 500

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// magic constants
#define LUA_SIGNATURE "\x1bLua"
#define LUA_VERSION 0x53
#define LUAC_FORMAT 0

#define LUAC_DATA "\x19\x93\r\n\x1a\n"
#define LUAC_INT 0x5678
#define LUAC_NUM ((double)370.5)

// Lua parameters
#define LUAI_MAXSHORTLEN 40

#define LUA_TNIL                0
#define LUA_TBOOLEAN            1
#define LUA_TLIGHTUSERDATA      2
#define LUA_TNUMBER             3
#define LUA_TSTRING             4
#define LUA_TTABLE              5
#define LUA_TFUNCTION           6
#define LUA_TUSERDATA           7
#define LUA_TTHREAD             8
#define LUA_NUMTAGS             9

#define LUA_TNUMFLT     (LUA_TNUMBER | (0 << 4))  /* float numbers */
#define LUA_TNUMINT     (LUA_TNUMBER | (1 << 4))  /* integer numbers */ 

#define LUA_TSHRSTR     (LUA_TSTRING | (0 << 4))  /* short strings */
#define LUA_TLNGSTR     (LUA_TSTRING | (1 << 4))  /* long strings */

// Platform dependent things
#define SIZE_INT 4
#define SIZE_SIZE_T 8
#define SIZE_INSTRUCTION 4
#define SIZE_LUA_INT 8
#define SIZE_LUA_NUMBER 8



//
// Lua Data Types
//

typedef uint8_t Byte;
typedef int32_t Int;
typedef uint32_t Instruction;
typedef int64_t lua_integer;
typedef double lua_float;


typedef enum { SHORT_STRING, LONG_STRING } StringType;
typedef struct {
    StringType typ;
    const char *str;
} String;

typedef struct {
    int typ;
    union {
        int b;
        lua_integer i;
        lua_float n;
    } u;
} Value;

// Function prototype
typedef struct Proto {
    Byte numparams;  /* number of fixed parameters */
    Byte is_vararg;
    Byte maxstacksize;  /* number of registers needed by this function */
    Int sizeupvalues;  /* size of 'upvalues' */
    Int sizek;  /* size of 'k' */
    Int sizecode;
    //Int sizelineinfo;
    Int sizep;  /* size of 'p' */
    //Int sizelocvars;
    Int linedefined;  /* debug information  */
    Int lastlinedefined;  /* debug information  */
    Value *k;  /* constants used by the function */
    Instruction *code;  /* opcodes */
    //struct Proto **p;  /* functions defined inside the function */
    //int *lineinfo;  /* map from opcodes to source lines (debug information) */
    //LocVar *locvars;  /* information about local variables (debug information) */
    //Upvaldesc *upvalues;  /* upvalue information */
    //struct LClosure *cache;  /* last-created closure with this prototype */
    String  *source;  /* used for debug information */
    //GCObject *gclist;
} Proto;

//
// Manipulating values
//

//static
//void get_nil(Value *v)
//{
//    assert(v->typ == LUA_TNIL);
//}

static
int get_bool(Value *v)
{
    assert(v->typ == LUA_TBOOLEAN);
    return v->u.b;
}

static
lua_integer get_int(Value *v)
{
    assert(v->typ == LUA_TNUMINT);
    return v->u.i;
}

static
lua_float get_float(Value *v)
{
    assert(v->typ == LUA_TNUMFLT);
    return v->u.n;
}


static
void set_nil(Value *v)
{
    v->typ = LUA_TNIL;
}

static
void set_bool(Value *v, int b)
{
    v->typ = LUA_TBOOLEAN;
    v->u.b = b;
}

static
void set_int(Value *v, lua_integer i)
{
    v->typ = LUA_TNUMINT;
    v->u.i = i;
}

static
void set_float(Value *v, lua_float n)
{
    v->typ = LUA_TNUMFLT;
    v->u.n = n;
}


// =========
// Instructions 
// =========


/*===========================================================================
  We assume that instructions are unsigned numbers.
  All instructions have an opcode in the first 6 bits.
  Instructions can have the following fields:
        'A' : 8 bits
        'B' : 9 bits
        'C' : 9 bits
        'Ax' : 26 bits ('A', 'B', and 'C' together)
        'Bx' : 18 bits ('B' and 'C' together)
        'sBx' : signed Bx

  A signed argument is represented in excess K; that is, the number
  value is the unsigned value minus K. K is exactly the maximum value
  for that argument (so that -max is represented by 0, and +max is
  represented by 2*max), which is half the maximum for the corresponding
  unsigned argument.
===========================================================================*/

/*
 *  +---+---+---+----+
 *  | B | C | A | OP |
 *  +----+--+---+----+
 *    9   9   8   6
 *
 *  +-------+---+----+
 *  |   Bx  | A | OP |
 *  +-------+---+----+
 *      18    8   6
 *
 *  +-----------+----+
 *  |     Ax    | OP |
 *  +-----------+----+
 *        26      6
 */

#define SIZE_C          9
#define SIZE_B          9
#define SIZE_Bx         (SIZE_C + SIZE_B)
#define SIZE_A          8
#define SIZE_Ax         (SIZE_C + SIZE_B + SIZE_A)

#define SIZE_OP         6

#define POS_OP          0
#define POS_A           (POS_OP + SIZE_OP)
#define POS_C           (POS_A + SIZE_A)
#define POS_B           (POS_C + SIZE_C)
#define POS_Bx          POS_C
#define POS_Ax          POS_A


#define MAXARG_A        ((1<<SIZE_A)-1)
#define MAXARG_B        ((1<<SIZE_B)-1)
#define MAXARG_C        ((1<<SIZE_C)-1)

#define MAXARG_Ax       ((1<<SIZE_Ax)-1)
#define MAXARG_Bx       ((1<<SIZE_Bx)-1)
#define MAXARG_sBx      (MAXARG_Bx>>1)    // 'sBx' is signed

static inline
uint32_t OP(Instruction instr)
{
    return (instr >> POS_OP) & 0x3f;
}

static inline
uint32_t A(Instruction instr)
{
    return (instr >> POS_A) & 0xff;
}

static inline
uint32_t B(Instruction instr)
{
    return (instr >> POS_B) & 0x1ff;
}

static inline
uint32_t C(Instruction instr)
{
    return (instr >> POS_C) & 0x1ff;
}

static inline
uint32_t Ax(Instruction instr)
{
    return (instr >>  POS_Ax) & 0x3ffffff;
}

static inline
uint32_t Bx(Instruction instr)
{
    return (instr >> POS_Bx) & 0x3ffff;
}

static inline
int32_t sBx(Instruction instr)
{
    return ((int32_t) Bx(instr)) - MAXARG_sBx;
}

// Constant s

/*
** R(x) - register
** Kst(x) - constant (in constant table)
** RK(x) == if ISK(x) then Kst(INDEXK(x)) else R(x)
*/

/* this bit 1 means constant (0 means register) */
#define BITRK           (1 << (SIZE_B - 1))

/* test whether value is a constant */
static
uint32_t ISK(uint32_t x)
{
    return x & BITRK;
}

/* gets the index of the constant */
static
uint32_t INDEXK(uint32_t x)
{
    return x & ~BITRK;
}

/* code a constant index as a RK value */
//static 
//uint32_t RKASK(uint32_t x)
//{
//    return (x | BITRK);
//}

typedef enum {
/*----------------------------------------------------------------------
name          code     args    description
------------------------------------------------------------------------*/
OP_MOVE       =  0, /*  A B     R(A) := R(B)                                    */
OP_LOADK      =  1, /*  A Bx    R(A) := Kst(Bx)                                 */
OP_LOADKX     =  2, /*  A       R(A) := Kst(extra arg)                          */
OP_LOADBOOL   =  3, /*  A B C   R(A) := (Bool)B; if (C) pc++                    */
OP_LOADNIL    =  4, /*  A B     R(A), R(A+1), ..., R(A+B) := nil                */
OP_GETUPVAL   =  5, /*  A B     R(A) := UpValue[B]                              */

OP_GETTABUP   =  6, /*  A B C   R(A) := UpValue[B][RK(C)]                       */
OP_GETTABLE   =  7, /*  A B C   R(A) := R(B)[RK(C)]                             */

OP_SETTABUP   =  8, /*  A B C   UpValue[A][RK(B)] := RK(C)                      */
OP_SETUPVAL   =  9, /*  A B     UpValue[B] := R(A)                              */
OP_SETTABLE   = 10, /*  A B C   R(A)[RK(B)] := RK(C)                            */

OP_NEWTABLE   = 11, /*  A B C   R(A) := {} (size = B,C)                         */

OP_SELF       = 12, /*  A B C   R(A+1) := R(B); R(A) := R(B)[RK(C)]   */

OP_ADD        = 13, /*  A B C   R(A) := RK(B) + RK(C)                           */
OP_SUB        = 14, /*  A B C   R(A) := RK(B) - RK(C)                           */
OP_MUL        = 15, /*  A B C   R(A) := RK(B) * RK(C)                           */
OP_MOD        = 16, /*  A B C   R(A) := RK(B) % RK(C)                           */
OP_POW        = 17, /*  A B C   R(A) := RK(B) ^ RK(C)                           */
OP_DIV        = 18, /*  A B C   R(A) := RK(B) / RK(C)                           */
OP_IDIV       = 19, /*  A B C   R(A) := RK(B) // RK(C)                          */
OP_BAND       = 20, /*  A B C   R(A) := RK(B) & RK(C)                           */
OP_BOR        = 21, /*  A B C   R(A) := RK(B) | RK(C)                           */
OP_BXOR       = 22, /*  A B C   R(A) := RK(B) ~ RK(C)                           */
OP_SHL        = 23, /*  A B C   R(A) := RK(B) << RK(C)                          */
OP_SHR        = 24, /*  A B C   R(A) := RK(B) >> RK(C)                          */
OP_UNM        = 25, /*  A B     R(A) := -R(B)                                   */
OP_BNOT       = 26, /*  A B     R(A) := ~R(B)                                   */
OP_NOT        = 27, /*  A B     R(A) := not R(B)                                */
OP_LEN        = 28, /*  A B     R(A) := length of R(B)                          */

OP_CONCAT     = 29, /*  A B C   R(A) := R(B).. ... ..R(C)                       */

OP_JMP        = 30, /*  A sBx   pc+=sBx; if (A) close all upvalues >= R(A - 1)  */
OP_EQ         = 31, /*  A B C   if ((RK(B) == RK(C)) ~= A) then pc++            */
OP_LT         = 32, /*  A B C   if ((RK(B) <  RK(C)) ~= A) then pc++            */
OP_LE         = 33, /*  A B C   if ((RK(B) <= RK(C)) ~= A) then pc++            */

OP_TEST       = 34, /*  A C     if not (R(A) <=> C) then pc++                   */
OP_TESTSET    = 35, /*  A B C   if (R(B) <=> C) then R(A) := R(B) else pc++     */

OP_CALL       = 36, /*  A B C   R(A), ... ,R(A+C-2) := R(A)(R(A+1), ... ,R(A+B-1)) */
OP_TAILCALL   = 37, /*  A B C   return R(A)(R(A+1), ... ,R(A+B-1))              */
OP_RETURN     = 38, /*  A B     return R(A), ... ,R(A+B-2)      (see note)      */

OP_FORLOOP    = 39, /*  A sBx   R(A)+=R(A+2); if R(A) <?= R(A+1) then { pc+=sBx; R(A+3)=R(A) }*/
OP_FORPREP    = 40, /*  A sBx   R(A)-=R(A+2); pc+=sBx                           */

OP_TFORCALL   = 41, /*  A C     R(A+3), ... ,R(A+2+C) := R(A)(R(A+1), R(A+2));  */
OP_TFORLOOP   = 42, /*  A sBx   if R(A+1) ~= nil then { R(A)=R(A+1); pc += sBx }*/

OP_SETLIST    = 43, /*  A B C   R(A)[(C-1)*FPF+i] := R(A+i), 1 <= i <= B        */

OP_CLOSURE    = 44, /*  A Bx    R(A) := closure(KPROTO[Bx])                     */

OP_VARARG     = 45, /*  A B     R(A), R(A+1), ..., R(A+B-2) = vararg            */

OP_EXTRAARG   = 46, /*  Ax      extra (larger) argument for previous opcode     */
} OpCode;

#define NUM_OPCODES     (1 + ((int) OP_EXTRAARG))

/*===========================================================================
  Notes:
  (*) In OP_CALL, if (B == 0) then B = top. If (C == 0), then 'top' is
  set to last_result+1, so next open instruction (OP_CALL, OP_RETURN,
  OP_SETLIST) may use 'top'.

  (*) In OP_VARARG, if (B == 0) then use actual number of varargs and
  set top (like in OP_CALL with C == 0).

  (*) In OP_RETURN, if (B == 0) then return up to 'top'.

  (*) In OP_SETLIST, if (B == 0) then B = 'top'; if (C == 0) then next
  'instruction' is EXTRAARG(real C).

  (*) In OP_LOADKX, the next 'instruction' is always EXTRAARG.

  (*) For comparisons, A specifies what condition the test should accept
  (true or false).

  (*) All 'skips' (pc++) assume that next instruction is a jump.

===========================================================================*/


static
const char *const lua_opnames[] = {
    "MOVE",
    "LOADK",
    "LOADKX",
    "LOADBOOL",
    "LOADNIL",
    "GETUPVAL",
    "GETTABUP",
    "GETTABLE",
    "SETTABUP",
    "SETUPVAL",
    "SETTABLE",
    "NEWTABLE",
    "SELF",
    "ADD",
    "SUB",
    "MUL",
    "MOD",
    "POW",
    "DIV",
    "IDIV",
    "BAND",
    "BOR",
    "BXOR",
    "SHL",
    "SHR",
    "UNM",
    "BNOT",
    "NOT",
    "LEN",
    "CONCAT",
    "JMP",
    "EQ",
    "LT",
    "LE",
    "TEST",
    "TESTSET",
    "CALL",
    "TAILCALL",
    "RETURN",
    "FORLOOP",
    "FORPREP",
    "TFORCALL",
    "TFORLOOP",
    "SETLIST",
    "CLOSURE",
    "VARARG",
    "EXTRAARG",
    NULL
};


enum OpMode {iABC, iABx, iAsBx, iAx};  /* basic instruction format */

enum OpArgMask {
    OpArgN,  /* argument is not used */
    OpArgU,  /* argument is used */
    OpArgR,  /* argument is a register or a jump offset */
    OpArgK   /* argument is a constant or register/constant */
};

#define opmode(t,a,b,c,m) (((t)<<7) | ((a)<<6) | ((b)<<4) | ((c)<<2) | (m))

#define getOpMode(m)    (((enum OpMode) lua_opmodes[m]) & 3)
#define getBMode(m)     (((enum OpArgMask) (lua_opmodes[m] >> 4)) & 3)
#define getCMode(m)     (((enum OpArgMask) (lua_opmodes[m] >> 2)) & 3)
#define testAMode(m)    (lua_opmodes[m] & (1 << 6))
#define testTMode(m)    (lua_opmodes[m] & (1 << 7))

static
const Byte lua_opmodes[] = {
/*       T  A    B       C     mode                opcode       */
  opmode(0, 1, OpArgR, OpArgN, iABC)            /* OP_MOVE */
 ,opmode(0, 1, OpArgK, OpArgN, iABx)            /* OP_LOADK */
 ,opmode(0, 1, OpArgN, OpArgN, iABx)            /* OP_LOADKX */
 ,opmode(0, 1, OpArgU, OpArgU, iABC)            /* OP_LOADBOOL */
 ,opmode(0, 1, OpArgU, OpArgN, iABC)            /* OP_LOADNIL */
 ,opmode(0, 1, OpArgU, OpArgN, iABC)            /* OP_GETUPVAL */
 ,opmode(0, 1, OpArgU, OpArgK, iABC)            /* OP_GETTABUP */
 ,opmode(0, 1, OpArgR, OpArgK, iABC)            /* OP_GETTABLE */
 ,opmode(0, 0, OpArgK, OpArgK, iABC)            /* OP_SETTABUP */
 ,opmode(0, 0, OpArgU, OpArgN, iABC)            /* OP_SETUPVAL */
 ,opmode(0, 0, OpArgK, OpArgK, iABC)            /* OP_SETTABLE */
 ,opmode(0, 1, OpArgU, OpArgU, iABC)            /* OP_NEWTABLE */
 ,opmode(0, 1, OpArgR, OpArgK, iABC)            /* OP_SELF */
 ,opmode(0, 1, OpArgK, OpArgK, iABC)            /* OP_ADD */
 ,opmode(0, 1, OpArgK, OpArgK, iABC)            /* OP_SUB */
 ,opmode(0, 1, OpArgK, OpArgK, iABC)            /* OP_MUL */
 ,opmode(0, 1, OpArgK, OpArgK, iABC)            /* OP_MOD */
 ,opmode(0, 1, OpArgK, OpArgK, iABC)            /* OP_POW */
 ,opmode(0, 1, OpArgK, OpArgK, iABC)            /* OP_DIV */
 ,opmode(0, 1, OpArgK, OpArgK, iABC)            /* OP_IDIV */
 ,opmode(0, 1, OpArgK, OpArgK, iABC)            /* OP_BAND */
 ,opmode(0, 1, OpArgK, OpArgK, iABC)            /* OP_BOR */
 ,opmode(0, 1, OpArgK, OpArgK, iABC)            /* OP_BXOR */
 ,opmode(0, 1, OpArgK, OpArgK, iABC)            /* OP_SHL */
 ,opmode(0, 1, OpArgK, OpArgK, iABC)            /* OP_SHR */
 ,opmode(0, 1, OpArgR, OpArgN, iABC)            /* OP_UNM */
 ,opmode(0, 1, OpArgR, OpArgN, iABC)            /* OP_BNOT */
 ,opmode(0, 1, OpArgR, OpArgN, iABC)            /* OP_NOT */
 ,opmode(0, 1, OpArgR, OpArgN, iABC)            /* OP_LEN */
 ,opmode(0, 1, OpArgR, OpArgR, iABC)            /* OP_CONCAT */
 ,opmode(0, 0, OpArgR, OpArgN, iAsBx)           /* OP_JMP */
 ,opmode(1, 0, OpArgK, OpArgK, iABC)            /* OP_EQ */
 ,opmode(1, 0, OpArgK, OpArgK, iABC)            /* OP_LT */
 ,opmode(1, 0, OpArgK, OpArgK, iABC)            /* OP_LE */
 ,opmode(1, 0, OpArgN, OpArgU, iABC)            /* OP_TEST */
 ,opmode(1, 1, OpArgR, OpArgU, iABC)            /* OP_TESTSET */
 ,opmode(0, 1, OpArgU, OpArgU, iABC)            /* OP_CALL */
 ,opmode(0, 1, OpArgU, OpArgU, iABC)            /* OP_TAILCALL */
 ,opmode(0, 0, OpArgU, OpArgN, iABC)            /* OP_RETURN */
 ,opmode(0, 1, OpArgR, OpArgN, iAsBx)           /* OP_FORLOOP */
 ,opmode(0, 1, OpArgR, OpArgN, iAsBx)           /* OP_FORPREP */
 ,opmode(0, 0, OpArgN, OpArgU, iABC)            /* OP_TFORCALL */
 ,opmode(0, 1, OpArgR, OpArgN, iAsBx)           /* OP_TFORLOOP */
 ,opmode(0, 0, OpArgU, OpArgU, iABC)            /* OP_SETLIST */
 ,opmode(0, 1, OpArgU, OpArgN, iABx)            /* OP_CLOSURE */
 ,opmode(0, 1, OpArgU, OpArgN, iABC)            /* OP_VARARG */
 ,opmode(0, 0, OpArgU, OpArgU, iAx)             /* OP_EXTRAARG */
};


// Parsing bytecode
// ================

#define EOF_ERROR "unexpected end of file"
//
void error_default() {
    fprintf(stderr, "Error\n");
    exit(1);
}
//

static
void error(const char *msg)
{
    fprintf(stderr, "%s\n", msg);
    exit(1);
}

static
int checkLiteral(FILE* F, const char *str)
{
    size_t n = strlen(str);
    for (size_t i=0; i<n; i++) {
        char c;
        size_t nread = fread(&c, sizeof(c), 1, F);
        if (nread != 1) { error(EOF_ERROR); }
        if (c != str[i]) { return 0; }
    }
    return 1;
}

static
void loadVector(FILE *F, void *v, size_t size, size_t n)
{
    if (n != fread(v, size, n, F) ) { error(EOF_ERROR); }
}

Byte loadByte(FILE *F)
{
    Byte b;
    loadVector(F, &b, 1, 1);
    return b;
}

Int loadInt(FILE * F)
{
    Int n;
    loadVector(F, &n, SIZE_INT, 1);
    return n;
}

size_t loadSize(FILE *F)
{
    size_t n;
    loadVector(F, &n, SIZE_SIZE_T, 1);
    return n;
}

lua_integer loadInteger(FILE *F)
{
    lua_integer n;
    loadVector(F, &n, SIZE_LUA_INT, 1);
    return n;
}

lua_float loadNumber(FILE *F)
{
    lua_float n;
    loadVector(F, &n, SIZE_LUA_NUMBER, 1);
    return n;
}


static
String * loadString(FILE * F)
{
    size_t size = loadByte(F);
    if (size == 0xFF) {
        size = loadSize(F);
    }

    if (size == 0) {
        return NULL;
    } else {
        // TODO: intern strings?

        char * buff = calloc(size, 1);
        loadVector(F, buff, 1, size-1);
        buff[size] = '\0';
       
        String *string = calloc(1, sizeof(String));
        string->typ = LONG_STRING;
        string->str = buff;

        return string;
    }
}

static
void loadValue(FILE *F, Value *v)
{
    Byte typ = loadByte(F);
    switch (typ) {
        case LUA_TNIL: {
            set_nil(v);
        } break;

        case LUA_TBOOLEAN: {
            set_bool(v, loadByte(F));
        } break;

        case LUA_TNUMFLT: {
            set_float(v, loadNumber(F));
        } break;

        case LUA_TNUMINT: {
            set_int(v, loadInteger(F));
        } break;

        case LUA_TSHRSTR:
        case LUA_TLNGSTR: {
            error("not implemented: string constants");
            //String * string = loadString(F);
            //assert(string != NULL);
            //printf("str: %s\n", string->str);
        } break;

        default: {
            assert(0);
        }
    }
}

static
Proto * loadFunction(FILE *F, String *parent_source)
{
    Proto *f = calloc(1, sizeof(Proto));

    f->source = loadString(F);
    if (!f->source) { f->source = parent_source; }
    if (!f->source) { error("missing source!"); }
    f->linedefined = loadInt(F);
    f->lastlinedefined = loadInt(F);
    f->numparams = loadByte(F);
    f->is_vararg = loadByte(F);
    f->maxstacksize = loadByte(F);

    // LoadCode(S, f);
    {
        f->sizecode = loadInt(F);
        if (f->sizecode < 0) { error("negative size, wtf"); }

        f->code = calloc(f->sizecode, sizeof(Instruction));
        loadVector(F, f->code, SIZE_INSTRUCTION, f->sizecode);
    }

    //LoadConstants(S, f);
    {
        f->sizek = loadInt(F);
        if (f->sizek < 0) { error("negative size, wtf"); }

        f->k = calloc(f->sizek, sizeof(Value));
        for(int i=0; i < f->sizek; i++){ 
            loadValue(F, &f->k[i]);
        }
    }

    //LoadUpvalues(S, f);
    {
        f->sizeupvalues = loadInt(F);
        if (f->sizeupvalues < 0) { error("negative size, wtf"); }

        for (int i=0; i < f->sizeupvalues; i++) {
            //error("not implemented");

            // name = NULL;
            /*Byte instack =*/ loadByte(F);
            /*Byte idx =*/ loadByte(F);
            //printf("upvalues[%zu].instack = %d\n", i, instack);
            //printf("upvalues[%zu].idx = %d\n", i, idx);
        }
    }

    //LoadProtos(S, f);
    {
        f->sizep = loadInt(F);
        if (f->sizep < 0) { error("negative size, wtf"); }

        for (int i=0; i < f->sizep; i++) {
            error("not implemented: loading inner functions");
        }
    }

    //LoadDebug(S, f);
    {
        // TODO
    }

    return f;
}

Proto * loadChunkBytecode(FILE *F)
{

    // Header
    { 
        if (!checkLiteral(F, LUA_SIGNATURE)) { error("bad signature"); }
        if (loadByte(F) != LUA_VERSION) { error("lua version mismatch"); }
        if (loadByte(F) != LUAC_FORMAT) { error("luac format mismatch"); }
        if (!checkLiteral(F, LUAC_DATA)) { error("corrupted"); }
        if (loadByte(F) != SIZE_INT) { error("wrong size: machine int"); }
        if (loadByte(F) != SIZE_SIZE_T) { error("wrong size: size_t"); }
        if (loadByte(F) != SIZE_INSTRUCTION) { error("wrong size: Instruction"); }
        if (loadByte(F) != SIZE_LUA_INT) { error("wrong size: lua_Integer"); }
        if (loadByte(F) != SIZE_LUA_NUMBER) { error("wrong size: lua_Number"); }
        if (loadInteger(F) != LUAC_INT) { error("endianess mismatch"); }
        if (loadNumber(F) != LUAC_NUM) { error("bad floating point format"); }
    }

    /*Byte closureSize =*/ loadByte(F); //????
    //printf("closureSize(?) = %u\n", closureSize);
    Proto *f = loadFunction(F, NULL);

    return f;
}

#define MYK(x)          (-1-(x))
void printInstruction(Instruction instr)
{
    uint32_t op = OP(instr);
    uint32_t a  = A(instr);
    uint32_t b  = B(instr);
    uint32_t c  = C(instr);
    uint32_t ax = Ax(instr);
    uint32_t bx = Bx(instr);
    int32_t sbx = sBx(instr);

    printf("%-10s", lua_opnames[op]);
    switch (getOpMode(op)){
        case iABC:
            printf("%d",a);
            if (getBMode(op)!=OpArgN) printf(" %d",ISK(b) ? (MYK(INDEXK(b))) : b);
            if (getCMode(op)!=OpArgN) printf(" %d",ISK(c) ? (MYK(INDEXK(c))) : c);
            break;
        case iABx:
            printf("%d",a);
            if (getBMode(op)==OpArgK) printf(" %d",MYK(bx));
            if (getBMode(op)==OpArgU) printf(" %d",bx);
            break;
        case iAsBx:
            printf("%d %d",a,sbx);
            break;
        case iAx:
            printf("%d",MYK(ax));
            break;
    }
    printf("\n");
}

void printValue(Value *v)
{
  switch (v->typ) {
    case LUA_TNIL:
      printf("nil\n");
      break;

    case LUA_TBOOLEAN:
      printf("%s\n", ( get_bool(v) ? "true" : "false")); 
      break;

    case LUA_TNUMFLT:
      printf("%lf\n", get_float(v));
      break;

    case LUA_TNUMINT:
      printf("%ld\n", get_int(v));
      break;

    default:
      assert(0);
      break;
  }
}

void printChunkBytecode(Proto *f)
{
    printf("numparams = %d\n", f->numparams);
    printf("is_vararg = %d\n", f->is_vararg);
    printf("maxstacksize = %d\n", f->maxstacksize);
    printf("sizeupvalues = %d\n", f->sizeupvalues);
    printf("sizep = %d\n", f->sizep);
    printf("linedefined = %d\n", f->linedefined);
    printf("lastlinedefined = %d\n", f->lastlinedefined);
    printf("source = %s\n", f->source->str);

    printf("\n");
    printf("sizek = %d\n", f->sizek);
    for(int i=0; i < f->sizek; i++) {
        printf("  k[%d] = ", i);
        printValue(&f->k[i]);
    }

    printf("\n");
    printf("sizecode = %d\n", f->sizecode);
    for(int i=0; i < f->sizecode; i++) {
        printf("  code[%d] = ", i+1); // 1-based to match "luac -l"
        printInstruction(f->code[i]);
    }
}

//
// Runtime
//

static inline
int isNumerical(Value *v) {
  return (v->typ == LUA_TNUMINT) || (v->typ == LUA_TNUMFLT);
}

static
lua_float castToFloat(Value *v)
{
    if (v->typ == LUA_TNUMFLT) {
        return v->u.n;
    } else if (v->typ == LUA_TNUMINT) {
        return (lua_float) v->u.i;
    } else {
        assert(0);
    }
}

static
int isTruthy(Value *v) {
    switch (v->typ) {
        case LUA_TNIL: return 0;
        case LUA_TBOOLEAN: return v->u.b;
        default: return 1;
    }
}



void vm_ADD(Value *out, Value *v1, Value *v2)
{
    if (isNumerical(v1) && isNumerical(v2)) {
        if (v1->typ == LUA_TNUMINT && v2->typ == LUA_TNUMINT) {
            lua_integer i1 = v1->u.i;
            lua_integer i2 = v2->u.i;
            set_int(out, i1 + i2);
        } else {
            lua_float n1 = castToFloat(v1);
            lua_float n2 = castToFloat(v2);
            set_float(out, n1 + n2);
        }
    } else {
        error("type error - vm_ADD");
        assert(0);
    } 
}

static
void vm_SUB(Value *out, Value *v1, Value *v2)
{
    if (isNumerical(v1) && isNumerical(v2)) {
        if (v1->typ == LUA_TNUMINT && v2->typ == LUA_TNUMINT) {
            lua_integer i1 = v1->u.i;
            lua_integer i2 = v2->u.i;
            set_int(out, i1 - i2);
        } else {
            lua_float n1 = castToFloat(v1);
            lua_float n2 = castToFloat(v2);
            set_float(out, n1 - n2);
        }
    } else {
        error("type error - vm_SUB");
        assert(0);
    } 
}

static
void vm_MUL(Value *out, Value *v1, Value *v2)
{
    if (isNumerical(v1) && isNumerical(v2)) {
        if (v1->typ == LUA_TNUMINT && v2->typ == LUA_TNUMINT) {
            lua_integer i1 = v1->u.i;
            lua_integer i2 = v2->u.i;
            set_int(out, i1 * i2);
        } else {
            lua_float n1 = castToFloat(v1);
            lua_float n2 = castToFloat(v2);
            set_float(out, n1 * n2);
        }
    } else {
        error("type error");
        error("vm_MUL");
        assert(0);
    } 
}

static
void vm_DIV(Value *out, Value *v1, Value *v2)
{
    // Float division (always with floats)

    if (isNumerical(v1) && isNumerical(v2)) {
        lua_float n1 = castToFloat(v1);
        lua_float n2 = castToFloat(v2);
        set_float(out, n1 / n2);
    } else {
        error("type error - vm_DIV");
        assert(0);
    }
}

static
void vm_MOD(Value *out, Value *v1, Value *v2)
{
    if (isNumerical(v1) && isNumerical(v2)) {
        if (v1->typ == LUA_TNUMINT && v2->typ == LUA_TNUMINT) {
            lua_integer i1 = v1->u.i;
            lua_integer i2 = v2->u.i;
            set_int(out, i1 % i2);
        } else {
            lua_float n1 = castToFloat(v1);
            lua_float n2 = castToFloat(v2);
            set_float(out, fmod(n1, n2));
        }
    } else {
        error("type error - vm_MOD");
        assert(0);
    } 

}


static
void vm_IDIV(Value *out, Value *v1, Value *v2)
{
    if (isNumerical(v1) && isNumerical(v2)) {
        if (v1->typ == LUA_TNUMINT && v2->typ == LUA_TNUMINT) {
            lua_integer i1 = v1->u.i;
            lua_integer i2 = v2->u.i;
            set_int(out, i1 / i2);
        } else {
            lua_float n1 = castToFloat(v1);
            lua_float n2 = castToFloat(v2);
            set_float(out, floor(n1/n2));
        }
    } else {
        error("type error - vm_IDIV");
        assert(0);
    } 
}

static
void vm_POW(Value *out, Value *v1, Value *v2)
{
    // Float division (always with floats)

    if (isNumerical(v1) && isNumerical(v2)) {
        lua_float n1 = castToFloat(v1);
        lua_float n2 = castToFloat(v2);
        set_float(out, pow(n1, n2));
    } else {
        error("type error - vm_POW");
        assert(0);
    }
}

static
void vm_UNM(Value *out, Value *v1)
{
    if (v1->typ == LUA_TNUMINT) {
        set_int(out, - v1->u.i);
    } else if (v1->typ == LUA_TNUMFLT) {
        set_float(out, - v1->u.n);
    } else {
        error("type error - vm_UNM");
        assert(0);
    }
}

static
void vm_NOT(Value *out, Value *v1)
{
    set_bool(out, !isTruthy(v1));
}

static
uint32_t vm_EQ(Value *v1, Value *v2)
{
    switch (v1->typ) {
        case LUA_TNIL: {
            if (v2->typ == LUA_TNIL) return 1;
            return 0;
        } break;

        case LUA_TBOOLEAN: {
            if (v2->typ != LUA_TBOOLEAN) return (v1->u.b == v2->u.b);
            return 0;
        } break;

        case LUA_TNUMFLT: {
            if (v2->typ == LUA_TNUMFLT) return (v1->u.n == v2->u.n);
            if (v2->typ == LUA_TNUMINT) return (v1->u.n == (lua_float) v2->u.i);
            return 0;
        } break;

        case LUA_TNUMINT: {
            if (v2->typ == LUA_TNUMINT) return (v1->u.i == v2->u.i);
            if (v2->typ == LUA_TNUMFLT) return ((lua_float) v1->u.i == v2->u.n);
            return 0;
        } break;

        default: {
            error("not implemented: vm_EQ");
            assert(0);
        } break;
    }
}

static
uint32_t vm_LT(Value *v1, Value *v2)
{
  if (isNumerical(v1) && isNumerical(v2)) {
    if (v1->typ == LUA_TNUMINT && v2->typ == LUA_TNUMINT) {
      lua_integer i1 = v1->u.i;
      lua_integer i2 = v2->u.i;
      return (i1 < i2);
    } else {
      lua_float n1 = castToFloat(v1);
      lua_float n2 = castToFloat(v2);
      return (n1 < n2);
    }
  } else {
    error("type error - vm_LT");
    assert(0);
  } 
}

uint32_t vm_LE(Value *v1, Value *v2)
{
  if (isNumerical(v1) && isNumerical(v2)) {
    if (v1->typ == LUA_TNUMINT && v2->typ == LUA_TNUMINT) {
      lua_integer i1 = v1->u.i;
      lua_integer i2 = v2->u.i;
      return (i1 <= i2);
    } else {
      lua_float n1 = castToFloat(v1);
      lua_float n2 = castToFloat(v2);
      return (n1 <= n2);
    }
  } else {
    error("type error - vm_LE");
    assert(0);
  } 
}

//
// Interpreter
//

//MiniLuaState Struct
typedef struct MiniLuaState {
    Proto *proto;
    Value *registers;
    size_t return_begin;
    size_t return_end;
} MiniLuaState;

#define R(n) &mls->registers[n]
#define K(n) &constants[n]
#define RK(n) (ISK(n) ? K(INDEXK(n)) : R(n))

//
size_t step_in_C(MiniLuaState *mls, Instruction inst, uint32_t op, Value *constants) {
    size_t pc_offset = 0;
    switch (op) {
        case OP_MOVE: {
            // R(A) := R(B)
            Value *a = R(A(inst));
            Value *b = R(B(inst));
            *a = *b;
        } break;

        case OP_LOADK: {
            Value *a = R(A(inst));
            Value *b = K(Bx(inst));
            *a = *b;
        } break;

        case OP_ADD: {
            Value *a = R(A(inst));
            Value *b = RK(B(inst));
            Value *c = RK(C(inst));
            vm_ADD(a,b,c);
        } break; 

        case OP_SUB: {
            Value *a = R(A(inst));
            Value *b = RK(B(inst));
            Value *c = RK(C(inst));
            vm_SUB(a,b,c);
        } break; 

        case OP_MUL: {
            Value *a = R(A(inst));
            Value *b = RK(B(inst));
            Value *c = RK(C(inst));
            vm_MUL(a,b,c);
        } break; 

        case OP_DIV: {
            Value *a = R(A(inst));
            Value *b = RK(B(inst));
            Value *c = RK(C(inst));
            vm_DIV(a,b,c);
        } break; 

        case OP_MOD: {
            Value *a = R(A(inst));
            Value *b = RK(B(inst));
            Value *c = RK(C(inst));
            vm_MOD(a,b,c);
        } break; 

        case OP_IDIV: {
            Value *a = R(A(inst));
            Value *b = RK(B(inst));
            Value *c = RK(C(inst));
            vm_IDIV(a,b,c);
        } break; 

        case OP_POW: {
            Value *a = R(A(inst));
            Value *b = RK(B(inst));
            Value *c = RK(C(inst));
            vm_POW(a,b,c);
        } break; 

        case OP_UNM: {
            Value *a = R(A(inst));
            Value *b = R(B(inst));
            vm_UNM(a, b);
        } break;

        case OP_NOT: {
            Value *a = R(A(inst));
            Value *b = R(B(inst));
            vm_NOT(a, b);
        } break;

        case OP_JMP: {
            // uint32_t a = A(instr);
            int32_t sbx = sBx(inst);
            pc_offset = sbx;
            // if (a) close upvalues;
        } break;

        case OP_EQ: {
            uint32_t a = A(inst);
            Value *b = RK(B(inst));
            Value *c = RK(C(inst));
            if ( vm_EQ(b, c) != a ) { 
                pc_offset++;
            }
        } break;

        case OP_LT: {
            uint32_t a = A(inst);
            Value *b = RK(B(inst));
            Value *c = RK(C(inst));
            if ( vm_LT(b, c) != a ) { 
                pc_offset++;
            }
        } break;

        case OP_LE: {
            uint32_t a = A(inst);
            Value *b = RK(B(inst));
            Value *c = RK(C(inst));
            if ( vm_LE(b, c) != a ) { 
                pc_offset++;
            }
        } break;

        case OP_FORLOOP: {
            uint32_t ia = A(inst);
            int32_t sbx = sBx(inst);
            Value *init  = R(ia + 0);
            Value *limit = R(ia + 1);
            Value *step  = R(ia + 2);
            Value *var   = R(ia + 3);

            vm_ADD(init, init, step);
            if (vm_LE(init, limit)) {
                pc_offset = sbx;
                *var = *init;
            } 
        } break;

        case OP_FORPREP: {
            uint32_t ia = A(inst);
            int32_t sbx = sBx(inst);
            Value *init = R(ia + 0);
            Value *step = R(ia + 2);
            
            vm_SUB(init, init, step);
            pc_offset = sbx;
        } break;

        default:
            fprintf(stderr, "Opcode %s is not implemented yet\n", lua_opnames[op]);
            exit(1);
            break;
    }
    
    return pc_offset;
}

void interpret(MiniLuaState *mls);
/* void interpret(MiniLuaState *mls) */
/* { */
/*     Value *constants = mls->proto->k; */

/*     size_t pc = 0; */
/*     while (1) { */

/*         Instruction inst = mls->proto->code[pc++]; */
/*         uint32_t op = OP(inst); */

/*         if (op == OP_RETURN) { */
/*             uint32_t a = A(inst); */
/*             uint32_t b = B(inst); */
/*             if (b == 0) { */
/*                 error("not implemented: OP_RETURN with b == 0"); */
/*             } */
/*             mls->return_begin = a; */
/*             mls->return_end   = a + b - 1; */
/*             return; */
/*         } */

/*         pc += step(mls, inst, op, constants); */
/*     } */

/*     return; */
/* } */
//


int main(int argc, char **argv)
{
    assert(sizeof(Int) == SIZE_INT);
    assert(sizeof(size_t) == SIZE_SIZE_T);
    assert(sizeof(Instruction) == SIZE_INSTRUCTION);
    assert(sizeof(lua_integer) == SIZE_LUA_INT);
    assert(sizeof(lua_float) == SIZE_LUA_NUMBER);

    if (argc <= 1) {
      fprintf(stderr, "usage: %s FILE [NITER]\n", argv[0]);
      exit(1);
    }

    FILE *F = fopen(argv[1], "r");
    if (!F) {
      fprintf(stderr, "could not open file %s\n", argv[1]);
      exit(1);
    }

    int niter;
    if (argc <= 2) {
        niter = 1;
    } else {
        int nr = sscanf(argv[2], "%d", &niter);
        if (nr != 1) {
            fprintf(stderr, "%s is not a number\n", argv[2]);
            exit(1);
        }
    }
    
    MiniLuaState *mls = calloc(1, sizeof(MiniLuaState));
    mls->proto = loadChunkBytecode(F);
    mls->registers = calloc(mls->proto->maxstacksize, sizeof(Value));
    for (int i = 0; i < mls->proto->maxstacksize; i++) {
        set_nil(&mls->registers[i]);
    }
    mls->return_begin = 0;
    mls->return_end = 0;
    
    for (int i=0; i < niter; i++) {
        interpret(mls);
    }

    for (size_t i = mls->return_begin; i < mls->return_end; i++) {
        printValue(&mls->registers[i]);
    }

    return 0;
}

