/a\/b|c\.d|[^x-z]+$|\tq/ { print substr($0, 2), substr($0, 1, 3), index($0, "\"quoted\\\""), split($2, parts, ","), sprintf("%5.2f", $1) >> "log" }
!($1 == "x") || !/y/ { print exp(1), log(2), sqrt(9), int(3.9) }
($1 < $2) { print "lt" } ($1 <= $2) { print "le" } ($1 != $2) { print "ne" } ($1 >= $2) { print "ge" }
$NF ~ /pat/ { $2 = "new"; print }
