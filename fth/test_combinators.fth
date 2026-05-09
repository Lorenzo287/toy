\ Test quotation combinators

1 2 3 [ + ] dip .s empty      \ Should print <2> 3 3

5 [ 1 + ] keep .s empty       \ Should print <2> 5 6
5 [ drop ] keep .s empty      \ Should print <1> 5

[ 1 2 + ] i print             \ Should print 3
