"Additions: nip (1 2 -> 2)" print
1 2 nip .
drop

"Additions: tuck (1 2 -> 2 1 2)" print
1 2 tuck .s
empty

"Additions: pick (10 20 30 1 -> duplicate 20)" print
10 20 30 1 pick .s
empty

"Additions: roll (10 20 30 2 -> 20 30 10)" print
10 20 30 2 roll .s
empty

"Additions: empty clears the stack" print
1 2 3 empty .s

"Additions: cr prints a blank line after this:" print
cr

"Additions: page is registered" print
words

"Additions: see prints user definitions" print
: square dup * ;
'square see
'dup see
