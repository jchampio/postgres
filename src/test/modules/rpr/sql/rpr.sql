-- Singleton sets.
\! rpr_prefer a
\! rpr_prefer $
\! rpr_prefer ^
\! rpr_prefer "()"

-- Question mark quantifier (greedy and reluctant) and its verbose equivalent.
\! rpr_prefer a?
\! rpr_prefer a{0,1}
\! rpr_prefer a??
\! rpr_prefer a{0,1}?

-- Parenthesization.
\! rpr_prefer "(())"
\! rpr_prefer "(a?)"

-- Concatenation.
\! rpr_prefer "a b"
\! rpr_prefer "a b?"
\! rpr_prefer "a b c"
\! rpr_prefer "a b c?"
\! rpr_prefer "a b? c"
\! rpr_prefer "a b? c?"
\! rpr_prefer "(a b)"
