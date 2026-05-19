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
  YYSYMBOL_FLOATTK = 5,                    /* FLOATTK  */
  YYSYMBOL_VOIDTK = 6,                     /* VOIDTK  */
  YYSYMBOL_IFTK = 7,                       /* IFTK  */
  YYSYMBOL_ELSETK = 8,                     /* ELSETK  */
  YYSYMBOL_WHILETK = 9,                    /* WHILETK  */
  YYSYMBOL_BREAKTK = 10,                   /* BREAKTK  */
  YYSYMBOL_CONTINUETK = 11,                /* CONTINUETK  */
  YYSYMBOL_RETURNTK = 12,                  /* RETURNTK  */
  YYSYMBOL_GETINTTK = 13,                  /* GETINTTK  */
  YYSYMBOL_PRINTFTK = 14,                  /* PRINTFTK  */
  YYSYMBOL_LEQ = 15,                       /* LEQ  */
  YYSYMBOL_GEQ = 16,                       /* GEQ  */
  YYSYMBOL_EQL = 17,                       /* EQL  */
  YYSYMBOL_NEQ = 18,                       /* NEQ  */
  YYSYMBOL_LSS = 19,                       /* LSS  */
  YYSYMBOL_GRE = 20,                       /* GRE  */
  YYSYMBOL_AND = 21,                       /* AND  */
  YYSYMBOL_OR = 22,                        /* OR  */
  YYSYMBOL_NOT = 23,                       /* NOT  */
  YYSYMBOL_PLUS = 24,                      /* PLUS  */
  YYSYMBOL_MINU = 25,                      /* MINU  */
  YYSYMBOL_MULT = 26,                      /* MULT  */
  YYSYMBOL_DIV = 27,                       /* DIV  */
  YYSYMBOL_MOD = 28,                       /* MOD  */
  YYSYMBOL_ASSIGN = 29,                    /* ASSIGN  */
  YYSYMBOL_LPARENT = 30,                   /* LPARENT  */
  YYSYMBOL_RPARENT = 31,                   /* RPARENT  */
  YYSYMBOL_LBRACK = 32,                    /* LBRACK  */
  YYSYMBOL_RBRACK = 33,                    /* RBRACK  */
  YYSYMBOL_LBRACE = 34,                    /* LBRACE  */
  YYSYMBOL_RBRACE = 35,                    /* RBRACE  */
  YYSYMBOL_COMMA = 36,                     /* COMMA  */
  YYSYMBOL_SEMICN = 37,                    /* SEMICN  */
  YYSYMBOL_IDENFR = 38,                    /* IDENFR  */
  YYSYMBOL_INTCON = 39,                    /* INTCON  */
  YYSYMBOL_FLOATCONTK = 40,                /* FLOATCONTK  */
  YYSYMBOL_STRCON = 41,                    /* STRCON  */
  YYSYMBOL_LOWER_THAN_ELSE = 42,           /* LOWER_THAN_ELSE  */
  YYSYMBOL_YYACCEPT = 43,                  /* $accept  */
  YYSYMBOL_CompUnit = 44,                  /* CompUnit  */
  YYSYMBOL_CompUnitItem = 45,              /* CompUnitItem  */
  YYSYMBOL_GlobalIntItem = 46,             /* GlobalIntItem  */
  YYSYMBOL_GlobalVarInitOpt = 47,          /* GlobalVarInitOpt  */
  YYSYMBOL_GlobalVarMore = 48,             /* GlobalVarMore  */
  YYSYMBOL_Decl = 49,                      /* Decl  */
  YYSYMBOL_ConstDecl = 50,                 /* ConstDecl  */
  YYSYMBOL_ConstDefList = 51,              /* ConstDefList  */
  YYSYMBOL_BType = 52,                     /* BType  */
  YYSYMBOL_ConstDef = 53,                  /* ConstDef  */
  YYSYMBOL_ConstDefDims = 54,              /* ConstDefDims  */
  YYSYMBOL_ConstInitVal = 55,              /* ConstInitVal  */
  YYSYMBOL_ConstInitValList = 56,          /* ConstInitValList  */
  YYSYMBOL_VoidFuncDef = 57,               /* VoidFuncDef  */
  YYSYMBOL_VarDecl = 58,                   /* VarDecl  */
  YYSYMBOL_VarDefList = 59,                /* VarDefList  */
  YYSYMBOL_VarDef = 60,                    /* VarDef  */
  YYSYMBOL_VarDefDims = 61,                /* VarDefDims  */
  YYSYMBOL_InitVal = 62,                   /* InitVal  */
  YYSYMBOL_InitValList = 63,               /* InitValList  */
  YYSYMBOL_FuncFParams = 64,               /* FuncFParams  */
  YYSYMBOL_FuncFParamsList = 65,           /* FuncFParamsList  */
  YYSYMBOL_FuncFParam = 66,                /* FuncFParam  */
  YYSYMBOL_FuncFParamSuffix = 67,          /* FuncFParamSuffix  */
  YYSYMBOL_FuncFParamArrayDims = 68,       /* FuncFParamArrayDims  */
  YYSYMBOL_Block = 69,                     /* Block  */
  YYSYMBOL_BlockBegin = 70,                /* BlockBegin  */
  YYSYMBOL_BlockItemList = 71,             /* BlockItemList  */
  YYSYMBOL_Stmt = 72,                      /* Stmt  */
  YYSYMBOL_ExpOpt = 73,                    /* ExpOpt  */
  YYSYMBOL_PrintfArgs = 74,                /* PrintfArgs  */
  YYSYMBOL_Exp = 75,                       /* Exp  */
  YYSYMBOL_Cond = 76,                      /* Cond  */
  YYSYMBOL_LVal = 77,                      /* LVal  */
  YYSYMBOL_LValIndices = 78,               /* LValIndices  */
  YYSYMBOL_PrimaryExp = 79,                /* PrimaryExp  */
  YYSYMBOL_Number = 80,                    /* Number  */
  YYSYMBOL_UnaryExp = 81,                  /* UnaryExp  */
  YYSYMBOL_FuncRParams = 82,               /* FuncRParams  */
  YYSYMBOL_FuncRParamsList = 83,           /* FuncRParamsList  */
  YYSYMBOL_MulExp = 84,                    /* MulExp  */
  YYSYMBOL_AddExp = 85,                    /* AddExp  */
  YYSYMBOL_RelExp = 86,                    /* RelExp  */
  YYSYMBOL_EqExp = 87,                     /* EqExp  */
  YYSYMBOL_LAndExp = 88,                   /* LAndExp  */
  YYSYMBOL_LOrExp = 89,                    /* LOrExp  */
  YYSYMBOL_ConstExp = 90                   /* ConstExp  */
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
#define YYFINAL  15
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   297

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  43
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  48
/* YYNRULES -- Number of rules.  */
#define YYNRULES  111
/* YYNSTATES -- Number of states.  */
#define YYNSTATES  203

