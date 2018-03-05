# About

Okay, so far we have reconstruct the whole program, and
now we have better way to distinquish an assignment and 
a comparison for operator __=__.

The operator for assignment and comparison is working now.

```basic

Dim i as Integer, j As Long

i = 90
j = 90

i = j

If i = j Then
   DoSomething
End If

For i = 0 To 3
    puts "inside i..."
    For j = 0 To 6 Step 2
        puts "... inside j..."
    Next j
    puts "... out from j..."
Next i


```


## Parser Rules

Newly defined function_call rules is not yet completed, it can be used to call puts
from within BASIC, but don't use it for getting return values by calling a function
with n = foo(1234) - most likely you will get SIGSEGV now, because I haven't implement
the rule.

