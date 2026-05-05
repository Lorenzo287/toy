'factorial [ [ dup 0 == ] [ drop 1 ] [ dup 1 - factorial * ] ifelse ] def 

[ 5 3 1 ] [ factorial ] each .s
