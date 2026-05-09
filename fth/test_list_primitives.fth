\ Test list observer and constructor primitives

[ 1 2 3 ] first nip print  \ Should print 1
[ 1 2 3 ] rest nip print   \ Should print [2 3]

[ 1 2 3 ] uncons swap print print  \ Should print 1, then [2 3]

0 [ 1 2 3 ] cons print       \ Should print [0 1 2 3]
[ 1 2 ] [ 3 4 ] concat print \ Should print [1 2 3 4]

[] empty? nip print     \ Should print true
[ 1 ] empty? nip print  \ Should print false
