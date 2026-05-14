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
    FLOATTK = 260,                 /* FLOATTK  */
    VOIDTK = 261,                  /* VOIDTK  */
    IFTK = 262,                    /* IFTK  */
    ELSETK = 263,                  /* ELSETK  */
    WHILETK = 264,                 /* WHILETK  */
    BREAKTK = 265,                 /* BREAKTK  */
    CONTINUETK = 266,              /* CONTINUETK  */
    RETURNTK = 267,                /* RETURNTK  */
    GETINTTK = 268,                /* GETINTTK  */
    PRINTFTK = 269,                /* PRINTFTK  */
    LEQ = 270,                     /* LEQ  */
    GEQ = 271,                     /* GEQ  */
    EQL = 272,                     /* EQL  */
    NEQ = 273,                     /* NEQ  */
    LSS = 274,                     /* LSS  */
    GRE = 275,                     /* GRE  */
    AND = 276,                     /* AND  */
    OR = 277,                      /* OR  */
    NOT = 278,                     /* NOT  */
    PLUS = 279,                    /* PLUS  */
    MINU = 280,                    /* MINU  */
    MULT = 281,                    /* MULT  */
    DIV = 282,                     /* DIV  */
    MOD = 283,                     /* MOD  */
    ASSIGN = 284,                  /* ASSIGN  */
    LPARENT = 285,                 /* LPARENT  */
    RPARENT = 286,                 /* RPARENT  */
    LBRACK = 287,                  /* LBRACK  */
    RBRACK = 288,                  /* RBRACK  */
    LBRACE = 289,                  /* LBRACE  */
    RBRACE = 290,                  /* RBRACE  */
    COMMA = 291,                   /* COMMA  */
    SEMICN = 292,                  /* SEMICN  */
    IDENFR = 293,                  /* IDENFR  */
    INTCON = 294,                  /* INTCON  */
    FLOATCONTK = 295,              /* FLOATCONTK  */
    STRCON = 296,                  /* STRCON  */
    LOWER_THAN_ELSE = 297          /* LOWER_THAN_ELSE  */
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

#line 135 "parser.tab.h"

};
typedef union YYSTYPE YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define YYSTYPE_IS_DECLARED 1
#endif


extern YYSTYPE yylval;


int yyparse (void);


#endif /* !YY_YY_PARSER_TAB_H_INCLUDED  */
