/* A Bison parser, made by GNU Bison 3.8.2.  */

/* Bison implementation for Yacc-like parsers in C

   Copyright (C) 1984, 1989-1990, 2000-2015, 2018-2021 Free Software Foundation,
   Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.  */

/* As a special exception, you may create a larger work that contains
   part or all of the Bison parser skeleton and distribute that work
   under terms of your choice, so long as that work isn't itself a
   parser generator using the skeleton or a modified version thereof
   as a parser skeleton.  Alternatively, if you modify or redistribute
   the parser skeleton itself, you may (at your option) remove this
   special exception, which will cause the skeleton and the resulting
   Bison output files to be licensed under the GNU General Public
   License without this special exception.

   This special exception was added by the Free Software Foundation in
   version 2.2 of Bison.  */

/* C LALR(1) parser skeleton written by Richard Stallman, by
   simplifying the original so-called "semantic" parser.  */

/* DO NOT RELY ON FEATURES THAT ARE NOT DOCUMENTED in the manual,
   especially those whose name start with YY_ or yy_.  They are
   private implementation details that can be changed or removed.  */

/* All symbols defined below should begin with yy or YY, to avoid
   infringing on user name space.  This should be done even for local
   variables, as they might otherwise be expanded by user macros.
   There are some unavoidable exceptions within include files to
   define necessary library symbols; they are noted "INFRINGES ON
   USER NAME SPACE" below.  */

/* Identify Bison output, and Bison version.  */
#define YYBISON 30802

/* Bison version string.  */
#define YYBISON_VERSION "3.8.2"

/* Skeleton name.  */
#define YYSKELETON_NAME "yacc.c"

/* Pure parsers.  */
#define YYPURE 0

/* Push parsers.  */
#define YYPUSH 0

/* Pull parsers.  */
#define YYPULL 1




/* First part of user prologue.  */
#line 1 "Parser.y"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "compiler.h"

int yylex(void);
void yyerror(const char *s);
extern int yylineno;

#line 83 "parser.tab.c"

# ifndef YY_CAST
#  ifdef __cplusplus
#   define YY_CAST(Type, Val) static_cast<Type> (Val)
#   define YY_REINTERPRET_CAST(Type, Val) reinterpret_cast<Type> (Val)
#  else
#   define YY_CAST(Type, Val) ((Type) (Val))
#   define YY_REINTERPRET_CAST(Type, Val) ((Type) (Val))
#  endif
# endif
# ifndef YY_NULLPTR
#  if defined __cplusplus
#   if 201103L <= __cplusplus
#    define YY_NULLPTR nullptr
#   else
#    define YY_NULLPTR 0
#   endif
#  else
#   define YY_NULLPTR ((void*)0)
#  endif
# endif

#include "parser.tab.h"
/* Symbol kind.  */
enum yysymbol_kind_t
{
  YYSYMBOL_YYEMPTY = -2,
  YYSYMBOL_YYEOF = 0,                      /* "end of file"  */
  YYSYMBOL_YYerror = 1,                    /* error  */
  YYSYMBOL_YYUNDEF = 2,                    /* "invalid token"  */
  YYSYMBOL_CONSTTK = 3,                    /* CONSTTK  */
  YYSYMBOL_INTTK = 4,                      /* INTTK  */
  YYSYMBOL_VOIDTK = 5,                     /* VOIDTK  */
  YYSYMBOL_IFTK = 6,                       /* IFTK  */
  YYSYMBOL_ELSETK = 7,                     /* ELSETK  */
  YYSYMBOL_WHILETK = 8,                    /* WHILETK  */
  YYSYMBOL_BREAKTK = 9,                    /* BREAKTK  */
  YYSYMBOL_CONTINUETK = 10,                /* CONTINUETK  */
  YYSYMBOL_RETURNTK = 11,                  /* RETURNTK  */
  YYSYMBOL_GETINTTK = 12,                  /* GETINTTK  */
  YYSYMBOL_PRINTFTK = 13,                  /* PRINTFTK  */
  YYSYMBOL_LEQ = 14,                       /* LEQ  */
  YYSYMBOL_GEQ = 15,                       /* GEQ  */
  YYSYMBOL_EQL = 16,                       /* EQL  */
  YYSYMBOL_NEQ = 17,                       /* NEQ  */
  YYSYMBOL_LSS = 18,                       /* LSS  */
  YYSYMBOL_GRE = 19,                       /* GRE  */
  YYSYMBOL_AND = 20,                       /* AND  */
  YYSYMBOL_OR = 21,                        /* OR  */
  YYSYMBOL_NOT = 22,                       /* NOT  */
  YYSYMBOL_PLUS = 23,                      /* PLUS  */
  YYSYMBOL_MINU = 24,                      /* MINU  */
  YYSYMBOL_MULT = 25,                      /* MULT  */
  YYSYMBOL_DIV = 26,                       /* DIV  */
  YYSYMBOL_MOD = 27,                       /* MOD  */
  YYSYMBOL_ASSIGN = 28,                    /* ASSIGN  */
  YYSYMBOL_LPARENT = 29,                   /* LPARENT  */
  YYSYMBOL_RPARENT = 30,                   /* RPARENT  */
  YYSYMBOL_LBRACK = 31,                    /* LBRACK  */
  YYSYMBOL_RBRACK = 32,                    /* RBRACK  */
  YYSYMBOL_LBRACE = 33,                    /* LBRACE  */
  YYSYMBOL_RBRACE = 34,                    /* RBRACE  */
  YYSYMBOL_COMMA = 35,                     /* COMMA  */
  YYSYMBOL_SEMICN = 36,                    /* SEMICN  */
  YYSYMBOL_IDENFR = 37,                    /* IDENFR  */
  YYSYMBOL_INTCON = 38,                    /* INTCON  */
  YYSYMBOL_STRCON = 39,                    /* STRCON  */
  YYSYMBOL_LOWER_THAN_ELSE = 40,           /* LOWER_THAN_ELSE  */
  YYSYMBOL_YYACCEPT = 41,                  /* $accept  */
  YYSYMBOL_CompUnit = 42,                  /* CompUnit  */
  YYSYMBOL_CompUnitItem = 43,              /* CompUnitItem  */
  YYSYMBOL_GlobalIntItem = 44,             /* GlobalIntItem  */
  YYSYMBOL_GlobalVarInitOpt = 45,          /* GlobalVarInitOpt  */
  YYSYMBOL_GlobalVarMore = 46,             /* GlobalVarMore  */
  YYSYMBOL_Decl = 47,                      /* Decl  */
  YYSYMBOL_ConstDecl = 48,                 /* ConstDecl  */
  YYSYMBOL_ConstDefList = 49,              /* ConstDefList  */
  YYSYMBOL_BType = 50,                     /* BType  */
  YYSYMBOL_ConstDef = 51,                  /* ConstDef  */
  YYSYMBOL_ConstDefDims = 52,              /* ConstDefDims  */
  YYSYMBOL_ConstInitVal = 53,              /* ConstInitVal  */
  YYSYMBOL_ConstInitValList = 54,          /* ConstInitValList  */
  YYSYMBOL_VoidFuncDef = 55,               /* VoidFuncDef  */
  YYSYMBOL_VarDecl = 56,                   /* VarDecl  */
  YYSYMBOL_VarDefList = 57,                /* VarDefList  */
  YYSYMBOL_VarDef = 58,                    /* VarDef  */
  YYSYMBOL_VarDefDims = 59,                /* VarDefDims  */
  YYSYMBOL_InitVal = 60,                   /* InitVal  */
  YYSYMBOL_InitValList = 61,               /* InitValList  */
  YYSYMBOL_FuncFParams = 62,               /* FuncFParams  */
  YYSYMBOL_FuncFParamsList = 63,           /* FuncFParamsList  */
  YYSYMBOL_FuncFParam = 64,                /* FuncFParam  */
  YYSYMBOL_FuncFParamSuffix = 65,          /* FuncFParamSuffix  */
  YYSYMBOL_FuncFParamArrayDims = 66,       /* FuncFParamArrayDims  */
  YYSYMBOL_Block = 67,                     /* Block  */
  YYSYMBOL_BlockItemList = 68,             /* BlockItemList  */
  YYSYMBOL_Stmt = 69,                      /* Stmt  */
  YYSYMBOL_ExpOpt = 70,                    /* ExpOpt  */
  YYSYMBOL_PrintfArgs = 71,                /* PrintfArgs  */
  YYSYMBOL_Exp = 72,                       /* Exp  */
  YYSYMBOL_Cond = 73,                      /* Cond  */
  YYSYMBOL_LVal = 74,                      /* LVal  */
  YYSYMBOL_LValIndices = 75,               /* LValIndices  */
  YYSYMBOL_PrimaryExp = 76,                /* PrimaryExp  */
  YYSYMBOL_Number = 77,                    /* Number  */
  YYSYMBOL_UnaryExp = 78,                  /* UnaryExp  */
  YYSYMBOL_FuncRParams = 79,               /* FuncRParams  */
  YYSYMBOL_FuncRParamsList = 80,           /* FuncRParamsList  */
  YYSYMBOL_MulExp = 81,                    /* MulExp  */
  YYSYMBOL_AddExp = 82,                    /* AddExp  */
  YYSYMBOL_RelExp = 83,                    /* RelExp  */
  YYSYMBOL_EqExp = 84,                     /* EqExp  */
  YYSYMBOL_LAndExp = 85,                   /* LAndExp  */
  YYSYMBOL_LOrExp = 86,                    /* LOrExp  */
  YYSYMBOL_ConstExp = 87                   /* ConstExp  */
};
typedef enum yysymbol_kind_t yysymbol_kind_t;




#ifdef short
# undef short
#endif

/* On compilers that do not define __PTRDIFF_MAX__ etc., make sure
   <limits.h> and (if available) <stdint.h> are included
   so that the code can choose integer types of a good width.  */

#ifndef __PTRDIFF_MAX__
# include <limits.h> /* INFRINGES ON USER NAME SPACE */
# if defined __STDC_VERSION__ && 199901 <= __STDC_VERSION__
#  include <stdint.h> /* INFRINGES ON USER NAME SPACE */
#  define YY_STDINT_H
# endif
#endif

/* Narrow types that promote to a signed type and that can represent a
   signed or unsigned integer of at least N bits.  In tables they can
   save space and decrease cache pressure.  Promoting to a signed type
   helps avoid bugs in integer arithmetic.  */

#ifdef __INT_LEAST8_MAX__
typedef __INT_LEAST8_TYPE__ yytype_int8;
#elif defined YY_STDINT_H
typedef int_least8_t yytype_int8;
#else
typedef signed char yytype_int8;
#endif

#ifdef __INT_LEAST16_MAX__
typedef __INT_LEAST16_TYPE__ yytype_int16;
#elif defined YY_STDINT_H
typedef int_least16_t yytype_int16;
#else
typedef short yytype_int16;
#endif

