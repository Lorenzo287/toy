'small [ len nip 2 < ] def
'swapd [ rot swap ] def

'qsort [
    [ small ]
    [ ]
    [ uncons [ > ] split ]
    [ swapd cons concat ]
    binrec
] def

[ 3 1 4 1 5 2 ] qsort .s empty \ Should print <1> [1 1 2 3 4 5]

