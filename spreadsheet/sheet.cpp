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
    if(!pos.IsValid()) {
        throw InvalidPositionException("Invalid pos in SetCell()");
    }

    //Resize existing row or add new
    ResizeIndex(pos);

    //Store cell value
    CellPtr& new_cell = GetCellPtr(pos);
    new_cell = std::make_unique<Cell>();
    new_cell->Set(text);

    //If the cell is beyond current print area (max/bottom right cell), increase print area
    IncreasePrintArea(pos);
    //Upd count (for row deletion at 0)
    UpdCellInRowCount(pos);
}

const CellInterface* Sheet::GetCell(Position pos) const {
    if(!pos.IsValid()) {
        throw InvalidPositionException("Invalid pos in GetCell()");
    }
    return HasCell(pos) ? GetCellPtr(pos).get() : nullptr;
}

CellInterface* Sheet::GetCell(Position pos) {
    if(!pos.IsValid()) {
        throw InvalidPositionException("Invalid pos in GetCell()");
    }
    return HasCell(pos) ? GetCellPtr(pos).get() : nullptr;
}

void Sheet::ClearCell(Position pos) {
    if(!pos.IsValid()) {
        throw InvalidPositionException("Invalid pos in ClearCell()");
    }
    if(!HasCell(pos)) {
        return;
    }

    CellPtr& cell_ptr_ref = GetCellPtr(pos);
    cell_ptr_ref = nullptr;

    //Delete empty row from index
    UpdCellInRowCount(pos, -1);
    if(num_cells_in_row_[pos.row] == 0) {
        cell_index_[pos.row].clear();
    }
    //or, Shrink row to rightmost non-empty cell
    else {
        auto& cell_row = cell_index_[pos.row];
        auto non_empty_cell_it = std::prev(cell_row.end());
        //iterate backwards through all nullptrs
        while(*non_empty_cell_it == nullptr) {
            --non_empty_cell_it;
        }
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

Size Sheet::GetPrintableSize() const {
    return print_size_;
}

//===== Cell::Value printing ======
void CellValuePrinter(std::ostream& out, const Cell::Value& cv) {
    if(std::holds_alternative<std::string>(cv)) {
        out << std::get<std::string>(cv);
    }
    else if(std::holds_alternative<double>(cv)) {
        out << std::get<double>(cv);
    }
    else if(std::holds_alternative<FormulaError>(cv)) {
        out << std::get<FormulaError>(cv);
    }
}

//index[row][col]
void Sheet::PrintValues(std::ostream& output) const {
    for(int row = 0; row < GetPrintableSize().rows; ++row) {
        bool is_first = true;
        for(int col = 0; col < GetPrintableSize().cols; ++col) {
            if(!is_first) {
                output << '\t';
            }
            is_first = false;

            if(HasCell({row, col})) {
                const CellPtr& cell_ptr = GetCellPtr({row,col});
                if(cell_ptr) {
                    CellValuePrinter(output, cell_ptr->GetValue());
                }
            }
        }
        output << '\n';
    }
}

void Sheet::PrintTexts(std::ostream& output) const {
    for(int row = 0; row < GetPrintableSize().rows; ++row) {
        bool is_first = true;
        for(int col = 0; col < GetPrintableSize().cols; ++col) {
            if(!is_first) {
                output << '\t';
            }
            is_first = false;

            if(HasCell({row, col})) {
                const CellPtr& cell_ptr = GetCellPtr({row,col});
                if(cell_ptr) {
                    CellValuePrinter(output, cell_ptr->GetText());
                }
            }
        }
        output << '\n';
    }
}

Sheet::CellPtr& Sheet::GetCellPtr(Position pos) {
    return cell_index_[pos.row][pos.col];
}

const Sheet::CellPtr& Sheet::GetCellPtr(Position pos) const {
    return cell_index_[pos.row][pos.col];
}

void Sheet::IncreasePrintArea(Position pos) {
    if(print_size_.rows <= pos.row) {
        print_size_.rows = pos.row + 1;
    }
    if(print_size_.cols <= pos.col) {
        print_size_.cols = pos.col + 1;
    }
}

//resizes index if new cell outside current scope
void Sheet::ResizeIndex(Position pos) {
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

bool Sheet::HasCell(Position pos) const {
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
