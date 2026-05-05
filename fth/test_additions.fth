"Additions: nip (1 2 -> 2)" .
1 2 nip .

"Additions: tuck (1 2 -> 2 1 2)" .
1 2 tuck .s
empty

"Additions: pick (10 20 30 1 -> duplicate 20)" .
10 20 30 1 pick .s
empty

"Additions: roll (10 20 30 2 -> 20 30 10)" .
10 20 30 2 roll .s
empty

"Additions: empty clears the stack" .
1 2 3 empty .s

"Additions: cr prints a blank line after this:" .
cr

"Additions: page is registered" .
words

"Additions: see prints user definitions" .
: square dup * ;
'square see
'dup see
