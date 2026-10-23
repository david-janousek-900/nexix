{
	i = 1
	while (i <= NF) {
		if ($i ~ /[0-9]+/) count[$i]++
		else if (length($i) > 3)
			long++
		else
			continue
		i++
	}
	for (j = 0; j < 3; j++) s = s j
	for (;;) break
	for (w in count) print w, count[w] > "/dev/null"
	if (NF == 0) next
}
END { x = -y + +z * (2 - 3) / 4 % 5; x += 1; x -= 2; x *= 3; x /= 4; x %= 5; print ++x, x++, --x, x-- | "cat"; exit }
