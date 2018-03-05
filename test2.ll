; Output from Basic Interpreter Session; ModuleID = 'session'
source_filename = "session"

@0 = internal constant [20 x i8] c"n is greater than j\00"
@1 = internal constant [42 x i8] c"Holaaa... j comparison with 9000 invoked!\00"
@2 = internal constant [27 x i8] c"condition j > 1000 invoked\00"
@3 = internal constant [24 x i8] c"This is the Else branch\00"
@4 = internal constant [28 x i8] c"We are outside the If block\00"

define void @main() {
entry:
  %n = alloca i32
  %j = alloca i32
  store i32 30000, i32* %n
  store i32 9000, i32* %j
  %0 = load i32, i32* %n
  %1 = load i32, i32* %j
  %2 = icmp sgt i32 %0, %1
  br i1 %2, label %3, label %8

exit:                                             ; preds = %17
  ret void

; <label>:3:                                      ; preds = %entry
  %4 = call i32 @puts(i8* getelementptr inbounds ([20 x i8], [20 x i8]* @0, i32 0, i32 0))
  %5 = load i32, i32* %j
  %6 = sext i32 %5 to i64
  %7 = icmp eq i64 %6, 9000
  br i1 %7, label %8, label %13

; <label>:8:                                      ; preds = %3, %entry
  %9 = call i32 @puts(i8* getelementptr inbounds ([42 x i8], [42 x i8]* @1, i32 0, i32 0))
  %10 = load i32, i32* %j
  %11 = sext i32 %10 to i64
  %12 = icmp sgt i64 %11, 1000
  br i1 %12, label %13, label %15

; <label>:13:                                     ; preds = %8, %3
  %14 = call i32 @puts(i8* getelementptr inbounds ([27 x i8], [27 x i8]* @2, i32 0, i32 0))
  br label %15

; <label>:15:                                     ; preds = %13, %8
  %16 = call i32 @puts(i8* getelementptr inbounds ([24 x i8], [24 x i8]* @3, i32 0, i32 0))
  br label %17

; <label>:17:                                     ; preds = %15
  %18 = call i32 @puts(i8* getelementptr inbounds ([28 x i8], [28 x i8]* @4, i32 0, i32 0))
  br label %exit
}

declare i32 @puts(i8*)

