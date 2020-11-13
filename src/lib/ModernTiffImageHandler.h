#pragma once

#include "ObjectsBasicTypes.h"

#include <utility>

class PDFFormXObject;
class IByteReaderWithPosition;
class PDFWriter;

typedef std::pair<double, double> DoubleAndDoublePair;

class ModernTiffImageHandler
{
public:
	ModernTiffImageHandler();
	~ModernTiffImageHandler(void);

	PDFFormXObject* CreateFormXObjectFromTIFFStream(
		PDFWriter* inWriter, 
		IByteReaderWithPosition* inTIFFStream,
		ObjectIDType inFormXObjectId = 0);

	DoubleAndDoublePair ReadImageDimensions(IByteReaderWithPosition* inTIFFStream);
};
