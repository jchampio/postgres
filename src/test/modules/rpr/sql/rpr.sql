-- Singleton sets.
\! rpr_prefer a
\! rpr_prefer $
\! rpr_prefer ^
\! rpr_prefer "()"

-- Question mark quantifier (greedy and reluctant) and its verbose equivalent.
\! rpr_prefer a?
\! rpr_prefer a{0,1}
\! rpr_prefer a{,1}
\! rpr_prefer a??
\! rpr_prefer a{0,1}?
\! rpr_prefer a{,1}?

-- Bounded quantifiers.
\! rpr_prefer a{2}
\! rpr_prefer a{2,2}
\! rpr_prefer a{2,2}?
\! rpr_prefer a{1,3}
\! rpr_prefer a{1,3}?
\! rpr_prefer "^{3}"

\! rpr_prefer "(a|b){1,3}"
\! rpr_prefer "(a|b){1,3}?"

-- Empty matches do not appear in the factor's PL except at the very end or in
-- the positions prior to the quantifier's lower bound.
\! rpr_prefer "^{3,10}"
\! rpr_prefer "(a?){1,3}"
\! rpr_prefer "(a?){1,3}?"
\! rpr_prefer "(a?){2,3}"
\! rpr_prefer "(a?){2,3}?"
\! rpr_prefer "(a??){2,3}"
\! rpr_prefer "(a??){2,3}?"

-- Unbounded quantifiers, with a --max-rows setting.
\! rpr_prefer -m 3 "a*"
\! rpr_prefer -m 3 "a{0,}"
\! rpr_prefer -m 3 "a{,}"
\! rpr_prefer -m 3 "a*?"
\! rpr_prefer -m 3 "a{0,}?"
\! rpr_prefer -m 3 "a{,}?"

\! rpr_prefer -m 3 "a+"
\! rpr_prefer -m 3 "a{1,}"
\! rpr_prefer -m 3 "a+?"
\! rpr_prefer -m 3 "a{1,}?"

-- Parenthesization.
\! rpr_prefer "(())"
\! rpr_prefer "(a?)"

-- Exclusion.
\! rpr_prefer "{- a -}"
\! rpr_prefer "{- a b -}"
\! rpr_prefer "a {- a b -} b"

-- Concatenation.
\! rpr_prefer "a b"
\! rpr_prefer "a b?"
\! rpr_prefer "a b c"
\! rpr_prefer "a b c?"
\! rpr_prefer "a b? c"
\! rpr_prefer "a b? c?"
\! rpr_prefer "(a b)"

-- Alternation.
\! rpr_prefer "a|b"
\! rpr_prefer "a?|b?"
\! rpr_prefer "a??|b?"
\! rpr_prefer "a|b|c"
\! rpr_prefer "a|(b|c)"
\! rpr_prefer "a|(b|c)?"
\! rpr_prefer "(a|b)(c|d)"
