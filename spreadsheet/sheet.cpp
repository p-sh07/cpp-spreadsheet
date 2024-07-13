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
    UpdIndexSize(pos);

    //Store cell ref to cell ptr
    CellPtr& cell_at_pos = GetCellPtrRef(pos);

    //1.Add or modify existing cell
    if(!cell_at_pos) {
        //This is a new cell, should ne no dependent cells
        cell_at_pos = std::make_unique<Cell>(*this);
    } else {
        //go through dep cells graph & invalidate their cache
        InvalidateDepCellsCacheDFS(pos);
    }

    //2.Set Cell Value (Check for cycle inside the Cell::Set method)
    cell_at_pos->Set(text, pos);

    //TODO: How to keep Dependent cells updated ???

    //3.GetRefCells & add the new cell to their dependent Cells
    for(const auto& ref_cell : cell_at_pos->GetReferencedCells()) {
        //Here, we need to have empty cells in place for Referenced Cells
        //GetCellPtrRef(pos) will add empty cell @ pos if it doesn't exist
        GetCellPtrRef(ref_cell)->AddDependentCell(pos);
    }

    //If the cell is beyond current print area (max/bottom right cell), increase print area
    IncreasePrintArea(pos);

    //Upd count (for row.clear() at 0 non-nullptr cells)
    UpdCellInRowCount(pos);
}

const CellInterface* Sheet::GetCell(Position pos) const {
    if(!pos.IsValid()) {
        throw InvalidPositionException("Invalid pos in GetCell()");
    }
    return HasCell(pos) ? GetCellPtrRef(pos).get() : nullptr;
}

CellInterface* Sheet::GetCell(Position pos) {
    if(!pos.IsValid()) {
        throw InvalidPositionException("Invalid pos in GetCell()");
    }
    return HasCell(pos) ? GetCellPtrRef(pos).get() : nullptr;
}

void Sheet::ClearCell(Position pos) {
    if(!pos.IsValid()) {
        throw InvalidPositionException("Invalid pos in ClearCell()");
    }
    if(!HasCell(pos)) {
        return;
    }

    CellPtr& cell_ptr_ref = GetCellPtrRef(pos);
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

void Sheet::InvalidateDepCellsCacheDFS(Position start_cell) const {

    //TODO: Can re-write as general DFS algorithm and pass
    //functional for getting outgoing vertices (e.g. for re-use with HasCycle)

    //TODO: 2.Do you even need colors? Mb check for a valid cache instead? but, could have cells in graph that haven't been visited, but also have no cache (getValue() not called)
    CellColorMap cell_colors;
    std::stack<Position> cell_stack;

    //Lambda for checking cell color in the map
    auto is_white_vertex = [&cell_colors](const Position& pos) {
        return cell_colors.count(pos) == 0 || cell_colors.at(pos) == VertexColor::white;
    };

    cell_stack.push(start_cell);

    while(!cell_stack.empty()) {
        Position vertex = cell_stack.top();
        cell_stack.pop();

        if(is_white_vertex(vertex)) {
            cell_colors[vertex] = VertexColor::grey;

            //положить серую вершину в стэк для нахождения обратного пути
            cell_stack.push(vertex);

            GetCellPtrRef(vertex)->InvalidateCache();
        }

        //для каждого исходящего ребра (v,w):
        for(const Position& dep_cell : GetCellPtrRef(vertex)->GetDependentCells()) {
            if(is_white_vertex(dep_cell)) {
                cell_stack.push(dep_cell);
                cell_colors[dep_cell] = VertexColor::grey;

                GetCellPtrRef(dep_cell)->InvalidateCache();
            }
            //серая вершина попадается в стеке только на обратном пути
            else if (cell_colors[dep_cell] == VertexColor::grey) {
                cell_colors[dep_cell] = VertexColor::black;
            }
        }
    }
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

//TODO: Refactor using Functional !
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
                const CellPtr& cell_ptr = GetCellPtrRef({row,col});
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
                const CellPtr& cell_ptr = GetCellPtrRef({row,col});
                if(cell_ptr) {
                    CellValuePrinter(output, cell_ptr->GetText());
                }
            }
        }
        output << '\n';
    }
}

Sheet::CellPtr& Sheet::GetCellPtrRef(Position pos) {
    if(!HasCell(pos)) {
        SetCell(pos, ""); //->add empty cell, for ref cells
    }
    return cell_index_[pos.row][pos.col];
}

const Sheet::CellPtr& Sheet::GetCellPtrRef(Position pos) const {
    //const cannot add new cell, but will throw if out of bounds
    if(!HasCell(pos)) {
        throw std::out_of_range("Trying to get cell that doesn't exist with const method");
    }
    return cell_index_.at(pos.row).at(pos.col);
}

//===== Print area and index size helper func ====
void Sheet::IncreasePrintArea(Position pos) {
    if(print_size_.rows <= pos.row) {
        print_size_.rows = pos.row + 1;
    }
    if(print_size_.cols <= pos.col) {
        print_size_.cols = pos.col + 1;
    }
}

//resizes index if new cell outside current scope
void Sheet::UpdIndexSize(Position pos) {
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
