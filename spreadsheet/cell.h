#pragma once

#include "common.h"
#include "formula.h"

#include <optional>
#include <functional>
#include <unordered_set>
#include <vector>

class Sheet;

class Cell : public CellInterface {
public:
    Cell();
    Cell(Sheet& sheet);
    ~Cell();

    void Set(std::string text);
    void Clear();

    Value GetValue() const override;
    std::string GetText() const override;
    std::vector<Position> GetReferencedCells() const override;

    // Для графа зависимостей
    const std::vector<Position>& GetReferencedCellsRaw() const;
    void SetReferencedCells(const std::vector<Position>& refs);
    
    void AddDependent(Cell& dependent);
    void RemoveDependent(Cell& dependent);
    const std::vector<Cell*>& GetDependents() const;
    
    void InvalidateCache();
    
    bool HasFormula() const;

private:
    class Impl;
    class EmptyImpl;
    class TextImpl;
    class FormulaImpl;

    std::unique_ptr<Impl> impl_;
    Sheet* sheet_ = nullptr;

    // Граф зависимостей
    std::vector<Position> referenced_cells_;
    std::vector<Cell*> dependents_;

    // Кэш значения
    mutable std::optional<CellInterface::Value> cache_;
};