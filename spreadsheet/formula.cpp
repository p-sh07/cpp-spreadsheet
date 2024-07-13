#include "formula.h"

#include "FormulaAST.h"

#include <algorithm>
#include <cassert>
#include <cctype>
#include <sstream>

using namespace std::literals;

std::ostream& operator<<(std::ostream& output, FormulaError fe) {
    return output << "#ARITHM!";
}

/* Constructor example
 * class B {
public:
    // Обратите внимание на этот конструктор: он использует список инициализации.
    // Но в сам список мы передаём не готовый объект класса А, а создаём его
    // на месте. Конструктор класса А может выкинуть исключение. Обрабатывает её прямо
    // в списке инициализации (try catch).
    B(args) try
        : a_(A(args)) {
    } catch (const std::exception& exc) {
        // обрабатываем исключение
    }
private:
    А а_;
}; */

namespace {
class Formula : public FormulaInterface {
public:
// Реализуйте следующие методы:
    explicit Formula(std::string expression) try
        : ast_(ParseFormulaAST(std::move(expression))) {
    } catch (const std::exception& ex) {
        //unable to parse
        throw FormulaException("Unable to parse Fomula");
    }

    Value Evaluate() const override {
        try {
            return ast_.Execute();
        } catch(FormulaError& fr) {
            return fr;
        }
    }

    std::string GetExpression() const override{
        std::stringstream ss;
        try {
            ast_.PrintFormula(ss);
        } catch (const std::exception& ex) {
            return "";
        }

        return ss.str();
    }

private:
    FormulaAST ast_;
};
}  // namespace

std::unique_ptr<FormulaInterface> ParseFormula(std::string expression) {
    return std::make_unique<Formula>(std::move(expression));
}
