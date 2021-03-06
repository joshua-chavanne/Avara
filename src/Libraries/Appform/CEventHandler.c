/*
    Copyright �1994-1996, Juri Munkki
    All rights reserved.

    File: CEventHandler.c
    Created: Sunday, November 13, 1994, 22:11
    Modified: Monday, June 24, 1996, 3:08
*/

#define EVENTHANDLERMAIN
#include "CEventHandler.h"
#include "CWindowCommander.h"
#include "CommandList.h"
#include "LowMem.h"
#include "Folders.h"
#include "CCompactTagBase.h"
#include "Sound.h"

#define	MENUBARID		128
#define	APPLEMENUID		128

static
pascal
OSErr	AppleEventHandler(
			AppleEvent	*theAppleEvent,
			AppleEvent	*theReplyEvent,
			long		refCon);

static
pascal
OSErr	AppleEventHandler(
			AppleEvent	*theAppleEvent,
			AppleEvent	*theReplyEvent,
			long		refCon)
{
	CEventHandler	*theHandler;
	
	theHandler = (CEventHandler *)refCon;
	return theHandler->DoAppleEvent(theAppleEvent, theReplyEvent);
}

OSErr CEventHandler::DoAppleEvent(
	AppleEvent *theEvent, 
	AppleEvent *theReply)
{
	OSErr		err = errAEEventNotHandled;
	DescType	actualType;
	Size		actualSize;
	DescType	eventClass, eventID;
		
	AEGetAttributePtr(theEvent, keyEventClassAttr,
				typeType, &actualType, (Ptr) &eventClass, 
				sizeof(eventClass), &actualSize);
						

	AEGetAttributePtr(theEvent, keyEventIDAttr,
				typeType, &actualType, (Ptr) &eventID, 
				sizeof(eventID), &actualSize);
	
	if(eventClass == kCoreEventClass)
	{	switch(eventID)
		{	case kAEOpenApplication:
				if(needsNewDocument) DoCommand(kOpenApplicationCmd);
				err = noErr;
				break;
			case kAEOpenDocuments:
				{	AEDescList	docList;
					long		docCount;
					long		docIndex;
					FSSpec		myFSS;
					Size		actualSize;
					AEKeyword	keyword;
					DescType	returnedType;
				
					err = AEGetParamDesc(theEvent, keyDirectObject,
										typeAEList, &docList);
					
					if(err) break;
					
					err = AECountItems(&docList, &docCount);
					
					for(docIndex = 1; docIndex <= docCount; docIndex++)
					{	err = AEGetNthPtr(&docList, docIndex, typeFSS, &keyword,
											&returnedType, (Ptr)&myFSS, sizeof(myFSS),
											&actualSize);
						if(err) break;
						
						OpenFSSpec(&myFSS, true);
					}
					
					AEDisposeDesc(&docList);
				}
				
				if(needsNewDocument) DoCommand(kOpenApplicationCmd);
				break;
			case kAEPrintDocuments:
				needsNewDocument = false;
				//	err = noErr;
				break;
			case kAEQuitApplication:
				DoCommand(kQuitCmd);
				err = noErr;
				break;
		}
	}
	return err;
}

static
pascal
void	EventHandlerNotify(
	NMRecPtr	theReq)
{
	theReq->nmRefCon = false;
	NMRemove(theReq);
}

void	CEventHandler::IEventHandler()
{
	OSErr	iErr;
	
	CursHandle	tempCursor;
	
	ICommander(NULL);	//	No commanding object.

	gApplication = this;
	isSuspended = false;
	modelessLevel = 0;
	activeSleep = 1;
	suspendSleep = 120;

	warnUser = false;
	firstReserve = NULL;
	secondReserve = NULL;

	iErr = FSMakeFSSpec(0, 0, LMGetCurApName(), &appSpec);
	appResRef = CurResFile();

	myNotify.nmRefCon = false;
	myNotify.nmResp = NULL;	// NewNMProc(EventHandlerNotify);
	
	tempCursor = GetCursor(iBeamCursor);
	gIBeamCursor = **tempCursor;

	tempCursor = GetCursor(watchCursor);
	gWatchCursor = **tempCursor;

	defaultSystemFont = LMGetSysFontFam();
	defaultSystemFontSize = LMGetSysFontSize();

	itsMenuToCmdBase = new CTagBase;
	itsMenuToCmdBase->ITagBase();
	
	itsCmdToMenuBase = new CTagBase;
	itsCmdToMenuBase->ITagBase();

	lastMenuSelection = 0;

	itsWindowList = NULL;
	theTarget = NULL;

	quitFlag = false;
	needsNewDocument = true;
	
	prefsFileRef = 0;
	
	iErr = AEInstallEventHandler(typeWildCard, typeWildCard, 
								(AEEventHandlerProcPtr) AppleEventHandler, (long)this, false);
	
	AddMenus();
	
	InitApp();
}

