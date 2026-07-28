#pragma once

#include <vector>

#include "annotation/Annotation.h"
#include "capture/DesktopCapture.h"
#include "common/Geometry.h"
#include "export/ImageData.h"

namespace snaplite {

class ImageComposer {
public:
    static ImageData Crop(const DesktopImage& desktop, RectI selection);
    static ImageData Compose(const DesktopImage& desktop, RectI selection,
                             const std::vector<AnnotationPtr>& annotations);
};

}  // namespace snaplite
