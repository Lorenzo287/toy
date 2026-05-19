"fth/std/std.fth" load

'bench [
    { block count name }
    $name printf ": " print
    time
    0 $count [ $block exec ] times
    drop
    time swap - dup printf " ticks" print cr
] def

\ Redefine stdlib succ to avoid name collision for comparison
'std_succ [ 1 + ] def

[ 1 + ] 1000000 "Native (inline 1 +)" bench
[ succ ] 1000000 "Native (C-primitive succ)" bench
[ std_succ ] 1000000 "Stdlib (Forth-defined)" bench
