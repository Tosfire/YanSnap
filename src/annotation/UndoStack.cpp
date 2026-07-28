#include "annotation/UndoStack.h"

namespace snaplite {

UndoStack::UndoStack() {
    history_.emplace_back();
}

void UndoStack::Clear() {
    history_.clear();
    history_.emplace_back();
    position_ = 0;
}

void UndoStack::Add(AnnotationPtr annotation) {
    if (!annotation) {
        return;
    }
    auto state = Items();
    state.push_back(std::move(annotation));
    Commit(std::move(state));
}

bool UndoStack::Delete(std::size_t index) {
    if (index >= Items().size()) {
        return false;
    }
    auto state = Items();
    state.erase(state.begin() + static_cast<std::ptrdiff_t>(index));
    Commit(std::move(state));
    return true;
}

bool UndoStack::Undo() {
    if (!CanUndo()) {
        return false;
    }
    --position_;
    return true;
}

bool UndoStack::Redo() {
    if (!CanRedo()) {
        return false;
    }
    ++position_;
    return true;
}

void UndoStack::Commit(std::vector<AnnotationPtr> state) {
    history_.erase(history_.begin() + static_cast<std::ptrdiff_t>(position_ + 1), history_.end());
    history_.push_back(std::move(state));
    position_ = history_.size() - 1;
}

}  // namespace snaplite

