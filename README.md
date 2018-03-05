# About

Okay, so far we have reconstruct the whole program, and
now we have better way to distinquish an assignment and 
a comparison for operator __=__.

But the next _urgent_ work to do is to rewrite the function
for assigning a string into a variable.

The last successful one is only following LLVM examples,
by creating a GlobalString into a Global Variable in module
address space, either using _internal_ keyword, or simply
use unnamed\_addr default.

But we haven\'t dig quite deep into LLVM to find out if there is
any better way to do it, i.e without having to create a GlobalVariable.

PS:
It is still not right!

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


## Next Homework

Rewrite, or rearrange our ready to use functions from previous version,
that will be the function for IF statements, and also for creating
__Function Declarations__.


