[ 10 20 30 ] {list} 

\ EACH

$list len swap

"\n{" printf 
[ 
	printf
	1 - 
	[ 0 != ] [ "-"printf ] if
] each 
"}\n" print

drop

\ TIMES

"{" printf 
0
$list len nip
[
	$list over geth nip printf
	1 +
	[ $list len nip != ] [ "-"printf ] if
] times
"}\n" print

drop

\ WHILE

"{" printf 
0 
[ $list len nip != ]
[
	$list over geth nip printf
	1 +
	[ $list len nip != ] [ "-"printf ] if
] while
"}\n" print

drop
