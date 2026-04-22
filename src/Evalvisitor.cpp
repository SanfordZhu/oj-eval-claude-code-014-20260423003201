#include "Evalvisitor.h"
#include "Python3Parser.h"
#include <any>
#include <string>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <stdexcept>

using namespace antlr4;

// Helper methods implementation
std::any EvalVisitor::getVariable(const std::string& name) {
    auto it = variables.find(name);
    if (it != variables.end()) {
        return it->second;
    }
    // Variable not found, return None
    return nullptr;
}

void EvalVisitor::setVariable(const std::string& name, const std::any& value) {
    variables[name] = value;
}

std::string EvalVisitor::anyToString(const std::any& value) {
    if (!value.has_value()) {
        return "None";
    }

    if (value.type() == typeid(std::string)) {
        return std::any_cast<std::string>(value);
    } else if (value.type() == typeid(bool)) {
        return std::any_cast<bool>(value) ? "True" : "False";
    } else if (value.type() == typeid(double)) {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(6) << std::any_cast<double>(value);
        std::string s = oss.str();
        // Remove trailing zeros
        s.erase(s.find_last_not_of('0') + 1, std::string::npos);
        if (s.back() == '.') {
            s.push_back('0');
        }
        return s;
    } else if (value.type() == typeid(std::nullptr_t)) {
        return "None";
    } else {
        // Assume it's a big integer string
        try {
            return std::any_cast<std::string>(value);
        } catch (...) {
            return "Unknown";
        }
    }
}

bool EvalVisitor::anyToBool(const std::any& value) {
    if (!value.has_value()) {
        return false;
    }

    if (value.type() == typeid(bool)) {
        return std::any_cast<bool>(value);
    } else if (value.type() == typeid(double)) {
        return std::any_cast<double>(value) != 0.0;
    } else if (value.type() == typeid(std::string)) {
        std::string s = std::any_cast<std::string>(value);
        // For integers stored as strings
        if (!s.empty() && s != "0") {
            return true;
        }
        // For regular strings
        return !s.empty();
    } else if (value.type() == typeid(std::nullptr_t)) {
        return false;
    }
    return false;
}

double EvalVisitor::anyToDouble(const std::any& value) {
    if (!value.has_value()) {
        return 0.0;
    }

    if (value.type() == typeid(double)) {
        return std::any_cast<double>(value);
    } else if (value.type() == typeid(bool)) {
        return std::any_cast<bool>(value) ? 1.0 : 0.0;
    } else if (value.type() == typeid(std::string)) {
        try {
            return std::stod(std::any_cast<std::string>(value));
        } catch (...) {
            return 0.0;
        }
    }
    return 0.0;
}

std::string EvalVisitor::anyToIntString(const std::any& value) {
    if (!value.has_value()) {
        return "0";
    }

    if (value.type() == typeid(std::string)) {
        return std::any_cast<std::string>(value);
    } else if (value.type() == typeid(double)) {
        double d = std::any_cast<double>(value);
        std::stringstream ss;
        ss << static_cast<long long>(d);
        return ss.str();
    } else if (value.type() == typeid(bool)) {
        return std::any_cast<bool>(value) ? "1" : "0";
    }
    return "0";
}

// Visitor methods implementation
std::any EvalVisitor::visitFile_input(Python3Parser::File_inputContext *ctx) {
    return visitChildren(ctx);
}

std::any EvalVisitor::visitStmt(Python3Parser::StmtContext *ctx) {
    return visitChildren(ctx);
}

std::any EvalVisitor::visitSimple_stmt(Python3Parser::Simple_stmtContext *ctx) {
    return visitChildren(ctx);
}

std::any EvalVisitor::visitSmall_stmt(Python3Parser::Small_stmtContext *ctx) {
    return visitChildren(ctx);
}

