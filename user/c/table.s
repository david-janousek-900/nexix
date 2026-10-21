# Native AMD64 code tables for the retained V7 c1.
#
# cvopt input.  As in the supplied table.s, a list label is followed by
# shapes (%degree-type,degree-type) and the template expanded by cexpr.
# Native conventions: AT&T syntax; R/R1 name scratch registers in the
# width of the tree (R.q, R.l, R.w, R.b force a width, R.1/R.2 use the
# memory width of the first/second operand); floating values are doubles
# in xmm registers with the same numbers.  Y escapes are machine sequences
# written by c1 (see c10.c).  '%' starts a shape only at the start of a line.

	.data
	.balign	8
	.globl	regtab
regtab:
	.long	106,0
	.quad	cr106
	.long	30,0
	.quad	cr70
	.long	31,0
	.quad	cr70
	.long	32,0
	.quad	cr32
	.long	33,0
	.quad	cr32
	.long	37,0
	.quad	cr37
	.long	38,0
	.quad	cr38
	.long	80,0
	.quad	cr80
	.long	40,0
	.quad	cr40
	.long	41,0
	.quad	cr40
	.long	47,0
	.quad	cr40
	.long	48,0
	.quad	cr40
	.long	49,0
	.quad	cr40
	.long	42,0
	.quad	cr42
	.long	43,0
	.quad	cr43
	.long	44,0
	.quad	cr43
	.long	14,0
	.quad	cr43
	.long	45,0
	.quad	cr45
	.long	46,0
	.quad	cr45
	.long	17,0
	.quad	cr45
	.long	70,0
	.quad	cr70
	.long	71,0
	.quad	cr70
	.long	77,0
	.quad	cr70
	.long	78,0
	.quad	cr70
	.long	79,0
	.quad	cr70
	.long	72,0
	.quad	cr72
	.long	73,0
	.quad	cr73
	.long	74,0
	.quad	cr73
	.long	75,0
	.quad	cr75
	.long	76,0
	.quad	cr75
	.long	18,0
	.quad	cr75
	.long	102,0
	.quad	cr102
	.long	51,0
	.quad	cr51
	.long	52,0
	.quad	cr51
	.long	56,0
	.quad	cr51
	.long	57,0
	.quad	cr51
	.long	58,0
	.quad	cr51
	.long	59,0
	.quad	cr51
	.long	109,0
	.quad	cr51
	.long	16,0
	.quad	cr16
	.long	93,0
	.quad	cr93
	.long	94,0
	.quad	cr93
	.long	95,0
	.quad	cr93
	.long	96,0
	.quad	cr93
	.long	0,0
	.quad	0

	.globl	efftab
efftab:
	.long	80,0
	.quad	ci80
	.long	0,0
	.quad	0

	.globl	cctab
cctab:
	.long	106,0
	.quad	cc106
	.long	47,0
	.quad	cc47
	.long	60,0
	.quad	cc60
	.long	61,0
	.quad	cc60
	.long	62,0
	.quad	cc60
	.long	63,0
	.quad	cc60
	.long	64,0
	.quad	cc60
	.long	65,0
	.quad	cc60
	.long	66,0
	.quad	cc60
	.long	67,0
	.quad	cc60
	.long	68,0
	.quad	cc60
	.long	69,0
	.quad	cc60
	.long	0,0
	.quad	0

	.globl	sptab
sptab:
	.long	106,0
	.quad	cs106
	.long	0,0
	.quad	0

# load a value
cr106:
%z,n
	xorl	R.l,R.l

%c,n
	Yk

%a,n
%af,n
	Ym	A1,R

%n*,n
%nf*,n
	F*
	Ym	#1(R.q),R

%e,n
	Yk

%n,n
%nf,n
	F

# ++,-- postfix
cr32:
%a,c
	Ym	A1,R
	IB1	A2,A1

%ad,af
	Ym	A1,R
	movapd	R,R1
	IBF	A2,R1
	movsd	R1,A1

%af,af
	Ym	A1,R
	movapd	R,R1
	IBF	A2,R1
	cvtsd2ss	R1,%xmm15
	movss	%xmm15,A1

