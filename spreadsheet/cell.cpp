#include "cell.h"
#include "sheet.h"
#include "FormulaAST.h"

#include <cassert>
#include <iostream>
#include <string>
#include <sstream>
#include <optional>

using namespace std::literals;

// Base implementation class
class Cell::Impl {
public:
    virtual ~Impl() = default;
    virtual CellInterface::Value GetValue(const SheetInterface& sheet) const = 0;
    virtual std::string GetText() const = 0;
    virtual std::vector<Position> GetReferencedCells() const { return {}; }
    virtual bool IsFormula() const { return false; }
};

// Empty cell implementation
class Cell::EmptyImpl : public Cell::Impl {
public:
    CellInterface::Value GetValue(const SheetInterface&) const override {
        return ""s;
    }
    
    std::string GetText() const override {
        return ""s;
    }
};

// Text cell implementation
class Cell::TextImpl : public Cell::Impl {
public:
    explicit TextImpl(std::string text) : text_(std::move(text)) {}
    
    CellInterface::Value GetValue(const SheetInterface&) const override {
        if (!text_.empty() && text_[0] == ESCAPE_SIGN) {
            return text_.substr(1);
        }
        return text_;
    }
    
    std::string GetText() const override {
        return text_;
    }
    
private:
    std::string text_;
};

// Formula cell implementation
class Cell::FormulaImpl : public Cell::Impl {
public:
    explicit FormulaImpl(std::unique_ptr<FormulaInterface> formula) 
        : formula_(std::move(formula)) {
    }
    
    CellInterface::Value GetValue(const SheetInterface& sheet) const override {
        auto result = formula_->Evaluate(sheet);
        if (std::holds_alternative<double>(result)) {
            return std::get<double>(result);
        }
        return std::get<FormulaError>(result);
    }
    
    std::string GetText() const override {
        return FORMULA_SIGN + formula_->GetExpression();
    }

    std::vector<Position> GetReferencedCells() const override {
        return formula_->GetReferencedCells();
    }
    
    bool IsFormula() const override { return true; }
    
private:
    std::unique_ptr<FormulaInterface> formula_;
};

// Cell implementation
Cell::Cell() : impl_(std::make_unique<EmptyImpl>()), sheet_(nullptr) {}

Cell::Cell(Sheet& sheet) : impl_(std::make_unique<EmptyImpl>()), sheet_(&sheet) {}

Cell::~Cell() = default;

void Cell::Set(std::string text) {
    std::unique_ptr<Impl> new_impl;

    if (text.empty()) {
        new_impl = std::make_unique<EmptyImpl>();
    }
    else if (text.size() > 1 && text[0] == FORMULA_SIGN) {
        std::string expr = text.substr(1);
        // ParseFormula может выбросить FormulaException
        auto formula = ParseFormula(std::move(expr));
        new_impl = std::make_unique<FormulaImpl>(std::move(formula));
    }
    else {
        new_impl = std::make_unique<TextImpl>(std::move(text));
    }

    impl_ = std::move(new_impl);
    cache_.reset();
}

void Cell::Clear() {
    impl_ = std::make_unique<EmptyImpl>();
    referenced_cells_.clear();
    cache_.reset();
}

Cell::Value Cell::GetValue() const {
    if (!cache_) {
        if (sheet_) {
            cache_ = impl_->GetValue(*sheet_);
        } else {
            // Для временных ячеек (проверка циклов)
            Sheet empty_sheet;
            cache_ = impl_->GetValue(empty_sheet);
        }
    }
    return *cache_;
}

std::string Cell::GetText() const {
    return impl_->GetText();
}

std::vector<Position> Cell::GetReferencedCells() const {
    return impl_->GetReferencedCells();
}

const std::vector<Position>& Cell::GetReferencedCellsRaw() const {
    return referenced_cells_;
}

void Cell::SetReferencedCells(const std::vector<Position>& refs) {
    referenced_cells_ = refs;
}

void Cell::AddDependent(Cell& dependent) {
    dependents_.push_back(&dependent);
}

void Cell::RemoveDependent(Cell& dependent) {
    dependents_.erase(
        std::remove(dependents_.begin(), dependents_.end(), &dependent),
        dependents_.end()
    );
}

const std::vector<Cell*>& Cell::GetDependents() const {
    return dependents_;
}

void Cell::InvalidateCache() {
    cache_.reset();
}

bool Cell::HasFormula() const {
    return impl_->IsFormula();
}