void	CEventHandler::InitApp()
{
	Handle		versResource;
	
	versResource = GetResource('vers', 1);
	if(versResource && GetHandleSize(versResource) > 8)
	{	StringPtr	theVers;
	
		theVers = ((StringPtr)*versResource)+6;
		BlockMoveData(theVers, shortVersString, theVers[0] + 1);
	}
	else
	{	shortVersString[0] = 1;
		shortVersString[1] = '?';
	}

	LoadPreferences();
}	

long	CEventHandler::StringToCommand(
	StringPtr	theString)
{	
	char	theChar;
	short	i;
	long	theResult = 0;
	long	theBase = 1;
	
	i = theString[0];
	while(i)
	{	theChar = theString[i--];
		if(theChar >= '0' && theChar <= '9')
		{	theResult += theBase * (theChar - '0');
			theBase *= 10;
		}
		else
		if(theChar == '#')
		{	theString[0] = i;
			i = 0;
		}
		else
		{	theResult = 0;
			i = 0;
		}
	}
	
	return theResult;
}

void	CEventHandler::AddOneMenu(
	MenuHandle	theMenu,
	short		beforeId)
{
	short		itemCount;
	short		i;

	InsertMenu(theMenu, beforeId);
	
	itemCount = CountMItems(theMenu);
	
	for(i = 1; i <= itemCount; i++)
	{	Str255	theString;
		long	itemCommand;
		long	longMenu;
	
		GetItem(theMenu, i, theString);
		itemCommand = StringToCommand(theString);
		
		longMenu = MENUITEMTOLONG(((*theMenu)->menuID), i);

		if(itemCommand)
		{	itsCmdToMenuBase->WriteLong(itemCommand, longMenu);			
			itsMenuToCmdBase->WriteLong(longMenu, itemCommand);
		}
		
		SetItem(theMenu, i, theString);
	}
}

/*
**	Registers a whole menu (except items that have their own
**	command numbers) as a command. Especially useful, if you
**	have a menu created by AddResMenu.
*/
void	CEventHandler::RegisterResMenu(
	short		menuId,
	long		itemCommand)
{
	long	longMenu;
	
	longMenu = ((long)menuId) << 16;

	itsCmdToMenuBase->WriteLong(itemCommand, longMenu);			
	itsMenuToCmdBase->WriteLong(longMenu, itemCommand);
}

void	CEventHandler::AddMenus()
{
	Handle	menuListHandle;
	short	*listData;
	short	menuCount;
	
	menuListHandle = GetResource('MBAR', MENUBARID);
	HLock(menuListHandle);
	listData = (short *) *menuListHandle;
	
	menuCount = *listData++;
	
	while(menuCount--)
	{	short		menuRes;
		MenuHandle	theMenu;
	
		menuRes = *listData++;
		
		theMenu = GetMenu(menuRes);
		AddOneMenu(theMenu, 0);
		
		if(menuRes == APPLEMENUID)
		{	AddResMenu(theMenu,'DRVR');
		}
	}
	
	DrawMenuBar();
	HUnlock(menuListHandle);
	ReleaseResource(menuListHandle);
}

void	CEventHandler::DisableCommand(
	long	theCommand)
{
	long		longMenu;
	short		shortMenu;
	MenuHandle	theMenu;

	shortMenu = longMenu = itsCmdToMenuBase->ReadLong(theCommand, 0);
	if(longMenu)
	{	theMenu = GetMHandle(longMenu >> 16);
		DisableItem(theMenu, shortMenu);
	}
}

void	CEventHandler::EnableCommand(
	long	theCommand)
{
	long		longMenu;
	short		shortMenu;
	MenuHandle	theMenu;

	shortMenu = longMenu = itsCmdToMenuBase->ReadLong(theCommand, 0);
	if(longMenu)
	{	theMenu = GetMHandle(longMenu >> 16);
		EnableItem(theMenu, shortMenu);
	}
}

void	CEventHandler::CheckMarkCommand(
	long	theCommand,
	Boolean	flag)
{
	long		longMenu;
	short		shortMenu;
	MenuHandle	theMenu;

	shortMenu = longMenu = itsCmdToMenuBase->ReadLong(theCommand, 0);
	if(shortMenu)
	{	theMenu = GetMHandle(longMenu >> 16);
		CheckItem(theMenu, shortMenu, flag);
	}
}

