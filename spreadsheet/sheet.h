#pragma once

#include "cell.h"
#include "common.h"

#include <stack>
#include <functional>

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

    CellPtr& GetCellPtr(Position pos);
    const CellPtr& GetCellPtr(Position pos) const;

    void IncreasePrintArea(Position pos);
    void ResizeIndex(Position pos);
    void UpdCellInRowCount(Position pos, int count = 1);
    bool HasCell(Position pos) const;
    int FindLastNonEmptyRow() const;
};
