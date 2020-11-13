#include "PDFWriter.h"
#include "PDFPage.h"
#include "PDFFormXObject.h"
#include "PageContentContext.h"
#include "InputFile.h"

#include "./lib/ModernTiffImageHandler.h"

#include <iostream>
#include <string>
#include <list>

using namespace std;

static EStatusCode placeImage(
	PDFWriter& inWriter, 
	const string& inImagePath) {
	EStatusCode status;
	InputFile tiffFile;

	do {
        status = tiffFile.OpenFile(inImagePath);
        if (status != eSuccess) {
            std::cout<<"failed to open tiff file for reading\n";
            break;
        }

        PDFPage* page = new PDFPage();
		page->SetMediaBox(PDFRectangle(0,0,595,842));
		PageContentContext* pageContentContext = inWriter.StartPageContentContext(page);


		ModernTiffImageHandler tiffHandler;

        PDFFormXObject* imageFormXObject = tiffHandler.CreateFormXObjectFromTIFFStream(&inWriter, tiffFile.GetInputStream());
        if(!imageFormXObject) {
            std::cout<<"failed to create xobject from tiff";
            break;
        }

        // place the image in page bottom left and discard form. scaled to quarter
		pageContentContext->q();
		pageContentContext->cm(1,0,0,1,0,0);
		pageContentContext->Do(page->GetResourcesDictionary().AddFormXObjectMapping(imageFormXObject->GetObjectID()));
		pageContentContext->Q();
		delete imageFormXObject;

		inWriter.EndPageContentContext(pageContentContext);
		inWriter.WritePageAndRelease(page);        
	} while(false);

	return status;
}

typedef list<PDFFormXObject*> PDFFormXObjectList;
typedef list<string> StringList;

static EStatusCode placeImagesOnPage(
	PDFWriter& inWriter,
	const StringList& inImagePaths) {

	PDFFormXObjectList xobjects;
	EStatusCode status = eSuccess;

	for(StringList::const_iterator it=inImagePaths.begin();it != inImagePaths.end() && status == eSuccess;++it) {
		InputFile tiffFile;

        status = tiffFile.OpenFile(*it);
        if (status != eSuccess) {
            std::cout<<"failed to open tiff file for reading\n";
            break;
        }

		ModernTiffImageHandler tiffHandler;

        PDFFormXObject* imageFormXObject = tiffHandler.CreateFormXObjectFromTIFFStream(&inWriter, tiffFile.GetInputStream());
        if(!imageFormXObject) {
			status = eFailure;
            std::cout<<"failed to create xobject from tiff";
            break;
        }
		xobjects.push_back(imageFormXObject);
	}

	do {
		if(status!= eSuccess)
			break;

        PDFPage* page = new PDFPage();
		page->SetMediaBox(PDFRectangle(0,0,595,842));
		PageContentContext* pageContentContext = inWriter.StartPageContentContext(page);

		for(PDFFormXObjectList::iterator it = xobjects.begin(); it != xobjects.end();++it) {
			// place the image in page bottom left and discard form. scaled to quarter
			pageContentContext->q();
			pageContentContext->cm(1,0,0,1,0,0);
			pageContentContext->Do(page->GetResourcesDictionary().AddFormXObjectMapping((*it)->GetObjectID()));
			pageContentContext->Q();
		}

		inWriter.EndPageContentContext(pageContentContext);
		inWriter.WritePageAndRelease(page); 

	} while(false);

	for(PDFFormXObjectList::iterator it = xobjects.begin(); it != xobjects.end();++it) {
		delete *it;
	}	

	return status;
}


int main(int argc, char* argv[])
{
    PDFWriter pdfWriter;
    EStatusCode status;
	InputFile tiffFile;
	do
	{
		status = pdfWriter.StartPDF("./etc/output.pdf", ePDFVersion17);
		if(status != PDFHummus::eSuccess)
		{
			std::cout<<"failed to start PDF\n";
			break;
		}	


		status = placeImage(pdfWriter,"./etc/MARBLES.TIF");
		if(status != PDFHummus::eSuccess)
		{
			std::cout<<"failed to place tiff image ./etc/MARBLES.TIF\n";
			break;
		}

		status = placeImage(pdfWriter,"./etc/strike.tif");
		if(status != PDFHummus::eSuccess)
		{
			std::cout<<"failed to place tiff image ./etc/strike.tif\n";
			break;
		}  


		StringList stringList;
		stringList.push_back("./etc/MARBLES.TIF");
		stringList.push_back("./etc/strike.tif");

		status = placeImagesOnPage(pdfWriter, stringList);
		if(status != PDFHummus::eSuccess)
		{
			std::cout<<"failed to place tiff image ./etc/MARBLES.TIF\n";
			break;
		}

		status = pdfWriter.EndPDF();
		if(status != PDFHummus::eSuccess)
		{
			std::cout<<"failed in end PDF\n";
			break;
		}        

    }while(false);


    std::cout<<"hello world\n";
}