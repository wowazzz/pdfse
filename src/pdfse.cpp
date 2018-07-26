// PDF Spots Extractor

#include <iostream>
#include <cstdlib>
#include <iomanip>
#include <string>
#include <algorithm>
#include <podofo/podofo.h>
#include "getopt_pp.h"

using namespace std;
using namespace PoDoFo;

const PdfName NONE_COLOR("None");


void HelpMsg()
{
    cout << endl << "Usage:"
	 << endl << " pdfse input_file.pdf [-options] Spot1 [ ... SpotN ]"
	 << endl << endl
	 << "Options:"
         << endl << "  -d, --debug    enable Debug mode."
         << endl << endl;
}

inline bool ends_with(string const &value, string const &ending)
{
    if (ending.size() > value.size()) return false;
    return equal(ending.rbegin(), ending.rend(), value.rbegin());
}

void WriteArgumentsAndKeyword( vector<PdfVariant> &rArgs, const char* pszKeyword, PdfOutputDevice &rDevice )
{
    vector<PdfVariant>::const_iterator it = rArgs.begin();
    while(it != rArgs.end())
    {
        (*it).Write(&rDevice, ePdfWriteMode_Compact);
        ++it;
    }
    
    rArgs.clear();
    
    if (pszKeyword) 
    {
        rDevice.Write(" ", 1);
        rDevice.Write(pszKeyword, strlen(pszKeyword));
        rDevice.Write("\n", 1);
    }
}

vector<PdfReference> GetColorRefs( const PdfMemDocument &pdf )
{
    vector<PdfReference> colorRefs;
    
    for ( int pn = 0; pn < pdf.GetPageCount(); ++pn ) 
    {
        PdfPage* page = pdf.GetPage(pn);
        PdfObject* pageRes = (*page).GetResources();
        if ( pageRes == NULL ) continue; 
        if( (*pageRes).IsDictionary()
            && (*pageRes).GetDictionary().HasKey("ColorSpace") 
            && (*pageRes).GetDictionary().GetKey("ColorSpace")
                                               ->IsDictionary() )
        {
            PdfDictionary colorSpace = (*pageRes)
                                        .GetDictionary()
                                        .GetKey("ColorSpace")
                                        ->GetDictionary();
            TKeyMap::iterator it = colorSpace.GetKeys().begin();
            while( it != colorSpace.GetKeys().end() )
            {
                if ( (*it).second->IsReference() )
                {
                    PdfReference ref = (*it).second->GetReference();
                    if ( std::count(colorRefs.begin(),
                                colorRefs.end(),
                                ref) == 0 )
                    {
                        colorRefs.push_back( ref );
                    }
                }
                ++it;
            }
        }
    }

    return colorRefs;
}

string CreateSpaces ( string &name )
// Converts #20 sequences to spaces
{
  string nameWithSpaces(name);
  while ( nameWithSpaces.find("#20") != std::string::npos )
      nameWithSpaces.replace ( nameWithSpaces.find("#20"), 3, " " );
  return nameWithSpaces;
}

bool MustBeRemoved( string rawSpotName, vector<string> & spotsToRemove )
{
    if ( spotsToRemove.size() < 1 ) return true;

    string spotName;
    // Change %20 sequences to spaces
    spotName = CreateSpaces(rawSpotName);
    std::transform(spotName.begin(), spotName.end(), spotName.begin(), ::tolower);
    vector<string>::iterator it = spotsToRemove.begin();
    while ( it != spotsToRemove.end() )
    {
        string tempSpot = *it;
        std::transform(tempSpot.begin(), tempSpot.end(), tempSpot.begin(), ::tolower);
//cout << rawSpotName << " " << *it << " !!!!!" << endl;
        if ( spotName.find(tempSpot) != std::string::npos ) return true;
        ++it;
    }
    return false;
}