%n*,c
	F*
	Ym	#1(R.q),R1
	IB1	A2,#1(R.q)
	movq	R1.q,R.q

%nd*,af
	F*
	Ym	#1(R.q),R1
	movapd	R1,R1+
	IBF	A2,R1+
	movsd	R1+,#1(R.q)
	movapd	R1,R

%nf*,af
	F*
	Ym	#1(R.q),R1
	movapd	R1,R1+
	IBF	A2,R1+
	cvtsd2ss	R1+,%xmm15
	movss	%xmm15,#1(R.q)
	movapd	R1,R

# - unary
cr37:
%n,n
	F
	negBF	R

%nf,n
	F
	Yn

# ~
cr38:
%n,n
	F
	notBF	R

# =
cr80:
%a,c
	movB1	A2,A1
	Ym	A1,R

%a,n
	S
	movB1	R.1,A1

%ad,nf
	S
	movsd	R,A1

%af,nf
	S
	cvtsd2ss	R,%xmm15
	movss	%xmm15,A1

%n*,c
	F*
	movB1	A2,#1(R.q)
	Ym	#1(R.q),R

%n*,e
	F*
	S1
	movB1	R1.1,#1(R.q)
	movq	R1.q,R.q

%nd*,ef
	F*
	S1
	movsd	R1,#1(R.q)
	movapd	R1,R

%nf*,ef
	F*
	S1
	cvtsd2ss	R1,%xmm15
	movss	%xmm15,#1(R.q)
	movapd	R1,R

%n*,n
	FS*
	S
	YP
	movB1	R.1,#1(R1.q)

%nd*,nf
	FS*
	S
	YP
	movsd	R,#1(R1.q)

%nf*,nf
	FS*
	S
	YP
	cvtsd2ss	R,%xmm15
	movss	%xmm15,#1(R1.q)

# field assign, value in reg.
cr16:
%a,n
	S
	andB1	Z,A1
	orB1	R.1,A1
	Ym	A1,R

%n*,n
	FS*
	S
	YP
	andB1	Z,#1(R1.q)
	orB1	R.1,#1(R1.q)
	Ym	#1(R1.q),R

# +, -, |, &, ^
cr40:
%n,z
	F

%[add1:]
%n,aw
%nf,ad
	F
	IBF	A2,R

%[add2:]
%n,e
%nf,ef
	F
	S1
	IBF	R1,R

%[add3:]
%n,n
	SS
	F
	YP
	IBF	R1,R

%[add4:]
%nf,nf
	SS
	F
	YQ
	IBF	R1,R

# *
cr42:
%n,aw
%nf,ad
%	[add1]

%n,e
%nf,ef
%	[add2]

%n,n
%	[add3]

%nf,nf
%	[add4]

# /, %, pointer difference
cr43:
%nf,ad
%	[add1]

%nf,ef
%	[add2]

%nf,nf
%	[add4]

%n,e
	F
	S1
	Yd

%n,n
	SS
	F
	YP
	Yd

# >>, <<
cr45:
%n,c
	F
	IBF	A2,R

%n,e
	F
	S1
	Yh

%n,n
	SS
	F
	YP
	Yh

# =+, =-, =|, =&, =^, ++ and -- prefix
cr70:
%[asop1:]
%a,c
	IB1	A2,A1
	Ym	A1,R

%[asop2:]
%a,n
	S
	IB1	R.1,A1
	Ym	A1,R

%[asopd:]
%ad,nf
	S
	movapd	R,R1
	Ym	A1,R
	IBF	R1,R
	movsd	R,A1

%[asopf:]
%af,nf
	S
	movapd	R,R1
	Ym	A1,R
	IBF	R1,R
	cvtsd2ss	R,%xmm15
	movss	%xmm15,A1

%[asop3:]
%n*,c
	F*
	IB1	A2,#1(R.q)
	Ym	#1(R.q),R

%[asop4:]
%n*,e
	F*
	S1
	IB1	R1.1,#1(R.q)
	Ym	#1(R.q),R

%[asop5:]
%n*,n
	FS*
	S
	YP
	IB1	R.1,#1(R1.q)
	Ym	#1(R1.q),R

