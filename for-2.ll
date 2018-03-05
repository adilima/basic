; Output from Basic Interpreter Session; ModuleID = 'session'
source_filename = "session"

@0 = internal constant [17 x i8] c"My name is James\00"
@1 = internal constant [18 x i8] c"... and I am Bond\00"
@2 = internal constant [37 x i8] c"Then we are out from the loop blocks\00"

define void @main() {
entry:
  %i = alloca i32
  %j = alloca i32
  store i32 0, i32* %i
  br label %0

exit:                                             ; preds = %8
  ret void

; <label>:0:                                      ; preds = %5, %entry
  %1 = load i32, i32* %i
  %2 = sext i32 %1 to i64
  %3 = icmp sgt i64 %2, 4
  br i1 %3, label %8, label %4

; <label>:4:                                      ; preds = %0
  store i32 0, i32* %j
  br label %10

; <label>:5:                                      ; preds = %19
  %6 = load i32, i32* %i
  %7 = add i32 %6, 1
  store i32 %7, i32* %i
  br label %0

; <label>:8:                                      ; preds = %0
  %9 = call i32 @puts(i8* getelementptr inbounds ([37 x i8], [37 x i8]* @2, i32 0, i32 0))
  br label %exit

; <label>:10:                                     ; preds = %16, %4
  %11 = load i32, i32* %j
  %12 = sext i32 %11 to i64
  %13 = icmp sgt i64 %12, 3
  br i1 %13, label %19, label %14

; <label>:14:                                     ; preds = %10
  %15 = call i32 @puts(i8* getelementptr inbounds ([17 x i8], [17 x i8]* @0, i32 0, i32 0))
  br label %16

; <label>:16:                                     ; preds = %14
  %17 = load i32, i32* %j
  %18 = add i32 %17, 1
  store i32 %18, i32* %j
  br label %10

; <label>:19:                                     ; preds = %10
  %20 = call i32 @puts(i8* getelementptr inbounds ([18 x i8], [18 x i8]* @1, i32 0, i32 0))
  br label %5
}

declare i32 @puts(i8*)