/* YYMAXUTOK -- Last valid token kind.  */
#define YYMAXUTOK   297


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
      35,    36,    37,    38,    39,    40,    41,    42
};

#if YYDEBUG
/* YYRLINE[YYN] -- Source line where rule number YYN was defined.  */
static const yytype_int16 yyrline[] =
{
       0,    78,    78,    85,    94,    96,    98,   103,   108,   110,
     123,   124,   130,   134,   142,   143,   147,   152,   158,   166,
     167,   171,   182,   183,   191,   193,   198,   203,   209,   217,
     222,   227,   232,   238,   246,   248,   254,   255,   263,   265,
     270,   275,   281,   289,   293,   299,   307,   313,   314,   320,
     321,   329,   337,   343,   344,   349,   357,   359,   361,   363,
     365,   367,   369,   371,   373,   375,   377,   382,   388,   389,
     394,   400,   408,   412,   416,   422,   423,   431,   432,   433,
     437,   438,   442,   443,   448,   450,   452,   454,   456,   461,
     465,   471,   479,   480,   482,   484,   489,   490,   492,   497,
     498,   500,   502,   504,   509,   510,   512,   517,   518,   523,
     524,   529
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
  "FLOATTK", "VOIDTK", "IFTK", "ELSETK", "WHILETK", "BREAKTK",
  "CONTINUETK", "RETURNTK", "GETINTTK", "PRINTFTK", "LEQ", "GEQ", "EQL",
  "NEQ", "LSS", "GRE", "AND", "OR", "NOT", "PLUS", "MINU", "MULT", "DIV",
  "MOD", "ASSIGN", "LPARENT", "RPARENT", "LBRACK", "RBRACK", "LBRACE",
  "RBRACE", "COMMA", "SEMICN", "IDENFR", "INTCON", "FLOATCONTK", "STRCON",
  "LOWER_THAN_ELSE", "$accept", "CompUnit", "CompUnitItem",
  "GlobalIntItem", "GlobalVarInitOpt", "GlobalVarMore", "Decl",
  "ConstDecl", "ConstDefList", "BType", "ConstDef", "ConstDefDims",
  "ConstInitVal", "ConstInitValList", "VoidFuncDef", "VarDecl",
  "VarDefList", "VarDef", "VarDefDims", "InitVal", "InitValList",
  "FuncFParams", "FuncFParamsList", "FuncFParam", "FuncFParamSuffix",
  "FuncFParamArrayDims", "Block", "BlockBegin", "BlockItemList", "Stmt",
  "ExpOpt", "PrintfArgs", "Exp", "Cond", "LVal", "LValIndices",
  "PrimaryExp", "Number", "UnaryExp", "FuncRParams", "FuncRParamsList",
  "MulExp", "AddExp", "RelExp", "EqExp", "LAndExp", "LOrExp", "ConstExp", YY_NULLPTR
};

static const char *
yysymbol_name (yysymbol_kind_t yysymbol)
{
  return yytname[yysymbol];
}
#endif

#define YYPACT_NINF (-157)

#define yypact_value_is_default(Yyn) \
  ((Yyn) == YYPACT_NINF)

#define YYTABLE_NINF (-1)

#define yytable_value_is_error(Yyn) \
  0

/* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
   STATE-NUM.  */
static const yytype_int16 yypact[] =
{
      60,    79,  -157,  -157,   -26,    26,  -157,  -157,  -157,  -157,
     -24,  -157,  -157,   -18,   -20,  -157,  -157,    -3,    50,  -157,
    -157,    52,  -157,     3,    20,    23,    18,  -157,    49,   -18,
    -157,    12,    54,    59,    58,  -157,    12,    90,    37,   257,
    -157,  -157,  -157,   217,   257,  -157,  -157,  -157,  -157,    93,
      12,    79,  -157,    12,    96,   257,   257,   257,   257,    94,
      97,  -157,  -157,  -157,  -157,  -157,  -157,  -157,  -157,    42,
      72,    72,    98,    62,    53,   199,  -157,  -157,   102,   147,
     103,  -157,  -157,  -157,  -157,    99,  -157,  -157,  -157,   106,
    -157,  -157,    65,   235,   108,   257,   257,   257,   257,   257,
    -157,    18,  -157,    37,  -157,  -157,    68,  -157,   123,   125,
     101,   126,   239,   132,  -157,  -157,    18,  -157,  -157,   127,
    -157,   136,  -157,  -157,  -157,  -157,    37,  -157,  -157,   135,
     131,   257,  -157,  -157,  -157,    42,    42,  -157,  -157,  -157,
     217,   257,   257,  -157,  -157,  -157,   137,   128,  -157,   257,
     141,  -157,  -157,   257,   142,  -157,   145,    72,    21,    88,
     157,   146,   148,  -157,     2,   143,   257,  -157,  -157,   181,
     257,   257,   257,   257,   257,   257,   257,   257,   181,   152,
     257,     8,  -157,   150,   176,    72,    72,    72,    72,    21,
      21,    88,   157,  -157,  -157,  -157,   159,   257,  -157,   181,
    -157,  -157,  -157
};

/* YYDEFACT[STATE-NUM] -- Default reduction number in state STATE-NUM.
   Performed when YYTABLE does not specify something else to do.  Zero
   means the default is an error.  */
static const yytype_int8 yydefact[] =
{
       0,     0,    19,    20,     0,     0,     2,     5,     4,    14,
       0,     6,    15,     0,     0,     1,     3,    36,     0,    32,
      22,     0,    17,     0,     0,    10,     0,    31,     0,     0,
      16,     0,     0,     0,    43,    44,     0,     0,     0,     0,
      12,    36,    33,     0,     0,    18,    52,    29,    53,    47,
       0,     0,     7,     0,     0,     0,     0,     0,     0,     0,
      75,    80,    81,    11,    38,    78,    82,    79,    92,    96,
      72,   111,     0,     0,    34,     0,    21,    24,     0,    68,
       0,    46,    30,    45,     8,     0,    88,    86,    87,     0,
      39,    41,     0,     0,    74,     0,     0,     0,     0,     0,
      37,     0,     9,     0,    25,    27,     0,    23,     0,     0,
       0,     0,     0,     0,    51,    54,     0,    58,    55,     0,
      69,    78,    49,    85,    77,    40,     0,    83,    90,     0,
      89,     0,    93,    94,    95,    97,    98,    13,    35,    26,
       0,     0,     0,    62,    63,    64,     0,     0,    57,     0,
      48,    42,    84,     0,     0,    28,     0,    99,   104,   107,
     109,    73,     0,    65,     0,     0,     0,    91,    76,    68,
       0,     0,     0,     0,     0,     0,     0,     0,    68,     0,
       0,     0,    56,     0,    59,   102,   103,   100,   101,   105,
     106,   108,   110,    61,    66,    70,     0,     0,    50,    68,
      67,    71,    60
};