void	CEventHandler::PrepareMenus()
{
	long	key = 0;
	long	theCommand;
	long	longMenu;
	
	while(0 <= (longMenu = itsMenuToCmdBase->GetNextTag(&key)))
	{	MenuHandle	theMenu;
		short		shortMenu = longMenu;

		theMenu = GetMHandle(longMenu >> 16);
		DisableItem(theMenu, shortMenu);
		if(shortMenu)
		{	CheckItem(theMenu, shortMenu, false);
		}
		else
		{	short	i,count;
		
			count = CountMItems(theMenu);
			for(i = 0;i <= count; i++)
			{	CheckItem(theMenu, i, false);
			}
		}
	}
	
	if(theTarget)
	{	theTarget->AdjustMenus(this);
	}
	else
	{	AdjustMenus(this);
	}
}

void	CEventHandler::AdjustMenus(
	CEventHandler	*master)
{
	EnableCommand(kQuitCmd);

}

void	CEventHandler::AttemptQuit()
{
	CWindowCommander	*aWindow, *nextWindow;
	
	aWindow = itsWindowList;
	quitFlag = true;

	while(quitFlag && aWindow)
	{	CloseRequestResult	windStatus;
	
		nextWindow = aWindow->nextWindow;

		windStatus = aWindow->CloseRequest(true);
		switch(windStatus)
		{	case kDontQuit:
				quitFlag = false;
				break;
			case kDidHide:
				aWindow->Dispose();
				break;
		}
		aWindow = nextWindow;
	}
}

void	CEventHandler::BroadcastCommand(
	long	theCommand)
{
	CWindowCommander	*wind, *next;
	
	commandResult = 0;

	wind = itsWindowList;
	
	while(wind)
	{	next = wind->nextWindow;
		wind->DoCommand(theCommand);
		wind = next;
	}
}

void	CEventHandler::DoCommand(
	long	theCommand)
{
	switch(theCommand)
	{	case kQuitCmd:
			AttemptQuit();
			break;
		default:
			inherited::DoCommand(theCommand);
			break;
	}
}

void	CEventHandler::GetLastMenuString(
	StringPtr	destStr)
{
	if(destStr)
	{	if(lastMenuSelection)
		{	MenuHandle	theMenu;
		
			theMenu = GetMHandle(lastMenuSelection >> 16);
			GetItem(theMenu, (short)lastMenuSelection, destStr);
		}
		else
		{	destStr[0] = 0;
		}
	}
}

void	CEventHandler::DoMenuCommand(
	long	cmdTag)
{
	long	cmdNum;
	long	key = 0;
	
	cmdNum = itsMenuToCmdBase->ReadLong(cmdTag, 0);

	if(cmdNum == 0)
	{	cmdNum = itsMenuToCmdBase->ReadLong(cmdTag & 0xFFFF0000, 0);	
	}

	if(cmdNum == 0)
	{	if((cmdTag >> 16) == APPLEMENUID)
		{	Str255	name;
		
			GetItem(GetMHandle(APPLEMENUID), cmdTag & 0xFFFF, name);
			OpenDeskAcc(name);
		}
	}
	else
	{	if(theTarget)
		{	theTarget->DoCommand(cmdNum);
		}
		else
		{	DoCommand(cmdNum);
		}
	}
}

void	CEventHandler::DoMouseDown()
{
	WindowPtr	theWindow;
	short		partCode;
	
	partCode = FindWindow(theEvent.where, &theWindow);
	
	switch(partCode)
	{
    	case inMenuBar:		//	{in menu bar}
    		PrepareMenus();
    		lastMenuSelection = MenuSelect(theEvent.where);
    		HiliteMenu(0);
    		if(lastMenuSelection)
    		{	DoMenuCommand(lastMenuSelection);
    		}
    		break;
    	case inSysWindow:	//	{in system window}
			SystemClick(&theEvent,theWindow);
			break;

		case inDesk:		//	{none of the following}

    	case inContent:		//	{in content region (except grow, if active)}
    	case inDrag:		//	{in drag region}
    	case inGrow:		//	{in grow region (active window only)}
    	case inGoAway:		//	{in go-away region (active window only)}
    	case inZoomIn:		//	
    	case inZoomOut:		//	
    	default:
    		if(itsWindowList)
    		{	itsWindowList->RawClick(theWindow, partCode, &theEvent);
    		}
    		break;
	}
}

void	CEventHandler::Idle()
{
	if(itsWindowList)
		itsWindowList->DoEvent(this, &theEvent);
}

