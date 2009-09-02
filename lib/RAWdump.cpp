/*
 * Copyright 2006-2009 The MZmine 2 Development Team
 * 
 * This file is part of MZmine 2.
 * 
 * MZmine 2 is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any later
 * version.
 * 
 * MZmine 2 is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
 * A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License along with
 * MZmine 2; if not, write to the Free Software Foundation, Inc., 51 Franklin
 * St, Fifth Floor, Boston, MA 02110-1301 USA
 *
 * ------------------------------------------------------------------------------
 *
 * This program binds to the Xrawfile2.dll library of Xcalibur and dumps the
 * contents of a given RAW file as text data. The code is partly based on ReAdW 
 * program (GPL). To compile this source, you can use Microsoft Visual C++ 
 * command line compiler:
 * 
 * 1) setup the compiler environment by running 'vcvars32.bat' in the Visual C++
 *    bin directory 
 *
 * 2) build RAWdump.exe by running 'cl.exe RAWdump.cpp'  
 *
 */
 
#include <comutil.h>
#include <stdio.h>
#include <io.h>
#include <fcntl.h>

#pragma comment(lib, "comsuppw.lib")

#import "C:\Xcalibur\System\Programs\XRawfile2.dll" named_guids

typedef struct _datapeak
{
    double dMass;
    double dIntensity;
} DataPeak;

