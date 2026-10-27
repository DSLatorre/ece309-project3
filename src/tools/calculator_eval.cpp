// src/tools/calculator_eval.cpp — YOU IMPLEMENT (spec §4.6).
//
// calc_evaluate: evaluate a well-formed token list with two stacks
// (one for numbers, one for operators and '('). See spec §4.6 and the
// worked trace in Appendix C. std::stack or std::vector are both fine.

#include "tools/calculator.h"

#include <vector>

namespace {

using Kind = CalcToken::Kind;

bool is_operator(Kind kind) {
    return kind == Kind::Plus || kind == Kind::Minus ||
           kind == Kind::Times || kind == Kind::Divide;
}

int precedence(Kind kind) {
    if (kind == Kind::Times || kind == Kind::Divide) return 2;
    if (kind == Kind::Plus || kind == Kind::Minus) return 1;
    return 0;
}

bool apply_top(std::vector<double>* values, std::vector<Kind>* ops,
               std::string* error) {
    if (ops->empty() || !is_operator(ops->back())) {
        *error = "missing operator";
        return false;
    }
    if (values->size() < 2) {
        *error = "missing operand";
        return false;
    }

    const Kind op = ops->back();
    ops->pop_back();
    // The right operand is on top of the stack. Pop it first so subtraction
    // and division do not accidentally run backward.
    const double right = values->back();
    values->pop_back();
    const double left = values->back();
    values->pop_back();

    double result = 0.0;
    switch (op) {
        case Kind::Plus:   result = left + right; break;
        case Kind::Minus:  result = left - right; break;
        case Kind::Times:  result = left * right; break;
        case Kind::Divide:
            if (right == 0.0) {
                *error = "division by zero";
                return false;
            }
            result = left / right;
            break;
        default:
            *error = "invalid operator";
            return false;
    }

    values->push_back(result);
    return true;
}

}  // namespace

bool calc_evaluate(const std::vector<CalcToken>& tokens, double* out,
                   std::string* error) {
    error->clear();
    std::vector<double> values;
    std::vector<Kind> ops;
    // Keep track of whether the next token should be an operand.
    bool expect_operand = true;

    for (const CalcToken& token : tokens) {
        if (token.kind == Kind::Number) {
            if (!expect_operand) {
                *error = "expected an operator";
                return false;
            }
            values.push_back(token.value);
            expect_operand = false;
            continue;
        }
        if (token.kind == Kind::LParen) {
            if (!expect_operand) {
                *error = "expected an operator";
                return false;
            }
            ops.push_back(token.kind);
            continue;
        }
        if (token.kind == Kind::RParen) {
            if (expect_operand) {
                *error = "expected an operand";
                return false;
            }
            // Finish only the work inside this pair of parentheses. An outer
            // operator stays on the stack until normal precedence reaches it.
            while (!ops.empty() && ops.back() != Kind::LParen) {
                if (!apply_top(&values, &ops, error)) return false;
            }
            if (ops.empty()) {
                *error = "unmatched ')'";
                return false;
            }
            ops.pop_back();
            continue;
        }
        if (!is_operator(token.kind)) {
            *error = "invalid token";
            return false;
        }
        if (expect_operand) {
            *error = "expected an operand";
            return false;
        }

        // Include equal precedence so 10 - 4 - 3 evaluates left to right.
        while (!ops.empty() && is_operator(ops.back()) &&
               precedence(ops.back()) >= precedence(token.kind)) {
            if (!apply_top(&values, &ops, error)) return false;
        }
        ops.push_back(token.kind);
        expect_operand = true;
    }

    if (expect_operand) {
        *error = "expected an operand";
        return false;
    }

    while (!ops.empty()) {
        if (ops.back() == Kind::LParen) {
            *error = "unmatched '('";
            return false;
        }
        if (!apply_top(&values, &ops, error)) return false;
    }

    if (values.size() != 1) {
        *error = "malformed expression";
        return false;
    }
    *out = values.back();
    return true;
}