/* YYPGOTO[NTERM-NUM].  */
static const yytype_int16 yypgoto[] =
{
    -157,  -157,   192,  -157,  -157,  -157,   119,  -157,  -157,     1,
     170,  -157,   -66,  -157,  -157,  -157,  -157,   -22,   160,   -54,
    -157,   178,  -157,   149,  -157,  -157,    -8,  -157,  -157,  -156,
    -157,  -157,   -58,    61,   -76,  -157,  -157,  -157,   -38,  -157,
    -157,    11,   -28,   -59,    31,    32,  -157,     9
};

/* YYDEFGOTO[NTERM-NUM].  */
static const yytype_uint8 yydefgoto[] =
{
       0,     5,     6,     7,    40,    73,     8,     9,    21,    32,
      22,    28,    76,   106,    11,    12,    18,    19,    25,    63,
      92,    33,    34,    35,    81,   150,   117,    48,    79,   118,
     119,   181,    64,   156,    65,    94,    66,    67,    68,   129,
     130,    69,    70,   158,   159,   160,   161,    77
};

/* YYTABLE[YYPACT[STATE-NUM]] -- What to do in state STATE-NUM.  If
   positive, shift that token.  If negative, reduce the rule whose
   number is the opposite.  If YYTABLE_NINF, syntax error.  */
static const yytype_uint8 yytable[] =
{
      89,    10,    13,   121,    42,    91,    10,     2,     3,   105,
      23,    71,    14,   184,    17,    71,    71,    86,    87,    88,
      20,   120,   193,    47,     2,     3,    15,    24,    52,     1,
       2,     3,     4,   179,    31,   128,   170,   171,   180,   196,
     172,   173,    82,   202,   197,    84,    46,    71,    72,   138,
      54,    36,    38,    78,   146,    39,    41,   132,   133,   134,
      55,    56,    57,     1,     2,     3,     4,    58,    95,    96,
      97,    59,   151,   154,   155,    60,    61,    62,    43,   137,
     116,    44,   103,     2,     3,    39,    26,    27,    29,    30,
      50,   165,    49,   121,    51,   167,    98,    99,   101,   102,
     125,   126,   121,   139,   140,   174,   175,    54,   183,   135,
     136,   120,    71,   157,   157,   189,   190,    55,    56,    57,
     120,    53,   195,   121,    58,    80,    85,    93,    59,    90,
     123,   100,    60,    61,    62,   107,   122,   124,   143,   201,
     131,   120,   185,   186,   187,   188,   157,   157,   157,   157,
       1,     2,     3,   141,   108,   142,   109,   110,   111,   112,
      54,   113,   147,   144,   148,   149,   152,   153,   177,   164,
      55,    56,    57,   166,   163,   168,   169,    58,   176,   178,
     182,    46,   114,   198,   199,    60,    61,    62,   108,   194,
     109,   110,   111,   112,    54,   113,   200,    16,   115,    45,
      83,    74,    37,   162,    55,    56,    57,   191,     0,   192,
       0,    58,    54,     0,     0,    46,     0,     0,     0,    60,
      61,    62,    55,    56,    57,     0,     0,     0,     0,    58,
      54,     0,     0,    75,   104,     0,     0,    60,    61,    62,
      55,    56,    57,     0,     0,     0,     0,    58,    54,     0,
       0,    75,    54,     0,     0,    60,    61,    62,    55,    56,
      57,     0,    55,    56,    57,    58,   127,     0,     0,    58,
      54,     0,     0,    60,    61,    62,   145,    60,    61,    62,
      55,    56,    57,     0,     0,     0,     0,    58,     0,     0,
       0,     0,     0,     0,     0,    60,    61,    62
};

static const yytype_int16 yycheck[] =
{
      58,     0,     1,    79,    26,    59,     5,     4,     5,    75,
      30,    39,    38,   169,    38,    43,    44,    55,    56,    57,
      38,    79,   178,    31,     4,     5,     0,    30,    36,     3,
       4,     5,     6,    31,    31,    93,    15,    16,    36,    31,
      19,    20,    50,   199,    36,    53,    34,    75,    39,   103,
      13,    31,    29,    44,   112,    32,    38,    95,    96,    97,
      23,    24,    25,     3,     4,     5,     6,    30,    26,    27,
      28,    34,   126,   131,   140,    38,    39,    40,    29,   101,
      79,    32,    29,     4,     5,    32,    36,    37,    36,    37,
      31,   149,    38,   169,    36,   153,    24,    25,    36,    37,
      35,    36,   178,    35,    36,    17,    18,    13,   166,    98,
      99,   169,   140,   141,   142,   174,   175,    23,    24,    25,
     178,    31,   180,   199,    30,    32,    30,    30,    34,    35,
      31,    33,    38,    39,    40,    33,    33,    31,    37,   197,
      32,   199,   170,   171,   172,   173,   174,   175,   176,   177,
       3,     4,     5,    30,     7,    30,     9,    10,    11,    12,
      13,    14,    30,    37,    37,    29,    31,    36,    22,    41,
      23,    24,    25,    32,    37,    33,    31,    30,    21,    31,
      37,    34,    35,    33,     8,    38,    39,    40,     7,    37,
       9,    10,    11,    12,    13,    14,    37,     5,    79,    29,
      51,    41,    24,   142,    23,    24,    25,   176,    -1,   177,
      -1,    30,    13,    -1,    -1,    34,    -1,    -1,    -1,    38,
      39,    40,    23,    24,    25,    -1,    -1,    -1,    -1,    30,
      13,    -1,    -1,    34,    35,    -1,    -1,    38,    39,    40,
      23,    24,    25,    -1,    -1,    -1,    -1,    30,    13,    -1,
      -1,    34,    13,    -1,    -1,    38,    39,    40,    23,    24,
      25,    -1,    23,    24,    25,    30,    31,    -1,    -1,    30,
      13,    -1,    -1,    38,    39,    40,    37,    38,    39,    40,
      23,    24,    25,    -1,    -1,    -1,    -1,    30,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    38,    39,    40
};

/* YYSTOS[STATE-NUM] -- The symbol kind of the accessing symbol of
   state STATE-NUM.  */