void	CEventHandler::ExternalEvent(
	EventRecord	*anEvent)
{
	theEvent = *anEvent;
	DispatchEvent();
}

void	CEventHandler::DispatchEvent()
{
	GotEvent();
	switch(theEvent.what)
	{	case mouseDown:
			DoMouseDown();
			break;
		case keyDown:
			if(theEvent.modifiers & cmdKey)
			{	long	cmdTag;
			
				PrepareMenus();
				cmdTag = MenuKey(theEvent.message);
				DoMenuCommand(cmdTag);
				HiliteMenu(0);
				break;
			}
		case autoKey:
			if(itsWindowList)	itsWindowList->DoEvent(this, &theEvent);
			break;
		case kHighLevelEvent:
			AEProcessAppleEvent(&theEvent);
			break;

		case app4Evt:
			isSuspended = !(theEvent.message & 1);
			if(!isSuspended)
			{	InitCursor();
				modelessLevel = 0;
			
				if(myNotify.nmRefCon)
				{	myNotify.nmRefCon = false;
					NMRemove(&myNotify);
				}
			}
			else
				modelessLevel = 1;

			theEvent.message = (long)FrontWindow();
			theEvent.what = activateEvt;
			theEvent.modifiers = isSuspended ? 0 : activeFlag;
			if(itsWindowList)	itsWindowList->DoEvent(this, &theEvent);
		break;

		case diskEvt:
			if(HiWord(theEvent.message))
			{	Point	thePoint = {64,64};
				DIBadMount(thePoint,theEvent.message);
			}
			break;

		default:
			Idle();
			break;
	}
}

void	CEventHandler::GotEvent()
{
	if(warnUser)
	{	Str255		theString;
		CCommander	*saved;
	
		GetIndString(theString, kApplicationStringList, 
						secondReserve ? kFirstMemoryWarningString : kSecondMemoryWarningString);
		ParamText(theString, theString, theString, theString);
		saved = BeginDialog();
		Alert(256, 0);
		EndDialog(saved);
		warnUser = false;
	}
}

void	CEventHandler::HandleOneEvent(
	short	eventMask)
{
	WaitNextEvent(eventMask, &theEvent, isSuspended ? suspendSleep : activeSleep, NULL);
	DispatchEvent();
}

void	CEventHandler::UnqueueMaskedEvents(
	short	eventMask)
{
	EventRecord		theEvent;
	
	while(EventAvail(eventMask, &theEvent))
	{	HandleOneEvent(eventMask);
	}
}

void	CEventHandler::EventLoop()
{
	while(quitFlag == false)
	{	HandleOneEvent(everyEvent);
	}
}

void	CEventHandler::Dispose()
{
	SavePreferences();
	prefsBase->Dispose();
	if(prefsFileRef)
	{	FSClose(prefsFileRef);
		prefsFileRef = 0;
	}

	itsMenuToCmdBase->Dispose();
	itsCmdToMenuBase->Dispose();
	
	inherited::Dispose();
}

void	CEventHandler::Geneva9SystemFont()
{
	LMSetSysFontFam(geneva);
	LMSetSysFontSize(9);
}

void	CEventHandler::StandardSystemFont()
{
	LMSetSysFontFam(defaultSystemFont);
	LMSetSysFontSize(defaultSystemFontSize);
}

//	Subclass responsibility:
OSErr	CEventHandler::OpenFSSpec(
	FSSpec	*theFile,
	Boolean	notify)
{

}

Boolean	CEventHandler::AvoidReopen(
	FSSpec	*theFile,
	Boolean	doSelect)
{
	CWindowCommander	*aWindow;
	
	aWindow = itsWindowList;
	
	while(aWindow)
	{	if(aWindow->AvoidReopen(theFile, doSelect))
		{	return true;
		}
		
		aWindow = aWindow->nextWindow;
	}
	
	return false;
}

CCommander *CEventHandler::BeginDialog()
{
	CWindowCommander	*theActive = NULL;
	
	modelessLevel++;
	
	if(itsWindowList)
	{	theActive = itsWindowList->FindActiveWindow();
		if(theActive)
		{	theActive->isActive = false;
			HiliteWindow(theActive->itsWindow, false);
			theActive->DoActivateEvent();
			theActive->DoUpdate();
		}
	}
	return theActive;
}

void	CEventHandler::EndDialog(
	CCommander	*theOldActive)
{
	CWindowCommander	*theActive;

	modelessLevel--;

	theActive = (CWindowCommander *)theOldActive;
	if(theActive)
	{	theActive->isActive = true;
		HiliteWindow(theActive->itsWindow, true);
		theActive->DoActivateEvent();
		theActive->DoUpdate();
	}
}