/* Work around bug in HP-UX 11.23, which defines these macros
   incorrectly for preprocessor constants.  This workaround can likely
   be removed in 2023, as HPE has promised support for HP-UX 11.23
   (aka HP-UX 11i v2) only through the end of 2022; see Table 2 of
   <https://h20195.www2.hpe.com/V2/getpdf.aspx/4AA4-7673ENW.pdf>.  */
#ifdef __hpux
# undef UINT_LEAST8_MAX
# undef UINT_LEAST16_MAX
# define UINT_LEAST8_MAX 255
# define UINT_LEAST16_MAX 65535
#endif

#if defined __UINT_LEAST8_MAX__ && __UINT_LEAST8_MAX__ <= __INT_MAX__
typedef __UINT_LEAST8_TYPE__ yytype_uint8;
#elif (!defined __UINT_LEAST8_MAX__ && defined YY_STDINT_H \
       && UINT_LEAST8_MAX <= INT_MAX)
typedef uint_least8_t yytype_uint8;
#elif !defined __UINT_LEAST8_MAX__ && UCHAR_MAX <= INT_MAX
typedef unsigned char yytype_uint8;
#else
typedef short yytype_uint8;
#endif

#if defined __UINT_LEAST16_MAX__ && __UINT_LEAST16_MAX__ <= __INT_MAX__
typedef __UINT_LEAST16_TYPE__ yytype_uint16;
#elif (!defined __UINT_LEAST16_MAX__ && defined YY_STDINT_H \
       && UINT_LEAST16_MAX <= INT_MAX)
typedef uint_least16_t yytype_uint16;
#elif !defined __UINT_LEAST16_MAX__ && USHRT_MAX <= INT_MAX
typedef unsigned short yytype_uint16;
#else
typedef int yytype_uint16;
#endif

#ifndef YYPTRDIFF_T
# if defined __PTRDIFF_TYPE__ && defined __PTRDIFF_MAX__
#  define YYPTRDIFF_T __PTRDIFF_TYPE__
#  define YYPTRDIFF_MAXIMUM __PTRDIFF_MAX__
# elif defined PTRDIFF_MAX
#  ifndef ptrdiff_t
#   include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  endif
#  define YYPTRDIFF_T ptrdiff_t
#  define YYPTRDIFF_MAXIMUM PTRDIFF_MAX
# else
#  define YYPTRDIFF_T long
#  define YYPTRDIFF_MAXIMUM LONG_MAX
# endif
#endif

#ifndef YYSIZE_T
# ifdef __SIZE_TYPE__
#  define YYSIZE_T __SIZE_TYPE__
# elif defined size_t
#  define YYSIZE_T size_t
# elif defined __STDC_VERSION__ && 199901 <= __STDC_VERSION__
#  include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  define YYSIZE_T size_t
# else
#  define YYSIZE_T unsigned
# endif
#endif

#define YYSIZE_MAXIMUM                                  \
  YY_CAST (YYPTRDIFF_T,                                 \
           (YYPTRDIFF_MAXIMUM < YY_CAST (YYSIZE_T, -1)  \
            ? YYPTRDIFF_MAXIMUM                         \
            : YY_CAST (YYSIZE_T, -1)))

#define YYSIZEOF(X) YY_CAST (YYPTRDIFF_T, sizeof (X))


/* Stored state numbers (used for stacks). */
typedef yytype_uint8 yy_state_t;

/* State numbers in computations.  */
typedef int yy_state_fast_t;

#ifndef YY_
# if defined YYENABLE_NLS && YYENABLE_NLS
#  if ENABLE_NLS
#   include <libintl.h> /* INFRINGES ON USER NAME SPACE */
#   define YY_(Msgid) dgettext ("bison-runtime", Msgid)
#  endif
# endif
# ifndef YY_
#  define YY_(Msgid) Msgid
# endif
#endif


#ifndef YY_ATTRIBUTE_PURE
# if defined __GNUC__ && 2 < __GNUC__ + (96 <= __GNUC_MINOR__)
#  define YY_ATTRIBUTE_PURE __attribute__ ((__pure__))
# else
#  define YY_ATTRIBUTE_PURE
# endif
#endif

#ifndef YY_ATTRIBUTE_UNUSED
# if defined __GNUC__ && 2 < __GNUC__ + (7 <= __GNUC_MINOR__)
#  define YY_ATTRIBUTE_UNUSED __attribute__ ((__unused__))
# else
#  define YY_ATTRIBUTE_UNUSED
# endif
#endif

/* Suppress unused-variable warnings by "using" E.  */
#if ! defined lint || defined __GNUC__
# define YY_USE(E) ((void) (E))
#else
# define YY_USE(E) /* empty */
#endif

/* Suppress an incorrect diagnostic about yylval being uninitialized.  */
#if defined __GNUC__ && ! defined __ICC && 406 <= __GNUC__ * 100 + __GNUC_MINOR__
# if __GNUC__ * 100 + __GNUC_MINOR__ < 407
#  define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN                           \
    _Pragma ("GCC diagnostic push")                                     \
    _Pragma ("GCC diagnostic ignored \"-Wuninitialized\"")
# else
#  define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN                           \
    _Pragma ("GCC diagnostic push")                                     \
    _Pragma ("GCC diagnostic ignored \"-Wuninitialized\"")              \
    _Pragma ("GCC diagnostic ignored \"-Wmaybe-uninitialized\"")
# endif
# define YY_IGNORE_MAYBE_UNINITIALIZED_END      \
    _Pragma ("GCC diagnostic pop")
#else
# define YY_INITIAL_VALUE(Value) Value
#endif
#ifndef YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_END
#endif
#ifndef YY_INITIAL_VALUE
# define YY_INITIAL_VALUE(Value) /* Nothing. */
#endif

#if defined __cplusplus && defined __GNUC__ && ! defined __ICC && 6 <= __GNUC__
# define YY_IGNORE_USELESS_CAST_BEGIN                          \
    _Pragma ("GCC diagnostic push")                            \
    _Pragma ("GCC diagnostic ignored \"-Wuseless-cast\"")
# define YY_IGNORE_USELESS_CAST_END            \
    _Pragma ("GCC diagnostic pop")
#endif
#ifndef YY_IGNORE_USELESS_CAST_BEGIN
# define YY_IGNORE_USELESS_CAST_BEGIN
# define YY_IGNORE_USELESS_CAST_END
#endif


#define YY_ASSERT(E) ((void) (0 && (E)))

#if !defined yyoverflow

/* The parser invokes alloca or malloc; define the necessary symbols.  */

# ifdef YYSTACK_USE_ALLOCA
#  if YYSTACK_USE_ALLOCA
#   ifdef __GNUC__
#    define YYSTACK_ALLOC __builtin_alloca
#   elif defined __BUILTIN_VA_ARG_INCR
#    include <alloca.h> /* INFRINGES ON USER NAME SPACE */
#   elif defined _AIX
#    define YYSTACK_ALLOC __alloca
#   elif defined _MSC_VER
#    include <malloc.h> /* INFRINGES ON USER NAME SPACE */
#    define alloca _alloca
#   else
#    define YYSTACK_ALLOC alloca
#    if ! defined _ALLOCA_H && ! defined EXIT_SUCCESS
#     include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
      /* Use EXIT_SUCCESS as a witness for stdlib.h.  */
#     ifndef EXIT_SUCCESS
#      define EXIT_SUCCESS 0
#     endif
#    endif
#   endif
#  endif
# endif

# ifdef YYSTACK_ALLOC
   /* Pacify GCC's 'empty if-body' warning.  */
#  define YYSTACK_FREE(Ptr) do { /* empty */; } while (0)
#  ifndef YYSTACK_ALLOC_MAXIMUM
    /* The OS might guarantee only one guard page at the bottom of the stack,
       and a page size can be as small as 4096 bytes.  So we cannot safely
       invoke alloca (N) if N exceeds 4096.  Use a slightly smaller number
       to allow for a few compiler-allocated temporary stack slots.  */
#   define YYSTACK_ALLOC_MAXIMUM 4032 /* reasonable circa 2006 */
#  endif
# else
#  define YYSTACK_ALLOC YYMALLOC
#  define YYSTACK_FREE YYFREE
#  ifndef YYSTACK_ALLOC_MAXIMUM
#   define YYSTACK_ALLOC_MAXIMUM YYSIZE_MAXIMUM
#  endif
#  if (defined __cplusplus && ! defined EXIT_SUCCESS \
       && ! ((defined YYMALLOC || defined malloc) \
             && (defined YYFREE || defined free)))
#   include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#   ifndef EXIT_SUCCESS
#    define EXIT_SUCCESS 0
#   endif
#  endif
#  ifndef YYMALLOC
#   define YYMALLOC malloc
#   if ! defined malloc && ! defined EXIT_SUCCESS
void *malloc (YYSIZE_T); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
#  ifndef YYFREE
#   define YYFREE free
#   if ! defined free && ! defined EXIT_SUCCESS
void free (void *); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
# endif
#endif /* !defined yyoverflow */

#if (! defined yyoverflow \
     && (! defined __cplusplus \
         || (defined YYSTYPE_IS_TRIVIAL && YYSTYPE_IS_TRIVIAL)))

/* A type that is properly aligned for any stack member.  */
union yyalloc
{
  yy_state_t yyss_alloc;
  YYSTYPE yyvs_alloc;
};

/* The size of the maximum gap between one aligned stack and the next.  */
# define YYSTACK_GAP_MAXIMUM (YYSIZEOF (union yyalloc) - 1)

/* The size of an array large to enough to hold all stacks, each with
   N elements.  */
# define YYSTACK_BYTES(N) \
     ((N) * (YYSIZEOF (yy_state_t) + YYSIZEOF (YYSTYPE)) \
      + YYSTACK_GAP_MAXIMUM)

# define YYCOPY_NEEDED 1

/* Relocate STACK from its old location to the new one.  The
   local variables YYSIZE and YYSTACKSIZE give the old and new number of
   elements in the stack, and YYPTR gives the new location of the
   stack.  Advance YYPTR to a properly aligned location for the next
   stack.  */
# define YYSTACK_RELOCATE(Stack_alloc, Stack)                           \
    do                                                                  \
      {                                                                 \
        YYPTRDIFF_T yynewbytes;                                         \
        YYCOPY (&yyptr->Stack_alloc, Stack, yysize);                    \
        Stack = &yyptr->Stack_alloc;                                    \
        yynewbytes = yystacksize * YYSIZEOF (*Stack) + YYSTACK_GAP_MAXIMUM; \
        yyptr += yynewbytes / YYSIZEOF (*yyptr);                        \
      }                                                                 \
    while (0)

#endif

#if defined YYCOPY_NEEDED && YYCOPY_NEEDED
/* Copy COUNT objects from SRC to DST.  The source and destination do
   not overlap.  */
# ifndef YYCOPY
#  if defined __GNUC__ && 1 < __GNUC__
#   define YYCOPY(Dst, Src, Count) \
      __builtin_memcpy (Dst, Src, YY_CAST (YYSIZE_T, (Count)) * sizeof (*(Src)))