int main(int argc, char* argv[]) {

    // Disable output buffering and set output to binary mode
    setvbuf(stdout, 0, _IONBF, 0);
    _setmode(fileno(stdout), _O_BINARY);
    
    if (argc != 2) {
        fprintf(stdout, "ERROR: This program accepts exactly 1 argument\n");
        return 1;
    }

    HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);    
    if (FAILED(hr)) {
        fprintf(stdout, "ERROR: Unable to initialize COM\n");
        return 1;
    }

    // Make an instance from XRawfile class defined in XRawFile2.dll.
    XRAWFILE2Lib::IXRawfile3Ptr rawFile = NULL;
    hr = rawFile.CreateInstance("XRawfile.XRawfile.1");
    if (FAILED(hr)) {
        fprintf(stdout, "ERROR: Unable to initialize Xcalibur 2.0 interface, try running the command regsvr32 'C:\\<path_to_Xcalibur_dll>\\XRawfile2.dll'\n");
        return 1;
    }

    // Open the Thermo RAW file
    char *filename = argv[1];

    hr = rawFile->Open(filename);
    if (FAILED(hr)) {
        fprintf(stdout, "ERROR: Unable to open XCalibur RAW file %s\n", filename);
        return 1;
    }

    // Look for data that belong to the first mass spectra device in the file
    rawFile->SetCurrentController(0, 1);
    
    long firstScanNumber = 0, lastScanNumber = 0;

    // Verifies if can get the first scan
    hr = rawFile->GetFirstSpectrumNumber(&firstScanNumber);
    if (FAILED(hr)) {
        fprintf(stdout, "ERROR: Unable to get first scan\n");
        return 1;
    }

    // Ask for the last scan number to prepare memory space, for cycle 
    // and final verification
    rawFile->GetLastSpectrumNumber(&lastScanNumber);
    long totalNumScans = (lastScanNumber - firstScanNumber) + 1;

    fprintf(stdout, "NUMBER OF SCANS: %ld\n", totalNumScans);

    // Prepare a wide character string to read the filter line
    BSTR bstrFilter;

    // Read totalnumber of scans, passing values to MZmine application
    for (long curScanNum = 1; curScanNum <= totalNumScans; curScanNum++) {

        bstrFilter = NULL;
        rawFile->GetFilterForScanNum(curScanNum, &bstrFilter);

        if (bstrFilter == NULL){
            fprintf(stdout, "ERROR: Could not extract scan filter line for scan #%d\n", curScanNum);
            return 1;
        }

        char *thermoFilterLine = _com_util::ConvertBSTRToString(bstrFilter);

        fprintf(stdout, "SCAN NUMBER: %ld\n", curScanNum);
        fprintf(stdout, "SCAN FILTER: %s\n", thermoFilterLine);
    
        SysFreeString(bstrFilter);

        long numDataPoints = -1; // points in both the m/z and intensity arrays
        double retentionTimeInMinutes = -1;
        double minObservedMZ_ = -1;
        double maxObservedMZ_ = -1;
        double totalIonCurrent_ = -1;
        double basePeakMZ_ = -1;
        double basePeakIntensity_ = -1;
        long channel; // unused
        long uniformTime; // unused
        double frequency; // unused
        double precursorMz = 0;
        long precursorCharge = 0;

        rawFile->GetScanHeaderInfoForScanNum(
            curScanNum, 
            &numDataPoints, 
            &retentionTimeInMinutes, 
            &minObservedMZ_,
            &maxObservedMZ_,
            &totalIonCurrent_,
            &basePeakMZ_,
            &basePeakIntensity_,
            &channel, // unused
            &uniformTime, // unused
            &frequency // unused
        );

        fprintf(stdout, "RETENTION TIME: %f\n", retentionTimeInMinutes);

        // Check if the scan is MS/MS scan
        if (strstr(thermoFilterLine, "ms ") == NULL) {

                // precursorMz
                VARIANT varValue;
                VariantInit(&varValue);
                rawFile->GetTrailerExtraValueForScanNum(curScanNum, "Monoisotopic M/Z:" , &varValue);

                if( varValue.vt == VT_R4 ){ 
                    precursorMz = (double) varValue.fltVal;
                }else if( varValue.vt == VT_R8 ) {
                    precursorMz = varValue.dblVal;
                }else if ( varValue.vt != VT_ERROR ) {
                    precursorMz = 0;
                }
                
                // precursorCharge
                VariantClear(&varValue);
                rawFile->GetTrailerExtraValueForScanNum(curScanNum, "Charge State:" , &varValue);

                if( varValue.vt == VT_I2 ) 
                    precursorCharge = varValue.iVal;

                VariantClear(&varValue);
                fprintf(stdout, "PRECURSOR: %f %d\n", precursorMz, precursorCharge);
        
        }

        // Cleanup memory
        delete[] thermoFilterLine;

        VARIANT varMassList;
        // initiallize variant to VT_EMPTY
        VariantInit(&varMassList);

        VARIANT varPeakFlags; // unused
        // initiallize variant to VT_EMPTY
        VariantInit(&varPeakFlags);

        // set up the parameters to read the scan
        long dataPoints = 0;
        long scanNum = curScanNum;
        LPCTSTR szFilter = NULL;        // No filter
        long intensityCutoffType = 0;        // No cutoff
        long intensityCutoffValue = 0;    // No cutoff
        long maxNumberOfPeaks = 0;        // 0 : return all data peaks
        double centroidPeakWidth = 0;        // No centroiding
        bool centroidThisScan = false;

        rawFile->GetMassListFromScanNum(
            &scanNum,
            szFilter,             // filter
            intensityCutoffType, // intensityCutoffType
            intensityCutoffValue, // intensityCutoffValue
            maxNumberOfPeaks,     // maxNumberOfPeaks
            centroidThisScan,        // centroid result?
            &centroidPeakWidth,    // centroidingPeakWidth
            &varMassList,        // massList
            &varPeakFlags,        // peakFlags
            &dataPoints);        // array size

        // Get a pointer to the SafeArray
        SAFEARRAY FAR* psa = varMassList.parray;
        DataPeak* pDataPeaks = NULL;
        SafeArrayAccessData(psa, (void**)(&pDataPeaks));
        
        // Print data points
        fprintf(stdout, "DATA POINTS: %d\n", dataPoints);
        
        // Dump the binary data
        fwrite(pDataPeaks, 16, dataPoints, stdout);

        // Cleanup
        SafeArrayUnaccessData(psa); // Release the data handle
        VariantClear(&varMassList); // Delete all memory associated with the variant
        VariantClear(&varPeakFlags); // and reinitialize to VT_EMPTY

        if( varMassList.vt != VT_EMPTY ) {
            SAFEARRAY FAR* psa = varMassList.parray;
            varMassList.parray = NULL;
            SafeArrayDestroy( psa ); // Delete the SafeArray
        }

        if(varPeakFlags.vt != VT_EMPTY ) {
            SAFEARRAY FAR* psa = varPeakFlags.parray;
            varPeakFlags.parray = NULL;
            SafeArrayDestroy( psa ); // Delete the SafeArray
        }
        
    }

    // Finalize link to XRawfile2.dll library
    hr = rawFile->Close();
    if (FAILED(hr)) {
        fprintf(stdout, "ERROR: Error trying to close the RAW file\n");
        return 1;
    }
    
    CoUninitialize();    
    
    return 0;
}