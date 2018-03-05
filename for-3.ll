; Output from Basic Interpreter Session; ModuleID = 'session'
source_filename = "session"

@0 = internal constant [25 x i8] c"entered i, entering j...\00"
@1 = internal constant [28 x i8] c"entered i, j, entering k...\00"
@2 = internal constant [16 x i8] c"... inside k...\00"
@3 = internal constant [18 x i8] c"... out from k...\00"
@4 = internal constant [18 x i8] c"... out from j...\00"
@5 = internal constant [51 x i8] c"out from all blocks, the program is terminating...\00"

define void @main() {
entry:
  %i = alloca i64
  %j = alloca i64
  %k = alloca i64
  store i64 0, i64* %i
  br label %0

exit:                                             ; preds = %8
  ret void

; <label>:0:                                      ; preds = %5, %entry
  %1 = load i64, i64* %i
  %2 = icmp sgt i64 %1, 3
  br i1 %2, label %8, label %3

; <label>:3:                                      ; preds = %0
  %4 = call i32 @puts(i8* getelementptr inbounds ([25 x i8], [25 x i8]* @0, i32 0, i32 0))
  store i64 0, i64* %j
  br label %10

; <label>:5:                                      ; preds = %18
  %6 = load i64, i64* %i
  %7 = add i64 %6, 1
  store i64 %7, i64* %i
  br label %0

; <label>:8:                                      ; preds = %0
  %9 = call i32 @puts(i8* getelementptr inbounds ([51 x i8], [51 x i8]* @5, i32 0, i32 0))
  br label %exit

; <label>:10:                                     ; preds = %15, %3
  %11 = load i64, i64* %j
  %12 = icmp sgt i64 %11, 2
  br i1 %12, label %18, label %13

; <label>:13:                                     ; preds = %10
  %14 = call i32 @puts(i8* getelementptr inbounds ([28 x i8], [28 x i8]* @1, i32 0, i32 0))
  store i64 0, i64* %k
  br label %20

; <label>:15:                                     ; preds = %28
  %16 = load i64, i64* %j
  %17 = add i64 %16, 1
  store i64 %17, i64* %j
  br label %10

; <label>:18:                                     ; preds = %10
  %19 = call i32 @puts(i8* getelementptr inbounds ([18 x i8], [18 x i8]* @4, i32 0, i32 0))
  br label %5

; <label>:20:                                     ; preds = %25, %13
  %21 = load i64, i64* %k
  %22 = icmp sgt i64 %21, 4
  br i1 %22, label %28, label %23

; <label>:23:                                     ; preds = %20
  %24 = call i32 @puts(i8* getelementptr inbounds ([16 x i8], [16 x i8]* @2, i32 0, i32 0))
  br label %25

; <label>:25:                                     ; preds = %23
  %26 = load i64, i64* %k
  %27 = add i64 %26, 1
  store i64 %27, i64* %k
  br label %20

; <label>:28:                                     ; preds = %20
  %29 = call i32 @puts(i8* getelementptr inbounds ([18 x i8], [18 x i8]* @3, i32 0, i32 0))
  br label %15
}

declare i32 @puts(i8*)

