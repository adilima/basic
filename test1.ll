; Output from Basic Interpreter Session; ModuleID = 'session'
source_filename = "session"

@0 = internal constant [30 x i8] c"dValue is greater than sValue\00"
@1 = internal constant [27 x i8] c"dValue is less than sValue\00"
@2 = internal constant [23 x i8] c"Out of If/End If block\00"

define void @main() {
entry:
  %dValue = alloca double
  %sValue = alloca float
  store double 3.140000e+00, double* %dValue
  store float 0x3FFCA233A0000000, float* %sValue
  %0 = load double, double* %dValue
  %1 = load float, float* %sValue
  %2 = fpext float %1 to double
  %3 = fcmp ogt double %0, %2
  br i1 %3, label %4, label %6

exit:                                             ; preds = %8
  ret void

; <label>:4:                                      ; preds = %entry
  %5 = call i32 @puts(i8* getelementptr inbounds ([30 x i8], [30 x i8]* @0, i32 0, i32 0))
  br label %6

; <label>:6:                                      ; preds = %4, %entry
  %7 = call i32 @puts(i8* getelementptr inbounds ([27 x i8], [27 x i8]* @1, i32 0, i32 0))
  br label %8

; <label>:8:                                      ; preds = %6
  %9 = call i32 @puts(i8* getelementptr inbounds ([23 x i8], [23 x i8]* @2, i32 0, i32 0))
  br label %exit
}

declare i32 @puts(i8*)

