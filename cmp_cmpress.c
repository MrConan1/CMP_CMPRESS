/*****************************************************************************/
/* cmp_cmpress.c - Program to perform compression compatible with            */
/*                 Sega's CMP routines (Used by Sega Saturn)                 */
/*                                                                           */
/* v1.0  1/29/18   Initial Version                                           */
/* v1.1  2/04/18   Improved 8-bit compression                                */
/* v1.2  2/13/18   Updated 16/32-bit alg to match 8-bit                      */
/*****************************************************************************/


/* Includes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "compress_rtns.h"

/* Defines */
#define MIN_ARGS  5
#define HDR_BYTE_CMP    0x0000
#define HDR_WORD_CMP    0x0400
#define HDR_LONG_CMP    0x0C00
#define HDR_SIZE_4BYTE  0x0008
#define PROG_VERSION    "1.2"

/* Prototypes */
void printUsage();




/*****************************************************************************/
/* main - Checks input arguments, calls compression routine, puts header on  */
/*        compressed data and outputs to a file.                             */
/*****************************************************************************/
int main(int argc, char** argv){

	FILE* ofile;
	char* pCmprData, *pHdr;
	unsigned short* pUshortHdr;
	char hdr[8];
	static char inputFname[300];
	static char outputFname[300];
	int cmprTypeErr, cmprType, forceHdrSize32, hdrSizeBytes, x, rval;
	int fileOffset, dataSizeBytes, cmprSizeBytes, decmprSizeBytes;

	/* Init */
	cmprTypeErr = cmprType = forceHdrSize32 = hdrSizeBytes = 0;
	fileOffset = dataSizeBytes = 0;
	cmprSizeBytes = decmprSizeBytes = 0;
	pCmprData = NULL;

	printf("cmp_cmpress v%s\n",PROG_VERSION);

    /* Check # of input arguments */
	if(argc < MIN_ARGS){
		printf("Error in number of input arguments\n");
		printUsage();
		return -1;
	}


	/***********************************************************/
	/* Check for help, ignore everything else if help is found */
	/***********************************************************/
	for(x = 1; x < argc; x++){
		if(strcmp(argv[x],"-h") == 0){
			printUsage();
			return 0;
		}
	}


	/**************************************/
	/* Look for compression type argument */
	/**************************************/
	cmprTypeErr = cmprType = 0;
	if(strcmp(argv[1],"-t") == 0){

		switch(atoi(argv[2])){
			case 8:
				cmprType = BYTE_CMP_TYPE;
				break;
			case 16:
				cmprType = SHORT_CMP_TYPE;
				break;
			case 32:
				cmprType = LONG_CMP_TYPE;
				break;
			default:
				cmprTypeErr = 1;
		}
	}
	else
		cmprTypeErr = 1;

	/* Check for compression type argument error */
	if(cmprTypeErr){
		printf("Error in compression type.\n");
		printUsage();
		return -1;
	}


	/**********************************/
	/* Parse Optional Input Arguments */
	/**********************************/
	for(x = 3; x < argc; x++){

		/* File Offset */
		if(strcmp(argv[x],"-f") == 0){
			if(argc > (x+1)){
				x++;
				fileOffset = atoi(argv[x]);
			}
		}

		/* Data to Compress in Bytes */
		else if(strcmp(argv[x],"-s") == 0){
			if(argc > (x+1)){
				x++;
				dataSizeBytes = atoi(argv[x]);
				/* Saturn CD is only going to have at most 700MB */
				if(dataSizeBytes > (700*1024*1024)){
					printf("Error, data size > 700MB\n");
					return -1;
				}
			}
		}

		/* Force 32-bit size in the header for reporting */
		/* of the uncompressed size in bytes.  Otherwise */
		/* this will be a 16-bit field if the # of bytes */
		/* can fit inside.                               */
		else if(strcmp(argv[x],"-w") == 0){
			forceHdrSize32 = 1;
		}

		/* End of optional arguments */
		else
			break;

	}

	/********************************/
	/* Parse Input/Output Filenames */
	/********************************/
	if( (argc - x) != 2){
        printf("Error in input arguments\n");
		printUsage();
		return -1;
	}
	else{
		strcpy(inputFname,  argv[x++]);
		strcpy(outputFname, argv[x]);
	}


	/***************************/
	/* Perform the Compression */
	/***************************/
    rval = cmp_compress(inputFname, fileOffset, dataSizeBytes, 
		cmprType, &cmprSizeBytes, &decmprSizeBytes, &pCmprData);
	if(rval < 0){
		printf("Error encountered during compression.\n");
		return -1;
	}



	/************************************/
	/* Construct the Compression Header */
	/************************************/

	///////////////////////////////////////////////////////////////////////////
	// Header Format - Variable Length (32-bits or 64-bits)
	// Word 0 [16-bits]
    //   Compression Type
	//   Value:  0000_YY00 0000_Z000
    //           where: YY = 00 (8-bit RLE), 01 (16-bit RLE), 10 (32-bit RLE)
	//                  Z determines format of the rest of the header
	//
    // If Z = 0:
	// Word 1 [16-bits]
	//   Decompression Size in Bytes.
	// End of Header
	//
	// If Z = 1:
	// Word 1 [16-bits]
	//   16-bit padding.  Required for alignment of the 32-bit size to follow
	// Words 2,3 [Combine for 32-bit LW]
	//   Decompression Size in Bytes.
	// End of Header
    ///////////////////////////////////////////////////////////////////////////
	pHdr = hdr;
	pUshortHdr = (unsigned short*)hdr;
	memset(pHdr,0,8);  /* Zero the header */

	/* Fill in the compression type */
	switch(cmprType){
		case BYTE_CMP_TYPE:
			*pUshortHdr = HDR_BYTE_CMP;
			break;
		case SHORT_CMP_TYPE:
			*pUshortHdr = HDR_WORD_CMP;
			break;
		case LONG_CMP_TYPE:
			*pUshortHdr = HDR_LONG_CMP;
			break;
	}

	/* Set flag indictating that 2 or 4 bytes are set */
	/* Aside in the header for the decompressed size  */
	/* If 4 bytes, then 2 bytes of padding exist between the */
	/* header and size information */
	/* Also copy in the size information */
	if(forceHdrSize32 || (decmprSizeBytes > 65535)){
		*pUshortHdr |= HDR_SIZE_4BYTE;
		swap16(pUshortHdr);
		swap32(&decmprSizeBytes);
		memcpy(pHdr+4,&decmprSizeBytes,4);
		hdrSizeBytes = 8;
	}
	else{
		unsigned short shrt_decmprSize = (unsigned short)decmprSizeBytes;
		swap16(pUshortHdr);
		swap16(&shrt_decmprSize);
		memcpy(pHdr+2,&shrt_decmprSize,2);
		hdrSizeBytes = 4;
	}


	/***********************************************************/
	/* Write the header and compressed data to the output file */
	/***********************************************************/
	ofile = NULL;
	ofile = fopen(outputFname,"wb");
	if(ofile == NULL){
		printf("Error opening output file for writing.\n");
		return -1;
	}
	fwrite(pHdr,1,hdrSizeBytes,ofile);
	fwrite(pCmprData,1,cmprSizeBytes,ofile);
	fclose(ofile);


	/* Success */
	printf("Compression Completed Sucessfully!\n");

	return 0;
}




/*****************************************************************************/
/* printUsage - Displays command line parameters.                            */
/*****************************************************************************/
void printUsage(){

	printf("cmp_cmpress -t cmprType [options] inputFile outputFile\n");
	printf("  where cmprType is: 8, 16, or 32\n");
	printf("    Available options:\n");
	printf("      -f offset Byte offset in input file to begin compression\n");
	printf("      -h        Help, Prints this message\n");
	printf("      -s size   Maximum number of bytes to compress\n");
	printf("      -w        Force 32-bit size in header\n\n");
	return;
}
