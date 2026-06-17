# Combinator and Control Examples

Toy is most useful when deferred code is treated as a value. A callable is
either an atomic quoted symbol such as `'upper` or a compound quotation such as
`[ upper ]`. The words below are the ones whose purpose is easiest to miss if
you read them like ordinary function calls.

Ordinary words consume their declared inputs. Predicate callables used by
control and predicate combinators are the exception: they run in a stack
sandbox, read one boolean result, and restore the surrounding data stack.

## Preserving and Branching Values

Use `dip` when one value should wait while a callable works on the stack below
it.

```toy
1 2 10 [ + ] dip    \ leaves 3 10
```

Use `keep` when a callable should consume a value but the original value is
still needed afterward.

```toy
10 [ 1 + ] keep     \ leaves 10 11
```

Use `bi` when two different callables should inspect the same value and leave
both results.

```toy
10 [ 1 + ] [ 2 * ] bi   \ leaves 11 20
```

Use `app2` when the same callable should be applied independently to two
values.

```toy
3 4 [ square ] app2     \ leaves 9 16
```

Use `cleave` for several independent projections of one value, and `construct`
when those projections should be collected into a vector.

```toy
5 [ [ 1 + ] [ square ] [ 2 * ] ] cleave
\ leaves 6 25 10

5 [ [ 1 + ] [ square ] [ 2 * ] ] construct
\ leaves [6 25 10]
```

## Temporary Stacks

`infra` runs a callable with a vector as a temporary stack, then returns the
temporary stack as a vector. This is useful when a vector is better handled as a
small stack program.

```toy
[ 2 3 4 ] [ + * ] infra    \ leaves [14]
```

The surrounding stack is restored before the result vector is pushed.

## Sequence Combinators

Vectors, lists, and strings share sequence words where the result type is clear.
Strings are byte sequences, so a string item is a one-byte string.

```toy
[ 1 2 3 4 ] 'square map           \ leaves [1 4 9 16]
( 1 2 3 4 ) 'square map           \ leaves (1 4 9 16)
"toy" 'upper map                  \ leaves "TOY"

[ 1 2 3 4 5 ] [ 2 % 0 == ] filter \ leaves [2 4]
"toy" [ "o" != ] filter           \ leaves "ty"

0 [ 1 2 3 4 ] [ + ] fold          \ leaves 10
"" "abc" [ concat ] each          \ leaves "abc"
```

`split` has two related forms. With a predicate callable it partitions a vector,
list, or string into matching and non-matching sequences. With two strings it
splits a string by a separator.

```toy
3 [ 1 2 3 4 ] [ > ] split         \ leaves 3 [1 2] [3 4]
"a,b,c" "," split                 \ leaves ["a" "b" "c"]
"abc" [ "b" == ] split            \ leaves "b" "ac"
```

`merge` combines two already sorted sequences. The predicate callable receives
the next left item and the next right item; true takes from the left sequence.

```toy
[ 1 3 5 ] [ 2 4 6 ] [ < ] merge   \ leaves [1 2 3 4 5 6]
"bd" "ac" [ < ] merge             \ leaves "abcd"
```

## Repetition

Use `times` for repeated effects on the current stack.

```toy
0 3 [ 1 + ] times                 \ leaves 3
```

Use `replicate` when each run should start from the same surrounding stack and
produce exactly one collected result.

```toy
3 [ "x" ] replicate               \ leaves ["x" "x" "x"]
```

## Control

`if`, `ifelse`, `while`, and `cond` accept predicate callables. Predicate
callables observe the current stack without consuming it.

```toy
5 [ 0 > ] [ "positive" ] if        \ leaves 5 "positive"

5 [
    [ [ 0 < ] [ "negative" ] ]
    [ [ 0 == ] [ "zero" ] ]
    [ [ true ] [ "positive" ] ]
] cond                             \ leaves 5 "positive"
```

`try` restores the stack that existed after `body` and `handler` were consumed
before running the handler.

```toy
10 [ "bad input" error ] [ 0 ] try \ leaves 10 0
```

## Recursion Schemes

`linrec` expresses linear recursion: test, base case, pre-recursive step, and
post-recursive step.

```toy
'fact [
    [ 1 <= ]        \ done?
    [ ]             \ base case leaves n
    [ dup pred ]    \ keep n, recurse on n - 1
    [ * ]           \ combine n and fact(n - 1)
    linrec
] def

5 fact              \ leaves 120
```

`binrec` is useful when the recursive step produces two subproblems, as in
quicksort.

```toy
'qsort [
    [ len 2 < ]
    [ ]
    [ uncons [ > ] split ]
    [ swapd cons concat ]
    binrec
] def

[ 3 1 4 1 5 2 ] qsort
\ leaves [1 1 2 3 4 5]
```

`genrec` is the general form: test, base case, before recursion, after
recursion. Reach for it when `linrec` or `binrec` no longer match the shape.

`treerec` maps a tree-shaped vector recursively. The leaf callable receives
each non-vector leaf; the node callable receives each transformed child vector.

```toy
[ 1 [ 2 3 ] 4 ] [ ] [ 0 swap [ + ] fold ] treerec
\ leaves 10
```

## Words Worth Keeping an Eye On

`app2` overlaps with `bi`, but it is still useful when both values need the same
callable. `cleave` and `construct` also overlap at the implementation level,
but they serve different stack shapes: `cleave` leaves multiple outputs, while
`construct` packages those outputs as data.
