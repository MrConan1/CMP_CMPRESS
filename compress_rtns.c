/*****************************************************************************/
/* compress_rtns.c - CMP-Compatible Compression Routines.                    */
/*                   CMP is really just a form of RLE compression.           */
/*****************************************************************************/


///////////////////////////////////////////////////////////////////////////////
// Data Compression Format                                                   //
// Two types of data are encountered: compressed and direct copy.            //
// Compression occurs using X-bit data types (X = 8, 16, or 32).             //
//                                                                           //
// ---------------------------------------------------------------------     //
// Compressed copy:  | (Length-2) # of X-bit matches   | X-bit pattern |     //
// ---------------------------------------------------------------------     //
// Note that compression requires at least 2 in-order matches.               //
// Length and pattern occupy X-bits                                          //
//                                                                           //
// ------------------------------------------------------------------------  //
// Direct copy:      | Neg length in #X-bit units | Data to directly copy |  //
// ------------------------------------------------------------------------  //
///////////////////////////////////////////////////////////////////////////////

/* Includes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "compress_rtns.h"




/*****************************************************************************/
/* cmp_compress - Top Level Compression routine.                             */
/* Returns: 0 on success, -1 on failure.                                     */
/*****************************************************************************/
int cmp_compress(char* inputFname, unsigned int fileOffset, 
				 int reqDataSizeBytes, int cmprType, 
				 int* cmprSizeBytes, 
				 int* decmprSizeBytes, 
				 char** pCmprData)
{
    char* ibuffer = NULL;
    unsigned int fsize = 0;
	FILE* infile = NULL;
	int sizeBytes = 0;
	int rval = 0;

	/* Open the input file for reading */
	infile = fopen(inputFname,"rb");
	if(infile == NULL){
		printf("Error opening file %s\n",inputFname);
		return -1;
	}

	/* Allocate memory for the input data to be compressed */
	fseek(infile,0,SEEK_END);
	fsize = ftell(infile);
	if(reqDataSizeBytes == 0)
		sizeBytes = fsize-fileOffset;
	else
		sizeBytes = reqDataSizeBytes;
	ibuffer = (char*)malloc(sizeBytes);
	if(ibuffer == NULL){
		printf("Error allocing memory for input data\n");
		return -1;
	}

	/* Jump to starting offset of input file and read */
	/* the data to be compressed to a buffer */
	fseek(infile,fileOffset,SEEK_SET);
	sizeBytes = fread(ibuffer,1,sizeBytes,infile);
	fclose(infile);
	if(sizeBytes <= 0){
		printf("Error reading from input file\n");
		return -1;
	}
	*decmprSizeBytes = sizeBytes;


	/* Compress Based on Selected Pattern Length: 8/16/32-bit */
	switch(cmprType){

		case BYTE_CMP_TYPE:
		{
			if(cmpr_8bit(ibuffer,sizeBytes,pCmprData,cmprSizeBytes) < 0){
				printf("8-bit compression failed.\n");
				rval = -1;
			}
		}
		break;

		case SHORT_CMP_TYPE:
		{
			int numShorts = 0;
			numShorts = sizeBytes / 2;
			if((sizeBytes % 2) != 0)
				numShorts++;
			if(cmpr_16bit((short*)ibuffer,sizeBytes,
				(short**)pCmprData,cmprSizeBytes) < 0){
				printf("16-bit compression failed.\n");
				rval = -1;
			}
		}
		break;

		case LONG_CMP_TYPE:
		{
			int numLongs = 0;
			numLongs = sizeBytes / 4;
			if((sizeBytes % 4) != 0)
				numLongs++;
			if(cmpr_32bit((int*)ibuffer,numLongs,
				(int**)pCmprData,cmprSizeBytes) < 0){
				printf("32-bit compression failed.\n");
				rval = -1;
			}
		}
		break;

		default:
			printf("Error, incorrect compression type specified.\n");
			rval = -1;
	}

	/* Free Resources */
	if(ibuffer != NULL)
		free(ibuffer);

	return rval;
}