#  else
#   define YYCOPY(Dst, Src, Count)              \
      do                                        \
        {                                       \
          YYPTRDIFF_T yyi;                      \
          for (yyi = 0; yyi < (Count); yyi++)   \
            (Dst)[yyi] = (Src)[yyi];            \
        }                                       \
      while (0)
#  endif
# endif
#endif /* !YYCOPY_NEEDED */

/* YYFINAL -- State number of the termination state.  */
#define YYFINAL  16
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   252

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  41
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  47
/* YYNRULES -- Number of rules.  */
#define YYNRULES  108
/* YYNSTATES -- Number of states.  */
#define YYNSTATES  200

/* YYMAXUTOK -- Last valid token kind.  */
#define YYMAXUTOK   295


/* YYTRANSLATE(TOKEN-NUM) -- Symbol number corresponding to TOKEN-NUM
   as returned by yylex, with out-of-bounds checking.  */
#define YYTRANSLATE(YYX)                                \
  (0 <= (YYX) && (YYX) <= YYMAXUTOK                     \
   ? YY_CAST (yysymbol_kind_t, yytranslate[YYX])        \
   : YYSYMBOL_YYUNDEF)

/* YYTRANSLATE[TOKEN-NUM] -- Symbol number corresponding to TOKEN-NUM
   as returned by yylex.  */
static const yytype_int8 yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     1,     2,     3,     4,
       5,     6,     7,     8,     9,    10,    11,    12,    13,    14,
      15,    16,    17,    18,    19,    20,    21,    22,    23,    24,
      25,    26,    27,    28,    29,    30,    31,    32,    33,    34,
      35,    36,    37,    38,    39,    40
};

#if YYDEBUG
/* YYRLINE[YYN] -- Source line where rule number YYN was defined.  */
static const yytype_int16 yyrline[] =
{
       0,    78,    78,    85,    94,    96,    98,   103,   108,   110,
     123,   124,   130,   134,   142,   143,   147,   152,   158,   166,
     170,   181,   182,   190,   192,   197,   202,   208,   216,   221,
     226,   231,   237,   245,   247,   253,   254,   262,   264,   269,
     274,   280,   288,   292,   298,   306,   312,   313,   319,   320,
     328,   334,   335,   340,   348,   350,   352,   354,   356,   358,
     360,   362,   364,   366,   368,   373,   379,   380,   385,   391,
     399,   403,   407,   413,   414,   422,   423,   424,   428,   432,
     433,   438,   440,   442,   444,   446,   451,   455,   461,   469,
     470,   472,   474,   479,   480,   482,   487,   488,   490,   492,
     494,   499,   500,   502,   507,   508,   513,   514,   519
};
#endif

/** Accessing symbol of state STATE.  */
#define YY_ACCESSING_SYMBOL(State) YY_CAST (yysymbol_kind_t, yystos[State])

#if YYDEBUG || 0
/* The user-facing name of the symbol whose (internal) number is
   YYSYMBOL.  No bounds checking.  */
static const char *yysymbol_name (yysymbol_kind_t yysymbol) YY_ATTRIBUTE_UNUSED;

/* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals.  */
static const char *const yytname[] =
{
  "\"end of file\"", "error", "\"invalid token\"", "CONSTTK", "INTTK",
  "VOIDTK", "IFTK", "ELSETK", "WHILETK", "BREAKTK", "CONTINUETK",
  "RETURNTK", "GETINTTK", "PRINTFTK", "LEQ", "GEQ", "EQL", "NEQ", "LSS",
  "GRE", "AND", "OR", "NOT", "PLUS", "MINU", "MULT", "DIV", "MOD",
  "ASSIGN", "LPARENT", "RPARENT", "LBRACK", "RBRACK", "LBRACE", "RBRACE",
  "COMMA", "SEMICN", "IDENFR", "INTCON", "STRCON", "LOWER_THAN_ELSE",
  "$accept", "CompUnit", "CompUnitItem", "GlobalIntItem",
  "GlobalVarInitOpt", "GlobalVarMore", "Decl", "ConstDecl", "ConstDefList",
  "BType", "ConstDef", "ConstDefDims", "ConstInitVal", "ConstInitValList",
  "VoidFuncDef", "VarDecl", "VarDefList", "VarDef", "VarDefDims",
  "InitVal", "InitValList", "FuncFParams", "FuncFParamsList", "FuncFParam",
  "FuncFParamSuffix", "FuncFParamArrayDims", "Block", "BlockItemList",
  "Stmt", "ExpOpt", "PrintfArgs", "Exp", "Cond", "LVal", "LValIndices",
  "PrimaryExp", "Number", "UnaryExp", "FuncRParams", "FuncRParamsList",
  "MulExp", "AddExp", "RelExp", "EqExp", "LAndExp", "LOrExp", "ConstExp", YY_NULLPTR
};

static const char *
yysymbol_name (yysymbol_kind_t yysymbol)
{
  return yytname[yysymbol];
}
#endif

#define YYPACT_NINF (-149)

#define yypact_value_is_default(Yyn) \
  ((Yyn) == YYPACT_NINF)

#define YYTABLE_NINF (-1)

#define yytable_value_is_error(Yyn) \
  0

/* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
   STATE-NUM.  */
static const yytype_int16 yypact[] =
{
      83,    15,    -8,     1,    60,  -149,  -149,  -149,  -149,     8,
    -149,  -149,  -149,    10,    88,    96,  -149,  -149,  -149,    26,
    -149,  -149,    32,  -149,     9,     3,    11,     5,     8,  -149,
      51,    10,  -149,     7,   101,    25,   107,  -149,    20,   214,
    -149,     7,   114,    20,  -149,   173,   214,  -149,  -149,  -149,
     120,     7,    15,   123,   214,   214,   214,   214,    47,   124,
    -149,  -149,  -149,  -149,  -149,  -149,  -149,    65,    71,    71,
     122,    62,  -149,     7,  -149,    90,  -149,  -149,   125,   137,
     126,  -149,  -149,  -149,   132,  -149,  -149,  -149,   133,  -149,
    -149,    69,   192,   134,   214,   214,   214,   214,   214,  -149,
       8,  -149,  -149,  -149,  -149,    73,  -149,   127,   135,   119,
     131,   211,   139,  -149,  -149,  -149,  -149,   136,  -149,   141,
    -149,  -149,  -149,  -149,    20,  -149,  -149,   143,   142,   214,
    -149,  -149,  -149,    65,    65,  -149,  -149,   173,   214,   214,
    -149,  -149,  -149,   148,   147,  -149,   214,   156,  -149,  -149,
     214,   157,  -149,   158,    71,    59,    94,   171,   169,   168,
    -149,   -10,   164,   214,  -149,  -149,   170,   214,   214,   214,
     214,   214,   214,   214,   214,   170,   165,   214,    -9,  -149,
     177,   198,    71,    71,    71,    71,    59,    59,    94,   171,
    -149,  -149,  -149,   176,   214,  -149,   170,  -149,  -149,  -149
};

/* YYDEFACT[STATE-NUM] -- Default reduction number in state STATE-NUM.
   Performed when YYTABLE does not specify something else to do.  Zero
   means the default is an error.  */
static const yytype_int8 yydefact[] =
{
       0,     0,     0,     0,     0,     2,     5,     4,    14,     0,
       6,    15,    19,     0,    35,     0,     1,     3,    35,     0,
      31,    21,     0,    17,     0,    10,     0,    33,     0,    30,
       0,     0,    16,     0,     0,     0,    42,    43,     0,     0,
      12,     0,     0,     0,    32,     0,     0,    18,    51,     7,
      46,     0,     0,     0,     0,     0,     0,     0,     0,    73,
      78,    11,    37,    76,    79,    77,    89,    93,    70,   108,
       0,     0,    28,     0,    34,     0,    20,    23,     0,    66,
       0,    45,     8,    44,     0,    85,    83,    84,     0,    38,
      40,     0,     0,    72,     0,     0,     0,     0,     0,    36,
       0,     9,    29,    24,    26,     0,    22,     0,     0,     0,
       0,     0,     0,    50,    52,    56,    53,     0,    67,    76,
      48,    82,    75,    39,     0,    80,    87,     0,    86,     0,
      90,    91,    92,    94,    95,    13,    25,     0,     0,     0,
      60,    61,    62,     0,     0,    55,     0,    47,    41,    81,
       0,     0,    27,     0,    96,   101,   104,   106,    71,     0,
      63,     0,     0,     0,    88,    74,    66,     0,     0,     0,
       0,     0,     0,     0,     0,    66,     0,     0,     0,    54,
       0,    57,    99,   100,    97,    98,   102,   103,   105,   107,
      59,    64,    68,     0,     0,    49,    66,    65,    69,    58
};

/* YYPGOTO[NTERM-NUM].  */
static const yytype_int16 yypgoto[] =
{
    -149,  -149,   209,  -149,  -149,  -149,   138,  -149,  -149,     4,
     187,  -149,   -71,  -149,  -149,  -149,  -149,   -25,   201,   -41,
    -149,   194,  -149,   172,  -149,  -149,   -27,  -149,  -148,  -149,
    -149,   -57,    86,   -70,  -149,  -149,  -149,   -44,  -149,  -149,
      18,   -38,   -50,    54,    57,  -149,   -23
};

/* YYDEFGOTO[NTERM-NUM].  */
static const yytype_uint8 yydefgoto[] =
{
       0,     4,     5,     6,    40,    71,     7,     8,    22,     9,
      23,    30,    76,   105,    10,    11,    19,    20,    25,    61,
      91,    35,    36,    37,    81,   147,   115,    79,   116,   117,
     178,    62,   153,    63,    93,    64,    65,    66,   127,   128,
      67,    68,   155,   156,   157,   158,    77
};

/* YYTABLE[YYPACT[STATE-NUM]] -- What to do in state STATE-NUM.  If
   positive, shift that token.  If negative, reduce the rule whose
   number is the opposite.  If YYTABLE_NINF, syntax error.  */
static const yytype_uint8 yytable[] =
{
      88,    69,    74,    44,   104,    13,    49,    69,    69,   119,
      85,    86,    87,    12,    72,    12,    70,    90,   181,    12,
     176,   193,   118,    78,    82,   177,   194,   190,    34,    14,
      34,    38,    53,    43,    39,   126,    39,    69,    15,    33,
      48,    41,    54,    55,    56,    18,   102,    21,   199,    57,
     130,   131,   132,    58,   143,    51,    34,    59,    60,    53,
      16,    28,    29,     1,     2,     3,   152,    31,    32,    54,
      55,    56,   151,   167,   168,   135,    57,   169,   170,    45,
      58,    89,    46,   148,    59,    60,     1,     2,     3,   162,
      94,    95,    96,   164,    97,    98,   119,   100,   101,    69,
     154,   154,    53,   123,   124,   119,   180,   136,   137,   118,
     171,   172,    54,    55,    56,   133,   134,    24,   118,    57,
     192,   186,   187,    75,   103,    26,   119,    59,    60,   182,
     183,   184,   185,   154,   154,   154,   154,   198,    50,   118,
       1,    12,    52,   107,    73,   108,   109,   110,   111,    53,
     112,    80,    84,    92,    99,   140,   138,   106,   120,    54,
      55,    56,   121,   122,   139,   129,    57,   141,   144,   146,
      48,   113,   145,   149,    59,    60,   107,   150,   108,   109,
     110,   111,    53,   112,   160,    53,   161,   163,   166,   165,
     174,   173,    54,    55,    56,    54,    55,    56,   175,    57,
     179,   191,    57,    48,    53,   196,    75,    59,    60,   195,
      59,    60,   197,    17,    54,    55,    56,   114,    47,    27,
      42,    57,   125,    53,    83,   159,    53,   188,     0,    59,
      60,   189,     0,    54,    55,    56,    54,    55,    56,     0,
      57,     0,     0,    57,     0,     0,     0,   142,    59,    60,
       0,    59,    60
};

