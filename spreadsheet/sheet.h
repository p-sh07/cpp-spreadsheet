#pragma once

#include "cell.h"
#include "common.h"

#include <stack>
#include <iostream>
#include <type_traits>

class Sheet : public SheetInterface {
public:
    ~Sheet();

    void SetCell(Position pos, std::string text) override;

    const CellInterface* GetCell(Position pos) const override;
    CellInterface* GetCell(Position pos) override;

    void ClearCell(Position pos) override;

    Size GetPrintableSize() const override;

    void PrintValues(std::ostream& output) const override;
    void PrintTexts(std::ostream& output) const override;

private:
    using CellPtr = std::unique_ptr<Cell>;
    using CellRow = std::deque<CellPtr>;

    //index[row][col]
    std::deque<CellRow> cell_index_;
    std::deque<int> num_cells_in_row_;

    Size print_size_;

    Sheet::CellPtr& GetRefOrMakeNewCell(Position pos);

    Cell* GetCellRawPtr(Position pos);
    const Cell* GetCellRawPtr(Position pos) const;

    CellPtr& MakeNewCell(Position pos);

    //Увеличивает размер отображаемой (печатной) области, когда ячейка выходит за пределы текущего размера
    void UpdPrintAreaSize(Position pos);
    void UpdIndexSize(Position pos);

    //По умолчанию увеличивает счетчик непустых ячеек в ряду на 1
    void UpdCellInRowCount(Position pos, int count = 1);

    //Обновляет размеры индекса, счетчик непустых ячеек и PrintArea после стирания ячейки
    void ProcessCellClear(Position pos);

    //Выбросит исключение InvalidPositionException если pos не валиден
    void CheckCellPos(Position pos) const;

    //Проверяет, есть ли в графе ячейна на позиции pos
    bool HasCell(Position pos) const;

    int  FindLastNonEmptyRow() const;

    template<typename OutputValueGetter>
    void OutputAllCells(std::ostream& out, OutputValueGetter out_get) const;
};

namespace {
struct CellValuePrinter {
    std::ostream& out;
    void operator()(std::monostate) {
        out << "";
    }
    void operator()(std::string str) {
        out << str;
    }
    void operator()(double dbl) {
        out << dbl;
    }
    void operator()(FormulaError err) {
        out << err;
    }
};

}//namespace

template<typename OutputValueGetter>
void Sheet::OutputAllCells(std::ostream& out, OutputValueGetter out_get) const {
    for(int row = 0; row < GetPrintableSize().rows; ++row) {
        bool is_first = true;
        for(int col = 0; col < GetPrintableSize().cols; ++col) {
            if(!is_first) {
                out << '\t';
            }
            is_first = false;

            if(HasCell({row, col})) {
                const auto cell_ptr = GetCellRawPtr({row,col});
                if(cell_ptr) {
                    auto cell_get_val = out_get(cell_ptr);
                    if constexpr(std::is_same_v<Cell::Value, std::decay_t<decltype(cell_get_val)>>) {
                        std::visit(CellValuePrinter{out}, cell_get_val);
                    } else { //if not a variant, then it's a string
                        out << cell_get_val;
                    }
                }
            }
        }
        out << '\n';
    }
}
