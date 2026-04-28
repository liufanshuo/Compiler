/* A Bison parser, made by GNU Bison 3.8.2.  */

/* Bison interface for Yacc-like parsers in C

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

/* DO NOT RELY ON FEATURES THAT ARE NOT DOCUMENTED in the manual,
   especially those whose name start with YY_ or yy_.  They are
   private implementation details that can be changed or removed.  */

#ifndef YY_YY_PARSER_TAB_H_INCLUDED
# define YY_YY_PARSER_TAB_H_INCLUDED
/* Debug traces.  */
#ifndef YYDEBUG
# define YYDEBUG 0
#endif
#if YYDEBUG
extern int yydebug;
#endif
/* "%code requires" blocks.  */
#line 13 "Parser.y"

#include "compiler.h"

#line 53 "parser.tab.h"

/* Token kinds.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
  enum yytokentype
  {
    YYEMPTY = -2,
    YYEOF = 0,                     /* "end of file"  */
    YYerror = 256,                 /* error  */
    YYUNDEF = 257,                 /* "invalid token"  */
    CONSTTK = 258,                 /* CONSTTK  */
    INTTK = 259,                   /* INTTK  */
    VOIDTK = 260,                  /* VOIDTK  */
    IFTK = 261,                    /* IFTK  */
    ELSETK = 262,                  /* ELSETK  */
    WHILETK = 263,                 /* WHILETK  */
    BREAKTK = 264,                 /* BREAKTK  */
    CONTINUETK = 265,              /* CONTINUETK  */
    RETURNTK = 266,                /* RETURNTK  */
    GETINTTK = 267,                /* GETINTTK  */
    PRINTFTK = 268,                /* PRINTFTK  */
    LEQ = 269,                     /* LEQ  */
    GEQ = 270,                     /* GEQ  */
    EQL = 271,                     /* EQL  */
    NEQ = 272,                     /* NEQ  */
    LSS = 273,                     /* LSS  */
    GRE = 274,                     /* GRE  */
    AND = 275,                     /* AND  */
    OR = 276,                      /* OR  */
    NOT = 277,                     /* NOT  */
    PLUS = 278,                    /* PLUS  */
    MINU = 279,                    /* MINU  */
    MULT = 280,                    /* MULT  */
    DIV = 281,                     /* DIV  */
    MOD = 282,                     /* MOD  */
    ASSIGN = 283,                  /* ASSIGN  */
    LPARENT = 284,                 /* LPARENT  */
    RPARENT = 285,                 /* RPARENT  */
    LBRACK = 286,                  /* LBRACK  */
    RBRACK = 287,                  /* RBRACK  */
    LBRACE = 288,                  /* LBRACE  */
    RBRACE = 289,                  /* RBRACE  */
    COMMA = 290,                   /* COMMA  */
    SEMICN = 291,                  /* SEMICN  */
    IDENFR = 292,                  /* IDENFR  */
    INTCON = 293,                  /* INTCON  */
    STRCON = 294,                  /* STRCON  */
    LOWER_THAN_ELSE = 295          /* LOWER_THAN_ELSE  */
  };
  typedef enum yytokentype yytoken_kind_t;
#endif

/* Value type.  */
#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
union YYSTYPE
{
#line 17 "Parser.y"

    char *str;
    int intval;
    IntList int_list;
    Expr *expr;
    ExprList expr_list;
    InitVal *init;
    InitValList init_list;
    LVal *lval;
    DeclItem *decl_item;
    DeclItemList decl_items;
    Decl *decl;
    Param *param;
    ParamList params;
    Stmt *stmt;
    Block *block;
    BlockItemList block_items;
    FuncDef *func;
    TopLevelItem *top_item;
    TopLevelList top_items;
    TypeSpec type_spec;

#line 133 "parser.tab.h"

};
typedef union YYSTYPE YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define YYSTYPE_IS_DECLARED 1
#endif


extern YYSTYPE yylval;


int yyparse (void);


#endif /* !YY_YY_PARSER_TAB_H_INCLUDED  */
