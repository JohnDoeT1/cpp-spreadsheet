#include "sheet.h"

#include "formula.h"

#include <algorithm>
#include <queue>
#include <set>
#include <sstream>
#include <utility>

using namespace std;

////////////////////////////////////////////////////////////
// Проверка позиции
////////////////////////////////////////////////////////////

void Sheet::ValidatePosition(Position pos) {
    if (!pos.IsValid()) {
        throw InvalidPositionException("Invalid position: " + pos.ToString());
    }
}

Sheet::Key Sheet::MakeKey(Position pos) {
    return static_cast<Key>(pos.row) * Position::MAX_COLS + pos.col;
}

////////////////////////////////////////////////////////////
// Доступ к конкретной Cell
////////////////////////////////////////////////////////////

const Cell* Sheet::GetConcreteCell(Position pos) const {
    ValidatePosition(pos);

    const Key key = MakeKey(pos);
    auto it = cells_.find(key);
    if (it == cells_.end()) {
        return nullptr;
    }
    return it->second.get();
}

Cell* Sheet::GetConcreteCell(Position pos) {
    ValidatePosition(pos);

    const Key key = MakeKey(pos);
    auto it = cells_.find(key);
    if (it == cells_.end()) {
        return nullptr;
    }
    return it->second.get();
}

////////////////////////////////////////////////////////////
// Получить ячейку или создать пустую
////////////////////////////////////////////////////////////

Cell& Sheet::GetOrCreateCell(Position pos) {
    ValidatePosition(pos);

    const Key key = MakeKey(pos);
    auto it = cells_.find(key);

    if (it == cells_.end()) {
        auto cell = make_unique<Cell>(*this);
        Cell* ptr = cell.get();
        cells_.emplace(key, std::move(cell));
        return *ptr;
    }

    return *it->second;
}

////////////////////////////////////////////////////////////
// Интерфейс GetCell
////////////////////////////////////////////////////////////

const CellInterface* Sheet::GetCell(Position pos) const {
    return GetConcreteCell(pos);
}

CellInterface* Sheet::GetCell(Position pos) {
    return GetConcreteCell(pos);
}

////////////////////////////////////////////////////////////
// Извлечение ссылок из временной ячейки
////////////////////////////////////////////////////////////

std::vector<Position> Sheet::ExtractReferencedCells(const Cell& tmp_cell) {
    return tmp_cell.GetReferencedCells();
}

////////////////////////////////////////////////////////////
// Проверка циклов (DFS поиск пути)
////////////////////////////////////////////////////////////

bool Sheet::HasPathToTarget(Position start, Position target) const {
    if (!start.IsValid()) {
        return false;
    }

    queue<Position> q;
    set<Position> visited;
    q.push(start);

    while (!q.empty()) {
        const Position cur = q.front();
        q.pop();

        if (cur == target) {
            return true;
        }

        if (!visited.insert(cur).second) {
            continue;
        }

        const Cell* cell = GetConcreteCell(cur);
        if (!cell) {
            continue;
        }

        for (const Position& next : cell->GetReferencedCellsRaw()) {
            if (!next.IsValid()) {
                continue;
            }

            q.push(next);
        }
    }

    return false;
}

bool Sheet::WouldIntroduceCycle(Position pos, const vector<Position>& new_refs) const {
    // Проверка на прямую ссылку на себя
    for (const Position& ref : new_refs) {
        if (ref == pos) {
            return true;
        }
    }

    // Проверка на цикл через другие ячейки
    for (const Position& ref : new_refs) {
        if (HasPathToTarget(ref, pos)) {
            return true;
        }
    }

    return false;
}

////////////////////////////////////////////////////////////
// Перестройка зависимостей
////////////////////////////////////////////////////////////

void Sheet::RebuildDependencies(Position pos, const vector<Position>& new_refs) {
    Cell& cell = GetOrCreateCell(pos);

    for (const Position& old_ref : cell.GetReferencedCellsRaw()) {
        if (!old_ref.IsValid()) continue;
        if (Cell* ref_cell = GetConcreteCell(old_ref)) {
            ref_cell->RemoveDependent(cell);
        }
    }

    cell.SetReferencedCells(new_refs);

    for (const Position& new_ref : new_refs) {
        if (!new_ref.IsValid()) continue;
        if (Cell* ref_cell = GetConcreteCell(new_ref)) {
            ref_cell->AddDependent(cell);
        }
    }
}

////////////////////////////////////////////////////////////
// Инвалидация кэша вверх по dependents
////////////////////////////////////////////////////////////