static const yytype_int8 yystos[] =
{
       0,     3,     4,     5,     6,    44,    45,    46,    49,    50,
      52,    57,    58,    52,    38,     0,    45,    38,    59,    60,
      38,    51,    53,    30,    30,    61,    36,    37,    54,    36,
      37,    31,    52,    64,    65,    66,    31,    64,    29,    32,
      47,    38,    60,    29,    32,    53,    34,    69,    70,    38,
      31,    36,    69,    31,    13,    23,    24,    25,    30,    34,
      38,    39,    40,    62,    75,    77,    79,    80,    81,    84,
      85,    85,    90,    48,    61,    34,    55,    90,    90,    71,
      32,    67,    69,    66,    69,    30,    81,    81,    81,    75,
      35,    62,    63,    30,    78,    26,    27,    28,    24,    25,
      33,    36,    37,    29,    35,    55,    56,    33,     7,     9,
      10,    11,    12,    14,    35,    49,    52,    69,    72,    73,
      75,    77,    33,    31,    31,    35,    36,    31,    75,    82,
      83,    32,    81,    81,    81,    84,    84,    60,    62,    35,
      36,    30,    30,    37,    37,    37,    75,    30,    37,    29,
      68,    62,    31,    36,    75,    55,    76,    85,    86,    87,
      88,    89,    76,    37,    41,    75,    32,    75,    33,    31,
      15,    16,    19,    20,    17,    18,    21,    22,    31,    31,
      36,    74,    37,    75,    72,    85,    85,    85,    85,    86,
      86,    87,    88,    72,    37,    75,    31,    36,    33,     8,
      37,    75,    72
};

/* YYR1[RULE-NUM] -- Symbol kind of the left-hand side of rule RULE-NUM.  */
static const yytype_int8 yyr1[] =
{
       0,    43,    44,    44,    45,    45,    45,    46,    46,    46,
      47,    47,    48,    48,    49,    49,    50,    51,    51,    52,
      52,    53,    54,    54,    55,    55,    55,    56,    56,    57,
      57,    58,    59,    59,    60,    60,    61,    61,    62,    62,
      62,    63,    63,    64,    65,    65,    66,    67,    67,    68,
      68,    69,    70,    71,    71,    71,    72,    72,    72,    72,
      72,    72,    72,    72,    72,    72,    72,    72,    73,    73,
      74,    74,    75,    76,    77,    78,    78,    79,    79,    79,
      80,    80,    81,    81,    81,    81,    81,    81,    81,    82,
      83,    83,    84,    84,    84,    84,    85,    85,    85,    86,
      86,    86,    86,    86,    87,    87,    87,    88,    88,    89,
      89,    90
};

