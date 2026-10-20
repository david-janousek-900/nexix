# Host bootstrap yacc for equation grammars

These sources derive from reference/v7/usr/src/cmd/yacc. They are built only as
a host build helper for the retained eqn/neqn grammars, so no system yacc/bison
package is required. This does not complete the native yacc roadmap tasks.

Native host adaptations: 32-bit lookahead bit sets, variadic diagnostics, an
explicit state-item allocation (no int/pointer overlay), separate optimizer
scratch arrays, and a configurable parser-template location. Parser generation
uses the immutable V7 usr/lib/yaccpar template. The generated equation parser
uses intptr_t semantic values for both register numbers and text pointers.