std::any EvalVisitor::visitExpr_stmt(Python3Parser::Expr_stmtContext *ctx) {
    // Handle assignment: testlist ('=' testlist)*
    auto testlists = ctx->testlist();
    if (testlists.size() > 1) {
        // Multiple assignments: a = b = c = value
        std::any value = visit(testlists.back());
        for (size_t i = 0; i < testlists.size() - 1; i++) {
            // For now, handle simple variable assignment
            // In a full implementation, we'd need to handle tuple unpacking
            auto testlist = testlists[i];
            // Simple case: single variable
            auto tests = testlist->test();
            if (tests.size() == 1) {
                auto test = tests[0];
                auto not_tests = test->or_test()->and_test(0)->not_test();
                if (!not_tests.empty()) {
                    auto atom = not_tests[0]->comparison()->arith_expr(0)->term(0)->factor(0)->atom_expr()->atom();
                    if (atom->NAME()) {
                        std::string varName = atom->NAME()->getText();
                        setVariable(varName, value);
                    }
                }
            }
        }
        return value;
    } else if (ctx->augassign()) {
        // Augmented assignment: testlist augassign testlist
        auto left = visit(testlists[0]);
        auto right = visit(testlists[1]);
        std::string op = ctx->augassign()->getText();

        // For now, handle simple variable case
        auto tests = testlists[0]->test();
        if (tests.size() == 1) {
            auto test = tests[0];
            auto not_tests = test->or_test()->and_test(0)->not_test();
            if (!not_tests.empty()) {
                auto atom = not_tests[0]->comparison()->arith_expr(0)->term(0)->factor(0)->atom_expr()->atom();
                if (atom->NAME()) {
                    std::string varName = atom->NAME()->getText();

                    // Perform operation
                    if (op == "+=") {
                        // Handle based on types
                        if (left.type() == typeid(double) || right.type() == typeid(double)) {
                            double result = anyToDouble(left) + anyToDouble(right);
                            setVariable(varName, result);
                            return result;
                        } else {
                            // Big integer addition
                            std::string leftStr = anyToIntString(left);
                            std::string rightStr = anyToIntString(right);
                            // Simple implementation - in real implementation would use big integer library
                            try {
                                long long l = std::stoll(leftStr);
                                long long r = std::stoll(rightStr);
                                std::string result = std::to_string(l + r);
                                setVariable(varName, result);
                                return result;
                            } catch (...) {
                                // Fallback for very large numbers
                                setVariable(varName, leftStr + "+" + rightStr);
                                return leftStr + "+" + rightStr;
                            }
                        }
                    }
                    // Add other operators as needed
                }
            }
        }
    }

    // Single expression, return its value
    return visitChildren(ctx);
}

std::any EvalVisitor::visitAugassign(Python3Parser::AugassignContext *ctx) {
    return visitChildren(ctx);
}

std::any EvalVisitor::visitFlow_stmt(Python3Parser::Flow_stmtContext *ctx) {
    return visitChildren(ctx);
}

std::any EvalVisitor::visitBreak_stmt(Python3Parser::Break_stmtContext *ctx) {
    return visitChildren(ctx);
}

std::any EvalVisitor::visitContinue_stmt(Python3Parser::Continue_stmtContext *ctx) {
    return visitChildren(ctx);
}

std::any EvalVisitor::visitReturn_stmt(Python3Parser::Return_stmtContext *ctx) {
    return visitChildren(ctx);
}

std::any EvalVisitor::visitCompound_stmt(Python3Parser::Compound_stmtContext *ctx) {
    return visitChildren(ctx);
}

std::any EvalVisitor::visitIf_stmt(Python3Parser::If_stmtContext *ctx) {
    return visitChildren(ctx);
}

std::any EvalVisitor::visitWhile_stmt(Python3Parser::While_stmtContext *ctx) {
    return visitChildren(ctx);
}

std::any EvalVisitor::visitSuite(Python3Parser::SuiteContext *ctx) {
    return visitChildren(ctx);
}

std::any EvalVisitor::visitTest(Python3Parser::TestContext *ctx) {
    return visitChildren(ctx);
}

std::any EvalVisitor::visitOr_test(Python3Parser::Or_testContext *ctx) {
    auto and_tests = ctx->and_test();
    std::any result = visit(and_tests[0]);

    // Short-circuit evaluation
    if (anyToBool(result)) {
        return result;
    }

    for (size_t i = 1; i < and_tests.size(); i++) {
        result = visit(and_tests[i]);
        if (anyToBool(result)) {
            return result;
        }
    }

    return result;
}

std::any EvalVisitor::visitAnd_test(Python3Parser::And_testContext *ctx) {
    auto not_tests = ctx->not_test();
    std::any result = visit(not_tests[0]);

    // Short-circuit evaluation
    if (!anyToBool(result)) {
        return result;
    }

    for (size_t i = 1; i < not_tests.size(); i++) {
        result = visit(not_tests[i]);
        if (!anyToBool(result)) {
            return result;
        }
    }

    return result;
}

std::any EvalVisitor::visitNot_test(Python3Parser::Not_testContext *ctx) {
    if (ctx->NOT()) {
        std::any child = visit(ctx->not_test());
        return !anyToBool(child);
    }
    return visit(ctx->comparison());
}

