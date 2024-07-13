#pragma once

#include "cell.h"
#include "common.h"

#include <stack>
#include <functional>
#pragma once

#include "cell.h"
#include "common.h"

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

    const Cell* GetCellRawPtr(Position pos) const;
    Cell* GetCellRawPtr(Position pos);

private:
    using CellPtr = std::unique_ptr<Cell>;
    using CellRow = std::deque<CellPtr>;

    //index[row][col]
    std::deque<CellRow> cell_index_;
    std::deque<int> num_cells_in_row_;

    Size print_size_;

    //NB! this function will add new Empty cell if no cell at Pos
    CellPtr& GetCellPtrRef(Position pos);
    const CellPtr& GetCellPtrRef(Position pos) const;

    //Пройти по графу зависимых ячеек и сбросить их кэш
    void InvalidateDepCellsCacheDFS(Position start_cell) const;

    void IncreasePrintArea(Position pos);
    void UpdIndexSize(Position pos);

    //По умолчанию увеличивает счетчик непустых ячеек в ряду на 1.
    //Можно передать count = -1 для декремента
    void UpdCellInRowCount(Position pos, int count = 1);
    bool HasCell(Position pos) const;
    int  FindLastNonEmptyRow() const;
};