static const yytype_int16 yycheck[] =
{
      57,    39,    43,    28,    75,     1,    33,    45,    46,    79,
      54,    55,    56,     4,    41,     4,    39,    58,   166,     4,
      30,    30,    79,    46,    51,    35,    35,   175,    24,    37,
      26,    28,    12,    28,    31,    92,    31,    75,    37,    30,
      33,    30,    22,    23,    24,    37,    73,    37,   196,    29,
      94,    95,    96,    33,   111,    30,    52,    37,    38,    12,
       0,    35,    36,     3,     4,     5,   137,    35,    36,    22,
      23,    24,   129,    14,    15,   100,    29,    18,    19,    28,
      33,    34,    31,   124,    37,    38,     3,     4,     5,   146,
      25,    26,    27,   150,    23,    24,   166,    35,    36,   137,
     138,   139,    12,    34,    35,   175,   163,    34,    35,   166,
      16,    17,    22,    23,    24,    97,    98,    29,   175,    29,
     177,   171,   172,    33,    34,    29,   196,    37,    38,   167,
     168,   169,   170,   171,   172,   173,   174,   194,    37,   196,
       3,     4,    35,     6,    30,     8,     9,    10,    11,    12,
      13,    31,    29,    29,    32,    36,    29,    32,    32,    22,
      23,    24,    30,    30,    29,    31,    29,    36,    29,    28,
      33,    34,    36,    30,    37,    38,     6,    35,     8,     9,
      10,    11,    12,    13,    36,    12,    39,    31,    30,    32,
      21,    20,    22,    23,    24,    22,    23,    24,    30,    29,
      36,    36,    29,    33,    12,     7,    33,    37,    38,    32,
      37,    38,    36,     4,    22,    23,    24,    79,    31,    18,
      26,    29,    30,    12,    52,   139,    12,   173,    -1,    37,
      38,   174,    -1,    22,    23,    24,    22,    23,    24,    -1,
      29,    -1,    -1,    29,    -1,    -1,    -1,    36,    37,    38,
      -1,    37,    38
};

/* YYSTOS[STATE-NUM] -- The symbol kind of the accessing symbol of
   state STATE-NUM.  */
static const yytype_int8 yystos[] =
{
       0,     3,     4,     5,    42,    43,    44,    47,    48,    50,
      55,    56,     4,    50,    37,    37,     0,    43,    37,    57,
      58,    37,    49,    51,    29,    59,    29,    59,    35,    36,
      52,    35,    36,    30,    50,    62,    63,    64,    28,    31,
      45,    30,    62,    28,    58,    28,    31,    51,    33,    67,
      37,    30,    35,    12,    22,    23,    24,    29,    33,    37,
      38,    60,    72,    74,    76,    77,    78,    81,    82,    82,
      87,    46,    67,    30,    60,    33,    53,    87,    87,    68,
      31,    65,    67,    64,    29,    78,    78,    78,    72,    34,
      60,    61,    29,    75,    25,    26,    27,    23,    24,    32,
      35,    36,    67,    34,    53,    54,    32,     6,     8,     9,
      10,    11,    13,    34,    47,    67,    69,    70,    72,    74,
      32,    30,    30,    34,    35,    30,    72,    79,    80,    31,
      78,    78,    78,    81,    81,    58,    34,    35,    29,    29,
      36,    36,    36,    72,    29,    36,    28,    66,    60,    30,
      35,    72,    53,    73,    82,    83,    84,    85,    86,    73,
      36,    39,    72,    31,    72,    32,    30,    14,    15,    18,
      19,    16,    17,    20,    21,    30,    30,    35,    71,    36,
      72,    69,    82,    82,    82,    82,    83,    83,    84,    85,
      69,    36,    72,    30,    35,    32,     7,    36,    72,    69
};

/* YYR1[RULE-NUM] -- Symbol kind of the left-hand side of rule RULE-NUM.  */
static const yytype_int8 yyr1[] =
{
       0,    41,    42,    42,    43,    43,    43,    44,    44,    44,
      45,    45,    46,    46,    47,    47,    48,    49,    49,    50,
      51,    52,    52,    53,    53,    53,    54,    54,    55,    55,
      56,    57,    57,    58,    58,    59,    59,    60,    60,    60,
      61,    61,    62,    63,    63,    64,    65,    65,    66,    66,
      67,    68,    68,    68,    69,    69,    69,    69,    69,    69,
      69,    69,    69,    69,    69,    69,    70,    70,    71,    71,
      72,    73,    74,    75,    75,    76,    76,    76,    77,    78,
      78,    78,    78,    78,    78,    78,    79,    80,    80,    81,
      81,    81,    81,    82,    82,    82,    83,    83,    83,    83,
      83,    84,    84,    84,    85,    85,    86,    86,    87
};

/* YYR2[RULE-NUM] -- Number of symbols on the right-hand side of rule RULE-NUM.  */
static const yytype_int8 yyr2[] =
{
       0,     2,     1,     2,     1,     1,     1,     5,     6,     6,
       0,     2,     0,     3,     1,     1,     4,     1,     3,     1,
       4,     0,     4,     1,     2,     3,     1,     3,     5,     6,
       3,     1,     3,     2,     4,     0,     4,     1,     2,     3,
       1,     3,     1,     1,     3,     3,     0,     3,     0,     4,
       3,     0,     2,     2,     4,     2,     1,     5,     7,     5,
       2,     2,     2,     3,     5,     6,     0,     1,     2,     3,
       1,     1,     2,     0,     4,     3,     1,     1,     1,     1,
       3,     4,     3,     2,     2,     2,     1,     1,     3,     1,
       3,     3,     3,     1,     3,     3,     1,     3,     3,     3,
       3,     1,     3,     3,     1,     3,     1,     3,     1
};


enum { YYENOMEM = -2 };

#define yyerrok         (yyerrstatus = 0)
#define yyclearin       (yychar = YYEMPTY)

#define YYACCEPT        goto yyacceptlab
#define YYABORT         goto yyabortlab
#define YYERROR         goto yyerrorlab
#define YYNOMEM         goto yyexhaustedlab


#define YYRECOVERING()  (!!yyerrstatus)

#define YYBACKUP(Token, Value)                                    \
  do                                                              \
    if (yychar == YYEMPTY)                                        \
      {                                                           \
        yychar = (Token);                                         \
        yylval = (Value);                                         \
        YYPOPSTACK (yylen);                                       \
        yystate = *yyssp;                                         \
        goto yybackup;                                            \
      }                                                           \
    else                                                          \
      {                                                           \
        yyerror (YY_("syntax error: cannot back up")); \
        YYERROR;                                                  \
      }                                                           \
  while (0)

/* Backward compatibility with an undocumented macro.
   Use YYerror or YYUNDEF. */
#define YYERRCODE YYUNDEF


/* Enable debugging if requested.  */
#if YYDEBUG

# ifndef YYFPRINTF
#  include <stdio.h> /* INFRINGES ON USER NAME SPACE */
#  define YYFPRINTF fprintf
# endif

# define YYDPRINTF(Args)                        \
do {                                            \
  if (yydebug)                                  \
    YYFPRINTF Args;                             \
} while (0)




# define YY_SYMBOL_PRINT(Title, Kind, Value, Location)                    \
do {                                                                      \
  if (yydebug)                                                            \
    {                                                                     \
      YYFPRINTF (stderr, "%s ", Title);                                   \
      yy_symbol_print (stderr,                                            \
                  Kind, Value); \
      YYFPRINTF (stderr, "\n");                                           \
    }                                                                     \
} while (0)


/*-----------------------------------.
| Print this symbol's value on YYO.  |
`-----------------------------------*/

static void
yy_symbol_value_print (FILE *yyo,
                       yysymbol_kind_t yykind, YYSTYPE const * const yyvaluep)
{
  FILE *yyoutput = yyo;
  YY_USE (yyoutput);
  if (!yyvaluep)
    return;
  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  YY_USE (yykind);
  YY_IGNORE_MAYBE_UNINITIALIZED_END
}


/*---------------------------.
| Print this symbol on YYO.  |
`---------------------------*/

static void
yy_symbol_print (FILE *yyo,
                 yysymbol_kind_t yykind, YYSTYPE const * const yyvaluep)
{
  YYFPRINTF (yyo, "%s %s (",
             yykind < YYNTOKENS ? "token" : "nterm", yysymbol_name (yykind));

  yy_symbol_value_print (yyo, yykind, yyvaluep);
  YYFPRINTF (yyo, ")");
}

/*------------------------------------------------------------------.
| yy_stack_print -- Print the state stack from its BOTTOM up to its |
| TOP (included).                                                   |
`------------------------------------------------------------------*/

static void
yy_stack_print (yy_state_t *yybottom, yy_state_t *yytop)
{
  YYFPRINTF (stderr, "Stack now");
  for (; yybottom <= yytop; yybottom++)
    {
      int yybot = *yybottom;
      YYFPRINTF (stderr, " %d", yybot);
    }
  YYFPRINTF (stderr, "\n");
}

# define YY_STACK_PRINT(Bottom, Top)                            \
do {                                                            \
  if (yydebug)                                                  \
    yy_stack_print ((Bottom), (Top));                           \
} while (0)


/*------------------------------------------------.
| Report that the YYRULE is going to be reduced.  |
`------------------------------------------------*/

static void
yy_reduce_print (yy_state_t *yyssp, YYSTYPE *yyvsp,
                 int yyrule)
{
  int yylno = yyrline[yyrule];
  int yynrhs = yyr2[yyrule];
  int yyi;
  YYFPRINTF (stderr, "Reducing stack by rule %d (line %d):\n",
             yyrule - 1, yylno);
  /* The symbols being reduced.  */
  for (yyi = 0; yyi < yynrhs; yyi++)
    {
      YYFPRINTF (stderr, "   $%d = ", yyi + 1);
      yy_symbol_print (stderr,
                       YY_ACCESSING_SYMBOL (+yyssp[yyi + 1 - yynrhs]),
                       &yyvsp[(yyi + 1) - (yynrhs)]);
      YYFPRINTF (stderr, "\n");
    }
}

# define YY_REDUCE_PRINT(Rule)          \
do {                                    \
  if (yydebug)                          \
    yy_reduce_print (yyssp, yyvsp, Rule); \
} while (0)

