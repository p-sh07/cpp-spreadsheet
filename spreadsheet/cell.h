#pragma once

#include "common.h"
#include "formula.h"

#include <unordered_set>

class Cell : public CellInterface {
public:
    Cell(const SheetInterface& sheet);
    ~Cell();

    void Set(std::string text, Position pos);
    void Clear();

    Value GetValue() const override;
    std::string GetText() const override;

    std::vector<Position> GetReferencedCells() const override;

    void AddDependentCell(Position pos);
    const CellsPos GetDependentCells() const;

    void InvalidateCache() const;

private:

    class Impl {
    public:
        Impl(const SheetInterface& sheet);
        virtual ~Impl() = default;

        virtual void Set(std::string data, Position pos);
        virtual Value GetValue() const = 0;
        virtual std::string GetText() const;
        virtual std::vector<Position> GetRefCells() const = 0;

        //NB: В базовом методе ничего не делает
        virtual void InvalidateCache() const;

    protected:
        std::string text_ = "";

        //Ссылка для поиска цикла в формулах
        const SheetInterface& sheet_;

        //Кеш для хранения уже вычесленного значения или ошибки
        mutable std::optional<Value> cache_;
    };

    //Empty возвращает пустую строку в обеих Get функциях
    //Использует методы базового класса
    class EmptyImpl final : public Impl {
    public:
        using Impl::Impl;
        ~EmptyImpl() override = default;

        Value GetValue() const override;
        std::vector<Position> GetRefCells() const override;
    };

    class TextImpl final : public Impl {
    public:
        using Impl::Impl;
        ~TextImpl() override = default;

        //Вернет double, если в string text_ ячейки записано число
        //При повторном запросе вернет кэш. Кэш текстовой ячейки не инвалидируется
        Value GetValue() const override;
        std::vector<Position> GetRefCells() const override;
    };

    class FormulaImpl final : public Impl {
    public:
        using Impl::Impl;
        ~FormulaImpl() override = default;

        void Set(std::string text, Position pos) override;

        //Возвращает кэшированное значение, если оно есть, иначе высчитывает заново
        Value GetValue() const override;

        //Возвращает Foumula->GetExpression()
        std::string GetText() const override;

        std::vector<Position> GetRefCells() const override;

        void InvalidateCache() const override;

    private:

        //Только FormulaImpl ячейка использует граф и имеет зависимости от других ячеек:
        std::unique_ptr<FormulaInterface> formula_obj_ = nullptr;

        //Проходит граф ячеек от которых зависит (referenced), в методе Set(),
        //при наличии цикла выбросит исключение CircularDependencyException
        //->Запускается именно в методе Set, тк значение ячейки не должно быть изменено
        //при добавлении новой формулой циклической зависимости в sheet_
        bool HasCycle(Position pos, const std::vector<Position>& start_vertex_ref_cells);

        //Проходит граф ячеек, используется в методе GetValue() при расчете значения ячейки, если кеш пуст
        //Value ComputeValue();

        Value ConvertToCellVal(const FormulaInterface::Value& formula_return) const;

        //TODO: Keep track of dependent cells! When to remove?
        //Value UpdateDependents(const SheetInterface& sheet) const;
    };

    //Внутрення реализация функционала ячейки
    std::unique_ptr<Impl> impl_ = nullptr;


    //Контейнер ячеек, значение которых зависит от этой ячейки -> инвалидация кеша при изменении
    std::unordered_set<Position, PositionHash> dependent_cells_;

    //Ссылка на Sheet для обработки зависимых формул
    const SheetInterface& sheet_;
};
