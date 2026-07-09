#include "formula.h"

#include "FormulaAST.h"

#include <algorithm>
#include <cassert>
#include <cctype>
#include <sstream>

using namespace std::literals;

std::ostream& operator<<(std::ostream& output, const FormulaError& fe) {
    return output << fe.ToString();
}

namespace {

class Formula : public FormulaInterface {
public:
    explicit Formula(std::string expression)
        : ast_(ParseFormulaAST(expression)) {
    }

    Value Evaluate(const SheetInterface& sheet) const override {
        try {
            auto args = [&](Position pos) -> double {
                if (!pos.IsValid()) {
                    throw FormulaError(FormulaError::Category::Ref);
                }
                const CellInterface* cell = sheet.GetCell(pos);
                if (!cell) {
                    return 0.0;
                }
                const auto& value = cell->GetValue();
                if (std::holds_alternative<double>(value)) {
                    return std::get<double>(value);
                }
                if (std::holds_alternative<std::string>(value)) {
                    const auto& str = std::get<std::string>(value);
                    if (str.empty()) {
                        return 0.0;
                    }
                    std::istringstream in(str);
                    double result;
                    if (in >> result && in.eof()) {
                        return result;
                    }
                    throw FormulaError(FormulaError::Category::Value);
                }
                throw std::get<FormulaError>(value);
            };
            return ast_.Execute(args);
        } catch (const FormulaError& fe) {
            return fe;
        }
    }

    std::string GetExpression() const override {
        std::ostringstream out;
        ast_.PrintFormula(out);
        return out.str();
    }

    std::vector<Position> GetReferencedCells() const override {
        std::vector<Position> result;
        for (const auto& cell : ast_.GetCells()) {
            result.push_back(cell);
        }
        result.erase(std::unique(result.begin(), result.end()), result.end());
        return result;
    }

private:
    FormulaAST ast_;
};

}  // namespace

std::unique_ptr<FormulaInterface> ParseFormula(std::string expression) {
    try {
        return std::make_unique<Formula>(std::move(expression));
    }
    catch (const ParsingError& e) {
        throw FormulaException(e.what());
    }
    catch (const std::exception& e) {
        throw FormulaException(e.what());
    }
}