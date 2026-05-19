\ Test source loading and dictionary persistence.

"fth/std/std.fth" load
6 square
41 inc
3 pred
[ 1 2 3 4 5 ] [ 3 < ] filter
.s empty
