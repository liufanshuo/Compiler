source_filename = "output.ll"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target triple = "x86_64-pc-linux-gnu"

@.fmt.int = private unnamed_addr constant [3 x i8] c"%d\00"

declare i32 @printf(i8*, ...)
declare i32 @scanf(i8*, ...)
declare i32 @getchar()
declare i32 @putchar(i32)
declare i32 @getarray(i32*)
declare i8* @memset(i8*, i32, i64)
declare void @putarray(i32, i32*)


define dso_local i32 @main() {
entry:
  call i32 @putchar(i32 48)
  call i32 @putchar(i32 10)
  ret i32 0
}

