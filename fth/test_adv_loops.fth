[ 10 20 30 ] {list} 

\ EACH

$list dup len swap

"\n{" printf 
[ 
	printf
	1 - 
	[ 0 != ] [ "-"printf ] if
] each 
"}\n" .

drop

\ TIMES

"{" printf 
0
$list len
[
	$list over geth printf
	1 +
	[ $list len != ] [ "-"printf ] if
] times
"}\n" .

drop

\ WHILE

"{" printf 
0 
[ $list len != ]
[
	$list over geth printf
	1 +
	[ $list len != ] [ "-"printf ] if
] while
"}\n" .

drop