/*****************************************************************************/
/* cmpr_8bit - 8-bit CMP Compression routine.                                */
/* Inputs: pData, pointer to uncompressed data stream                        */
/*         numBytes, number of 8-bit bytes in uncompr stream                 */
/*         outData, used to allocate compressed stream                       */
/*         comprSizeBytes, size of the compressed data stream                */
/* Returns: 0 on success, -1 on failure.                                     */
/*****************************************************************************/
int cmpr_8bit(char* pData, int numBytes, char** outData, int* cmprSizeBytes){

	int i, pattDetectFlg, maxCmprSizeBytes, unitSizeBytes, runtarget;
    unsigned int u_unmatchedCount;
	char unmatchedCount, pattern, runLength;
	char* pCmrData;
	char* startLoc   = pData;
	char* patternLoc = pData;
	pattDetectFlg = 0;
	unmatchedCount = 0;
	unitSizeBytes = 1;
	runtarget = 2;
	*cmprSizeBytes = 0;

	/* Allocate Memory for compressed data stream */
	/* Assume the compressed data will not exceed the original size  */
	/* If expansion does occur, the code that follows will abort out */
	maxCmprSizeBytes = numBytes*unitSizeBytes;
	*outData = (char*)malloc(maxCmprSizeBytes);
	if(*outData == NULL){
		printf("Error allocating memory for compressed data stream\n");
		return -1;
	}
	pCmrData = *outData;

	/* While data exists, continue to attempt compression */
	while(numBytes > 0){

	    /* For patterns, look for a run of at least 2   */
		/* Then determine how long the pattern runs for */
		i = 1;

	    if( (numBytes >= runtarget) && 
			( ((runtarget == 2) && (*patternLoc == *(patternLoc+i))) ||
			( ((runtarget == 3) && (*patternLoc == *(patternLoc+i)) && (*patternLoc == *(patternLoc+i+1))) )) ){

			numBytes-= runtarget;
			i+=runtarget-1; 
			runLength = runtarget-2;
			runtarget = 2;
			pattDetectFlg = 1;

			while((numBytes > 0) && (runLength < MAX_S_BYTE)){
				if(*patternLoc == *(patternLoc+i)){
					i++;
					runLength++;
					numBytes--;
				}
				else
					break;
			}
		}


		/* If a pattern was found, write out unmatched data to the */
		/* compression stream, followed by the pattern data        */
		if(pattDetectFlg){
			pattDetectFlg = 0;

			/* Compression Stream - Unmatched Data */
			if(startLoc != patternLoc){
				u_unmatchedCount = (unsigned int)(-1*unmatchedCount);
                *cmprSizeBytes += u_unmatchedCount*unitSizeBytes + unitSizeBytes;
				if(*cmprSizeBytes > maxCmprSizeBytes){
					printf("Error in compression, expansion occurred.\n");
					return -1;
				}
				memcpy(pCmrData,&unmatchedCount,unitSizeBytes);           pCmrData++;
				memcpy(pCmrData,startLoc,u_unmatchedCount*unitSizeBytes); pCmrData+=u_unmatchedCount;
                unmatchedCount = 0;
			}
            
			/* Compression Stream - Pattern Data */
			*cmprSizeBytes += (unitSizeBytes*2);
			if(*cmprSizeBytes > maxCmprSizeBytes){
				printf("Error in compression, expansion occurred.\n");
				return -1;
			}
			memcpy(pCmrData,&runLength,unitSizeBytes);   pCmrData++;
			pattern = *patternLoc;
			memcpy(pCmrData,&pattern,unitSizeBytes);     pCmrData++;

			patternLoc += i;
			startLoc = patternLoc;
		}
		else{

			/* No new pattern was found on this comparison */
			numBytes--;
			patternLoc++;

            /* If the number of unmatched units exceeds the maximum allowed */
            /* then write that data to the output stream now */
            unmatchedCount--;
			if(unmatchedCount == MIN_S_BYTE){
                u_unmatchedCount = (unsigned int)(-1*(unmatchedCount+(char)1)) + 1;
				*cmprSizeBytes += u_unmatchedCount*unitSizeBytes + unitSizeBytes;
				if(*cmprSizeBytes > maxCmprSizeBytes){
					printf("Error in compression, expansion occurred.\n");
					return -1;
				}
	       	    memcpy(pCmrData,&unmatchedCount,unitSizeBytes);           pCmrData++;
		        memcpy(pCmrData,startLoc,u_unmatchedCount*unitSizeBytes); pCmrData+=u_unmatchedCount;
   			    startLoc = patternLoc;
                unmatchedCount = 0;
				runtarget = 2;
			}
			else if(unmatchedCount == (MIN_S_BYTE+1))
				runtarget = 2;
			else
                runtarget = 3;
		}
	}

	/* Write out any remaining unmatched data to the compression stream */
	if(startLoc != patternLoc){
        u_unmatchedCount = (unsigned int)(-1*unmatchedCount);
		*cmprSizeBytes += u_unmatchedCount*unitSizeBytes + unitSizeBytes;
		if(*cmprSizeBytes > maxCmprSizeBytes){
			printf("Error in compression, expansion occurred.\n");
			return -1;
		}
		memcpy(pCmrData,&unmatchedCount,unitSizeBytes);           pCmrData++;
		memcpy(pCmrData,startLoc,u_unmatchedCount*unitSizeBytes); pCmrData+=u_unmatchedCount;
	}

	return 0;
}




