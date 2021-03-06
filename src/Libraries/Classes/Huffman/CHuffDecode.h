/*
    Copyright �1994, Juri Munkki
    All rights reserved.

    File: CHuffDecode.h
    Created: Tuesday, September 27, 1994, 23:03
    Modified: Wednesday, September 28, 1994, 1:55
*/

#pragma once
#include "CAbstractHuffPipe.h"

#define	HUFFLOOKUPBITS		8
#define	HUFFLOOKUPSIZE		(1<<HUFFLOOKUPBITS)
#define	BITBUFFERSIZE		512
#define	DECODEBUFFERSIZE	512

class CHuffDecode : public CAbstractHuffPipe
{
public:
		long			outputPosition;
		long			bitPosition;
		long			bytesInBuffer;
		char			codeLengths[NUMSYMBOLS];
		HuffTreeNode	*lookupBuf[HUFFLOOKUPSIZE];
		unsigned char	*outputBuffer;
		unsigned char	*bitBuffer;
		
	virtual	OSErr		Open();
	virtual	void		CreateLookupBuffer();
	virtual	OSErr		PipeData(Ptr dataPtr, long len);
	virtual	OSErr		Close();
	virtual	void		Dispose();
};