/* Nonzero means print parse trace.  It is left uninitialized so that
   multiple parsers can coexist.  */
int yydebug;
#else /* !YYDEBUG */
# define YYDPRINTF(Args) ((void) 0)
# define YY_SYMBOL_PRINT(Title, Kind, Value, Location)
# define YY_STACK_PRINT(Bottom, Top)
# define YY_REDUCE_PRINT(Rule)
#endif /* !YYDEBUG */


/* YYINITDEPTH -- initial size of the parser's stacks.  */
#ifndef YYINITDEPTH
# define YYINITDEPTH 200
#endif

/* YYMAXDEPTH -- maximum size the stacks can grow to (effective only
   if the built-in stack extension method is used).

   Do not make this value too large; the results are undefined if
   YYSTACK_ALLOC_MAXIMUM < YYSTACK_BYTES (YYMAXDEPTH)
   evaluated with infinite-precision integer arithmetic.  */

#ifndef YYMAXDEPTH
# define YYMAXDEPTH 10000
#endif






/*-----------------------------------------------.
| Release the memory associated to this symbol.  |
`-----------------------------------------------*/

static void
yydestruct (const char *yymsg,
            yysymbol_kind_t yykind, YYSTYPE *yyvaluep)
{
  YY_USE (yyvaluep);
  if (!yymsg)
    yymsg = "Deleting";
  YY_SYMBOL_PRINT (yymsg, yykind, yyvaluep, yylocationp);

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  YY_USE (yykind);
  YY_IGNORE_MAYBE_UNINITIALIZED_END
}


/* Lookahead token kind.  */
int yychar;

/* The semantic value of the lookahead symbol.  */
YYSTYPE yylval;
/* Number of syntax errors so far.  */
int yynerrs;




/*----------.
| yyparse.  |
`----------*/