/* YYR2[RULE-NUM] -- Number of symbols on the right-hand side of rule RULE-NUM.  */
static const yytype_int8 yyr2[] =
{
       0,     2,     1,     2,     1,     1,     1,     5,     6,     6,
       0,     2,     0,     3,     1,     1,     4,     1,     3,     1,
       1,     4,     0,     4,     1,     2,     3,     1,     3,     5,
       6,     3,     1,     3,     2,     4,     0,     4,     1,     2,
       3,     1,     3,     1,     1,     3,     3,     0,     3,     0,
       4,     3,     1,     0,     2,     2,     4,     2,     1,     5,
       7,     5,     2,     2,     2,     3,     5,     6,     0,     1,
       2,     3,     1,     1,     2,     0,     4,     3,     1,     1,
       1,     1,     1,     3,     4,     3,     2,     2,     2,     1,
       1,     3,     1,     3,     3,     3,     1,     3,     3,     1,
       3,     3,     3,     3,     1,     3,     3,     1,     3,     1,
       3,     1
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
#line 1328 "parser.tab.c"
    break;

  case 3: /* CompUnit: CompUnit CompUnitItem  */
#line 86 "Parser.y"
        {
            (yyval.top_items) = (yyvsp[-1].top_items);
            top_level_list_push(&(yyval.top_items), (yyvsp[0].top_item));
            g_program = make_program((yyval.top_items));
        }
#line 1338 "parser.tab.c"
    break;

  case 4: /* CompUnitItem: Decl  */
#line 95 "Parser.y"
        { (yyval.top_item) = make_top_decl((yyvsp[0].decl)); }
#line 1344 "parser.tab.c"
    break;

  case 5: /* CompUnitItem: GlobalIntItem  */
#line 97 "Parser.y"
        { (yyval.top_item) = (yyvsp[0].top_item); }
#line 1350 "parser.tab.c"
    break;

  case 6: /* CompUnitItem: VoidFuncDef  */
#line 99 "Parser.y"
        { (yyval.top_item) = (yyvsp[0].top_item); }
#line 1356 "parser.tab.c"
    break;

  case 7: /* GlobalIntItem: BType IDENFR LPARENT RPARENT Block  */
#line 104 "Parser.y"
        {
            ParamList list = {0};
            (yyval.top_item) = make_top_func(make_func((yyvsp[-4].type_spec), (yyvsp[-3].str), list, (yyvsp[0].block)));
        }
#line 1365 "parser.tab.c"
    break;

  case 8: /* GlobalIntItem: BType IDENFR LPARENT FuncFParams RPARENT Block  */
#line 109 "Parser.y"
        { (yyval.top_item) = make_top_func(make_func((yyvsp[-5].type_spec), (yyvsp[-4].str), (yyvsp[-2].params), (yyvsp[0].block))); }
#line 1371 "parser.tab.c"
    break;

  case 9: /* GlobalIntItem: BType IDENFR VarDefDims GlobalVarInitOpt GlobalVarMore SEMICN  */
#line 111 "Parser.y"
        {
            DeclItemList list = {0};
            decl_item_list_push(&list, make_decl_item((yyvsp[-4].str), (yyvsp[-3].int_list), (yyvsp[-2].init)));
            for (int i = 0; i < (yyvsp[-1].decl_items).count; ++i) {
                decl_item_list_push(&list, (yyvsp[-1].decl_items).items[i]);
            }
            (yyval.top_item) = make_top_decl(make_decl((yyvsp[-5].type_spec), false, list));
        }
#line 1384 "parser.tab.c"
    break;

  case 10: /* GlobalVarInitOpt: %empty  */
#line 123 "Parser.y"
        { (yyval.init) = NULL; }
#line 1390 "parser.tab.c"
    break;

  case 11: /* GlobalVarInitOpt: ASSIGN InitVal  */
#line 125 "Parser.y"
        { (yyval.init) = (yyvsp[0].init); }
#line 1396 "parser.tab.c"
    break;

  case 12: /* GlobalVarMore: %empty  */
#line 130 "Parser.y"
        {
            DeclItemList list = {0};
            (yyval.decl_items) = list;
        }
#line 1405 "parser.tab.c"
    break;

  case 13: /* GlobalVarMore: GlobalVarMore COMMA VarDef  */
#line 135 "Parser.y"
        {
            (yyval.decl_items) = (yyvsp[-2].decl_items);
            decl_item_list_push(&(yyval.decl_items), (yyvsp[0].decl_item));
        }
#line 1414 "parser.tab.c"
    break;

  case 14: /* Decl: ConstDecl  */
#line 142 "Parser.y"
                { (yyval.decl) = (yyvsp[0].decl); }
#line 1420 "parser.tab.c"
    break;

  case 15: /* Decl: VarDecl  */
#line 143 "Parser.y"
              { (yyval.decl) = (yyvsp[0].decl); }
#line 1426 "parser.tab.c"
    break;

  case 16: /* ConstDecl: CONSTTK BType ConstDefList SEMICN  */
#line 148 "Parser.y"
        { (yyval.decl) = make_decl((yyvsp[-2].type_spec), true, (yyvsp[-1].decl_items)); }
#line 1432 "parser.tab.c"
    break;

  case 17: /* ConstDefList: ConstDef  */
#line 153 "Parser.y"
        {
            DeclItemList list = {0};
            decl_item_list_push(&list, (yyvsp[0].decl_item));
            (yyval.decl_items) = list;
        }
#line 1442 "parser.tab.c"
    break;

  case 18: /* ConstDefList: ConstDefList COMMA ConstDef  */
#line 159 "Parser.y"
        {
            (yyval.decl_items) = (yyvsp[-2].decl_items);
            decl_item_list_push(&(yyval.decl_items), (yyvsp[0].decl_item));
        }
#line 1451 "parser.tab.c"
    break;

  case 19: /* BType: INTTK  */
#line 166 "Parser.y"
            { (yyval.type_spec) = TYPE_INT; }
#line 1457 "parser.tab.c"
    break;

  case 20: /* BType: FLOATTK  */
#line 167 "Parser.y"
              { (yyval.type_spec) = TYPE_FLOAT; }
#line 1463 "parser.tab.c"
    break;

  case 21: /* ConstDef: IDENFR ConstDefDims ASSIGN ConstInitVal  */
#line 172 "Parser.y"
        {
            (yyval.decl_item) = make_decl_item((yyvsp[-3].str), (yyvsp[-2].int_list), (yyvsp[0].init));
            if ((yyvsp[-2].int_list).count == 0 && (yyvsp[0].init)->is_expr) {
                register_const_binding((yyvsp[-3].str), eval_const_ast_expr((yyvsp[0].init)->expr));
            }
        }
#line 1474 "parser.tab.c"
    break;

  case 22: /* ConstDefDims: %empty  */
#line 182 "Parser.y"
        { IntList list = {0}; (yyval.int_list) = list; }
#line 1480 "parser.tab.c"
    break;

  case 23: /* ConstDefDims: ConstDefDims LBRACK ConstExp RBRACK  */
#line 184 "Parser.y"
        {
            (yyval.int_list) = (yyvsp[-3].int_list);
            int_list_push(&(yyval.int_list), eval_const_ast_expr((yyvsp[-1].expr)));
        }
#line 1489 "parser.tab.c"
    break;

  case 24: /* ConstInitVal: ConstExp  */
#line 192 "Parser.y"
        { (yyval.init) = make_expr_init((yyvsp[0].expr)); }
#line 1495 "parser.tab.c"
    break;

  case 25: /* ConstInitVal: LBRACE RBRACE  */
#line 194 "Parser.y"
        {
            InitValList list = {0};
            (yyval.init) = make_list_init(list);
        }
#line 1504 "parser.tab.c"
    break;

  case 26: /* ConstInitVal: LBRACE ConstInitValList RBRACE  */
#line 199 "Parser.y"
        { (yyval.init) = make_list_init((yyvsp[-1].init_list)); }
#line 1510 "parser.tab.c"
    break;

  case 27: /* ConstInitValList: ConstInitVal  */
#line 204 "Parser.y"
        {
            InitValList list = {0};
            init_list_push(&list, (yyvsp[0].init));
            (yyval.init_list) = list;
        }
#line 1520 "parser.tab.c"
    break;

  case 28: /* ConstInitValList: ConstInitValList COMMA ConstInitVal  */
#line 210 "Parser.y"
        {
            (yyval.init_list) = (yyvsp[-2].init_list);
            init_list_push(&(yyval.init_list), (yyvsp[0].init));
        }
#line 1529 "parser.tab.c"
    break;

  case 29: /* VoidFuncDef: VOIDTK IDENFR LPARENT RPARENT Block  */
#line 218 "Parser.y"
        {
            ParamList list = {0};
            (yyval.top_item) = make_top_func(make_func(TYPE_VOID, (yyvsp[-3].str), list, (yyvsp[0].block)));
        }
#line 1538 "parser.tab.c"
    break;

  case 30: /* VoidFuncDef: VOIDTK IDENFR LPARENT FuncFParams RPARENT Block  */
#line 223 "Parser.y"
        { (yyval.top_item) = make_top_func(make_func(TYPE_VOID, (yyvsp[-4].str), (yyvsp[-2].params), (yyvsp[0].block))); }
#line 1544 "parser.tab.c"
    break;

  case 31: /* VarDecl: BType VarDefList SEMICN  */
#line 228 "Parser.y"
        { (yyval.decl) = make_decl((yyvsp[-2].type_spec), false, (yyvsp[-1].decl_items)); }
#line 1550 "parser.tab.c"
    break;

  case 32: /* VarDefList: VarDef  */
#line 233 "Parser.y"
        {
            DeclItemList list = {0};
            decl_item_list_push(&list, (yyvsp[0].decl_item));
            (yyval.decl_items) = list;
        }
#line 1560 "parser.tab.c"
    break;

  case 33: /* VarDefList: VarDefList COMMA VarDef  */
#line 239 "Parser.y"
        {
            (yyval.decl_items) = (yyvsp[-2].decl_items);
            decl_item_list_push(&(yyval.decl_items), (yyvsp[0].decl_item));
        }
#line 1569 "parser.tab.c"
    break;

  case 34: /* VarDef: IDENFR VarDefDims  */
#line 247 "Parser.y"
        { (yyval.decl_item) = make_decl_item((yyvsp[-1].str), (yyvsp[0].int_list), NULL); }
#line 1575 "parser.tab.c"
    break;

  case 35: /* VarDef: IDENFR VarDefDims ASSIGN InitVal  */
#line 249 "Parser.y"
        { (yyval.decl_item) = make_decl_item((yyvsp[-3].str), (yyvsp[-2].int_list), (yyvsp[0].init)); }
#line 1581 "parser.tab.c"
    break;

  case 36: /* VarDefDims: %empty  */
#line 254 "Parser.y"
        { IntList list = {0}; (yyval.int_list) = list; }
#line 1587 "parser.tab.c"
    break;

  case 37: /* VarDefDims: VarDefDims LBRACK ConstExp RBRACK  */
#line 256 "Parser.y"
        {
            (yyval.int_list) = (yyvsp[-3].int_list);
            int_list_push(&(yyval.int_list), eval_const_ast_expr((yyvsp[-1].expr)));
        }
#line 1596 "parser.tab.c"
    break;

  case 38: /* InitVal: Exp  */
#line 264 "Parser.y"
        { (yyval.init) = make_expr_init((yyvsp[0].expr)); }
#line 1602 "parser.tab.c"
    break;

  case 39: /* InitVal: LBRACE RBRACE  */
#line 266 "Parser.y"
        {
            InitValList list = {0};
            (yyval.init) = make_list_init(list);
        }
#line 1611 "parser.tab.c"
    break;

  case 40: /* InitVal: LBRACE InitValList RBRACE  */
#line 271 "Parser.y"
        { (yyval.init) = make_list_init((yyvsp[-1].init_list)); }
#line 1617 "parser.tab.c"
    break;

  case 41: /* InitValList: InitVal  */
#line 276 "Parser.y"
        {
            InitValList list = {0};
            init_list_push(&list, (yyvsp[0].init));
            (yyval.init_list) = list;
        }
#line 1627 "parser.tab.c"
    break;

  case 42: /* InitValList: InitValList COMMA InitVal  */
#line 282 "Parser.y"
        {
            (yyval.init_list) = (yyvsp[-2].init_list);
            init_list_push(&(yyval.init_list), (yyvsp[0].init));
        }
#line 1636 "parser.tab.c"
    break;

  case 43: /* FuncFParams: FuncFParamsList  */
#line 289 "Parser.y"
                      { (yyval.params) = (yyvsp[0].params); }
#line 1642 "parser.tab.c"
    break;

  case 44: /* FuncFParamsList: FuncFParam  */
#line 294 "Parser.y"
        {
            ParamList list = {0};
            param_list_push(&list, (yyvsp[0].param));
            (yyval.params) = list;
        }
#line 1652 "parser.tab.c"
    break;

  case 45: /* FuncFParamsList: FuncFParamsList COMMA FuncFParam  */
#line 300 "Parser.y"
        {
            (yyval.params) = (yyvsp[-2].params);
            param_list_push(&(yyval.params), (yyvsp[0].param));
        }
#line 1661 "parser.tab.c"
    break;

  case 46: /* FuncFParam: BType IDENFR FuncFParamSuffix  */
#line 308 "Parser.y"
        { (yyval.param) = make_param((yyvsp[-2].type_spec), (yyvsp[-1].str), (yyvsp[0].int_list).count > 0 || (yyvsp[0].int_list).capacity == -1, (yyvsp[0].int_list)); }
#line 1667 "parser.tab.c"
    break;

  case 47: /* FuncFParamSuffix: %empty  */
#line 313 "Parser.y"
        { IntList list = {0}; (yyval.int_list) = list; }
#line 1673 "parser.tab.c"
    break;

  case 48: /* FuncFParamSuffix: LBRACK RBRACK FuncFParamArrayDims  */
#line 315 "Parser.y"
        { (yyval.int_list) = (yyvsp[0].int_list); (yyval.int_list).capacity = -1; }
#line 1679 "parser.tab.c"
    break;

  case 49: /* FuncFParamArrayDims: %empty  */
#line 320 "Parser.y"
        { IntList list = {0}; (yyval.int_list) = list; }
#line 1685 "parser.tab.c"
    break;

  case 50: /* FuncFParamArrayDims: FuncFParamArrayDims LBRACK Exp RBRACK  */
#line 322 "Parser.y"
        {
            (yyval.int_list) = (yyvsp[-3].int_list);
            int_list_push(&(yyval.int_list), eval_const_ast_expr((yyvsp[-1].expr)));
        }
#line 1694 "parser.tab.c"
    break;

  case 51: /* Block: BlockBegin BlockItemList RBRACE  */
#line 330 "Parser.y"
        {
            parse_const_scope_pop();
            (yyval.block) = make_block((yyvsp[-1].block_items));
        }
#line 1703 "parser.tab.c"
    break;

  case 52: /* BlockBegin: LBRACE  */
#line 338 "Parser.y"
        { parse_const_scope_push(); }
#line 1709 "parser.tab.c"
    break;

  case 53: /* BlockItemList: %empty  */
#line 343 "Parser.y"
        { BlockItemList list = {0}; (yyval.block_items) = list; }
#line 1715 "parser.tab.c"
    break;

  case 54: /* BlockItemList: BlockItemList Decl  */
#line 345 "Parser.y"
        {
            (yyval.block_items) = (yyvsp[-1].block_items);
            block_item_list_push(&(yyval.block_items), BLOCK_ITEM_DECL, (yyvsp[0].decl));
        }
#line 1724 "parser.tab.c"
    break;

  case 55: /* BlockItemList: BlockItemList Stmt  */
#line 350 "Parser.y"
        {
            (yyval.block_items) = (yyvsp[-1].block_items);
            block_item_list_push(&(yyval.block_items), BLOCK_ITEM_STMT, (yyvsp[0].stmt));
        }
#line 1733 "parser.tab.c"
    break;

  case 56: /* Stmt: LVal ASSIGN Exp SEMICN  */
#line 358 "Parser.y"
        { (yyval.stmt) = make_assign_stmt((yyvsp[-3].lval), (yyvsp[-1].expr)); }
#line 1739 "parser.tab.c"
    break;

  case 57: /* Stmt: ExpOpt SEMICN  */
#line 360 "Parser.y"
        { (yyval.stmt) = make_expr_stmt((yyvsp[-1].expr)); }
#line 1745 "parser.tab.c"
    break;

  case 58: /* Stmt: Block  */
#line 362 "Parser.y"
        { (yyval.stmt) = make_block_stmt((yyvsp[0].block)); }
#line 1751 "parser.tab.c"
    break;

  case 59: /* Stmt: IFTK LPARENT Cond RPARENT Stmt  */
#line 364 "Parser.y"
        { (yyval.stmt) = make_if_stmt((yyvsp[-2].expr), (yyvsp[0].stmt), NULL); }
#line 1757 "parser.tab.c"
    break;

  case 60: /* Stmt: IFTK LPARENT Cond RPARENT Stmt ELSETK Stmt  */
#line 366 "Parser.y"
        { (yyval.stmt) = make_if_stmt((yyvsp[-4].expr), (yyvsp[-2].stmt), (yyvsp[0].stmt)); }
#line 1763 "parser.tab.c"
    break;

  case 61: /* Stmt: WHILETK LPARENT Cond RPARENT Stmt  */
#line 368 "Parser.y"
        { (yyval.stmt) = make_while_stmt((yyvsp[-2].expr), (yyvsp[0].stmt)); }
#line 1769 "parser.tab.c"
    break;

  case 62: /* Stmt: BREAKTK SEMICN  */
#line 370 "Parser.y"
        { (yyval.stmt) = make_break_stmt(); }
#line 1775 "parser.tab.c"
    break;

  case 63: /* Stmt: CONTINUETK SEMICN  */
#line 372 "Parser.y"
        { (yyval.stmt) = make_continue_stmt(); }
#line 1781 "parser.tab.c"
    break;

  case 64: /* Stmt: RETURNTK SEMICN  */
#line 374 "Parser.y"
        { (yyval.stmt) = make_return_stmt(NULL); }
#line 1787 "parser.tab.c"
    break;

  case 65: /* Stmt: RETURNTK Exp SEMICN  */
#line 376 "Parser.y"
        { (yyval.stmt) = make_return_stmt((yyvsp[-1].expr)); }
#line 1793 "parser.tab.c"
    break;

  case 66: /* Stmt: PRINTFTK LPARENT STRCON RPARENT SEMICN  */
#line 378 "Parser.y"
        {
            ExprList list = {0};
            (yyval.stmt) = make_printf_stmt((yyvsp[-2].str), list);
        }
#line 1802 "parser.tab.c"
    break;

  case 67: /* Stmt: PRINTFTK LPARENT STRCON PrintfArgs RPARENT SEMICN  */
#line 383 "Parser.y"
        { (yyval.stmt) = make_printf_stmt((yyvsp[-3].str), (yyvsp[-2].expr_list)); }
#line 1808 "parser.tab.c"
    break;

  case 68: /* ExpOpt: %empty  */
#line 388 "Parser.y"
        { (yyval.expr) = NULL; }
#line 1814 "parser.tab.c"
    break;

  case 69: /* ExpOpt: Exp  */
#line 390 "Parser.y"
        { (yyval.expr) = (yyvsp[0].expr); }
#line 1820 "parser.tab.c"
    break;

  case 70: /* PrintfArgs: COMMA Exp  */
#line 395 "Parser.y"
        {
            ExprList list = {0};
            expr_list_push(&list, (yyvsp[0].expr));
            (yyval.expr_list) = list;
        }
#line 1830 "parser.tab.c"
    break;

  case 71: /* PrintfArgs: PrintfArgs COMMA Exp  */
#line 401 "Parser.y"
        {
            (yyval.expr_list) = (yyvsp[-2].expr_list);
            expr_list_push(&(yyval.expr_list), (yyvsp[0].expr));
        }
#line 1839 "parser.tab.c"
    break;

  case 72: /* Exp: AddExp  */
#line 408 "Parser.y"
             { (yyval.expr) = (yyvsp[0].expr); }
#line 1845 "parser.tab.c"
    break;

  case 73: /* Cond: LOrExp  */
#line 412 "Parser.y"
             { (yyval.expr) = (yyvsp[0].expr); }
#line 1851 "parser.tab.c"
    break;

  case 74: /* LVal: IDENFR LValIndices  */
#line 417 "Parser.y"
        { (yyval.lval) = make_lval((yyvsp[-1].str), (yyvsp[0].expr_list)); }
#line 1857 "parser.tab.c"
    break;

  case 75: /* LValIndices: %empty  */
#line 422 "Parser.y"
        { ExprList list = {0}; (yyval.expr_list) = list; }
#line 1863 "parser.tab.c"
    break;

  case 76: /* LValIndices: LValIndices LBRACK Exp RBRACK  */
#line 424 "Parser.y"
        {
            (yyval.expr_list) = (yyvsp[-3].expr_list);
            expr_list_push(&(yyval.expr_list), (yyvsp[-1].expr));
        }
#line 1872 "parser.tab.c"
    break;

  case 77: /* PrimaryExp: LPARENT Exp RPARENT  */
#line 431 "Parser.y"
                          { (yyval.expr) = (yyvsp[-1].expr); }
#line 1878 "parser.tab.c"
    break;

  case 78: /* PrimaryExp: LVal  */
#line 432 "Parser.y"
           { (yyval.expr) = make_lval_expr((yyvsp[0].lval)); }
#line 1884 "parser.tab.c"
    break;

  case 79: /* PrimaryExp: Number  */
#line 433 "Parser.y"
             { (yyval.expr) = (yyvsp[0].expr); }
#line 1890 "parser.tab.c"
    break;

  case 80: /* Number: INTCON  */
#line 437 "Parser.y"
             { (yyval.expr) = make_number_expr(parse_int_literal((yyvsp[0].str))); }
#line 1896 "parser.tab.c"
    break;

  case 81: /* Number: FLOATCONTK  */
#line 438 "Parser.y"
                 { (yyval.expr) = make_float_number_expr(parse_int_literal((yyvsp[0].str))); }
#line 1902 "parser.tab.c"
    break;

  case 82: /* UnaryExp: PrimaryExp  */
#line 442 "Parser.y"
                 { (yyval.expr) = (yyvsp[0].expr); }
#line 1908 "parser.tab.c"
    break;

  case 83: /* UnaryExp: IDENFR LPARENT RPARENT  */
#line 444 "Parser.y"
        {
            ExprList list = {0};
            (yyval.expr) = make_call_expr((yyvsp[-2].str), list);
        }
#line 1917 "parser.tab.c"
    break;

  case 84: /* UnaryExp: IDENFR LPARENT FuncRParams RPARENT  */
#line 449 "Parser.y"
        { (yyval.expr) = make_call_expr((yyvsp[-3].str), (yyvsp[-1].expr_list)); }
#line 1923 "parser.tab.c"
    break;

  case 85: /* UnaryExp: GETINTTK LPARENT RPARENT  */
#line 451 "Parser.y"
        { (yyval.expr) = make_getint_expr(); }
#line 1929 "parser.tab.c"
    break;

  case 86: /* UnaryExp: PLUS UnaryExp  */
#line 453 "Parser.y"
        { (yyval.expr) = make_unary_expr(UNARY_PLUS, (yyvsp[0].expr)); }
#line 1935 "parser.tab.c"
    break;

  case 87: /* UnaryExp: MINU UnaryExp  */
#line 455 "Parser.y"
        { (yyval.expr) = make_unary_expr(UNARY_MINUS, (yyvsp[0].expr)); }
#line 1941 "parser.tab.c"
    break;

  case 88: /* UnaryExp: NOT UnaryExp  */
#line 457 "Parser.y"
        { (yyval.expr) = make_unary_expr(UNARY_NOT, (yyvsp[0].expr)); }
#line 1947 "parser.tab.c"
    break;

  case 89: /* FuncRParams: FuncRParamsList  */
#line 461 "Parser.y"
                      { (yyval.expr_list) = (yyvsp[0].expr_list); }
#line 1953 "parser.tab.c"
    break;

  case 90: /* FuncRParamsList: Exp  */
#line 466 "Parser.y"
        {
            ExprList list = {0};
            expr_list_push(&list, (yyvsp[0].expr));
            (yyval.expr_list) = list;
        }
#line 1963 "parser.tab.c"
    break;

  case 91: /* FuncRParamsList: FuncRParamsList COMMA Exp  */
#line 472 "Parser.y"
        {
            (yyval.expr_list) = (yyvsp[-2].expr_list);
            expr_list_push(&(yyval.expr_list), (yyvsp[0].expr));
        }
#line 1972 "parser.tab.c"
    break;

  case 92: /* MulExp: UnaryExp  */
#line 479 "Parser.y"
               { (yyval.expr) = (yyvsp[0].expr); }
#line 1978 "parser.tab.c"
    break;

  case 93: /* MulExp: MulExp MULT UnaryExp  */
#line 481 "Parser.y"
        { (yyval.expr) = make_binary_expr(BIN_MUL, (yyvsp[-2].expr), (yyvsp[0].expr)); }
#line 1984 "parser.tab.c"
    break;

  case 94: /* MulExp: MulExp DIV UnaryExp  */
#line 483 "Parser.y"
        { (yyval.expr) = make_binary_expr(BIN_DIV, (yyvsp[-2].expr), (yyvsp[0].expr)); }
#line 1990 "parser.tab.c"
    break;

  case 95: /* MulExp: MulExp MOD UnaryExp  */
#line 485 "Parser.y"
        { (yyval.expr) = make_binary_expr(BIN_MOD, (yyvsp[-2].expr), (yyvsp[0].expr)); }
#line 1996 "parser.tab.c"
    break;

  case 96: /* AddExp: MulExp  */
#line 489 "Parser.y"
             { (yyval.expr) = (yyvsp[0].expr); }
#line 2002 "parser.tab.c"
    break;

  case 97: /* AddExp: AddExp PLUS MulExp  */
#line 491 "Parser.y"
        { (yyval.expr) = make_binary_expr(BIN_ADD, (yyvsp[-2].expr), (yyvsp[0].expr)); }
#line 2008 "parser.tab.c"
    break;

  case 98: /* AddExp: AddExp MINU MulExp  */
#line 493 "Parser.y"
        { (yyval.expr) = make_binary_expr(BIN_SUB, (yyvsp[-2].expr), (yyvsp[0].expr)); }
#line 2014 "parser.tab.c"
    break;

  case 99: /* RelExp: AddExp  */
#line 497 "Parser.y"
             { (yyval.expr) = (yyvsp[0].expr); }
#line 2020 "parser.tab.c"
    break;

  case 100: /* RelExp: RelExp LSS AddExp  */
#line 499 "Parser.y"
        { (yyval.expr) = make_binary_expr(BIN_LT, (yyvsp[-2].expr), (yyvsp[0].expr)); }
#line 2026 "parser.tab.c"
    break;

  case 101: /* RelExp: RelExp GRE AddExp  */
#line 501 "Parser.y"
        { (yyval.expr) = make_binary_expr(BIN_GT, (yyvsp[-2].expr), (yyvsp[0].expr)); }
#line 2032 "parser.tab.c"
    break;

  case 102: /* RelExp: RelExp LEQ AddExp  */
#line 503 "Parser.y"
        { (yyval.expr) = make_binary_expr(BIN_LE, (yyvsp[-2].expr), (yyvsp[0].expr)); }
#line 2038 "parser.tab.c"
    break;

  case 103: /* RelExp: RelExp GEQ AddExp  */
#line 505 "Parser.y"
        { (yyval.expr) = make_binary_expr(BIN_GE, (yyvsp[-2].expr), (yyvsp[0].expr)); }
#line 2044 "parser.tab.c"
    break;

  case 104: /* EqExp: RelExp  */
#line 509 "Parser.y"
             { (yyval.expr) = (yyvsp[0].expr); }
#line 2050 "parser.tab.c"
    break;

  case 105: /* EqExp: EqExp EQL RelExp  */
#line 511 "Parser.y"
        { (yyval.expr) = make_binary_expr(BIN_EQ, (yyvsp[-2].expr), (yyvsp[0].expr)); }
#line 2056 "parser.tab.c"
    break;

  case 106: /* EqExp: EqExp NEQ RelExp  */
#line 513 "Parser.y"
        { (yyval.expr) = make_binary_expr(BIN_NE, (yyvsp[-2].expr), (yyvsp[0].expr)); }
#line 2062 "parser.tab.c"
    break;

  case 107: /* LAndExp: EqExp  */
#line 517 "Parser.y"
            { (yyval.expr) = (yyvsp[0].expr); }
#line 2068 "parser.tab.c"
    break;

  case 108: /* LAndExp: LAndExp AND EqExp  */
#line 519 "Parser.y"
        { (yyval.expr) = make_binary_expr(BIN_AND, (yyvsp[-2].expr), (yyvsp[0].expr)); }
#line 2074 "parser.tab.c"
    break;

  case 109: /* LOrExp: LAndExp  */
#line 523 "Parser.y"
              { (yyval.expr) = (yyvsp[0].expr); }
#line 2080 "parser.tab.c"
    break;

  case 110: /* LOrExp: LOrExp OR LAndExp  */
#line 525 "Parser.y"
        { (yyval.expr) = make_binary_expr(BIN_OR, (yyvsp[-2].expr), (yyvsp[0].expr)); }
#line 2086 "parser.tab.c"
    break;

  case 111: /* ConstExp: AddExp  */
#line 529 "Parser.y"
             { (yyval.expr) = (yyvsp[0].expr); }
#line 2092 "parser.tab.c"
    break;


#line 2096 "parser.tab.c"

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

#line 532 "Parser.y"


void yyerror(const char *s) {
    fprintf(stderr, "syntax error at line %d: %s\n", yylineno, s);
}
