// PDF Spots Extractor

#include <iostream>
#include <cstdlib>
#include <iomanip>
#include <string>
#include <vector>
#include <algorithm>
#include <podofo/podofo.h>
#include "getopt_pp.h"

using namespace std;
using namespace PoDoFo;

const PdfName NONE_COLOR("None");

struct SPOT {
    string name;
    string csId;
};


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

bool caseInsensitiveStringCompare(const string& str1, const string& str2)
{
    if (str1.size() != str2.size()) 
    {
        return false;
    }
    for (string::const_iterator c1 = str1.begin(), c2 = str2.begin(); c1 != str1.end(); ++c1, ++c2) 
    {
        if (tolower(*c1) != tolower(*c2)) 
        {
            return false;
        }
    }
    return true;
}

void RemoveObjectsExcept( const char *filename, struct SPOT *sp, vector<SPOT> *spotList )
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

	bool is_need_del = true;
    	bool is_inside_path = false;
        string cur_cs_name = "";
	bool inside_text = false;

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
		if ( (strcmp(pszKeyword, "Do") == 0) && (spotList == NULL) ) // removing raster objects
		{
		    pszKeyword = "\0";
		    args.clear();
		    WriteArgumentsAndKeyword(args, pszKeyword, device);
		    continue;
		}

		// removing text
		if (spotList == NULL)
		{
		    if ( strcmp(pszKeyword, "BT") == 0 )
		        inside_text = true;
		    if (inside_text)
		    {
			if ( strcmp(pszKeyword, "ET") == 0 )
			    inside_text = false;

			pszKeyword = "\0";
			args.clear();
			WriteArgumentsAndKeyword(args, pszKeyword, device);
			continue;
		    }
		}

		if ((strcmp(pszKeyword, "cs") == 0) || (strcmp(pszKeyword, "CS") == 0) ||
			(strcmp(pszKeyword, "scn") == 0) || (strcmp(pszKeyword, "SCN") == 0) ||
			(strcmp(pszKeyword, "sc") == 0) || (strcmp(pszKeyword, "SC") == 0)
			)
		{
		    // the spots
		    //cout << pszKeyword << endl;
        	    if ((strcmp(pszKeyword, "cs") == 0) || (strcmp(pszKeyword, "CS") == 0))
        	    {
            		if (args[0].IsName())
                	{
			    cur_cs_name = args[0].GetName().GetEscapedName();
			    //cout << cur_cs_name << endl;
			}
            		if (spotList == NULL)
                	    is_need_del = (cur_cs_name.compare(sp->csId) != 0);
            		else // sp == NULL
            		{
                	    is_need_del = false;
                	    for ( struct SPOT el : *spotList )
                	    {
                    		if (cur_cs_name.compare(el.csId) == 0)
                    		{
                        	    is_need_del = true;
                        	    break;
                    		}
                	    }
            		}
        	    }
		    WriteArgumentsAndKeyword(args, pszKeyword, device);
		    continue;
		}
		if ((strcmp(pszKeyword, "k") == 0) || (strcmp(pszKeyword, "K") == 0))
		{
		    // cmyk vector graphics
		    is_need_del = (spotList == NULL);
		    WriteArgumentsAndKeyword(args, pszKeyword, device);
		    continue;
		}

		// inside path checking
		if ( (strcmp(pszKeyword, "m") == 0) || (strcmp(pszKeyword, "re") == 0) )
		{
		    is_inside_path = true;
		}

		bool confirm_del = false;
		if (is_need_del && is_inside_path)
		    confirm_del = true;
		
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
		if (confirm_del)
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
    string tmp_el = filename;
    if (spotList == NULL)
        tmp_el.replace(tmp_el.rfind(".pdf"), sizeof(".pdf"), "." + sp->name + ".pdf");
    else
        tmp_el.replace(tmp_el.rfind(".pdf"), sizeof(".pdf"), ".remaining.pdf");

    pdf.Write( tmp_el.c_str() );
}

void MakeSpotList(const char *filename, vector<SPOT> &spotsList)
{
    PdfMemDocument pdf(filename);
    vector<PdfReference> colorRefs = GetColorRefs(pdf);
    int i=0;
    std::vector<PdfReference>::iterator it = colorRefs.begin();
    while ( it != colorRefs.end() )
    {
        if ( pdf.GetObjects().GetObject(*it)->IsArray() )
        {
            PdfArray colorArray = pdf.GetObjects().GetObject(*it)->GetArray();
            if ( (colorArray.GetSize() > 1)
                 && colorArray[0].IsName()
                 && (colorArray[0].GetName().GetEscapedName() == "Separation")
                 && colorArray[1].IsName() )
            {
                struct SPOT el;
                el.name = colorArray[1].GetName().GetEscapedName();
                el.name = CreateSpaces(el.name);
                el.csId = "CS" + to_string(i);
                spotsList.push_back(el);
            }
        }
        ++it;
        ++i;
    }
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

    // STEP 1. Make list of all available spots
    // load input PDF file
    cout << "Preparing..." << endl;
    vector<SPOT> spotsList;
    MakeSpotList(options[0].c_str(), spotsList);
    // get all spots from input parameters
    vector<SPOT> spotsRemove;
    string tempSpot, endPdf = ".pdf";
    vector<string>::iterator iter = options.begin();
    while ( iter != options.end() )
    {
        if ( not ends_with(*iter, endPdf) )
        {
            tempSpot = *iter;
            struct SPOT el;
            // searching
            for ( struct SPOT sp : spotsList )
            {
                if ( caseInsensitiveStringCompare(sp.name, tempSpot) )
                {
                    el = sp;
                    break;
                }
            }
            spotsRemove.push_back(el);
        }
        ++iter;
    }

    // STEP 2. Making all Spots files
    cout << "Creating files for selected spots..." << endl;
    for ( struct SPOT sp : spotsRemove )
    {
        cout << "  " << sp.name << endl;
        RemoveObjectsExcept(options[0].c_str(), &sp, NULL);
    }

    // STEP 3. Create "<*>.remaining.pdf" file
    cout << "Creating remaining file..." << endl;
    struct SPOT el;
    RemoveObjectsExcept(options[0].c_str(), &el, &spotsRemove);

    cout << "Done." << endl;

    return 0;
}