void Sheet::InvalidateCacheFrom(Cell& changed) {
    queue<Cell*> q;
    set<Cell*> visited;

    q.push(&changed);

    while (!q.empty()) {
        Cell* cur = q.front();
        q.pop();

        if (!visited.insert(cur).second) {
            continue;
        }

        cur->InvalidateCache();

        for (Cell* dependent : cur->GetDependents()) {
            q.push(dependent);
        }
    }
}

////////////////////////////////////////////////////////////
// SetCell — главный метод
////////////////////////////////////////////////////////////

void Sheet::SetCell(Position pos, string text) {
    ValidatePosition(pos);

    if (const Cell* existing = GetConcreteCell(pos)) {
        if (existing->GetText() == text) {
            return;
        }
    }

    if (text.empty()) {
        ClearCell(pos);
        return;
    }

    Cell tmp_cell;
    tmp_cell.Set(text);

    vector<Position> new_refs;
    if (text.size() > 1 && text[0] == FORMULA_SIGN) {
        new_refs = ExtractReferencedCells(tmp_cell);
    }

    if (WouldIntroduceCycle(pos, new_refs)) {
        throw CircularDependencyException("Circular dependency detected");
    }

    Cell& cell = GetOrCreateCell(pos);

    // Удаляем старые зависимости
    for (const Position& old_ref : cell.GetReferencedCellsRaw()) {
        if (old_ref.IsValid()) {
            if (Cell* ref_cell = GetConcreteCell(old_ref)) {
                ref_cell->RemoveDependent(cell);
            }
        }
    }

    // Устанавливаем новый текст
    cell.Set(text);

    // Устанавливаем новые ссылки
    cell.SetReferencedCells(new_refs);

    // Добавляем новые зависимости (только для существующих ячеек)
    for (const Position& new_ref : new_refs) {
        if (new_ref.IsValid()) {
            if (Cell* ref_cell = GetConcreteCell(new_ref)) {
                ref_cell->AddDependent(cell);
            }
        }
    }

    InvalidateCacheFrom(cell);
}

void Sheet::ClearCell(Position pos) {
    ValidatePosition(pos);

    const Key key = MakeKey(pos);
    auto it = cells_.find(key);
    if (it == cells_.end()) {
        return;
    }

    Cell* cell = it->second.get();

    // Удаляем старые зависимости
    for (const Position& old_ref : cell->GetReferencedCellsRaw()) {
        if (old_ref.IsValid()) {
            if (Cell* ref_cell = GetConcreteCell(old_ref)) {
                ref_cell->RemoveDependent(*cell);
            }
        }
    }

    cell->SetReferencedCells({});
    InvalidateCacheFrom(*cell);
    cells_.erase(it);
}

////////////////////////////////////////////////////////////
// Printable size — всегда пересчитывается
////////////////////////////////////////////////////////////

Size Sheet::GetPrintableSize() const {
    Size result{0, 0};

    for (const auto& [key, cell] : cells_) {
        if (!cell || cell->GetText().empty()) {
            continue;
        }

        Position pos{
            static_cast<int>(key / Position::MAX_COLS),
            static_cast<int>(key % Position::MAX_COLS)
        };

        result.rows = std::max(result.rows, pos.row + 1);
        result.cols = std::max(result.cols, pos.col + 1);
    }

    return result;
}

////////////////////////////////////////////////////////////
// Печать
////////////////////////////////////////////////////////////

void Sheet::PrintCells(
    ostream& output,
    const function<string(const CellInterface*)>& getter
) const {
    Size size = GetPrintableSize();
    
    for (int r = 0; r < size.rows; ++r) {
        bool first = true;
        for (int c = 0; c < size.cols; ++c) {
            if (!first) {
                output << '\t';
            }
            first = false;

            const Position pos{r, c};
            const CellInterface* cell = GetCell(pos);

            output << getter(cell);
        }
        output << '\n';
    }
}

void Sheet::PrintTexts(ostream& output) const {
    PrintCells(output, [](const CellInterface* cell) -> string {
        return cell ? cell->GetText() : string{};
    });
}

void Sheet::PrintValues(std::ostream& output) const {
    PrintCells(output, [](const CellInterface* cell) -> std::string {
        if (!cell) {
            return {};
        }

        const auto value = cell->GetValue();

        if (std::holds_alternative<std::string>(value)) {
            return std::get<std::string>(value);
        }

        if (std::holds_alternative<double>(value)) {
            std::ostringstream out;
            out << std::get<double>(value);
            return out.str();
        }

        std::ostringstream out;
        out << std::get<FormulaError>(value);
        return out.str();
    });
}

Sheet::~Sheet() = default;

////////////////////////////////////////////////////////////
// Фабрика
////////////////////////////////////////////////////////////

unique_ptr<SheetInterface> CreateSheet() {
    return make_unique<Sheet>();
}