std::any EvalVisitor::visitComparison(Python3Parser::ComparisonContext *ctx) {
    auto arith_exprs = ctx->arith_expr();
    if (arith_exprs.size() == 1) {
        return visit(arith_exprs[0]);
    }

    // Handle comparisons
    std::any left = visit(arith_exprs[0]);
    for (size_t i = 0; i < ctx->comp_op().size(); i++) {
        std::any right = visit(arith_exprs[i + 1]);
        std::string op = ctx->comp_op(i)->getText();

        // For simplicity, compare as doubles
        double leftVal = anyToDouble(left);
        double rightVal = anyToDouble(right);

        if (op == "<") {
            return leftVal < rightVal;
        } else if (op == ">") {
            return leftVal > rightVal;
        } else if (op == "<=") {
            return leftVal <= rightVal;
        } else if (op == ">=") {
            return leftVal >= rightVal;
        } else if (op == "==") {
            // Simple equality check
            if (left.type() == typeid(bool) && right.type() == typeid(bool)) {
                return std::any_cast<bool>(left) == std::any_cast<bool>(right);
            } else if (left.type() == typeid(double) && right.type() == typeid(double)) {
                return std::any_cast<double>(left) == std::any_cast<double>(right);
            } else {
                // Compare as strings
                return anyToString(left) == anyToString(right);
            }
        } else if (op == "!=") {
            if (left.type() == typeid(bool) && right.type() == typeid(bool)) {
                return std::any_cast<bool>(left) != std::any_cast<bool>(right);
            } else if (left.type() == typeid(double) && right.type() == typeid(double)) {
                return std::any_cast<double>(left) != std::any_cast<double>(right);
            } else {
                return anyToString(left) != anyToString(right);
            }
        }

        left = right;
    }

    return false;
}

std::any EvalVisitor::visitComp_op(Python3Parser::Comp_opContext *ctx) {
    return visitChildren(ctx);
}

std::any EvalVisitor::visitArith_expr(Python3Parser::Arith_exprContext *ctx) {
    auto terms = ctx->term();
    if (terms.size() == 1) {
        return visit(terms[0]);
    }

    std::any result = visit(terms[0]);
    for (size_t i = 0; i < ctx->addorsub_op().size(); i++) {
        std::any right = visit(terms[i + 1]);
        std::string op = ctx->addorsub_op(i)->getText();

        if (op == "+") {
            if (result.type() == typeid(double) || right.type() == typeid(double)) {
                result = anyToDouble(result) + anyToDouble(right);
            } else if (result.type() == typeid(std::string) || right.type() == typeid(std::string)) {
                result = anyToString(result) + anyToString(right);
            } else {
                // Integer addition
                std::string leftStr = anyToIntString(result);
                std::string rightStr = anyToIntString(right);
                try {
                    long long l = std::stoll(leftStr);
                    long long r = std::stoll(rightStr);
                    result = std::to_string(l + r);
                } catch (...) {
                    result = leftStr + "+" + rightStr;
                }
            }
        } else if (op == "-") {
            if (result.type() == typeid(double) || right.type() == typeid(double)) {
                result = anyToDouble(result) - anyToDouble(right);
            } else {
                std::string leftStr = anyToIntString(result);
                std::string rightStr = anyToIntString(right);
                try {
                    long long l = std::stoll(leftStr);
                    long long r = std::stoll(rightStr);
                    result = std::to_string(l - r);
                } catch (...) {
                    result = leftStr + "-" + rightStr;
                }
            }
        }
    }

    return result;
}

std::any EvalVisitor::visitAddorsub_op(Python3Parser::Addorsub_opContext *ctx) {
    return visitChildren(ctx);
}

std::any EvalVisitor::visitTerm(Python3Parser::TermContext *ctx) {
    auto factors = ctx->factor();
    if (factors.size() == 1) {
        return visit(factors[0]);
    }

    std::any result = visit(factors[0]);
    for (size_t i = 0; i < ctx->muldivmod_op().size(); i++) {
        std::any right = visit(factors[i + 1]);
        std::string op = ctx->muldivmod_op(i)->getText();

        if (op == "*") {
            if (result.type() == typeid(double) || right.type() == typeid(double)) {
                result = anyToDouble(result) * anyToDouble(right);
            } else {
                std::string leftStr = anyToIntString(result);
                std::string rightStr = anyToIntString(right);
                try {
                    long long l = std::stoll(leftStr);
                    long long r = std::stoll(rightStr);
                    result = std::to_string(l * r);
                } catch (...) {
                    result = leftStr + "*" + rightStr;
                }
            }
        } else if (op == "/") {
            double leftVal = anyToDouble(result);
            double rightVal = anyToDouble(right);
            if (rightVal == 0.0) {
                // Division by zero
                result = 0.0;
            } else {
                result = leftVal / rightVal;
            }
        } else if (op == "//") {
            // Integer division
            double leftVal = anyToDouble(result);
            double rightVal = anyToDouble(right);
            if (rightVal == 0.0) {
                result = 0.0;
            } else {
                result = std::floor(leftVal / rightVal);
            }
        } else if (op == "%") {
            // Modulo
            double leftVal = anyToDouble(result);
            double rightVal = anyToDouble(right);
            if (rightVal == 0.0) {
                result = 0.0;
            } else {
                result = std::fmod(leftVal, rightVal);
            }
        }
    }

    return result;
}