void LeaveOnlySpots( const char *filename, const char *output )
{
    // load input PDF file
    PdfMemDocument pdf(filename);

    for( int page_num = 0; page_num < pdf.GetPageCount(); page_num++ )
    {
        PdfPage* pPage = pdf.GetPage( page_num );
        PODOFO_RAISE_LOGIC_IF( !pPage, "Got null page pointer within valid page range" );

	EPdfContentsType t;
    	const char* pszKeyword;
    	PdfVariant var;
    	bool bReadToken;

    	PdfContentsTokenizer tokenizer( pPage );
    	vector<PdfVariant> args;
	PdfRefCountedBuffer buffer;
    	PdfOutputDevice device( &buffer );

	bool is_cmyk_now = true;
    	bool is_inside_path = false;

    	while( (bReadToken = tokenizer.ReadNext(t, pszKeyword, var)) )
    	{
	    if (t == ePdfContentsType_Variant)
	    {
		args.push_back(var);
	    }
	    else if (t == ePdfContentsType_ImageData) 
	    {
		args.push_back(var);
	    }
	    else if (t == ePdfContentsType_Keyword)
	    {
		if ((strcmp(pszKeyword, "cs") == 0) || (strcmp(pszKeyword, "CS") == 0) ||
			(strcmp(pszKeyword, "scn") == 0) || (strcmp(pszKeyword, "SCN") == 0) ||
			(strcmp(pszKeyword, "sc") == 0) || (strcmp(pszKeyword, "SC") == 0)
			)
		{
		    // the spots
		    is_cmyk_now = false;
		    WriteArgumentsAndKeyword(args, pszKeyword, device);
		    continue;
		}
		if ((strcmp(pszKeyword, "k") == 0) || (strcmp(pszKeyword, "K") == 0))
		{
		    // cmyk vector graphics
		    is_cmyk_now = true;
		    WriteArgumentsAndKeyword(args, pszKeyword, device);
		    continue;
		}

		// inside path checking
		if (strcmp(pszKeyword, "m") == 0)
		{
		    is_inside_path = true;
		}

		bool need_del = false;
		if (is_cmyk_now && is_inside_path)
		    need_del = true;
		
		if ((strcmp(pszKeyword, "S") == 0) ||
		    (strcmp(pszKeyword, "s") == 0) ||
		    (strcmp(pszKeyword, "f") == 0) ||
		    (strcmp(pszKeyword, "F") == 0) ||
		    (strcmp(pszKeyword, "f*") == 0) ||
		    (strcmp(pszKeyword, "B") == 0) ||
		    (strcmp(pszKeyword, "B*") == 0) ||
		    (strcmp(pszKeyword, "b") == 0) ||
		    (strcmp(pszKeyword, "b*") == 0) ||
		    (strcmp(pszKeyword, "n") == 0))
		{
		    is_inside_path = false;
		}
		if (need_del)
		{
		    pszKeyword = "\0";
		    args.clear();
		    WriteArgumentsAndKeyword(args, pszKeyword, device);
		    continue;
		}

		// for all others
		WriteArgumentsAndKeyword( args, pszKeyword, device );
	    }
	}

	// Write arguments if there are any left
	WriteArgumentsAndKeyword( args, NULL, device );
	// Set new contents stream
	pPage->GetContentsForAppending()->GetStream()->Set( buffer.GetBuffer(), buffer.GetSize() );

    }

    pdf.Write( output );
}

void RemoveAllSpots( const char *filename, vector<string> &spotsRemove )
{
    // load input PDF file
    PdfMemDocument pdf(filename);

    PdfVecObjects pdfObjs = pdf.GetObjects();
    vector<PdfReference> colorRefs = GetColorRefs(pdf);

    // iterate through all color arrays
    PdfObject* colorArrayObject;
    PdfArray colorArray;
    vector<PdfReference>::iterator it = colorRefs.begin();
    while ( it != colorRefs.end() )
    {
        if ( pdf.GetObjects().GetObject(*it)->IsArray() )
        {
            colorArrayObject = pdfObjs.GetObject(*it);
            colorArray = colorArrayObject->GetArray();
            if ( colorArray.GetSize() > 1
                 && colorArray[0].IsName()
                 && colorArray[0].GetName().GetEscapedName() == "Separation"
                 && colorArray[1].IsName() 
                 && MustBeRemoved( colorArray[1].GetName().GetEscapedName(), 
                                    spotsRemove ) )
            {
		//cout << "  " << colorArray[1].GetName().GetEscapedName() << endl;
                colorArray[1] = NONE_COLOR;
                (*colorArrayObject) = PdfObject (colorArrayObject->Reference(), colorArray );
            }
        }
        ++it;
    }

    string tmp_rem = filename;
    tmp_rem.replace(tmp_rem.rfind(".pdf"), sizeof(".pdf"), ".remaining.pdf");
    pdf.Write(tmp_rem.c_str());
}

