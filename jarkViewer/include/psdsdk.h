#pragma once
// the main include that always needs to be included in every translation unit that uses the PSD library
#include "psdsdk/Psd.h"

// for convenience reasons, we directly include the platform header from the PSD library.
// we could have just included <Windows.h> as well, but that is unnecessarily big, and triggers lots of warnings.
#include "psdsdk/PsdPlatform.h"

// in the sample, we use the provided malloc allocator for all memory allocations. likewise, we also use the provided
// native file interface.
// in your code, feel free to use whatever allocator you have lying around.
#include "psdsdk/PsdMallocAllocator.h"
#include "psdsdk/PsdNativeFile.h"

#include "psdsdk/PsdDocument.h"
#include "psdsdk/PsdColorMode.h"
#include "psdsdk/PsdLayer.h"
#include "psdsdk/PsdChannel.h"
#include "psdsdk/PsdChannelType.h"
#include "psdsdk/PsdLayerMask.h"
#include "psdsdk/PsdVectorMask.h"
#include "psdsdk/PsdLayerMaskSection.h"
#include "psdsdk/PsdImageDataSection.h"
#include "psdsdk/PsdImageResourcesSection.h"
#include "psdsdk/PsdParseDocument.h"
#include "psdsdk/PsdParseLayerMaskSection.h"
#include "psdsdk/PsdParseImageDataSection.h"
#include "psdsdk/PsdParseImageResourcesSection.h"
#include "psdsdk/PsdLayerCanvasCopy.h"
#include "psdsdk/PsdInterleave.h"
#include "psdsdk/PsdPlanarImage.h"
#include "psdsdk/PsdExport.h"
#include "psdsdk/PsdExportDocument.h"
#include "psdsdk/PsdParseColorModeDataSection.h"
