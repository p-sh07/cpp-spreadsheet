#include "formula.h"

#include "FormulaAST.h"

#include <algorithm>
#include <cassert>
#include <cctype>
#include <sstream>
#include <tuple>

using namespace std::literals;

std::ostream& operator<<(std::ostream& output, FormulaError fe) {
    return output << fe.ToString();
}

FormulaError::FormulaError(Category category)
    : category_(category) {
}

FormulaError::Category FormulaError::GetCategory() const {
    return category_;
}

bool FormulaError::operator==(FormulaError rhs) const {
    return category_ == rhs.GetCategory();
}

std::string_view FormulaError::ToString() const {
    switch (category_) {
    case Category::Arithmetic:
        return ARITHM_ERR_MSG;
        break;

    case Category::Ref:
        return REF_ERR_MSG;
        break;

    case Category::Value:
        return VALUE_ERR_MSG;
        break;

    default:
        return "";
        break;
    }
}

namespace {
class Formula : public FormulaInterface {
public:
    explicit Formula(std::string expression) try
        : ast_(ParseFormulaAST(std::move(expression))) {
    } catch (const std::exception& ex) {
        //unable to parse
        throw FormulaException("Unable to parse Fomula");
    }

    Value Evaluate(const SheetInterface& sheet) const override {
        try {
            return ast_.Execute(sheet);
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

    std::vector<Position> GetReferencedCells() const override {
        const auto& ref_cells_list = ast_.GetReferencedCells();

        //The list from AST is already sorted
        std::vector<Position> ref_cells(ref_cells_list.begin(), ref_cells_list.end());
        auto end_of_unique = std::unique(ref_cells.begin(), ref_cells.end());

        ref_cells.resize(end_of_unique - ref_cells.begin());
        return ref_cells;
    }

private:
    FormulaAST ast_;
};
}  // namespace

std::unique_ptr<FormulaInterface> ParseFormula(std::string expression) {
    return std::make_unique<Formula>(std::move(expression));
}
