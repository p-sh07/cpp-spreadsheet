#include "cell.h"

#include <cassert>
#include <iostream>
#include <string>
#include <optional>

Cell::Cell() {}

Cell::~Cell() {}

void Cell::Set(std::string text) {
    if(text.empty()) {
        Clear();
        return;
    }
    data_ = text;

    if(data_[0] == FORMULA_SYMBOL && data_.size() > 1) {
        type_ = CellType::Formula;
        formula_handler_ = ParseFormula(data_.substr(1));
    } else {
        type_ = CellType::Text;
    }
}

void Cell::Clear() {
    data_.clear();
    type_ = CellType::Empty;
    formula_handler_ = nullptr;
}

Cell::Value Cell::GetValue() const {
    switch(type_) {
    case CellType::Empty:
        return "";
        break;

    case CellType::Text:
        return data_[0] == ESCAPE_CHAR
        ? data_.substr(1)
        : data_;
        break;

    case CellType::Formula:
        if(!formula_handler_) {
            throw std::runtime_error("null formula_handler ptr for a formula cell");
        }
        return FormulaReturnConverter(formula_handler_->Evaluate());
        break;

    default:
        throw std::runtime_error("unknown Cell type");
        break;
    }

}

std::string Cell::GetText() const {

    if(type_ == CellType::Formula && formula_handler_) {
        if(!formula_handler_) {
            throw std::runtime_error("null formula_handler ptr for a formula cell");
        }
        return "=" + formula_handler_->GetExpression();
    }

    return data_;
}

Cell::Value Cell::FormulaReturnConverter(const FormulaInterface::Value& formula_return) const {
    if(std::holds_alternative<double>(formula_return)) {
        return std::get<double>(formula_return);
    } else {
        return std::get<FormulaError>(formula_return);
    }
}

// Cell::Value Cell::FormulaReturnConverter::operator()(double formula_result) const {
//     return {formula_result};
// }

// Cell::Value Cell::FormulaReturnConverter::operator()(FormulaError err) const {
//     return {err};
// }