/*****************************************************************************/
/* cmpr_16bit - 16-bit CMP Compression routine.                              */
/* Inputs: pData, pointer to uncompressed data stream                        */
/*         numShorts, number of 16-bit shorts in uncompr stream (round up)   */
/*         outData, used to allocate compressed stream                       */
/*         comprSizeBytes, size of the compressed data stream                */
/* Returns: 0 on success, -1 on failure.                                     */
/*****************************************************************************/
int cmpr_16bit(short* pData, int numShorts, short** outData, int* cmprSizeBytes){

	int i, pattDetectFlg, unitSizeBytes, maxCmprSizeBytes;
	short pattern, runLength, unmatchedCount;
	short* pCmrData;
	short* startLoc   = pData;
	short* patternLoc = pData;
	pattDetectFlg = 0;
	unmatchedCount = 0;
	unitSizeBytes = 2;
	*cmprSizeBytes = 0;

	/* Allocate Memory for compressed data stream */
	/* Assume the compressed data will not exceed the original size  */
	/* If expansion does occur, the code that follows will abort out */
	maxCmprSizeBytes = numShorts*unitSizeBytes;
	*outData = (short*)malloc(maxCmprSizeBytes);
	if(*outData == NULL){
		printf("Error allocating memory for compressed data stream\n");
		return -1;
	}
	pCmrData = *outData;


	/* While data exists, continue to attempt compression */
	while(numShorts > 0){

	    /* For patterns, look for a run of at least 2   */
		/* Then determine how long the pattern runs for */
		i = 1;
//	    if( (numShorts >= 2) && (*patternLoc == *(patternLoc+i)) ){
//			numShorts-= 2;
//			i++; 
//			runLength = 0;
//			pattDetectFlg = 1;


		if( (numShorts >= 3) && (*patternLoc == *(patternLoc+i)) && (*patternLoc == *(patternLoc+i+1)) ){
//printf("Pattern 0x%X \n",(*patternLoc)&0xFF);
			numShorts-= 3;
			i+=2; 
			runLength = 1;
			pattDetectFlg = 1;
			while((numShorts > 0) && (runLength < MAX_S_SHORT)){
				if(*patternLoc == *(patternLoc+i)){
					i++;
					runLength++;
					numShorts--;
				}
				else
					break;
			}
		}


		/* If a pattern was found, write out unmatched data to the */
		/* compression stream, followed by the pattern data        */
		if(pattDetectFlg){
			pattDetectFlg = 0;

			/* Compression Stream - Unmatched Data */
			if(startLoc != patternLoc){
				*cmprSizeBytes += (patternLoc-startLoc)*unitSizeBytes + unitSizeBytes;
				if(*cmprSizeBytes > maxCmprSizeBytes){
					printf("Error in compression, expansion occurred.\n");
					return -1;
				}

				unmatchedCount = -1 * (short)(patternLoc-startLoc);
				swap16(&unmatchedCount);
				memcpy(pCmrData,&unmatchedCount,unitSizeBytes);         pCmrData++;
				unmatchedCount = (short)(patternLoc-startLoc);
				memcpy(pCmrData,startLoc,unmatchedCount*unitSizeBytes); pCmrData+=unmatchedCount;
			}

			/* Compression Stream - Pattern Data */
			*cmprSizeBytes += (unitSizeBytes*2);
			if(*cmprSizeBytes > maxCmprSizeBytes){
				printf("Error in compression, expansion occurred.\n");
				return -1;
			}

			swap16(&runLength);
			memcpy(pCmrData,&runLength,unitSizeBytes);   pCmrData++;
			pattern = *patternLoc;
			swap16(&pattern);
			memcpy(pCmrData,&pattern,unitSizeBytes);     pCmrData++;

			patternLoc += i;
			startLoc = patternLoc;
		}
		else{

			/* No new pattern was found on this comparison */
			numShorts--;
			patternLoc++;

            /* If the number of unmatched units exceeds the maximum allowed */
            /* then write that data to the output stream now */
			unmatchedCount = (short)(patternLoc-startLoc);
			if(unmatchedCount == MAX_S_SHORT){
				*cmprSizeBytes += unmatchedCount*unitSizeBytes + unitSizeBytes;
				if(*cmprSizeBytes > maxCmprSizeBytes){
					printf("Error in compression, expansion occurred.\n");
					return -1;
				}

				unmatchedCount = -1 * (short)(patternLoc-startLoc);
		        swap16(&unmatchedCount);
	       	    memcpy(pCmrData,&unmatchedCount,unitSizeBytes);         pCmrData++;
		        unmatchedCount = (short)(patternLoc-startLoc);
		        memcpy(pCmrData,startLoc,unmatchedCount*unitSizeBytes); pCmrData+=unmatchedCount;
   			    startLoc = patternLoc;
			}
		}
	}

	/* Write out any remaining unmatched data to the compression stream */
	if(startLoc != patternLoc){
		*cmprSizeBytes += (patternLoc-startLoc)*unitSizeBytes + unitSizeBytes;
		if(*cmprSizeBytes > maxCmprSizeBytes){
			printf("Error in compression, expansion occurred.\n");
			return -1;
		}

		unmatchedCount = -1 * (short)(patternLoc-startLoc);
		swap32(&unmatchedCount);
		memcpy(pCmrData,&unmatchedCount,unitSizeBytes);         pCmrData++;
		unmatchedCount = (short)(patternLoc-startLoc);
		memcpy(pCmrData,startLoc,unmatchedCount*unitSizeBytes); pCmrData+=unmatchedCount;
	}

	return 0;
}




