; Output from Basic Interpreter Session; ModuleID = 'session'
source_filename = "session"

@0 = internal constant [24 x i8] c"This is inside the loop\00"
@1 = internal constant [33 x i8] c"and this is outside the for loop\00"

define void @main() {
entry:
  %i = alloca i32
  store i32 0, i32* %i
  br label %0

exit:                                             ; preds = %9
  ret void

; <label>:0:                                      ; preds = %6, %entry
  %1 = load i32, i32* %i
  %2 = sext i32 %1 to i64
  %3 = icmp sgt i64 %2, 6
  br i1 %3, label %9, label %4

; <label>:4:                                      ; preds = %0
  %5 = call i32 @puts(i8* getelementptr inbounds ([24 x i8], [24 x i8]* @0, i32 0, i32 0))
  br label %6

; <label>:6:                                      ; preds = %4
  %7 = load i32, i32* %i
  %8 = add i32 %7, 1
  store i32 %8, i32* %i
  br label %0

; <label>:9:                                      ; preds = %0
  %10 = call i32 @puts(i8* getelementptr inbounds ([33 x i8], [33 x i8]* @1, i32 0, i32 0))
  br label %exit
}

declare i32 @puts(i8*)

