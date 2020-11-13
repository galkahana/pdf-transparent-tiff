#include "ModernTiffImageHandler.h"
#include "IByteReaderWithPosition.h"
#include "DocumentContext.h"
#include "ObjectsContext.h"
#include "PDFWriter.h"
#include "MyStringBuf.h"
#include "DictionaryContext.h"
#include "InputStringBufferStream.h"
#include "OutputStreamTraits.h"
#include "PDFImageXObject.h"
#include "PDFStream.h"
#include "PDFFormXObject.h"
#include "ProcsetResourcesConstants.h"
#include "XObjectContentContext.h"

// tiff lib includes
#include "tiffconf.h"
#include "tiffio.h"

using namespace std;

/*
    Client handlers for Stream reading
*/ 
struct StreamWithPos
{
	IByteReaderWithPosition* mStream;
	LongFilePositionType mOriginalPosition;
};

static tsize_t STATIC_streamRead(thandle_t inData,tdata_t inBuffer,tsize_t inBufferSize)
{
	return (tsize_t)(((StreamWithPos*)inData)->mStream)->Read((Byte*)inBuffer,inBufferSize);
}

static tsize_t STATIC_streamWrite(thandle_t inData,tdata_t inBuffer,tsize_t inBufferSize)
{
	return 0; // not writing...just reading
}

static toff_t STATIC_streamSeek(thandle_t inData, toff_t inOffset, int inDirection)
{

    switch (inDirection) {
    case 0: // set
      (((StreamWithPos*)inData)->mStream)->SetPosition(inOffset);
      break;
    case 1: // current
	      (((StreamWithPos*)inData)->mStream)->Skip(inOffset);
      break;
    case 2: // from end
      ((StreamWithPos*)inData)->mStream->SetPositionFromEnd(inOffset);
      break;
    }	

	return (toff_t)((((StreamWithPos*)inData)->mStream)->GetCurrentPosition() - ((StreamWithPos*)inData)->mOriginalPosition);
}

static int STATIC_streamClose(thandle_t inData)
{
	return 0;
}

static toff_t STATIC_tiffSize(thandle_t inData) 
{ 
	LongFilePositionType currentPosition = ((StreamWithPos*)inData)->mStream->GetCurrentPosition();

	((StreamWithPos*)inData)->mStream->SetPositionFromEnd(0);

	LongFilePositionType size = ((StreamWithPos*)inData)->mStream->GetCurrentPosition() - ((StreamWithPos*)inData)->mOriginalPosition;

	((StreamWithPos*)inData)->mStream->SetPosition(currentPosition);

    return  (toff_t)size; 
}; 

static int STATIC_tiffMap(thandle_t, tdata_t*, toff_t*) 
{ 
 return 0; 
}; 

static void STATIC_tiffUnmap(thandle_t, tdata_t, toff_t) 
{ 
 return; 
}; 


static PDFImageXObject* createImageXObjectForTiff(PDFWriter* inWriter, uint32 w, uint32 h, uint32* raster) {
	PDFImageXObject* imageXObject = NULL;
	PDFStream* imageStream = NULL;
	ObjectsContext& objCxt = inWriter->GetObjectsContext();
	EStatusCode status = eSuccess;

	// make an initial run to detect alpha values
	bool isAlpha = false;
	uint32 npixels = w * h;
	for(uint32 i=0;i<npixels && !isAlpha;++i) {
		isAlpha =  *((IOBasicTypes::Byte*)(raster + i) + 3) < 0xFF;
	}

	do
	{

		// allocate image elements
		ObjectIDType imageXObjectObjectId = objCxt.GetInDirectObjectsRegistry().AllocateNewObjectID();
		ObjectIDType imageMaskObjectId = isAlpha ? objCxt.GetInDirectObjectsRegistry().AllocateNewObjectID():0;

		// create color image with reading also the alpha components, if there are any
		MyStringBuf alphaComponentsData;

		objCxt.StartNewIndirectObject(imageXObjectObjectId);
		DictionaryContext* imageContext = objCxt.StartDictionary();

		// type
		imageContext->WriteKey("Type");
		imageContext->WriteNameValue("XObject");

		// subtype
		imageContext->WriteKey("Subtype");
		imageContext->WriteNameValue("Image");

		// Width
		imageContext->WriteKey("Width");
		imageContext->WriteIntegerValue(w);

		// Height
		imageContext->WriteKey("Height");
		imageContext->WriteIntegerValue(h);

		// Bits Per Component
		imageContext->WriteKey("BitsPerComponent");
		imageContext->WriteIntegerValue(8);

		// Color Space
		imageContext->WriteKey("ColorSpace");
		imageContext->WriteNameValue("DeviceRGB");

		// Mask in case of Alpha
		if (isAlpha) {
			imageContext->WriteKey("SMask");
			imageContext->WriteNewObjectReferenceValue(imageMaskObjectId);
		}

		// now for the image
		imageStream = objCxt.StartPDFStream(imageContext);
		IByteWriter* writerStream = imageStream->GetWriteStream();


		for(uint32 i=0;i<npixels;++i) {
			// write color components to image stream RGB 
			writerStream->Write((IOBasicTypes::Byte*)(raster + i), 3);
		}

		objCxt.EndPDFStream(imageStream);	

		// if there's a soft mask, write it now
		if (isAlpha) {
			objCxt.StartNewIndirectObject(imageMaskObjectId);
			DictionaryContext* imageMaskContext = objCxt.StartDictionary();

			// type
			imageMaskContext->WriteKey("Type");
			imageMaskContext->WriteNameValue("XObject");

			// subtype
			imageMaskContext->WriteKey("Subtype");
			imageMaskContext->WriteNameValue("Image");

			// Width
			imageMaskContext->WriteKey("Width");
			imageMaskContext->WriteIntegerValue(w);

			// Height
			imageMaskContext->WriteKey("Height");
			imageMaskContext->WriteIntegerValue(h);

			// Bits Per Component
			imageMaskContext->WriteKey("BitsPerComponent");
			imageMaskContext->WriteIntegerValue(8);

			// Color Space
			imageMaskContext->WriteKey("ColorSpace");
			imageMaskContext->WriteNameValue("DeviceGray");

			PDFStream* imageMaskStream = objCxt.StartPDFStream(imageMaskContext);
			IByteWriter* writerMaskStream = imageMaskStream->GetWriteStream();

			// write the alpha samples
			for(uint32 i=0;i<npixels;++i) {
				writerMaskStream->Write((IOBasicTypes::Byte*)(raster + i) + 3, 1);
			}

			objCxt.EndPDFStream(imageMaskStream);
			delete imageMaskStream;
		}

		imageXObject = new PDFImageXObject(imageXObjectObjectId, KProcsetImageC);	
	} while(false);

	if (eFailure == status) {
		delete imageXObject;
		imageXObject = NULL;
	}
	delete imageStream;
	return imageXObject;	
}