int
yyparse (void)
{
    yy_state_fast_t yystate = 0;
    /* Number of tokens to shift before error messages enabled.  */
    int yyerrstatus = 0;

    /* Refer to the stacks through separate pointers, to allow yyoverflow
       to reallocate them elsewhere.  */

    /* Their size.  */
    YYPTRDIFF_T yystacksize = YYINITDEPTH;

    /* The state stack: array, bottom, top.  */
    yy_state_t yyssa[YYINITDEPTH];
    yy_state_t *yyss = yyssa;
    yy_state_t *yyssp = yyss;

    /* The semantic value stack: array, bottom, top.  */
    YYSTYPE yyvsa[YYINITDEPTH];
    YYSTYPE *yyvs = yyvsa;
    YYSTYPE *yyvsp = yyvs;

  int yyn;
  /* The return value of yyparse.  */
  int yyresult;
  /* Lookahead symbol kind.  */
  yysymbol_kind_t yytoken = YYSYMBOL_YYEMPTY;
  /* The variables used to return semantic value and location from the
     action routines.  */
  YYSTYPE yyval;



#define YYPOPSTACK(N)   (yyvsp -= (N), yyssp -= (N))

  /* The number of symbols on the RHS of the reduced rule.
     Keep to zero when no symbol should be popped.  */
  int yylen = 0;

  YYDPRINTF ((stderr, "Starting parse\n"));

  yychar = YYEMPTY; /* Cause a token to be read.  */

  goto yysetstate;


/*------------------------------------------------------------.
| yynewstate -- push a new state, which is found in yystate.  |
`------------------------------------------------------------*/
yynewstate:
  /* In all cases, when you get here, the value and location stacks
     have just been pushed.  So pushing a state here evens the stacks.  */
  yyssp++;


/*--------------------------------------------------------------------.
| yysetstate -- set current state (the top of the stack) to yystate.  |
`--------------------------------------------------------------------*/
yysetstate:
  YYDPRINTF ((stderr, "Entering state %d\n", yystate));
  YY_ASSERT (0 <= yystate && yystate < YYNSTATES);
  YY_IGNORE_USELESS_CAST_BEGIN
  *yyssp = YY_CAST (yy_state_t, yystate);
  YY_IGNORE_USELESS_CAST_END
  YY_STACK_PRINT (yyss, yyssp);

  if (yyss + yystacksize - 1 <= yyssp)
#if !defined yyoverflow && !defined YYSTACK_RELOCATE
    YYNOMEM;
#else
    {
      /* Get the current used size of the three stacks, in elements.  */
      YYPTRDIFF_T yysize = yyssp - yyss + 1;

# if defined yyoverflow
      {
        /* Give user a chance to reallocate the stack.  Use copies of
           these so that the &'s don't force the real ones into
           memory.  */
        yy_state_t *yyss1 = yyss;
        YYSTYPE *yyvs1 = yyvs;

        /* Each stack pointer address is followed by the size of the
           data in use in that stack, in bytes.  This used to be a
           conditional around just the two extra args, but that might
           be undefined if yyoverflow is a macro.  */
        yyoverflow (YY_("memory exhausted"),
                    &yyss1, yysize * YYSIZEOF (*yyssp),
                    &yyvs1, yysize * YYSIZEOF (*yyvsp),
                    &yystacksize);
        yyss = yyss1;
        yyvs = yyvs1;
      }
# else /* defined YYSTACK_RELOCATE */
      /* Extend the stack our own way.  */
      if (YYMAXDEPTH <= yystacksize)
        YYNOMEM;
      yystacksize *= 2;
      if (YYMAXDEPTH < yystacksize)
        yystacksize = YYMAXDEPTH;

      {
        yy_state_t *yyss1 = yyss;
        union yyalloc *yyptr =
          YY_CAST (union yyalloc *,
                   YYSTACK_ALLOC (YY_CAST (YYSIZE_T, YYSTACK_BYTES (yystacksize))));
        if (! yyptr)
          YYNOMEM;
        YYSTACK_RELOCATE (yyss_alloc, yyss);
        YYSTACK_RELOCATE (yyvs_alloc, yyvs);
#  undef YYSTACK_RELOCATE
        if (yyss1 != yyssa)
          YYSTACK_FREE (yyss1);
      }
# endif

      yyssp = yyss + yysize - 1;
      yyvsp = yyvs + yysize - 1;

      YY_IGNORE_USELESS_CAST_BEGIN
      YYDPRINTF ((stderr, "Stack size increased to %ld\n",
                  YY_CAST (long, yystacksize)));
      YY_IGNORE_USELESS_CAST_END

      if (yyss + yystacksize - 1 <= yyssp)
        YYABORT;
    }
#endif /* !defined yyoverflow && !defined YYSTACK_RELOCATE */


  if (yystate == YYFINAL)
    YYACCEPT;

  goto yybackup;


/*-----------.
| yybackup.  |
`-----------*/
yybackup:
  /* Do appropriate processing given the current state.  Read a
     lookahead token if we need one and don't already have one.  */

  /* First try to decide what to do without reference to lookahead token.  */
  yyn = yypact[yystate];
  if (yypact_value_is_default (yyn))
    goto yydefault;

  /* Not known => get a lookahead token if don't already have one.  */

  /* YYCHAR is either empty, or end-of-input, or a valid lookahead.  */
  if (yychar == YYEMPTY)
    {
      YYDPRINTF ((stderr, "Reading a token\n"));
      yychar = yylex ();
    }

  if (yychar <= YYEOF)
    {
      yychar = YYEOF;
      yytoken = YYSYMBOL_YYEOF;
      YYDPRINTF ((stderr, "Now at end of input.\n"));
    }
  else if (yychar == YYerror)
    {
      /* The scanner already issued an error message, process directly
         to error recovery.  But do not keep the error token as
         lookahead, it is too special and may lead us to an endless
         loop in error recovery. */
      yychar = YYUNDEF;
      yytoken = YYSYMBOL_YYerror;
      goto yyerrlab1;
    }
  else
    {
      yytoken = YYTRANSLATE (yychar);
      YY_SYMBOL_PRINT ("Next token is", yytoken, &yylval, &yylloc);
    }

  /* If the proper action on seeing token YYTOKEN is to reduce or to
     detect an error, take that action.  */
  yyn += yytoken;
  if (yyn < 0 || YYLAST < yyn || yycheck[yyn] != yytoken)
    goto yydefault;
  yyn = yytable[yyn];
  if (yyn <= 0)
    {
      if (yytable_value_is_error (yyn))
        goto yyerrlab;
      yyn = -yyn;
      goto yyreduce;
    }

  /* Count tokens shifted since error; after three, turn off error
     status.  */
  if (yyerrstatus)
    yyerrstatus--;

  /* Shift the lookahead token.  */
  YY_SYMBOL_PRINT ("Shifting", yytoken, &yylval, &yylloc);
  yystate = yyn;
  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END

  /* Discard the shifted token.  */
  yychar = YYEMPTY;
  goto yynewstate;


/*-----------------------------------------------------------.
| yydefault -- do the default action for the current state.  |
`-----------------------------------------------------------*/
yydefault:
  yyn = yydefact[yystate];
  if (yyn == 0)
    goto yyerrlab;
  goto yyreduce;


/*-----------------------------.
| yyreduce -- do a reduction.  |
`-----------------------------*/
yyreduce:
  /* yyn is the number of a rule to reduce with.  */
  yylen = yyr2[yyn];

  /* If YYLEN is nonzero, implement the default value of the action:
     '$$ = $1'.

     Otherwise, the following line sets YYVAL to garbage.
     This behavior is undocumented and Bison
     users should not rely upon it.  Assigning to YYVAL
     unconditionally makes the parser a bit smaller, and it avoids a
     GCC warning that YYVAL may be used uninitialized.  */
  yyval = yyvsp[1-yylen];


  YY_REDUCE_PRINT (yyn);
  switch (yyn)
    {
  case 2: /* CompUnit: CompUnitItem  */
#line 79 "Parser.y"
        {
            TopLevelList list = {0};
            top_level_list_push(&list, (yyvsp[0].top_item));
            (yyval.top_items) = list;
            g_program = make_program((yyval.top_items));
        }
#line 1310 "parser.tab.c"
    break;

  case 3: /* CompUnit: CompUnit CompUnitItem  */
#line 86 "Parser.y"
        {
            (yyval.top_items) = (yyvsp[-1].top_items);
            top_level_list_push(&(yyval.top_items), (yyvsp[0].top_item));
            g_program = make_program((yyval.top_items));
        }
#line 1320 "parser.tab.c"
    break;

  case 4: /* CompUnitItem: Decl  */
#line 95 "Parser.y"
        { (yyval.top_item) = make_top_decl((yyvsp[0].decl)); }
#line 1326 "parser.tab.c"
    break;

  case 5: /* CompUnitItem: GlobalIntItem  */
#line 97 "Parser.y"
        { (yyval.top_item) = (yyvsp[0].top_item); }
#line 1332 "parser.tab.c"
    break;

  case 6: /* CompUnitItem: VoidFuncDef  */
#line 99 "Parser.y"
        { (yyval.top_item) = (yyvsp[0].top_item); }
#line 1338 "parser.tab.c"
    break;

  case 7: /* GlobalIntItem: INTTK IDENFR LPARENT RPARENT Block  */
#line 104 "Parser.y"
        {
            ParamList list = {0};
            (yyval.top_item) = make_top_func(make_func(TYPE_INT, (yyvsp[-3].str), list, (yyvsp[0].block)));
        }
#line 1347 "parser.tab.c"
    break;

  case 8: /* GlobalIntItem: INTTK IDENFR LPARENT FuncFParams RPARENT Block  */
#line 109 "Parser.y"
        { (yyval.top_item) = make_top_func(make_func(TYPE_INT, (yyvsp[-4].str), (yyvsp[-2].params), (yyvsp[0].block))); }
#line 1353 "parser.tab.c"
    break;

  case 9: /* GlobalIntItem: INTTK IDENFR VarDefDims GlobalVarInitOpt GlobalVarMore SEMICN  */
#line 111 "Parser.y"
        {
            DeclItemList list = {0};
            decl_item_list_push(&list, make_decl_item((yyvsp[-4].str), (yyvsp[-3].int_list), (yyvsp[-2].init)));
            for (int i = 0; i < (yyvsp[-1].decl_items).count; ++i) {
                decl_item_list_push(&list, (yyvsp[-1].decl_items).items[i]);
            }
            (yyval.top_item) = make_top_decl(make_decl(false, list));
        }
#line 1366 "parser.tab.c"
    break;

  case 10: /* GlobalVarInitOpt: %empty  */
#line 123 "Parser.y"
        { (yyval.init) = NULL; }
#line 1372 "parser.tab.c"
    break;

  case 11: /* GlobalVarInitOpt: ASSIGN InitVal  */
#line 125 "Parser.y"
        { (yyval.init) = (yyvsp[0].init); }
#line 1378 "parser.tab.c"
    break;

  case 12: /* GlobalVarMore: %empty  */
#line 130 "Parser.y"
        {
            DeclItemList list = {0};
            (yyval.decl_items) = list;
        }
#line 1387 "parser.tab.c"
    break;

  case 13: /* GlobalVarMore: GlobalVarMore COMMA VarDef  */
#line 135 "Parser.y"
        {
            (yyval.decl_items) = (yyvsp[-2].decl_items);
            decl_item_list_push(&(yyval.decl_items), (yyvsp[0].decl_item));
        }
#line 1396 "parser.tab.c"
    break;

  case 14: /* Decl: ConstDecl  */
#line 142 "Parser.y"
                { (yyval.decl) = (yyvsp[0].decl); }
#line 1402 "parser.tab.c"
    break;

  case 15: /* Decl: VarDecl  */
#line 143 "Parser.y"
              { (yyval.decl) = (yyvsp[0].decl); }
#line 1408 "parser.tab.c"
    break;

  case 16: /* ConstDecl: CONSTTK BType ConstDefList SEMICN  */
#line 148 "Parser.y"
        { (yyval.decl) = make_decl(true, (yyvsp[-1].decl_items)); }
#line 1414 "parser.tab.c"
    break;

  case 17: /* ConstDefList: ConstDef  */
#line 153 "Parser.y"
        {
            DeclItemList list = {0};
            decl_item_list_push(&list, (yyvsp[0].decl_item));
            (yyval.decl_items) = list;
        }
#line 1424 "parser.tab.c"
    break;

  case 18: /* ConstDefList: ConstDefList COMMA ConstDef  */
#line 159 "Parser.y"
        {
            (yyval.decl_items) = (yyvsp[-2].decl_items);
            decl_item_list_push(&(yyval.decl_items), (yyvsp[0].decl_item));
        }
#line 1433 "parser.tab.c"
    break;

  case 19: /* BType: INTTK  */
#line 166 "Parser.y"
            { (yyval.type_spec) = TYPE_INT; }
#line 1439 "parser.tab.c"
    break;

  case 20: /* ConstDef: IDENFR ConstDefDims ASSIGN ConstInitVal  */
#line 171 "Parser.y"
        {
            (yyval.decl_item) = make_decl_item((yyvsp[-3].str), (yyvsp[-2].int_list), (yyvsp[0].init));
            if ((yyvsp[-2].int_list).count == 0 && (yyvsp[0].init)->is_expr) {
                register_const_binding((yyvsp[-3].str), eval_const_ast_expr((yyvsp[0].init)->expr));
            }
        }
#line 1450 "parser.tab.c"
    break;

  case 21: /* ConstDefDims: %empty  */
#line 181 "Parser.y"
        { IntList list = {0}; (yyval.int_list) = list; }
#line 1456 "parser.tab.c"
    break;

  case 22: /* ConstDefDims: ConstDefDims LBRACK ConstExp RBRACK  */
#line 183 "Parser.y"
        {
            (yyval.int_list) = (yyvsp[-3].int_list);
            int_list_push(&(yyval.int_list), eval_const_ast_expr((yyvsp[-1].expr)));
        }
#line 1465 "parser.tab.c"
    break;

  case 23: /* ConstInitVal: ConstExp  */
#line 191 "Parser.y"
        { (yyval.init) = make_expr_init((yyvsp[0].expr)); }
#line 1471 "parser.tab.c"
    break;

  case 24: /* ConstInitVal: LBRACE RBRACE  */
#line 193 "Parser.y"
        {
            InitValList list = {0};
            (yyval.init) = make_list_init(list);
        }
#line 1480 "parser.tab.c"
    break;

  case 25: /* ConstInitVal: LBRACE ConstInitValList RBRACE  */
#line 198 "Parser.y"
        { (yyval.init) = make_list_init((yyvsp[-1].init_list)); }
#line 1486 "parser.tab.c"
    break;

  case 26: /* ConstInitValList: ConstInitVal  */
#line 203 "Parser.y"
        {
            InitValList list = {0};
            init_list_push(&list, (yyvsp[0].init));
            (yyval.init_list) = list;
        }
#line 1496 "parser.tab.c"
    break;

  case 27: /* ConstInitValList: ConstInitValList COMMA ConstInitVal  */
#line 209 "Parser.y"
        {
            (yyval.init_list) = (yyvsp[-2].init_list);
            init_list_push(&(yyval.init_list), (yyvsp[0].init));
        }
#line 1505 "parser.tab.c"
    break;

  case 28: /* VoidFuncDef: VOIDTK IDENFR LPARENT RPARENT Block  */
#line 217 "Parser.y"
        {
            ParamList list = {0};
            (yyval.top_item) = make_top_func(make_func(TYPE_VOID, (yyvsp[-3].str), list, (yyvsp[0].block)));
        }
#line 1514 "parser.tab.c"
    break;

  case 29: /* VoidFuncDef: VOIDTK IDENFR LPARENT FuncFParams RPARENT Block  */
#line 222 "Parser.y"
        { (yyval.top_item) = make_top_func(make_func(TYPE_VOID, (yyvsp[-4].str), (yyvsp[-2].params), (yyvsp[0].block))); }
#line 1520 "parser.tab.c"
    break;

  case 30: /* VarDecl: BType VarDefList SEMICN  */
#line 227 "Parser.y"
        { (yyval.decl) = make_decl(false, (yyvsp[-1].decl_items)); }
#line 1526 "parser.tab.c"
    break;

  case 31: /* VarDefList: VarDef  */
#line 232 "Parser.y"
        {
            DeclItemList list = {0};
            decl_item_list_push(&list, (yyvsp[0].decl_item));
            (yyval.decl_items) = list;
        }
#line 1536 "parser.tab.c"
    break;

  case 32: /* VarDefList: VarDefList COMMA VarDef  */
#line 238 "Parser.y"
        {
            (yyval.decl_items) = (yyvsp[-2].decl_items);
            decl_item_list_push(&(yyval.decl_items), (yyvsp[0].decl_item));
        }
#line 1545 "parser.tab.c"
    break;

  case 33: /* VarDef: IDENFR VarDefDims  */
#line 246 "Parser.y"
        { (yyval.decl_item) = make_decl_item((yyvsp[-1].str), (yyvsp[0].int_list), NULL); }
#line 1551 "parser.tab.c"
    break;

  case 34: /* VarDef: IDENFR VarDefDims ASSIGN InitVal  */
#line 248 "Parser.y"
        { (yyval.decl_item) = make_decl_item((yyvsp[-3].str), (yyvsp[-2].int_list), (yyvsp[0].init)); }
#line 1557 "parser.tab.c"
    break;

  case 35: /* VarDefDims: %empty  */
#line 253 "Parser.y"
        { IntList list = {0}; (yyval.int_list) = list; }
#line 1563 "parser.tab.c"
    break;

  case 36: /* VarDefDims: VarDefDims LBRACK ConstExp RBRACK  */
#line 255 "Parser.y"
        {
            (yyval.int_list) = (yyvsp[-3].int_list);
            int_list_push(&(yyval.int_list), eval_const_ast_expr((yyvsp[-1].expr)));
        }
#line 1572 "parser.tab.c"
    break;

  case 37: /* InitVal: Exp  */
#line 263 "Parser.y"
        { (yyval.init) = make_expr_init((yyvsp[0].expr)); }
#line 1578 "parser.tab.c"
    break;

  case 38: /* InitVal: LBRACE RBRACE  */
#line 265 "Parser.y"
        {
            InitValList list = {0};
            (yyval.init) = make_list_init(list);
        }
#line 1587 "parser.tab.c"
    break;

  case 39: /* InitVal: LBRACE InitValList RBRACE  */
#line 270 "Parser.y"
        { (yyval.init) = make_list_init((yyvsp[-1].init_list)); }
#line 1593 "parser.tab.c"
    break;

  case 40: /* InitValList: InitVal  */
#line 275 "Parser.y"
        {
            InitValList list = {0};
            init_list_push(&list, (yyvsp[0].init));
            (yyval.init_list) = list;
        }
#line 1603 "parser.tab.c"
    break;

  case 41: /* InitValList: InitValList COMMA InitVal  */
#line 281 "Parser.y"
        {
            (yyval.init_list) = (yyvsp[-2].init_list);
            init_list_push(&(yyval.init_list), (yyvsp[0].init));
        }
#line 1612 "parser.tab.c"
    break;

  case 42: /* FuncFParams: FuncFParamsList  */
#line 288 "Parser.y"
                      { (yyval.params) = (yyvsp[0].params); }
#line 1618 "parser.tab.c"
    break;

  case 43: /* FuncFParamsList: FuncFParam  */
#line 293 "Parser.y"
        {
            ParamList list = {0};
            param_list_push(&list, (yyvsp[0].param));
            (yyval.params) = list;
        }
#line 1628 "parser.tab.c"
    break;

  case 44: /* FuncFParamsList: FuncFParamsList COMMA FuncFParam  */
#line 299 "Parser.y"
        {
            (yyval.params) = (yyvsp[-2].params);
            param_list_push(&(yyval.params), (yyvsp[0].param));
        }
#line 1637 "parser.tab.c"
    break;

  case 45: /* FuncFParam: BType IDENFR FuncFParamSuffix  */
#line 307 "Parser.y"
        { (yyval.param) = make_param((yyvsp[-1].str), (yyvsp[0].int_list).count > 0 || (yyvsp[0].int_list).capacity == -1, (yyvsp[0].int_list)); }
#line 1643 "parser.tab.c"
    break;

  case 46: /* FuncFParamSuffix: %empty  */
#line 312 "Parser.y"
        { IntList list = {0}; (yyval.int_list) = list; }
#line 1649 "parser.tab.c"
    break;

  case 47: /* FuncFParamSuffix: LBRACK RBRACK FuncFParamArrayDims  */
#line 314 "Parser.y"
        { (yyval.int_list) = (yyvsp[0].int_list); (yyval.int_list).capacity = -1; }
#line 1655 "parser.tab.c"
    break;

  case 48: /* FuncFParamArrayDims: %empty  */
#line 319 "Parser.y"
        { IntList list = {0}; (yyval.int_list) = list; }
#line 1661 "parser.tab.c"
    break;

  case 49: /* FuncFParamArrayDims: FuncFParamArrayDims LBRACK Exp RBRACK  */
#line 321 "Parser.y"
        {
            (yyval.int_list) = (yyvsp[-3].int_list);
            int_list_push(&(yyval.int_list), eval_const_ast_expr((yyvsp[-1].expr)));
        }
#line 1670 "parser.tab.c"
    break;

  case 50: /* Block: LBRACE BlockItemList RBRACE  */
#line 329 "Parser.y"
        { (yyval.block) = make_block((yyvsp[-1].block_items)); }
#line 1676 "parser.tab.c"
    break;

  case 51: /* BlockItemList: %empty  */
#line 334 "Parser.y"
        { BlockItemList list = {0}; (yyval.block_items) = list; }
#line 1682 "parser.tab.c"
    break;

  case 52: /* BlockItemList: BlockItemList Decl  */
#line 336 "Parser.y"
        {
            (yyval.block_items) = (yyvsp[-1].block_items);
            block_item_list_push(&(yyval.block_items), BLOCK_ITEM_DECL, (yyvsp[0].decl));
        }
#line 1691 "parser.tab.c"
    break;

  case 53: /* BlockItemList: BlockItemList Stmt  */
#line 341 "Parser.y"
        {
            (yyval.block_items) = (yyvsp[-1].block_items);
            block_item_list_push(&(yyval.block_items), BLOCK_ITEM_STMT, (yyvsp[0].stmt));
        }
#line 1700 "parser.tab.c"
    break;

  case 54: /* Stmt: LVal ASSIGN Exp SEMICN  */
#line 349 "Parser.y"
        { (yyval.stmt) = make_assign_stmt((yyvsp[-3].lval), (yyvsp[-1].expr)); }
#line 1706 "parser.tab.c"
    break;

  case 55: /* Stmt: ExpOpt SEMICN  */
#line 351 "Parser.y"
        { (yyval.stmt) = make_expr_stmt((yyvsp[-1].expr)); }
#line 1712 "parser.tab.c"
    break;

  case 56: /* Stmt: Block  */
#line 353 "Parser.y"
        { (yyval.stmt) = make_block_stmt((yyvsp[0].block)); }
#line 1718 "parser.tab.c"
    break;

  case 57: /* Stmt: IFTK LPARENT Cond RPARENT Stmt  */
#line 355 "Parser.y"
        { (yyval.stmt) = make_if_stmt((yyvsp[-2].expr), (yyvsp[0].stmt), NULL); }
#line 1724 "parser.tab.c"
    break;

  case 58: /* Stmt: IFTK LPARENT Cond RPARENT Stmt ELSETK Stmt  */
#line 357 "Parser.y"
        { (yyval.stmt) = make_if_stmt((yyvsp[-4].expr), (yyvsp[-2].stmt), (yyvsp[0].stmt)); }
#line 1730 "parser.tab.c"
    break;

  case 59: /* Stmt: WHILETK LPARENT Cond RPARENT Stmt  */
#line 359 "Parser.y"
        { (yyval.stmt) = make_while_stmt((yyvsp[-2].expr), (yyvsp[0].stmt)); }
#line 1736 "parser.tab.c"
    break;

  case 60: /* Stmt: BREAKTK SEMICN  */
#line 361 "Parser.y"
        { (yyval.stmt) = make_break_stmt(); }
#line 1742 "parser.tab.c"
    break;

  case 61: /* Stmt: CONTINUETK SEMICN  */
#line 363 "Parser.y"
        { (yyval.stmt) = make_continue_stmt(); }
#line 1748 "parser.tab.c"
    break;

  case 62: /* Stmt: RETURNTK SEMICN  */
#line 365 "Parser.y"
        { (yyval.stmt) = make_return_stmt(NULL); }
#line 1754 "parser.tab.c"
    break;

  case 63: /* Stmt: RETURNTK Exp SEMICN  */
#line 367 "Parser.y"
        { (yyval.stmt) = make_return_stmt((yyvsp[-1].expr)); }
#line 1760 "parser.tab.c"
    break;

  case 64: /* Stmt: PRINTFTK LPARENT STRCON RPARENT SEMICN  */
#line 369 "Parser.y"
        {
            ExprList list = {0};
            (yyval.stmt) = make_printf_stmt((yyvsp[-2].str), list);
        }
#line 1769 "parser.tab.c"
    break;

  case 65: /* Stmt: PRINTFTK LPARENT STRCON PrintfArgs RPARENT SEMICN  */
#line 374 "Parser.y"
        { (yyval.stmt) = make_printf_stmt((yyvsp[-3].str), (yyvsp[-2].expr_list)); }
#line 1775 "parser.tab.c"
    break;

  case 66: /* ExpOpt: %empty  */
#line 379 "Parser.y"
        { (yyval.expr) = NULL; }
#line 1781 "parser.tab.c"
    break;

  case 67: /* ExpOpt: Exp  */
#line 381 "Parser.y"
        { (yyval.expr) = (yyvsp[0].expr); }
#line 1787 "parser.tab.c"
    break;

  case 68: /* PrintfArgs: COMMA Exp  */
#line 386 "Parser.y"
        {
            ExprList list = {0};
            expr_list_push(&list, (yyvsp[0].expr));
            (yyval.expr_list) = list;
        }
#line 1797 "parser.tab.c"
    break;

  case 69: /* PrintfArgs: PrintfArgs COMMA Exp  */
#line 392 "Parser.y"
        {
            (yyval.expr_list) = (yyvsp[-2].expr_list);
            expr_list_push(&(yyval.expr_list), (yyvsp[0].expr));
        }
#line 1806 "parser.tab.c"
    break;

  case 70: /* Exp: AddExp  */
#line 399 "Parser.y"
             { (yyval.expr) = (yyvsp[0].expr); }
#line 1812 "parser.tab.c"
    break;

  case 71: /* Cond: LOrExp  */
#line 403 "Parser.y"
             { (yyval.expr) = (yyvsp[0].expr); }
#line 1818 "parser.tab.c"
    break;

  case 72: /* LVal: IDENFR LValIndices  */
#line 408 "Parser.y"
        { (yyval.lval) = make_lval((yyvsp[-1].str), (yyvsp[0].expr_list)); }
#line 1824 "parser.tab.c"
    break;

  case 73: /* LValIndices: %empty  */
#line 413 "Parser.y"
        { ExprList list = {0}; (yyval.expr_list) = list; }
#line 1830 "parser.tab.c"
    break;

  case 74: /* LValIndices: LValIndices LBRACK Exp RBRACK  */
#line 415 "Parser.y"
        {
            (yyval.expr_list) = (yyvsp[-3].expr_list);
            expr_list_push(&(yyval.expr_list), (yyvsp[-1].expr));
        }
#line 1839 "parser.tab.c"
    break;

  case 75: /* PrimaryExp: LPARENT Exp RPARENT  */
#line 422 "Parser.y"
                          { (yyval.expr) = (yyvsp[-1].expr); }
#line 1845 "parser.tab.c"
    break;

  case 76: /* PrimaryExp: LVal  */
#line 423 "Parser.y"
           { (yyval.expr) = make_lval_expr((yyvsp[0].lval)); }
#line 1851 "parser.tab.c"
    break;

  case 77: /* PrimaryExp: Number  */
#line 424 "Parser.y"
             { (yyval.expr) = (yyvsp[0].expr); }
#line 1857 "parser.tab.c"
    break;

  case 78: /* Number: INTCON  */
#line 428 "Parser.y"
             { (yyval.expr) = make_number_expr(parse_int_literal((yyvsp[0].str))); }
#line 1863 "parser.tab.c"
    break;

  case 79: /* UnaryExp: PrimaryExp  */
#line 432 "Parser.y"
                 { (yyval.expr) = (yyvsp[0].expr); }
#line 1869 "parser.tab.c"
    break;

  case 80: /* UnaryExp: IDENFR LPARENT RPARENT  */
#line 434 "Parser.y"
        {
            ExprList list = {0};
            (yyval.expr) = make_call_expr((yyvsp[-2].str), list);
        }
#line 1878 "parser.tab.c"
    break;

  case 81: /* UnaryExp: IDENFR LPARENT FuncRParams RPARENT  */
#line 439 "Parser.y"
        { (yyval.expr) = make_call_expr((yyvsp[-3].str), (yyvsp[-1].expr_list)); }
#line 1884 "parser.tab.c"
    break;

  case 82: /* UnaryExp: GETINTTK LPARENT RPARENT  */
#line 441 "Parser.y"
        { (yyval.expr) = make_getint_expr(); }
#line 1890 "parser.tab.c"
    break;

  case 83: /* UnaryExp: PLUS UnaryExp  */
#line 443 "Parser.y"
        { (yyval.expr) = make_unary_expr(UNARY_PLUS, (yyvsp[0].expr)); }
#line 1896 "parser.tab.c"
    break;

  case 84: /* UnaryExp: MINU UnaryExp  */
#line 445 "Parser.y"
        { (yyval.expr) = make_unary_expr(UNARY_MINUS, (yyvsp[0].expr)); }
#line 1902 "parser.tab.c"
    break;

  case 85: /* UnaryExp: NOT UnaryExp  */
#line 447 "Parser.y"
        { (yyval.expr) = make_unary_expr(UNARY_NOT, (yyvsp[0].expr)); }
#line 1908 "parser.tab.c"
    break;

  case 86: /* FuncRParams: FuncRParamsList  */
#line 451 "Parser.y"
                      { (yyval.expr_list) = (yyvsp[0].expr_list); }
#line 1914 "parser.tab.c"
    break;

  case 87: /* FuncRParamsList: Exp  */
#line 456 "Parser.y"
        {
            ExprList list = {0};
            expr_list_push(&list, (yyvsp[0].expr));
            (yyval.expr_list) = list;
        }
#line 1924 "parser.tab.c"
    break;

  case 88: /* FuncRParamsList: FuncRParamsList COMMA Exp  */
#line 462 "Parser.y"
        {
            (yyval.expr_list) = (yyvsp[-2].expr_list);
            expr_list_push(&(yyval.expr_list), (yyvsp[0].expr));
        }
#line 1933 "parser.tab.c"
    break;

  case 89: /* MulExp: UnaryExp  */
#line 469 "Parser.y"
               { (yyval.expr) = (yyvsp[0].expr); }
#line 1939 "parser.tab.c"
    break;

  case 90: /* MulExp: MulExp MULT UnaryExp  */
#line 471 "Parser.y"
        { (yyval.expr) = make_binary_expr(BIN_MUL, (yyvsp[-2].expr), (yyvsp[0].expr)); }
#line 1945 "parser.tab.c"
    break;

  case 91: /* MulExp: MulExp DIV UnaryExp  */
#line 473 "Parser.y"
        { (yyval.expr) = make_binary_expr(BIN_DIV, (yyvsp[-2].expr), (yyvsp[0].expr)); }
#line 1951 "parser.tab.c"
    break;

  case 92: /* MulExp: MulExp MOD UnaryExp  */
#line 475 "Parser.y"
        { (yyval.expr) = make_binary_expr(BIN_MOD, (yyvsp[-2].expr), (yyvsp[0].expr)); }
#line 1957 "parser.tab.c"
    break;

  case 93: /* AddExp: MulExp  */
#line 479 "Parser.y"
             { (yyval.expr) = (yyvsp[0].expr); }
#line 1963 "parser.tab.c"
    break;

  case 94: /* AddExp: AddExp PLUS MulExp  */
#line 481 "Parser.y"
        { (yyval.expr) = make_binary_expr(BIN_ADD, (yyvsp[-2].expr), (yyvsp[0].expr)); }
#line 1969 "parser.tab.c"
    break;

  case 95: /* AddExp: AddExp MINU MulExp  */
#line 483 "Parser.y"
        { (yyval.expr) = make_binary_expr(BIN_SUB, (yyvsp[-2].expr), (yyvsp[0].expr)); }
#line 1975 "parser.tab.c"
    break;

  case 96: /* RelExp: AddExp  */
#line 487 "Parser.y"
             { (yyval.expr) = (yyvsp[0].expr); }
#line 1981 "parser.tab.c"
    break;

  case 97: /* RelExp: RelExp LSS AddExp  */
#line 489 "Parser.y"
        { (yyval.expr) = make_binary_expr(BIN_LT, (yyvsp[-2].expr), (yyvsp[0].expr)); }
#line 1987 "parser.tab.c"
    break;

  case 98: /* RelExp: RelExp GRE AddExp  */
#line 491 "Parser.y"
        { (yyval.expr) = make_binary_expr(BIN_GT, (yyvsp[-2].expr), (yyvsp[0].expr)); }
#line 1993 "parser.tab.c"
    break;

  case 99: /* RelExp: RelExp LEQ AddExp  */
#line 493 "Parser.y"
        { (yyval.expr) = make_binary_expr(BIN_LE, (yyvsp[-2].expr), (yyvsp[0].expr)); }
#line 1999 "parser.tab.c"
    break;

  case 100: /* RelExp: RelExp GEQ AddExp  */
#line 495 "Parser.y"
        { (yyval.expr) = make_binary_expr(BIN_GE, (yyvsp[-2].expr), (yyvsp[0].expr)); }
#line 2005 "parser.tab.c"
    break;

  case 101: /* EqExp: RelExp  */
#line 499 "Parser.y"
             { (yyval.expr) = (yyvsp[0].expr); }
#line 2011 "parser.tab.c"
    break;

  case 102: /* EqExp: EqExp EQL RelExp  */
#line 501 "Parser.y"
        { (yyval.expr) = make_binary_expr(BIN_EQ, (yyvsp[-2].expr), (yyvsp[0].expr)); }
#line 2017 "parser.tab.c"
    break;

  case 103: /* EqExp: EqExp NEQ RelExp  */
#line 503 "Parser.y"
        { (yyval.expr) = make_binary_expr(BIN_NE, (yyvsp[-2].expr), (yyvsp[0].expr)); }
#line 2023 "parser.tab.c"
    break;

  case 104: /* LAndExp: EqExp  */
#line 507 "Parser.y"
            { (yyval.expr) = (yyvsp[0].expr); }
#line 2029 "parser.tab.c"
    break;

  case 105: /* LAndExp: LAndExp AND EqExp  */
#line 509 "Parser.y"
        { (yyval.expr) = make_binary_expr(BIN_AND, (yyvsp[-2].expr), (yyvsp[0].expr)); }
#line 2035 "parser.tab.c"
    break;

  case 106: /* LOrExp: LAndExp  */
#line 513 "Parser.y"
              { (yyval.expr) = (yyvsp[0].expr); }
#line 2041 "parser.tab.c"
    break;

  case 107: /* LOrExp: LOrExp OR LAndExp  */
#line 515 "Parser.y"
        { (yyval.expr) = make_binary_expr(BIN_OR, (yyvsp[-2].expr), (yyvsp[0].expr)); }
#line 2047 "parser.tab.c"
    break;

  case 108: /* ConstExp: AddExp  */
#line 519 "Parser.y"
             { (yyval.expr) = (yyvsp[0].expr); }
#line 2053 "parser.tab.c"
    break;


#line 2057 "parser.tab.c"

      default: break;
    }
  /* User semantic actions sometimes alter yychar, and that requires
     that yytoken be updated with the new translation.  We take the
     approach of translating immediately before every use of yytoken.
     One alternative is translating here after every semantic action,
     but that translation would be missed if the semantic action invokes
     YYABORT, YYACCEPT, or YYERROR immediately after altering yychar or
     if it invokes YYBACKUP.  In the case of YYABORT or YYACCEPT, an
     incorrect destructor might then be invoked immediately.  In the
     case of YYERROR or YYBACKUP, subsequent parser actions might lead
     to an incorrect destructor call or verbose syntax error message
     before the lookahead is translated.  */
  YY_SYMBOL_PRINT ("-> $$ =", YY_CAST (yysymbol_kind_t, yyr1[yyn]), &yyval, &yyloc);

  YYPOPSTACK (yylen);
  yylen = 0;

  *++yyvsp = yyval;

  /* Now 'shift' the result of the reduction.  Determine what state
     that goes to, based on the state we popped back to and the rule
     number reduced by.  */
  {
    const int yylhs = yyr1[yyn] - YYNTOKENS;
    const int yyi = yypgoto[yylhs] + *yyssp;
    yystate = (0 <= yyi && yyi <= YYLAST && yycheck[yyi] == *yyssp
               ? yytable[yyi]
               : yydefgoto[yylhs]);
  }

  goto yynewstate;


/*--------------------------------------.
| yyerrlab -- here on detecting error.  |
`--------------------------------------*/
yyerrlab:
  /* Make sure we have latest lookahead translation.  See comments at
     user semantic actions for why this is necessary.  */
  yytoken = yychar == YYEMPTY ? YYSYMBOL_YYEMPTY : YYTRANSLATE (yychar);
  /* If not already recovering from an error, report this error.  */
  if (!yyerrstatus)
    {
      ++yynerrs;
      yyerror (YY_("syntax error"));
    }

  if (yyerrstatus == 3)
    {
      /* If just tried and failed to reuse lookahead token after an
         error, discard it.  */

      if (yychar <= YYEOF)
        {
          /* Return failure if at end of input.  */
          if (yychar == YYEOF)
            YYABORT;
        }
      else
        {
          yydestruct ("Error: discarding",
                      yytoken, &yylval);
          yychar = YYEMPTY;
        }
    }

  /* Else will try to reuse lookahead token after shifting the error
     token.  */
  goto yyerrlab1;


/*---------------------------------------------------.
| yyerrorlab -- error raised explicitly by YYERROR.  |
`---------------------------------------------------*/
yyerrorlab:
  /* Pacify compilers when the user code never invokes YYERROR and the
     label yyerrorlab therefore never appears in user code.  */
  if (0)
    YYERROR;
  ++yynerrs;

  /* Do not reclaim the symbols of the rule whose action triggered
     this YYERROR.  */
  YYPOPSTACK (yylen);
  yylen = 0;
  YY_STACK_PRINT (yyss, yyssp);
  yystate = *yyssp;
  goto yyerrlab1;


/*-------------------------------------------------------------.
| yyerrlab1 -- common code for both syntax error and YYERROR.  |
`-------------------------------------------------------------*/
yyerrlab1:
  yyerrstatus = 3;      /* Each real token shifted decrements this.  */

  /* Pop stack until we find a state that shifts the error token.  */
  for (;;)
    {
      yyn = yypact[yystate];
      if (!yypact_value_is_default (yyn))
        {
          yyn += YYSYMBOL_YYerror;
          if (0 <= yyn && yyn <= YYLAST && yycheck[yyn] == YYSYMBOL_YYerror)
            {
              yyn = yytable[yyn];
              if (0 < yyn)
                break;
            }
        }

      /* Pop the current state because it cannot handle the error token.  */
      if (yyssp == yyss)
        YYABORT;


      yydestruct ("Error: popping",
                  YY_ACCESSING_SYMBOL (yystate), yyvsp);
      YYPOPSTACK (1);
      yystate = *yyssp;
      YY_STACK_PRINT (yyss, yyssp);
    }

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END


  /* Shift the error token.  */
  YY_SYMBOL_PRINT ("Shifting", YY_ACCESSING_SYMBOL (yyn), yyvsp, yylsp);

  yystate = yyn;
  goto yynewstate;


/*-------------------------------------.
| yyacceptlab -- YYACCEPT comes here.  |
`-------------------------------------*/
yyacceptlab:
  yyresult = 0;
  goto yyreturnlab;


/*-----------------------------------.
| yyabortlab -- YYABORT comes here.  |
`-----------------------------------*/
yyabortlab:
  yyresult = 1;
  goto yyreturnlab;


/*-----------------------------------------------------------.
| yyexhaustedlab -- YYNOMEM (memory exhaustion) comes here.  |
`-----------------------------------------------------------*/
yyexhaustedlab:
  yyerror (YY_("memory exhausted"));
  yyresult = 2;
  goto yyreturnlab;


/*----------------------------------------------------------.
| yyreturnlab -- parsing is finished, clean up and return.  |
`----------------------------------------------------------*/
yyreturnlab:
  if (yychar != YYEMPTY)
    {
      /* Make sure we have latest lookahead translation.  See comments at
         user semantic actions for why this is necessary.  */
      yytoken = YYTRANSLATE (yychar);
      yydestruct ("Cleanup: discarding lookahead",
                  yytoken, &yylval);
    }
  /* Do not reclaim the symbols of the rule whose action triggered
     this YYABORT or YYACCEPT.  */
  YYPOPSTACK (yylen);
  YY_STACK_PRINT (yyss, yyssp);
  while (yyssp != yyss)
    {
      yydestruct ("Cleanup: popping",
                  YY_ACCESSING_SYMBOL (+*yyssp), yyvsp);
      YYPOPSTACK (1);
    }
#ifndef yyoverflow
  if (yyss != yyssa)
    YYSTACK_FREE (yyss);
#endif

  return yyresult;
}

#line 522 "Parser.y"


void yyerror(const char *s) {
    fprintf(stderr, "syntax error at line %d: %s\n", yylineno, s);
}