%[asopnd:]
%nd*,nf
	FS*
	S
	YP
	Ym	#1(R1.q),R1+
	IBF	R,R1+
	movsd	R1+,#1(R1.q)
	movapd	R1+,R

%[asopnf:]
%nf*,nf
	FS*
	S
	YP
	Ym	#1(R1.q),R1+
	IBF	R,R1+
	cvtsd2ss	R1+,%xmm15
	movss	%xmm15,#1(R1.q)
	movapd	R1+,R

# =*
cr72:
%ad,nf
%	[asopd]

%af,nf
%	[asopf]

%nd*,nf
%	[asopnd]

%nf*,nf
%	[asopnf]

%a,n
	S
	movq	R.q,R1.q
	Ym	A1,R
	IBF	R1,R
	movB1	R.1,A1

%n*,n
	FS*
	S
	movq	R.q,R1.q
	Yp
	Ym	#1(R1+.q),R
	IBF	R1,R
	movB1	R.1,#1(R1+.q)

# =/, =%
cr73:
%ad,nf
%	[asopd]

%af,nf
%	[asopf]

%nd*,nf
%	[asopnd]

%nf*,nf
%	[asopnf]

%a,n
	S
	movq	R.q,R1.q
	Ym	A1,R
	Yd
	movB1	R.1,A1

%n*,n
	FS*
	S
	movq	R.q,R1.q
	Yp
	Ym	#1(R1+.q),R
	Yd
	movB1	R.1,#1(R1+.q)

# =>>, =<<
cr75:
%a,c
%	[asop1]

%n*,c
%	[asop3]

%a,n
	S
	movq	R.q,R1.q
	Ym	A1,R
	Yh
	movB1	R.1,A1

%n*,n
	FS*
	S
	movq	R.q,R1.q
	Yp
	Ym	#1(R1+.q),R
	Yh
	movB1	R.1,#1(R1+.q)

# goto
cr102:
%a,n
	jmp	*A1

%n,n
	F
	jmp	*R.q

# conversions
cr51:
%n,n
%nf,n
	F
	Yc

# max, min
cr93:
%n,e
%nf,ef
	F
	S1
	Yx

%n,n
	SS
	F
	YP
	Yx

%nf,nf
	SS
	F
	YQ
	Yx

# c code tables -- compile for side effects.
# =
ci80:
%a,c
	movB1	A2,A1

%a,n
	S
	movB1	R.1,A1

%ad,nf
	S
	movsd	R,A1

%af,nf
	S
	cvtsd2ss	R,%xmm15
	movss	%xmm15,A1

%n*,c
	F*
	movB1	A2,#1(R.q)

%n*,e
	F*
	S1
	movB1	R1.1,#1(R.q)

%nd*,ef
	F*
	S1
	movsd	R1,#1(R.q)

%nf*,ef
	F*
	S1
	cvtsd2ss	R1,%xmm15
	movss	%xmm15,#1(R.q)

%n*,n
	FS*
	S
	YP
	movB1	R.1,#1(R1.q)

%nd*,nf
	FS*
	S
	YP
	movsd	R,#1(R1.q)

%nf*,nf
	FS*
	S
	YP
	cvtsd2ss	R,%xmm15
	movss	%xmm15,#1(R1.q)

# c code tables -- set condition codes
cc106:
%awp,n
	cmpB1	$0,A1

# & as in "if ((a&b) != 0)"
cc47:
%n,c
	F
	testBF	A2,R

%n,e
	F
	S1
	testBF	R1,R

# relationals
cc60:
%awp,z
	cmpB1	$0,A1

%n,z
	F
	testBF	R,R

%n,c
%n,aw
	F
	cmpBF	A2,R

%nf,ad
	F
	ucomisd	A2,R

%n,e
	F
	S1
	cmpBF	R1,R

%nf,ef
	F
	S1
	ucomisd	R1,R

%n,n
	SS
	F
	YP
	cmpBF	R1,R

%nf,nf
	SS
	F
	YQ
	ucomisd	R1,R

# c code tables -- expression to the stack
cs106:
%c,n
%al,n
	Yu
