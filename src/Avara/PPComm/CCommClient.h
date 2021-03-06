/*
    Copyright �1995-1996, Juri Munkki
    All rights reserved.

    File: CCommClient.h
    Created: Thursday, February 23, 1995, 20:14
    Modified: Wednesday, August 14, 1996, 19:03
*/

#pragma once
#include "CCommChannel.h"

/*
**	Client end of PPC networking classes.
*/
class	CCommClient : public CCommChannel
{
public:
			unsigned long	userIdentity;
			short			itsRefNum;

	virtual	void			ICommClient(short bufferCount);
	virtual	void			Dispose();
	virtual	void			Connect();
	virtual	OSErr			Browse();
	virtual	void			Disconnect();
	virtual	void			WriteAndSignPacket(PacketInfo *thePacket);
	virtual	void			WritePacket(PacketInfo *thePacket);
	virtual	void			DisconnectSlot(short slotId);
	virtual	long			GetMaxRoundTrip(short distribution);
};