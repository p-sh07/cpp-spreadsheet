#include "cell.h"

#include <cassert>
#include <iostream>
#include <string>
#include <optional>
#include <stack>
#include <sstream>

///Марина, привет! Воспользовался моментом, что бы переписать реализацию через std::variant вместо наследования и Impl_
/// (и уменьшить дублирование кода использованием шаблонных функций Cell::PerformDFS и Sheet::OutputAllCells)
/// не знаю, насколько такая реализация лучше/хуже, мне кажется немного стало компактнее, понятнее и местами элегантнее
/// (хотя, возможно, и реализацию с наследованием можно было просто слегка довести до ума и тоже получилось бы ок)
/// Буду признателен за любую обратную связь по этому поводу =)

/// P.S.В первой версии кода сильно усложнил себе жизнь тем, что зачем-то передавал в ячейки const SheetInterface& sheet, поэтому
/// долго не получалось логически раскидать методы/проверки при добавлении новых ячеек. С неконстантным sheet
/// все гораздо проще оказалось, переместил все относящееся к ячейкам методы в Сell, как было написано в замечании в первом ревью

namespace{
struct CellTextGetter {
    const SheetInterface& sheet;

    std::string operator()(std::monostate) {
        return "";
    }

    std::string operator()(std::string str) {
        return str;
    }

    std::string operator()(const Cell::FormulaPtr& formula) {
        return FORMULA_SIGN + formula->GetExpression();
    }
};

std::optional<double> StrToDouble(const std::string& txt) {
    double conv_double;
    std::stringstream ss(txt);
    ss >> conv_double;

    //return the double if conversion successful, otherwise nullopt
    return (!ss.fail() && ss.eof())
               ? std::optional<double>{conv_double}
               : std::optional<double>{};
}
}//namespace

//========== Cell Public ==========
Cell::Cell(SheetInterface& sheet)
    : sheet_(sheet) {
}

Cell::~Cell() {}

void Cell::Set(Position pos, std::string text) {
    //Store pos for DFS algorithms (to identify this cell in tree)
    pos_in_sheet_ = pos;

    //1.Empty
    if(text.empty()) {
        Clear();
        return;
    }
    CellData new_data;

    //2.Formula
    if(text[0] == FORMULA_SIGN && text.size() > 1) {
        //remove leading '=' when parsing formula string
        auto new_formula_obj = ParseFormula(text.substr(1));

        if(!new_formula_obj) {
            throw std::runtime_error("Invalid formula object returned by ParseFormula() in Cell::Set");
        }

        const auto new_cell_refs = new_formula_obj->GetReferencedCells();

        if(CheckFormulaForCycle(&new_cell_refs)) {
            throw CircularDependencyException("Circular dependency when adding new formula to cell");
        }

        //no parsing or cycle errors:
        new_data = std::move(new_formula_obj);
    }
    //3.Text
    else {
        new_data = text;

        //3.1.Double as text -> store in cache and read from cache, when using in formula
        //keep string input to preserve format for GetText (otherwise changes to 1.00000 etc)
        if(auto dbl_opt = StrToDouble(text)) {
            cache_ = dbl_opt.value();
        }
    }

    //4.New cell data was processed without exceptions, swap
    Clear();
    std::swap(data_variant_, new_data);

    //5.Add this cell to new ref cells as Dependent (if formula)
    AddAsDependentToRefCells();
}

///Здесь немного поменялась логика, поэтому уже не сделать через Set с передачей пустой строки (как было в замечанни в ревью)
///Надеюсь такой вариант тоже подойдет! (т.е. теперь наоборот Set("") с пустой строкой происходит через Clear() )
void Cell::Clear() {
    //When changing an existing non-empty cell, process dependents and invalidate caches
    if(!IsEmpty()) {
        RemoveCellFromDependents();
        InvalidateDependentCellsCaches();
        InvalidateCache();
    }
    data_variant_ = std::monostate();
}

Cell::Value Cell::GetValue() const {
    //0.Для пустой ячейки возвращаем std::variant<double> == 0.0;
    if(IsEmpty()) {
        return 0.0;
    }

    //1.Возвращает кеш, если он есть
    if(cache_.has_value()) {
        return cache_.value();
    }

    //2.Записать double в кэш, если это возможно
    if(HasFormula()) {
        auto formula_result = AsFormula()->Evaluate(sheet_);

        if(std::holds_alternative<FormulaError>(formula_result)) {
            //Формула вернула ошибку
            return std::get<FormulaError>(formula_result);
        } else {
            cache_ = std::get<double>(formula_result);
        }
    }

    //3.Вернуть текст, если ячейка не содержит число или формулу
    //(числовые ячейки, заданные строкой, всегда имеют валидный кэщ)
    if(HasString()) {
        return AsString()[0] == ESCAPE_SIGN ? AsString().substr(1) : AsString();
    }
    //4.Иначе вернуть значения заново посчитаного Кэша
    else {
        return cache_.value();
    }
}

std::string Cell::GetText() const {
    return std::visit(CellTextGetter{sheet_}, data_variant_);
}

std::vector<Position> Cell::GetReferencedCells() const {
    if(HasFormula()) {
        return AsFormula()->GetReferencedCells();
    }
    return {};
}

