#pragma once

#include <cstddef>
#include <vector>

#include "annotation/Annotation.h"

namespace snaplite {

class UndoStack {
public:
    UndoStack();

    void Clear();
    void Add(AnnotationPtr annotation);
    bool Delete(std::size_t index);
    bool Undo();
    bool Redo();

    [[nodiscard]] bool CanUndo() const noexcept { return position_ > 0; }
    [[nodiscard]] bool CanRedo() const noexcept { return position_ + 1 < history_.size(); }
    [[nodiscard]] const std::vector<AnnotationPtr>& Items() const noexcept {
        return history_[position_];
    }

private:
    void Commit(std::vector<AnnotationPtr> state);

    std::vector<std::vector<AnnotationPtr>> history_;
    std::size_t position_{};
};

}  // namespace snaplite

