#pragma once

#include "common.h"
#include "formula.h"


class Cell : public CellInterface {
public:
    Cell();
    ~Cell();

    void Set(std::string text) override;
    void Clear();

    Value GetValue() const override;
    std::string GetText() const override;

private:
    static const char FORMULA_SYMBOL = '=';
    static const char ESCAPE_CHAR = '\'';

    enum class CellType {
        Empty,
        Text,
        Formula,
        //Date, etc...
    };

    CellType type_ = CellType::Empty;
    std::string data_;
    std::unique_ptr<FormulaInterface> formula_handler_ = nullptr;

    Value FormulaReturnConverter(const FormulaInterface::Value& formula_return) const;

    // struct FormulaReturnConverter{
    //     Value operator()(double formula_result) const;
    //     Value operator()(FormulaError err) const;
    // };
};