void Cell::InvalidateCache() const {
    //Invalidate only for formula cells
    if(HasFormula()) {
        cache_.reset();
    }
}


//==== Работа с зависимыми ячейками ====
std::vector<Position> Cell::GetDependentCells() const {
    //Store dep cells as set, to avoid duplicates. Return as vec for compatibility
    return {dependent_cells_.begin(), dependent_cells_.end()};
}

void Cell::AddDependentCells(Position pos) const {
    dependent_cells_.insert(pos);

    for(const auto& dep_cell : sheet_.GetCell(pos)->GetDependentCells()) {
        dependent_cells_.insert(dep_cell);
    }
}

void Cell::RemoveDependentCells(Position pos) const {
    dependent_cells_.erase(pos);

    for(const auto& dep_cell : sheet_.GetCell(pos)->GetDependentCells()) {
        dependent_cells_.erase(dep_cell);
    }
}


//==== Проходы графа ячеек по DFS ====
//Проверить формулу на циклическую зависимость
bool Cell::CheckFormulaForCycle(const std::vector<Position>* start_cell_refs) {
    auto next_cells_getter = [](const CellInterface* cell_ptr) {
        return cell_ptr->GetReferencedCells();
    };

    //Ничего не делать с ячейками при проходе
    auto function_on_cells = [](const CellInterface* cell_ptr){};

    return PerformDFS(pos_in_sheet_, next_cells_getter, function_on_cells, start_cell_refs);
}

//Добавить эту ячейку в списки зависимых ячеек всем новым referenced cells
void Cell::AddAsDependentToRefCells() {
    //Non-dfs version
    for(auto& ref_cell_pos : GetReferencedCells()) {
        if(auto ref_cell_ptr =sheet_.GetCell(ref_cell_pos)) {
            ref_cell_ptr->AddDependentCells(pos_in_sheet_);
        }
    }

    /// Нужно ли проходить все дерево по dfs для добавления/удаления DependentCells?
    //DFS version
    // //Ничего не делать, если в ячейке не формула
    // if(!HasFormula()) {
    //     return;
    // }

    // auto next_cells_getter = [](const CellInterface* cell_ptr) {
    //     return cell_ptr->GetReferencedCells();
    // };

    // auto function_on_cells = [&](const CellInterface* cell_ptr) {
    //     cell_ptr->AddDependentCells(pos_in_sheet_);
    // };

    // PerformDFS(pos_in_sheet_, next_cells_getter, function_on_cells);
}

//При изменении ячейки, удалить ее (и ее зависимости) из зависимостей своих предыдущих RefCells
void Cell::RemoveCellFromDependents() {
    //Non-dfs version
    for(auto& ref_cell_pos : GetReferencedCells()) {
        if(auto ref_cell_ptr = sheet_.GetCell(ref_cell_pos)) {
            ref_cell_ptr->RemoveDependentCells(pos_in_sheet_);
        }
    }

    // auto next_cells_getter = [](const CellInterface* cell_ptr) {
    //     return cell_ptr->GetReferencedCells();
    // };

    // auto function_on_cells = [&](const CellInterface* cell_ptr) {
    //     cell_ptr->RemoveDependentCells(pos_in_sheet_);
    // };

    // PerformDFS(pos_in_sheet_, next_cells_getter, function_on_cells);
}

//При изменении ячейки, сбросить кеш всех зависимых ячеек
void Cell::InvalidateDependentCellsCaches() {
    auto next_cells_getter = [](const CellInterface* cell_ptr) {
        return cell_ptr->GetDependentCells();
    };

    auto function_on_cells = [](const CellInterface* cell_ptr) {
        cell_ptr->InvalidateCache();
    };

    PerformDFS(pos_in_sheet_, next_cells_getter, function_on_cells);
}

//==== Variant check/access =====
//TODO: Public or Private?
bool Cell::IsEmpty() const {
    return std::holds_alternative<std::monostate>(data_variant_);
}
bool Cell::HasDouble() const {
    //return std::holds_alternative<double>(data_variant_);
    return cache_.has_value();
}
bool Cell::HasString() const {
    return std::holds_alternative<std::string>(data_variant_);
}
bool Cell::HasFormula() const {
    return std::holds_alternative<FormulaPtr>(data_variant_);
}

// double Cell::AsDouble() const {
//     if(HasDouble()) {
//         //return std::get<double>(data_variant_);
//         return cache_.value();
//     } else if (HasFormula()) {
//         auto formula_result = AsFormula()->Evaluate(sheet_);
//         if(std::holds_alternative<double>(formula_result)) {
//             cache_ = std::get<double>(formula_result);
//             return cache_.value();
//         } else {
//             throw std::get<FormulaError>(formula_result);
//         }
//     } else {
//         throw std::runtime_error("Bad Double access attempt: does not hold and is not convertible to Double");
//     }
// }

std::string Cell::AsString() const {
    if(!HasString()) {
        throw std::runtime_error("Bad String-variant access attempt: does not hold str");
    }
    return std::get<std::string>(data_variant_);
}
const Cell::FormulaPtr& Cell::AsFormula() const {
    if(!HasFormula()) {
        throw std::runtime_error("Bad Formula-variant access attempt: does not hold formula");
    }
    return std::get<FormulaPtr>(data_variant_);
}
