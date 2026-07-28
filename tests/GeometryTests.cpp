#include <cassert>

#include "common/Geometry.h"

int wmain() {
    using snaplite::RectI;
    const RectI negative{-1920, -300, 0, 780};
    const RectI selection = RectI::FromPoints(POINT{-1800, -250}, POINT{-100, 700});
    assert(selection.Width() == 1700);
    assert(selection.Height() == 950);
    assert(snaplite::Intersect(selection, negative) == selection);

    const RectI moved = snaplite::ClampInside(RectI{-2100, -400, -400, 550}, negative);
    assert(moved.left == -1920);
    assert(moved.top == -300);
    assert(moved.Width() == 1700);
    assert(moved.Height() == 950);
    return 0;
}
