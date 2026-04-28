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

#ifndef YY_YY_Y_TAB_H_INCLUDED
# define YY_YY_Y_TAB_H_INCLUDED
/* Debug traces.  */
#ifndef YYDEBUG
# define YYDEBUG 0
#endif
#if YYDEBUG
extern int yydebug;
#endif

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
    IDENFR = 269,                  /* IDENFR  */
    INTCON = 270,                  /* INTCON  */
    STRCON = 271,                  /* STRCON  */
    LEQ = 272,                     /* LEQ  */
    GEQ = 273,                     /* GEQ  */
    EQL = 274,                     /* EQL  */
    NEQ = 275,                     /* NEQ  */
    LSS = 276,                     /* LSS  */
    GRE = 277,                     /* GRE  */
    AND = 278,                     /* AND  */
    OR = 279,                      /* OR  */
    NOT = 280,                     /* NOT  */
    PLUS = 281,                    /* PLUS  */
    MINU = 282,                    /* MINU  */
    MULT = 283,                    /* MULT  */
    DIV = 284,                     /* DIV  */
    MOD = 285,                     /* MOD  */
    ASSIGN = 286,                  /* ASSIGN  */
    LPARENT = 287,                 /* LPARENT  */
    RPARENT = 288,                 /* RPARENT  */
    LBRACK = 289,                  /* LBRACK  */
    RBRACK = 290,                  /* RBRACK  */
    LBRACE = 291,                  /* LBRACE  */
    RBRACE = 292,                  /* RBRACE  */
    COMMA = 293,                   /* COMMA  */
    SEMICN = 294,                  /* SEMICN  */
    LOWER_THAN_ELSE = 295          /* LOWER_THAN_ELSE  */
  };
  typedef enum yytokentype yytoken_kind_t;
#endif
/* Token kinds.  */
#define YYEMPTY -2
#define YYEOF 0
#define YYerror 256
#define YYUNDEF 257
#define CONSTTK 258
#define INTTK 259
#define VOIDTK 260
#define IFTK 261
#define ELSETK 262
#define WHILETK 263
#define BREAKTK 264
#define CONTINUETK 265
#define RETURNTK 266
#define GETINTTK 267
#define PRINTFTK 268
#define IDENFR 269
#define INTCON 270
#define STRCON 271
#define LEQ 272
#define GEQ 273
#define EQL 274
#define NEQ 275
#define LSS 276
#define GRE 277
#define AND 278
#define OR 279
#define NOT 280
#define PLUS 281
#define MINU 282
#define MULT 283
#define DIV 284
#define MOD 285
#define ASSIGN 286
#define LPARENT 287
#define RPARENT 288
#define LBRACK 289
#define RBRACK 290
#define LBRACE 291
#define RBRACE 292
#define COMMA 293
#define SEMICN 294
#define LOWER_THAN_ELSE 295

/* Value type.  */
#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
typedef int YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define YYSTYPE_IS_DECLARED 1
#endif


extern YYSTYPE yylval;


int yyparse (void);


#endif /* !YY_YY_Y_TAB_H_INCLUDED  */
