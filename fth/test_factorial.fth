'factorial1 [ [ 0 == ] [ drop 1 ] [ dup 1 - factorial1 * ] ifelse ] def 

[ 5 3 1 ] [ factorial1 ] each .s empty

'factorial2 [ [ 0 == ] [ drop 1 ] [ dup 1 - ] [ * ] linrec ] def

[ 5 3 1 ] [ factorial2 ] each .s empty

'null [ 0 == ] def
'factorial3 [ [ null ] [ succ ] [ dup pred ] [ * ] linrec ] def

[ 5 3 1 ] [ factorial3 ] each .s empty