PDFFormXObject* createImageFormXObjectFromImageXObject(
	PDFWriter* inWriter,
	PDFImageXObject* imageXObject, 
	ObjectIDType inFormXObjectID, 
	uint32 w, 
	uint32 h
) {
	PDFHummus::DocumentContext& docCxt = inWriter->GetDocumentContext();
	PDFFormXObject* formXObject = NULL;
	do
	{

		formXObject = docCxt.StartFormXObject(PDFRectangle(0, 0, w, h), inFormXObjectID);
		XObjectContentContext* xobjectContentContext = formXObject->GetContentContext();

		xobjectContentContext->q();
		xobjectContentContext->cm(w, 0, 0, h, 0, 0);
		xobjectContentContext->Do(formXObject->GetResourcesDictionary().AddImageXObjectMapping(imageXObject));
		xobjectContentContext->Q();

		EStatusCode status = docCxt.EndFormXObjectNoRelease(formXObject);
		if (status != PDFHummus::eSuccess)
		{
			delete formXObject;
			formXObject = NULL;
			break;
		}
	} while (false);
	return formXObject;
}

/*
    ModernTiffImageHandler class def
 */

ModernTiffImageHandler::ModernTiffImageHandler() {

}

ModernTiffImageHandler::~ModernTiffImageHandler() {

}

PDFFormXObject* ModernTiffImageHandler::CreateFormXObjectFromTIFFStream(
    PDFWriter* inWriter, 
    IByteReaderWithPosition* inTIFFStream,
	ObjectIDType inFormXObjectId) {
	
	TIFF* tif = NULL;		
	uint32* raster = NULL;
	PDFFormXObject* imageFormXObject = NULL;
	PDFImageXObject* imageXObject = NULL;
	    
    //TIFFSetErrorHandler(ReportError);
    //TIFFSetWarningHandler(ReportWarning);

    do {
        // Create client for input image stream
		StreamWithPos streamInfo;
		streamInfo.mStream = inTIFFStream;
		streamInfo.mOriginalPosition = inTIFFStream->GetCurrentPosition();
		
		tif = TIFFClientOpen("Stream","r",(thandle_t)&streamInfo,STATIC_streamRead,
																	STATIC_streamWrite,
																	STATIC_streamSeek,
																	STATIC_streamClose,
																	STATIC_tiffSize,
																	STATIC_tiffMap,
																	STATIC_tiffUnmap);
		if(!tif)
		{
			break;
		}

		// Grab image info
		uint32 w, h;
		TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &w);
		TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &h);

		// Allocate and read image data
		uint32 npixels = w * h;
		raster = (uint32*) _TIFFmalloc(npixels * sizeof (uint32));
		if (raster == NULL || !TIFFReadRGBAImageOriented(tif, w, h, raster, ORIENTATION_TOPLEFT, 0)) {
			break;
		}

		// create image xobject based on image data
		PDFImageXObject* imageXObject = createImageXObjectForTiff(inWriter, w, h, raster);
		if(!imageXObject)
			break;

		// can release image data now
	    _TIFFfree(raster);
		raster = NULL;

		// create form
		imageFormXObject = createImageFormXObjectFromImageXObject(
			inWriter, 
			imageXObject, 
			inFormXObjectId == 0 ? inWriter->GetObjectsContext().GetInDirectObjectsRegistry().AllocateNewObjectID() : 0 , w, h);

    } while(false);

	delete imageXObject;
	if(tif)
		TIFFClose(tif);
	if(raster)
		_TIFFfree(raster);

    return imageFormXObject;
}

DoubleAndDoublePair ModernTiffImageHandler::ReadImageDimensions(IByteReaderWithPosition* inTIFFStream) {
	StreamWithPos streamInfo;
	streamInfo.mStream = inTIFFStream;
	streamInfo.mOriginalPosition = inTIFFStream->GetCurrentPosition();
	
	TIFF* tif = TIFFClientOpen("Stream","r",(thandle_t)&streamInfo,STATIC_streamRead,
																STATIC_streamWrite,
																STATIC_streamSeek,
																STATIC_streamClose,
																STATIC_tiffSize,
																STATIC_tiffMap,
																STATIC_tiffUnmap);
	if(!tif)
	{
		return DoubleAndDoublePair(0,0);
	}

	// Grab image info
	uint32 w, h;
	TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &w);
	TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &h);

	TIFFClose(tif);

	return DoubleAndDoublePair(w,h);
}