void MakeSpotFile( const char *filename, string spotname )
{
    // load input PDF file
    PdfMemDocument pdf(filename);

    PdfVecObjects pdfObjs = pdf.GetObjects();
    vector<PdfReference> colorRefs = GetColorRefs(pdf);

    // iterate through all color arrays
    PdfObject* colorArrayObject;
    PdfArray colorArray;
    vector<PdfReference>::iterator it = colorRefs.begin();
    while ( it != colorRefs.end() )
    {
        if ( pdf.GetObjects().GetObject(*it)->IsArray() )
        {
            colorArrayObject = pdfObjs.GetObject(*it);
            colorArray = colorArrayObject->GetArray();
            if ( colorArray.GetSize() > 1
                 && colorArray[0].IsName()
                 && colorArray[0].GetName().GetEscapedName() == "Separation"
                 && colorArray[1].IsName() 
                 && ( strcasecmp( colorArray[1].GetName().GetEscapedName().c_str(), spotname.c_str() ) != 0  ))
            {
		cout << "  " << colorArray[1].GetName().GetEscapedName() << endl;
                colorArray[1] = NONE_COLOR;
                (*colorArrayObject) = PdfObject (colorArrayObject->Reference(), colorArray );
            }
        }
        ++it;
    }

    string tmp_spot = filename;
    spotname = "." + spotname + ".pdf";
    tmp_spot.replace(tmp_spot.rfind(".spots.pdf"), sizeof(".spots.pdf"), spotname.c_str());
    pdf.Write(tmp_spot.c_str());
}

int main( int argc, char* argv[] )
{
    GetOpt::GetOpt_pp cmd(argc, argv);

    // must be at least one Spot
    if (argc < 3)
    {
	HelpMsg();
	return 0;
    }

    // get command line input parameters
    vector<string> options;
    cmd >> GetOpt::GlobalOption(options);
	
    // logging
    bool is_log = false;
    if ( cmd >> GetOpt::OptionPresent('d', "debug"))
	is_log = true;
    PdfError::EnableDebug(is_log);
    PdfError::EnableLogging(is_log);

    // STEP 1. Prepare file "<*>.spots.pdf" for next work 
    cout << "Preparing..." << endl;
    string tmp_spots = options[0].c_str();
    tmp_spots.replace(tmp_spots.rfind(".pdf"), sizeof(".pdf"), ".spots.pdf");
    LeaveOnlySpots(options[0].c_str(), tmp_spots.c_str());

    vector<string> spotsRemove;
    string tempSpot, endPdf = ".pdf";
    vector<string>::iterator iter = options.begin();
    while ( iter != options.end() )
    {
        if ( not ends_with(*iter, endPdf) )
        {
            tempSpot = *iter;
            //std::transform(tempSpot.begin(), tempSpot.end(), tempSpot.begin(), ::tolower);
            spotsRemove.push_back(tempSpot);
        }
        ++iter;
    }

    // STEP 2. Create "<*>.remaining.pdf" file
    cout << "Creating remaining file..." << endl;
    RemoveAllSpots(options[0].c_str(), spotsRemove);

    // STEP 3. Create all Spots files
    cout << "Creating files for selected spots..." << endl;
    iter = spotsRemove.begin();
    while ( iter != spotsRemove.end() )
    {
	MakeSpotFile(tmp_spots.c_str(), *iter);
        ++iter;
    }

    remove(tmp_spots.c_str());
    cout << "Done." << endl;

    return 0;
}