std::any EvalVisitor::visitMuldivmod_op(Python3Parser::Muldivmod_opContext *ctx) {
    return visitChildren(ctx);
}

std::any EvalVisitor::visitFactor(Python3Parser::FactorContext *ctx) {
    if (ctx->ADD() || (ctx->children.size() > 0 && ctx->children[0]->getText() == "-")) {
        std::any child = visit(ctx->factor());
        std::string op = ctx->ADD() ? ctx->ADD()->getText() : "-";

        if (op == "+") {
            return child;
        } else if (op == "-") {
            if (child.type() == typeid(double)) {
                return -std::any_cast<double>(child);
            } else {
                std::string val = anyToIntString(child);
                try {
                    long long n = std::stoll(val);
                    return std::to_string(-n);
                } catch (...) {
                    return "-" + val;
                }
            }
        }
    }

    return visit(ctx->atom_expr());
}

std::any EvalVisitor::visitAtom_expr(Python3Parser::Atom_exprContext *ctx) {
    std::any atomValue = visit(ctx->atom());

    if (ctx->trailer()) {
        // Function call
        auto trailer = ctx->trailer();
        if (trailer->OPEN_PAREN()) {
            std::string funcName;
            if (atomValue.type() == typeid(std::string)) {
                funcName = std::any_cast<std::string>(atomValue);
            }

            // Handle built-in functions
            if (funcName == "__builtin_print") {
                if (trailer->arglist()) {
                    auto args = trailer->arglist()->argument();
                    std::string output;
                    for (size_t i = 0; i < args.size(); i++) {
                        if (i > 0) output += " ";
                        output += anyToString(visit(args[i]));
                    }
                    std::cout << output << std::endl;
                } else {
                    std::cout << std::endl;
                }
                return nullptr;
            }
            // Add other built-in functions as needed
        }
    }

    return atomValue;
}

std::any EvalVisitor::visitAtom(Python3Parser::AtomContext *ctx) {
    if (ctx->NAME()) {
        std::string name = ctx->NAME()->getText();
        // Check if it's a built-in function
        if (name == "print") {
            // Return a special marker for built-in functions
            return std::string("__builtin_print");
        }
        // Otherwise, return variable value
        return getVariable(name);
    } else if (ctx->NUMBER()) {
        std::string numStr = ctx->NUMBER()->getText();
        // Check if it's a float
        if (numStr.find('.') != std::string::npos) {
            try {
                return std::stod(numStr);
            } catch (...) {
                return 0.0;
            }
        } else {
            // Integer - store as string for arbitrary precision
            return numStr;
        }
    } else if (!ctx->STRING().empty()) {
        // Handle strings
        std::string str = ctx->getText();
        // Remove quotes
        if (str.size() >= 2 && (str[0] == '"' || str[0] == '\'')) {
            str = str.substr(1, str.size() - 2);
        }
        return str;
    } else if (ctx->TRUE()) {
        return true;
    } else if (ctx->FALSE()) {
        return false;
    } else if (ctx->NONE()) {
        return nullptr;
    } else if (ctx->OPEN_PAREN()) {
        // Parenthesized expression
        return visit(ctx->test());
    }

    return nullptr;
}

std::any EvalVisitor::visitTestlist(Python3Parser::TestlistContext *ctx) {
    auto tests = ctx->test();
    if (tests.size() == 1) {
        return visit(tests[0]);
    }

    // Tuple - for now return first element
    return visit(tests[0]);
}

std::any EvalVisitor::visitTrailer(Python3Parser::TrailerContext *ctx) {
    return visitChildren(ctx);
}

std::any EvalVisitor::visitArglist(Python3Parser::ArglistContext *ctx) {
    return visitChildren(ctx);
}

std::any EvalVisitor::visitArgument(Python3Parser::ArgumentContext *ctx) {
    return visitChildren(ctx);
}