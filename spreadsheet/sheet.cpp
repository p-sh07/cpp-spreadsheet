#include "sheet.h"

#include "cell.h"
#include "common.h"

#include <algorithm>
#include <functional>
#include <iostream>
#include <optional>

using namespace std::literals;

Sheet::~Sheet() {}

void Sheet::SetCell(Position pos, std::string text) {
    //1.Get existing, or make new cell
    auto& cell_ptr = GetRefOrMakeNewCell(pos);

    //2.Set Cell Value (Check for cycle inside the Cell::Set method)
    if(cell_ptr->GetText() != text) { //2.1.Check cell doesn't have same text already
        cell_ptr->Set(pos, text);
    }

    //3.1.If the cell is beyond current print area (max/bottom right cell), increase print area
    UpdPrintAreaSize(pos);

    //3.2.Increment cell in row count (num of non-empty cells contained in row)
    UpdCellInRowCount(pos);
}

const CellInterface* Sheet::GetCell(Position pos) const {
    return GetCellRawPtr(pos);
}

CellInterface* Sheet::GetCell(Position pos) {
    return GetCellRawPtr(pos);
}

void Sheet::ClearCell(Position pos) {
    if(!HasCell(pos)) {
        return;
    }
    //will not create new cell, make sure it exists
    auto cell_ptr = GetCellRawPtr(pos);
    if(cell_ptr) {
        cell_ptr->Clear();

        //Upd index & print_area
        ProcessCellClear(pos);
    }
}

Size Sheet::GetPrintableSize() const {
    return print_size_;
}

//index[row][col]
void Sheet::PrintValues(std::ostream& output) const {
    auto value_getter = [&](const Cell* ptr) {
        return ptr->GetValue();
    };
    OutputAllCells(output, value_getter);
}

void Sheet::PrintTexts(std::ostream& output) const {
    auto text_getter = [&](const Cell* ptr) {
        return ptr->GetText();
    };
    OutputAllCells(output, text_getter);
}

Sheet::CellPtr& Sheet::GetRefOrMakeNewCell(Position pos) {
    CheckCellPos(pos);

    UpdIndexSize(pos);
    auto& cell = cell_index_.at(pos.row).at(pos.col);

    //do not overwrite cells that are not nullptr
    if(cell) {
        return cell;
    }

    //make new empty cell
    cell = std::make_unique<Cell>(*this);
    cell->Set(pos, "");

    return cell;
}

Cell* Sheet::GetCellRawPtr(Position pos) {
    return HasCell(pos) ? cell_index_.at(pos.row).at(pos.col).get()
                        : nullptr;
}

const Cell* Sheet::GetCellRawPtr(Position pos) const {
    return HasCell(pos) ? cell_index_.at(pos.row).at(pos.col).get()
                        : nullptr;
}

//===== Print area and index size helper func ====
void Sheet::UpdPrintAreaSize(Position pos) {
    if(print_size_.rows <= pos.row) {
        print_size_.rows = pos.row + 1;
    }
    if(print_size_.cols <= pos.col) {
        print_size_.cols = pos.col + 1;
    }
}

//resizes index if new cell outside current scope
void Sheet::UpdIndexSize(Position pos) {
    CheckCellPos(pos);

    if(cell_index_.size() <= static_cast<size_t>(pos.row)) {
        cell_index_.resize(pos.row + 1);
    }
    if(cell_index_[pos.row].size() <= static_cast<size_t>(pos.col)) {
        cell_index_[pos.row].resize(pos.col + 1);
    }
}

void Sheet::UpdCellInRowCount(Position pos, int count) {
    //resize and update cell per row count
    if(num_cells_in_row_.size() <= static_cast<size_t>(pos.row)) {
        num_cells_in_row_.resize(pos.row + 1);
    } else if(num_cells_in_row_[pos.row] == 0 && count < 0) {
        throw std::runtime_error("Decrementing row with 0 cells");
    }
    num_cells_in_row_[pos.row] += count;
}

void Sheet::ProcessCellClear(Position pos) {
    //Delete empty row from index
    UpdCellInRowCount(pos, -1);
    if(num_cells_in_row_[pos.row] == 0) {
        cell_index_[pos.row].clear();
    }

    //or, Shrink row to rightmost non-empty cell (at least one non-empty)
    else {
        auto& cell_row = cell_index_[pos.row];
        auto non_empty_cell_it = cell_row.end();

        //iterate backwards through all nullptrs
        do{
            --non_empty_cell_it;
        } while (*non_empty_cell_it == nullptr
                 && non_empty_cell_it != cell_row.begin());

        cell_row.resize(non_empty_cell_it - cell_row.begin() + 1);
    }

    //Shrink print area if cell is at edge
    if(pos.row + 1 == print_size_.rows || pos.col + 1 == print_size_.cols) {
        size_t max_col = 0;
        for(const auto& row : cell_index_) {
            if(max_col < row.size()) {
                max_col = row.size();
            }
        }
        print_size_ = {FindLastNonEmptyRow() + 1, static_cast<int>(max_col)};
    }
}

void Sheet::CheckCellPos(Position pos) const {
    if(!pos.IsValid()) {
        throw InvalidPositionException("Invalid pos passed to Sheet");
    }
}

bool Sheet::HasCell(Position pos) const {
    CheckCellPos(pos);

    //Is within index sizes
    return static_cast<size_t>(pos.row) < cell_index_.size()
           && static_cast<size_t>(pos.col) < cell_index_[pos.row].size();
}

int Sheet::FindLastNonEmptyRow() const {
    if(cell_index_.empty()) {
        return -1;
    }
    auto last_non_empty = std::prev(cell_index_.end());
    while(last_non_empty->size() == 0) {
        if(last_non_empty == cell_index_.begin()) {
            return -1;
        }
        --last_non_empty;
    }
    return last_non_empty - cell_index_.begin();
}

std::unique_ptr<SheetInterface> CreateSheet() {
    return std::make_unique<Sheet>();
}
