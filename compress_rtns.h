/*****************************************************************************/
/* compress_rtns.h - Sega CMP Compatible Compression Routines.               */
/*****************************************************************************/
#ifndef COMPRESS_RTNS_H
#define COMPRESS_RTNS_H

//Defines
#define BYTE_CMP_TYPE  0   //1-byte RLE Pattern Compression
#define SHORT_CMP_TYPE 1  //2-byte RLE Pattern Compression
#define LONG_CMP_TYPE  2   //4-byte RLE Pattern Compression

#define swap16(a)   *a = ((*a >> 8) & 0x00FF) | \
	                     ((*a << 8) & 0xFF00)

#define swap32(a)  *a = ((*a >> 24) & 0x000000FF) | \
	                    ((*a >> 8)  & 0x0000FF00) | \
	                    ((*a << 8)  & 0x00FF0000) | \
					    ((*a << 24) & 0xFF000000)

//Max signed datatype values
#define MAX_S_BYTE		127
#define MAX_S_SHORT		32767
#define MAX_S_LONG		2147483647

//Fctn Prototypes
int cmp_compress(char* inputFname, unsigned int fileOffset, 
				 int dataSizeBytes, int cmprType, 
				 int* cmprSizeBytes, 
				 int* decmprSizeBytes, 
				 char** pCmprData);

int cmpr_8bit(char* pData, int numBytes, char** outData, int* cmprSizeBytes);
int cmpr_16bit(short* pData, int numShorts, short** outData, int* cmprSizeBytes);
int cmpr_32bit(int* pData, int numLongs, int** outData, int* cmprSizeBytes);

#endif
