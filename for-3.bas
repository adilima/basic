dim i as long, j as long, k as long
for i = 0 to 3
puts "entered i, entering j..."
for j = 0 to 2
puts "entered i, j, entering k..."
for k = 0 to 4
puts "... inside k..."
next k
puts "... out from k..."
next j
puts "... out from j..."
next i
puts "out from all blocks, the program is terminating..."
