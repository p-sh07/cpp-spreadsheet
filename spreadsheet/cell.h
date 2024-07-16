#pragma once

#include "common.h"
#include "formula.h"

#include <unordered_set>
#include <stack>

#include <iostream>

///Марина, привет! Воспользовался моментом, что бы переписать реализацию через std::variant вместо наследования и Impl_
/// (и уменьшить дублирование кода использованием шаблонных функций Cell::PerformDFS и Sheet::OutputAllCells)
/// не знаю, насколько такая реализация лучше/хуже, мне кажется немного стало компактнее, понятнее и местами элегантнее
/// (хотя, возможно, и реализацию с наследованием можно было просто слегка довести до ума и тоже получилось бы ок)
/// Буду признателен за любую обратную связь по этому поводу =)

/// P.S.В первой версии кода сильно усложнил себе жизнь тем, что зачем-то передавал в ячейки const SheetInterface& sheet, поэтому
/// долго не получалось логически раскидать методы/проверки при добавлении новых ячеек. С неконстантным sheet
/// все гораздо проще оказалось, переместил все относящееся к ячейкам методы в Сell, как было написано в замечании в первом ревью

class Cell : public CellInterface {
public:
    Cell(SheetInterface& sheet);
    ~Cell();

    //Position передается для алгоритма DFS, которому нужна позиция стартовой ячейки
    void Set(Position sheet_pos, std::string text);
    void Clear();

    bool IsEmpty() const;

    Value GetValue() const override;
    std::string GetText() const override;

    std::vector<Position> GetReferencedCells() const override;
    void InvalidateCache() const override;

    //=== Dependent cells interface ====
    //uses const & mutable to work via const CellInterface*
    std::vector<Position> GetDependentCells() const override;
    void AddDependentCells(Position pos) const override;
    void RemoveDependentCells(Position pos) const override;

    //Псевдонимы типов используемых в реализации cell
    using FormulaPtr = std::unique_ptr<FormulaInterface>;
    using CellData = std::variant<std::monostate, std::string, FormulaPtr>;

private:
    //Внутрення реализация функционала ячейки
    CellData data_variant_;

    //Кэш для формульной/текстовой ячейки
    mutable std::optional<double> cache_;

    //Контейнер ячеек, значение которых зависит от этой ячейки -> инвалидация кеша при изменении
    mutable std::unordered_set<Position, PositionHash> dependent_cells_;

    //Позиция ячейки в таблице
    Position pos_in_sheet_;

    //Ссылка на Sheet для обработки зависимостей от других ячеек
    SheetInterface& sheet_;

    //Пройти по графу ячеек, применяя к каждой функцию SetterFunc только один раз
    //Для получения "исходящих ребер" графа для ячейки используется GetterFunc
    //Прекратит обход и вернет true при обнаружении цикла
    //Если передан указатель на объект формулы стартовой ячейки,
    //то при начале обхода будет использовать он, а не GetterFunc
    template <typename GetterFunc, typename SetterFunc>
    bool PerformDFS(Position start_cell, GetterFunc get_next_cells, SetterFunc perform_func_on_cell,
                     const std::vector<Position>* start_cell_refs = nullptr);

    //Проверить формулу на циклическую зависимость
    bool CheckFormulaForCycle(const std::vector<Position>* start_cell_refs);

    //Добавить эту ячейку в списки зависимых ячеек всем новым referenced cells
    void AddAsDependentToRefCells();

    //При изменении ячейки, удалит ее из зависимостей своих предыдущих RefCells
    void RemoveCellFromDependents();

    //При изменении ячейки, сбросить кэш зависимых ячеек
    void InvalidateDependentCellsCaches();

    //Функции проверки доступа различных значений
    bool HasDouble() const;
    bool HasString() const;
    bool HasFormula() const;

    std::string AsString() const;
    const FormulaPtr& AsFormula() const;
};

template <typename GetterFunc, typename SetterFunc>
bool Cell::PerformDFS(Position start_cell, GetterFunc get_next_cells, SetterFunc perform_func_on_cell,
                 const std::vector<Position>* start_cell_refs) {
    CellColorMap cell_colors;
    std::stack<Position> cell_stack;

    //Lambda for checking cell color in the map
    auto is_white_vertex = [&cell_colors](const Position& pos) {
        return cell_colors.count(pos) == 0 || cell_colors.at(pos) == VertexColor::white;
    };

    cell_stack.push(start_cell);

    //Начать DFS
    while(!cell_stack.empty()) {
        Position vertex = cell_stack.top();
        cell_stack.pop();

        //Skip empty cells, GetCell could be nullptr
        auto vertex_cell_ptr = sheet_.GetCell(vertex);
        if(!vertex_cell_ptr) {
            continue;
        }

        if(is_white_vertex(vertex)) {
            cell_colors[vertex] = VertexColor::grey;

            perform_func_on_cell(vertex_cell_ptr);

            //положить серую вершину в стэк для нахождения обратного пути
            cell_stack.push(vertex);
        }

        //Для первой вершины при поиске цикла, ее содержание в sheet_ еще не было изменено,
        //Поэтому нужно взять RefCells напрямую из объекта формулы
        const auto& incident_vertices = (vertex == start_cell && start_cell_refs)
                                      ? *start_cell_refs
                                      : get_next_cells(vertex_cell_ptr);

        //для каждого исходящего ребра (v,w):
        for(const Position& next_cell : incident_vertices) {
            if(is_white_vertex(next_cell)) {
                cell_stack.push(next_cell);
                auto next_cell_ptr = sheet_.GetCell(next_cell);

                //Add empty cell in case it doesn't exist in sheet
                if(!next_cell_ptr) {
                    sheet_.SetCell(next_cell, "");
                }
                perform_func_on_cell(next_cell_ptr);
            }
            //серая вершина попадается в стеке только на обратном пути
            else if (cell_colors[next_cell] == VertexColor::grey) {
                cell_colors[next_cell] = VertexColor::black;
            }
            //Черная вершина -> найден цикл!
            else if (cell_colors[next_cell] == VertexColor::black) {
                return true;
            }
        }
    }
    //Нет цикла
    return false;
}
