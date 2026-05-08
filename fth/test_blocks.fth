[ 10 20 < ] [ "10 is less than 20\n" printf ] if
[ 30 20 < ] [ "30 is less than 20\n" printf ] [ "30 is not less than 20\n" printf ] ifelse

5 [ [ 0 > ] [ "positive" print ] [ "non-positive" print ] ifelse ] exec .s drop
5 neg [ [ 0 > ] [ "positive" print ] [ "non-positive" print ] ifelse ] exec .s drop
5 [ [ 0 > ] [ drop "positive drop" print ] [ drop "non-positive drop" print ] ifelse ] exec
