# field sums and a range pattern
BEGIN	{ FS = ":"; OFS = "-" }
NR == 1, /^end/	{ n++ }
$3 > 10 && $1 !~ /^#/	{ total += $3; print $1, $3 }
END	{ printf "%d %g\n", n, total }