/*****************************************************************************/
/* cmpr_32bit - 32-bit CMP Compression routine.                              */
/* Inputs: pData, pointer to uncompressed data stream                        */
/*         numLongs, number of 32-bit longs in uncompr stream (round up)     */
/*         outData, used to allocate compressed stream                       */
/*         comprSizeBytes, size of the compressed data stream                */
/* Returns: 0 on success, -1 on failure.                                     */
/*****************************************************************************/
int cmpr_32bit(int* pData, int numLongs, int** outData, int* cmprSizeBytes){

	int pattern, unitSizeBytes;
	int i, runLength, pattDetectFlg, unmatchedCount, maxCmprSizeBytes;
	int* pCmrData;
	int* startLoc   = pData;
	int* patternLoc = pData;
	pattDetectFlg = 0;
	unmatchedCount = 0;
	unitSizeBytes = 4;
	*cmprSizeBytes = 0;

	/* Allocate Memory for compressed data stream */
	/* Assume the compressed data will not exceed the original size  */
	/* If expansion does occur, the code that follows will abort out */
	maxCmprSizeBytes = numLongs*unitSizeBytes;
	*outData = (int*)malloc(maxCmprSizeBytes);
	if(*outData == NULL){
		printf("Error allocating memory for compressed data stream\n");
		return -1;
	}
	pCmrData = *outData;


	/* While data exists, continue to attempt compression */
	while(numLongs > 0){

	    /* For patterns, look for a run of at least 2   */
		/* Then determine how long the pattern runs for */
		i = 1;
		if( (numLongs >= 3) && (*patternLoc == *(patternLoc+i)) && (*patternLoc == *(patternLoc+i+1)) ){
			numLongs-= 3;
			i+=2; 
			runLength = 1;

//	    if( (numLongs >= 2) && (*patternLoc == *(patternLoc+i)) ){
//			numLongs-= 2;
//			i++; 
//			runLength = 0;
			pattDetectFlg = 1;
			while((numLongs > 0) && (runLength < MAX_S_LONG)){
				if(*patternLoc == *(patternLoc+i)){
					i++;
					runLength++;
					numLongs--;
				}
				else
					break;
			}
		}


		/* If a pattern was found, write out unmatched data to the */
		/* compression stream, followed by the pattern data        */
		if(pattDetectFlg){
			pattDetectFlg = 0;

			/* Compression Stream - Unmatched Data */
			if(startLoc != patternLoc){
				*cmprSizeBytes += (patternLoc-startLoc)*unitSizeBytes + unitSizeBytes;
				if(*cmprSizeBytes > maxCmprSizeBytes){
					printf("Error in compression, expansion occurred.\n");
					return -1;
				}

				unmatchedCount = -1 * (int)(patternLoc-startLoc);
				swap32(&unmatchedCount);
				memcpy(pCmrData,&unmatchedCount,unitSizeBytes);         pCmrData++;
				unmatchedCount = (int)(patternLoc-startLoc);
				memcpy(pCmrData,startLoc,unmatchedCount*unitSizeBytes); pCmrData+=unmatchedCount;
			}

			/* Compression Stream - Pattern Data */
			*cmprSizeBytes += (unitSizeBytes*2);
			if(*cmprSizeBytes > maxCmprSizeBytes){
				printf("Error in compression, expansion occurred.\n");
				return -1;
			}

			swap32(&runLength);
			memcpy(pCmrData,&runLength,unitSizeBytes);   pCmrData++;
			pattern = *patternLoc;
			swap32(&pattern);
			memcpy(pCmrData,&pattern,unitSizeBytes);     pCmrData++;

			patternLoc += i;
			startLoc = patternLoc;
		}
		else{

			/* No new pattern was found on this comparison */
			numLongs--;
			patternLoc++;

            /* If the number of unmatched units exceeds the maximum allowed */
            /* then write that data to the output stream now */
			unmatchedCount = (int)(patternLoc-startLoc);
			if(unmatchedCount == MAX_S_LONG){
				*cmprSizeBytes += unmatchedCount*unitSizeBytes + unitSizeBytes;
				if(*cmprSizeBytes > maxCmprSizeBytes){
					printf("Error in compression, expansion occurred.\n");
					return -1;
				}

				unmatchedCount = -1 * (int)(patternLoc-startLoc);
		        swap32(&unmatchedCount);
	       	    memcpy(pCmrData,&unmatchedCount,unitSizeBytes);         pCmrData++;
		        unmatchedCount = (int)(patternLoc-startLoc);
		        memcpy(pCmrData,startLoc,unmatchedCount*unitSizeBytes); pCmrData+=unmatchedCount;
   			    startLoc = patternLoc;
			}
		}
	}

	/* Write out any remaining unmatched data to the compression stream */
	if(startLoc != patternLoc){
		*cmprSizeBytes += (patternLoc-startLoc)*unitSizeBytes + unitSizeBytes;
		if(*cmprSizeBytes > maxCmprSizeBytes){
			printf("Error in compression, expansion occurred.\n");
			return -1;
		}

		unmatchedCount = -1 * (int)(patternLoc-startLoc);
		swap32(&unmatchedCount);
		memcpy(pCmrData,&unmatchedCount,unitSizeBytes);         pCmrData++;
		unmatchedCount = (int)(patternLoc-startLoc);
		memcpy(pCmrData,startLoc,unmatchedCount*unitSizeBytes); pCmrData+=unmatchedCount;
	}

	return 0;
}
