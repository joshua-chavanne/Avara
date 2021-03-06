/*
    Copyright �1994-1995, Juri Munkki
    All rights reserved.

    File: CHuffmanHandler.h
    Created: Thursday, December 22, 1994, 3:51
    Modified: Monday, February 20, 1995, 16:50
*/

#pragma once
#include "CAbstractHuffPipe.h"

typedef struct
{
	long	decodedSize;
	long	countBitmap[NUMSYMBOLS>>5];
} HuffHandleHeader;

#define	HUFFHANDLELOOKUPBITS		10
#define	HUFFHANDLELOOKUPSIZE		(1<<HUFFHANDLELOOKUPBITS)

class	CHuffmanHandler : public CAbstractHuffPipe
{
public:
			Handle			inHandle;
			Handle			outHandle;
			Ptr				outPointer;
			long			*codeStrings;
			short			*codeLengths;
			HuffTreeNode	**lookupBuf;
			Boolean			singleSymbolData;
			unsigned char	theSingleSymbol;

	virtual	void	CreateLookupBuffer();
	virtual	void	CreateSymbolTable();
	virtual	void	WriteCompressed();
	virtual	Handle	Compress(Handle sourceHandle);
	virtual	void	DecodeAll(unsigned char *source, unsigned char *dest);
	virtual	Handle	Uncompress(Handle sourceHandle);
	virtual	long	GetUncompressedLen(Handle sourceHandle);
	virtual	void	UncompressTo(Handle sourceHandle, Ptr toPtr);
};