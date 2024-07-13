#include "cell.h"

#include <cassert>
#include <iostream>
#include <string>
#include <optional>
#include <stack>

//========== Cell Common Public ==========
Cell::Cell(const SheetInterface& sheet)
    : sheet_(sheet) {
}

Cell::~Cell() {}

void Cell::Set(std::string text, Position pos) {
    //Empty
    if(text.empty()) {
        impl_ = std::make_unique<EmptyImpl>(sheet_);
        return;
    }

    //Formula or text
    std::unique_ptr<Impl> new_cell_contents;

    if(text[0] == FORMULA_SIGN && text.size() > 1) { //formula
        new_cell_contents = std::make_unique<FormulaImpl>(sheet_);
    } else { //text
        new_cell_contents = std::make_unique<TextImpl>(sheet_);
    }
    new_cell_contents->Set(text, pos);

    //If everything worked, replace cell contents:
    impl_ = std::move(new_cell_contents);
}

void Cell::Clear() {
    impl_ = nullptr;
}

Cell::Value Cell::GetValue() const {
    //For non-initialized cell, this will return veriant<std::string> == "";
    return impl_ ? impl_->GetValue() : Value{};
}

std::string Cell::GetText() const {
    return impl_ ? impl_->GetText() : "";
}

std::vector<Position> Cell::GetReferencedCells() const {
    return impl_ ? impl_->GetRefCells() : std::vector<Position>{};
}

void Cell::AddDependentCell(Position pos) {
    dependent_cells_.insert(pos);
}

const CellsPos Cell::GetDependentCells() const {
    return dependent_cells_;
}

void Cell::InvalidateCache() const {
    if(impl_) {
        impl_->InvalidateCache();
    }
}

//========== Base Cell Implementation ==========
Cell::Impl::Impl(const SheetInterface& sheet)
    : sheet_(sheet) {
}

void Cell::Impl::Set(std::string data, Position pos) {
    text_ = std::move(data);
}

Cell::Value Cell::Impl::GetValue() const {
    return text_;
}

std::string Cell::Impl::GetText() const {
    return text_;
}

//Обнуляет optional
void Cell::Impl::InvalidateCache() const {
}

std::vector<Position> Cell::Impl::GetRefCells() const {
    return {}; //returns only for formula cell!
}

//========== Emptu Cell Implementation ==========
Cell::Value Cell::EmptyImpl::GetValue() const {
    return 0.0;
}

std::vector<Position> Cell::EmptyImpl::GetRefCells() const {
    return {};
}

//========== Text Cell Implementation ==========
//Вернет double, если в string text_ ячейки записано число
Cell::Value Cell::TextImpl::GetValue() const {
    if(!cache_.has_value()) {
        try{
            //is not number if there are any letters
            auto it = std::find_if(text_.begin(), text_.end(), [](const char c) {
                return std::isalpha(c);
            });

            //No letters found, try converting to double
            if(it == text_.end()) {
                cache_ = std::stod(text_);
                return cache_.value();
            }
        } catch (std::exception& ex) {
            //potentially, error - could not convert to double
        }
    } else {
        return cache_.value();
    }
    //Если не число, возвращает string
    return text_[0] == ESCAPE_SIGN ? text_.substr(1) : text_;
}

std::vector<Position> Cell::TextImpl::GetRefCells() const {
    return {};
}

//========== Formula Cell Implementation ==========
void Cell::FormulaImpl::Set(std::string data, Position pos) {
    //Parse without leading '='. Can throw FormulaException
    formula_obj_ = ParseFormula(data.substr(1));

    //Выполняет проход по ячейкам, от которых зависит. Добавляет себя в их dependent_cells_
    if(HasCycle(pos, formula_obj_->GetReferencedCells())) {
        throw CircularDependencyException("New Cell formula has a cyclic dependency");
    }

    //Cell value (text) is only changed if no exception thrown by parsing
    text_ = data;
}

//Возвращает кэшированное значение, если оно есть, иначе высчитывает значение формулы
Cell::Value Cell::FormulaImpl::GetValue() const {
    //TODO: what is the return in case !formula_obj_ ?
    if(!cache_.has_value() && formula_obj_) {

        //Проход по RefCells происходит внутри дерева формулы, вызывается cell->GetValue()
        cache_.emplace(ConvertToCellVal(formula_obj_->Evaluate(sheet_)));
    }

    return cache_.value();
}

//Возвращает текст формулы без лишних скобок
std::string Cell::FormulaImpl::GetText() const {
    if(!formula_obj_) {
        return "#err formula_obj";
    }
    return "=" + formula_obj_->GetExpression();
}

//Обнуляет optional
void Cell::FormulaImpl::InvalidateCache() const {
    cache_.reset();
}

bool Cell::FormulaImpl::HasCycle(Position start_vertex, const std::vector<Position>& start_vertex_ref_cells) {
    //check if any of the added ref cells contain this cell
    CellColorMap cell_colors;
    std::stack<Position> cell_stack;

    //Lambda for checking cell color in the map
    auto is_white_vertex = [&cell_colors](const Position& pos) {
        return cell_colors.count(pos) == 0 || cell_colors.at(pos) == VertexColor::white;
    };

    cell_stack.push(start_vertex);

    //Начать DFS
    while(!cell_stack.empty()) {
        Position vertex = cell_stack.top();
        cell_stack.pop();

        if(is_white_vertex(vertex)) {
            cell_colors[vertex] = VertexColor::grey;

            //положить серую вершину в стэк для нахождения обратного пути
            cell_stack.push(vertex);
        }

        //Skip empty cells, GetCell could be nullptr!
        auto vertex_cell_ptr = sheet_.GetCell(vertex);
        if(!vertex_cell_ptr) {
            continue;
        }

        //Для первой вершины мы еще не изменили ее содержание в sheet_,
        //Поэтому нужно взять RefCells напрямую из объекта формулы
        //(передается в функцию по ссылке)
        const auto& ref_cells = (vertex == start_vertex)
                                    ? start_vertex_ref_cells
                                    : vertex_cell_ptr->GetReferencedCells();

        //для каждого исходящего ребра (v,w):
        for(const Position& ref_cell : ref_cells) {
            if(is_white_vertex(ref_cell)) {
                cell_stack.push(ref_cell);
            }
            //серая вершина попадается в стеке только на обратном пути
            else if (cell_colors[ref_cell] == VertexColor::grey) {
                cell_colors[ref_cell] = VertexColor::black;
            }
            //Черная вершина -> найден цикл!
            else if (cell_colors[ref_cell] == VertexColor::black) {
                return true;
            }
        }
    }
    //Нет цикла
    return false;
}

std::vector<Position> Cell::FormulaImpl::GetRefCells() const {
    return formula_obj_->GetReferencedCells();
}

CellInterface::Value Cell::FormulaImpl::ConvertToCellVal(const FormulaInterface::Value& formula_return) const {
    if(std::holds_alternative<double>(formula_return)) {
        return std::get<double>(formula_return);
    } else {
        return std::get<FormulaError>(formula_return);
    }
}