void	CEventHandler::LoadPreferences()
{
	FSSpec	thePrefs;
	OSErr	iErr;
	
	iErr = FindFolder(kOnSystemDisk, kPreferencesFolderType, true,
						&thePrefs.vRefNum, &thePrefs.parID);
	
	if(iErr != noErr)
	{	thePrefs.vRefNum = 0;
		thePrefs.parID = 0;
	}
	
	GetIndString(thePrefs.name, kApplicationStringList, kPreferencesFileNameString);
	if(thePrefs.name[0])
	{	iErr = FSpCreate(&thePrefs, gApplicationSignature, gApplicationPreferencesType, 0);
		iErr = FSpOpenDF(&thePrefs, fsRdWrPerm, &prefsFileRef);
	}
	else
	{	iErr = fnfErr;
	}
	
	prefsBase = new CCompactTagBase;
	prefsBase->ITagBase();

	if(iErr == noErr)	
	{	prefsBase->ReadFromFile(prefsFileRef);
	}
	else
	{	prefsFileRef = 0;
	}
}

void	CEventHandler::SavePreferences()
{
	OSErr	iErr;
	
	if(prefsFileRef != 0)
	{	iErr = SetFPos(prefsFileRef, fsFromStart, 0);
		iErr = prefsBase->WriteToFile(prefsFileRef);
		if(iErr)	SetEOF(prefsFileRef, 0);
	}
}

short	CEventHandler::ReadShortPref(
	OSType	tag,
	short	defaultValue)
{
	return prefsBase->ReadShort(tag, defaultValue);
}

void	CEventHandler::WriteShortPref(
	OSType	tag,
	short	value)
{
	prefsBase->WriteShort(tag, value);
}

long	CEventHandler::ReadLongPref(
	OSType	tag,
	long	defaultValue)
{
	return prefsBase->ReadLong(tag, defaultValue);
}

void	CEventHandler::WriteLongPref(
	OSType	tag,
	long	value)
{
	prefsBase->WriteLong(tag, value);
}

void	CEventHandler::ReadStringPref(
	OSType		tag,
	StringPtr	dest)
{
	prefsBase->ReadString(tag, dest);
}


void	CEventHandler::WriteStringPref(
	OSType		tag,
	StringPtr	value)
{
	prefsBase->WriteString(tag, value);
}

void	CEventHandler::NotifyUser(
	Handle	theSound)
{
	if(isSuspended)
	{	if(myNotify.nmRefCon == false)
		{	OSErr	theErr;

			myNotify.qType = nmType;
			myNotify.nmMark = 1;
			myNotify.nmIcon = NULL;
			myNotify.nmSound = theSound;
			myNotify.nmStr = NULL;
			myNotify.nmRefCon = true;
			theErr = NMInstall(&myNotify);
		}
	}
	else
	{	SndPlay(NULL, (SndListHandle)theSound, false);
	}
}

static
pascal
long	ApplicationGrowZoneProc(
	Size	bytesNeeded)
{
	long	result;
	long	oldA5;
	
	oldA5 = SetCurrentA5();

	result = gApplication->DoGrowZone(bytesNeeded);

	SetA5(oldA5);
	
	return result;
}

long	CEventHandler::DoGrowZone(
	Size	bytesNeeded)
{
	long	gotSize;

	if(firstReserve)
	{	gotSize = GetPtrSize(firstReserve);
		DisposePtr(firstReserve);
		firstReserve = NULL;
		warnUser = true;
		
		return gotSize;
	}
	
	if(secondReserve)
	{	gotSize = GetPtrSize(secondReserve);
		DisposePtr(secondReserve);
		secondReserve = NULL;
		warnUser = true;
		
		return gotSize;
	}
	
	return 0;
}

void	CEventHandler::InstallGrowZone(
	long	firstReserveSize,
	long	secondReserveSize)
{
	secondReserve = NewPtr(secondReserveSize);
	firstReserve = NewPtr(firstReserveSize);
	
	SetGrowZone(NewGrowZoneProc(ApplicationGrowZoneProc));
}

void	CEventHandler::SetCommandParams(
	long	param1,
	long	param2,
	long	param3,
	long	param4)
{
	commandParam[0] = param1;
	commandParam[1] = param2;
	commandParam[2] = param3;
	commandParam[3] = param4;
}

long	CEventHandler::GetCommandResult()
{
	return commandResult;
}