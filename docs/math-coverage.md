# Complete native math interfaces (task 80)

All sixteen C inputs retained in V7 libm are adapted under `libm/v7`. Native
`math.h` additionally supplies the PDP-11 assembly decomposition/remainder
helpers through libc. `gamma` was declared but its implementation was absent
from the pinned V7 distribution. The historical V6 implementation is retained
separately in `reference/supplemental/v6/gamma.s`, with a pinned source URL and
hash in `reference/supplemental/provenance.json`. No V7 reference bytes change.

`libm/v7/gamma.c` translates that source's Hart & Cheney rational approximation,
recurrence and asymptotic coefficients to native C. It returns log(abs(Gamma(x))),
not Gamma(x). The historical global `signgam` is **0 for positive Gamma, 1 for
negative Gamma**, matching the recovered assembly rather than modern ±1 APIs.
Negative nonintegers use reflection; nonpositive integer poles return HUGE with
EDOM, range overflow returns HUGE with ERANGE. IEEE NaNs propagate; positive
infinity is a range error and negative infinity a domain error. Positive small
arguments accumulate logarithms rather than overflowing an intermediate Gamma;
negative arguments reduce before multiplying by pi to retain precision near
poles. The original coefficients and finite-input algorithm remain identifiable.

Large sin/cos reduction now reduces the integral quadrant modulo four before
converting to native int. This removes an undefined out-of-range conversion;
it does not turn the historical reduction into a high-accuracy modern algorithm
for enormous arguments. Bessel order loops widen the native int argument before
negation and adding sixteen, so INT_MIN/INT_MAX do not overflow. Nonfinite Bessel
arguments return NaN/EDOM before recurrence; very high finite orders retain the
source's potentially expensive recurrence. hypot explicitly handles IEEE
infinities and NaNs. Native IEEE special-value adaptations are distinguished
from the unchanged finite V7 polynomial algorithms.

`make test-math-coverage` checks gamma/signgam, poles, reflection, subnormals,
large trigonometric inputs, Bessel order bounds and historical domain results
in private QEMU roots at 16/128 MiB. `make test-libc` retains numeric fixtures
for every function family, and `make test-libc-interfaces` checks all declarations
against the linked archives. This is V7 numerical behavior with documented native
adaptations, not C99 conformance or exhaustive correctly-rounded accuracy.
Installing the headers and archives is task 82; complete application ports keep
their individual roadmap tasks.
