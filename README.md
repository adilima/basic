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

```

The second one will fail for sure, and just like before, __i__ will get assigned
with the value of __j__, because there is still no way to tell that we use the operator
for comparison, and not for assignment.


## Parser Rules

Newly defined function_call rules is not yet completed, it can be used to call puts
from within BASIC, but don't use it for getting return values by calling a function
with n = foo(1234) - most likely you will get SIGSEGV now, because I haven't implement
the rule.

