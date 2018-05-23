// SearchView.cpp : implementation file
//
// Copyright (c) 1999-2005 by QUALCOMM, Incorporated
/* Copyright (c) 2016, Computer History Museum 
All rights reserved. 
Redistribution and use in source and binary forms, with or without modification, are permitted (subject to 
the limitations in the disclaimer below) provided that the following conditions are met: 
 * Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer. 
 * Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following 
   disclaimer in the documentation and/or other materials provided with the distribution. 
 * Neither the name of Computer History Museum nor the names of its contributors may be used to endorse or promote products 
   derived from this software without specific prior written permission. 
NO EXPRESS OR IMPLIED LICENSES TO ANY PARTY'S PATENT RIGHTS ARE GRANTED BY THIS LICENSE. THIS SOFTWARE IS PROVIDED BY THE 
COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT 
HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES 
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING 
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH 
DAMAGE. */



#include "stdafx.h"

#include <dos.h>

#include "eudora.h"
#include "resource.h"
#include "rs.h"
#include "cursor.h"
#include "fileutil.h"
#include "summary.h" 
#include "doc.h"
#include "ListCtrlEx.h"
#include "tocdoc.h"
#include "msgdoc.h"
#include "msgframe.h"
#include "mdichild.h"
#include "SearchDoc.h"
#include "headervw.h"
#include "compmsgd.h"

//#include "mboxtree.h"
#include "QCMailboxDirector.h"
#include "MBoxTreeCtrlCheck.h"
#include "QCMailboxCommand.h"
#include "QCCommandStack.h"
#include "TocView.h"
#include "Persona.h"
#include "LabelComboBox.h"
#include "guiutils.h" // ErrorDialog, EscapePressed
// #include "QCUtils.h" // RemovePrefixMT
#include "SearchCriteria.h"
#include "SearchEngine.h"
#include "SearchResult.h"
#include "QCFindMgr.h"
#include "filtersd.h"
#include "JunkMail.h"

#include "Progress.h"
#include "EudoraMsgs.h"
#include "QCSharewareManager.h"

#include "mainfrm.h"

#include "helpcntx.h"

#include "SearchView.h"
#include "XMLWriter.h"

// Include the files in the correct order to allow leak checking with malloc.
// CRTDBG_MAP_ALLOC already defined in stdafx.h
#include <stdlib.h>
#include <crtdbg.h>

#include "DebugNewHelpers.h"


// --------------------------------------------------------------------------

extern QCMailboxDirector		g_theMailboxDirector;
extern QCCommandStack			g_theCommandStack;

extern int						MessageCascadeSpot;

// --------------------------------------------------------------------------

#ifdef _DEBUG
// #define _MY_TRACE_
#define _MY_TICK_TRACE_
#endif // _DEBUG

#define CONTROL_EDGE_SPACING		(5)
#define STATUS_TEXT_MIN_WIDTH		(20)
#define MAX_LISTCOUNT				(SHRT_MAX)

#define TICK_DELTA                  (100)

// --------------------------------------------------------------------------

inline BOOL ControlDown()
  	{ return ((GetKeyState(VK_CONTROL) < 0) ? TRUE : FALSE); }

inline BOOL RightShift()
  	{ return ((GetKeyState(VK_RSHIFT) < 0) ? TRUE : FALSE); }

// --------------------------------------------------------------------------


//	Constants
const char *				CSearchView::XMLParser::kXMLBaseContainer = "SavedSearchInfo";
const char *				CSearchView::XMLParser::kKeyDataFormatVersion = "DataFormatVersion";
const char *				CSearchView::XMLParser::kKeySearchCriteriaCount = "SearchCriteriaCount";
const char *				CSearchView::XMLParser::kKeySearchCriteria = "SearchCriteria";
const char *				CSearchView::XMLParser::kKeySearchCriterion = "SearchCriterion";
const char *				CSearchView::XMLParser::kKeySortColumns = "SortColumns";
const char *				CSearchView::XMLParser::kKeySortBy = "SortBy";

//	Static element map
ElementMap					CSearchView::XMLParser::elementMapArr[] =
								{
									id_baseContainer,				const_cast<char *>(kXMLBaseContainer),
									id_keyDataFormatVersion,		const_cast<char *>(kKeyDataFormatVersion),
									id_keySearchCriteriaCount,		const_cast<char *>(kKeySearchCriteriaCount),
									id_keySearchCriteria,			const_cast<char *>(kKeySearchCriteria),
									id_keySearchCriterion,			const_cast<char *>(kKeySearchCriterion),
									id_keySortColumns,				const_cast<char *>(kKeySortColumns),
									id_keySortBy,					const_cast<char *>(kKeySortBy),
									id_none,						"Always the last one"
								};


void MyTrace(LPCTSTR lpszFormat, ...)
{
	va_list args;
	va_start(args, lpszFormat);

	int nBuf;
	TCHAR szBuffer[512];

	nBuf = _vsntprintf(szBuffer, (sizeof(szBuffer)/sizeof(TCHAR)), lpszFormat, args);

	// was there an error? was the expanded string too long?
	ASSERT(nBuf >= 0);

	OutputDebugString(szBuffer);

	va_end(args);
}


// --------------------------------------------------------------------------

class CMsgResult
{
public:
	CMsgResult()
		: m_sPath(""), m_nMsgID(0), m_sSearchStr(""),
			m_sMbxName(""), m_sSubject(""), m_sWho(""), m_DateSecs(0) { }

	CMsgResult(const SearchResult &result)
		: m_sPath(result.GetMbxPath()), m_nMsgID(result.GetMsgID()), m_sSearchStr(""),
			m_sMbxName(result.GetMbxPath()),
			m_sWho(result.GetWho()), m_DateSecs(result.GetSeconds())
	{
		// Instead of saving the original subject, we only
		// save what we are going to need for sorting
		m_sSubject = RemoveSubjectPrefixMT((LPCSTR)result.GetSubject());
	}

	CMsgResult &operator=(const CMsgResult &copy)
	{
		m_sPath = copy.m_sPath;
		m_sSearchStr = copy.m_sSearchStr;
		m_nMsgID = copy.m_nMsgID;
		m_sMbxName = copy.m_sMbxName;
		m_sSubject = copy.m_sSubject;
		m_sWho = copy.m_sWho;
		m_DateSecs = copy.m_DateSecs;

		return (*this);
	}

	CString m_sPath, m_sSearchStr;
	long m_nMsgID;

	// Sorting stuff -- these are only used in sorting, not display
	CString m_sMbxName, m_sSubject, m_sWho;
	long m_DateSecs;
}; // CMsgResult

// --------------------------------------------------------------------------

class SingleCritState
{
public:
	SingleCritState();
	SingleCritState(const SingleCritState &); // Copy constructor
	SingleCritState(const CriteriaObject &); // Init constructor
	~SingleCritState();

	SingleCritState &operator=(const SingleCritState &);
	bool UpdateObj(const CriteriaObject &obj);

	CriteriaObject m_CurObj;
	CriteriaVerbType m_CurVerbType;
	CriteriaValueType m_CurValueType;
	bool m_bCurShowUnits;
	CString m_UnitsStr;
};

// ------------------------------------------------------------------------------------------
//		* XMLParser::XMLParser														[Public]
// ------------------------------------------------------------------------------------------
//	XMLParser constructor.

CSearchView::XMLParser::XMLParser()
	:	XmlParser(), m_elementIDsQueue(), m_szElementData(),
		m_nSearchCriteriaCount(0), m_nCurrentSearchCriterion(0)
{

}


// ------------------------------------------------------------------------------------------
//		* XMLParser::initElementMap												   [Private]
// ------------------------------------------------------------------------------------------
//	Inits the element map for the base class XmlParser to use.
//
//	Parameters:
//		out_pMap:			Place to put our pre-existing element map

//	Returns:
//		true for success every time - providing the pre-existing element map is easy.

bool
CSearchView::XMLParser::initElementMap(
	ElementMap **			out_pMap)
{
	*out_pMap = elementMapArr;
	
	return true;
}


// ------------------------------------------------------------------------------------------
//		* XMLParser::handleData													   [Private]
// ------------------------------------------------------------------------------------------
//	Accumulates data for use in endElement.
//
//	Parameters:
//		in_nID:				ID for current element
//		in_pData:			Pointer to data to accumulate
//		in_nDataLength:		Length of data to accumulate

void
CSearchView::XMLParser::handleData(
	int						in_nID,
	const char *			in_pData,
	int						in_nDataLength)
{
	int		nNewDataLength = m_szElementData.GetLength() + in_nDataLength;
	
	char *	pElementData = m_szElementData.GetBuffer(nNewDataLength);
	
	strncat(pElementData, in_pData, in_nDataLength);

	m_szElementData.ReleaseBuffer(nNewDataLength);
}


// ------------------------------------------------------------------------------------------
//		* XMLParser::ProcessDataForID											   [Private]
// ------------------------------------------------------------------------------------------
//	Processes accumulated data for a given ID.
//
//	Parameters:
//		in_nID:				ID for element to process

void
CSearchView::XMLParser::ProcessDataForID(
	int						in_nID)
{
	switch (in_nID)
	{
		case id_keyDataFormatVersion:
			{
				//	atol will return 0 if there's an error parsing the data.
				//	That's why we started our data version numbering with 1.

				//	We don't currently use the data format version, and we may never
				//	need to (part of the point of XML is to avoid the need for this
				//	sort of thing). That said it never hurts to allow for future needs.
				//	
				//	Uncomment if information is needed in the future.
				//long		nDataFormatVersion = atol(m_szElementData);
			}
			break;
	
		case id_keySearchCriteriaCount:
			m_nSearchCriteriaCount = atol(m_szElementData);
			break;
				
		case id_keySearchCriterion:
			ASSERT(m_nCurrentSearchCriterion < MAX_CONTROLS_CRITERIA);
			if (m_nCurrentSearchCriterion < MAX_CONTROLS_CRITERIA)
			{
				m_szSearchCriteria[m_nCurrentSearchCriterion] = m_szElementData;
				m_nCurrentSearchCriterion++;
			}
			break;
	}
	
	m_szElementData.Empty();
}


// ------------------------------------------------------------------------------------------
//		* XMLParser::startElement												   [Private]
// ------------------------------------------------------------------------------------------
//	Called when we hit the start tag for a given element.
//
//	Parameters:
//		in_nID:				ID for current element
//		in_szName:			Name of element
//		in_AttributeArr:	Attributes found inside the start tag

int
CSearchView::XMLParser::startElement(
	int						in_nID,
	const char *			in_szName,
	const char **			in_AttributeArr)
{
	if ( !m_szElementData.IsEmpty() )
	{
		//	We have some data accumulated from inside an XML element, but now we're
		//	starting a new element - so process the data now.
		ProcessDataForID( m_elementIDsQueue.front() );
	}

	m_elementIDsQueue.push_front(in_nID);
	
	return 0;
}


// ------------------------------------------------------------------------------------------
//		* XMLParser::endElement													   [Private]
// ------------------------------------------------------------------------------------------
//	Called when we hit the end tag for a given element.
//
//	Parameters:
//		in_nID:				ID for current element
//		in_szName:			Name of element

int
CSearchView::XMLParser::endElement(
	int						in_nID,
	const char *			in_szName)
{
	if ( !m_szElementData.IsEmpty() )
	{
		//	We have some data accumulated from inside an XML element, but now we're
		//	ending the current element - so process the data now.
		ProcessDataForID(in_nID);
	}

	m_elementIDsQueue.pop_back();
	
	return 0;
}


// --------------------------------------------------------------------------

int CResultsListCtrl::CompareItems(LPARAM lpOne, LPARAM lpTwo, int nCol)
{
	return (CSearchView::CompareResultItems(lpOne, lpTwo, nCol));
}

int CSearchView::CompareResultItems(LPARAM lpOne, LPARAM lpTwo, int nCol)
{
	CMsgResult *pMR1 = (CMsgResult *) lpOne;
	CMsgResult *pMR2 = (CMsgResult *) lpTwo;

	switch (nCol)
	{
		case COLUMN_MAILBOX:
		{
			return (pMR1->m_sMbxName.CompareNoCase(pMR2->m_sMbxName));
		}
		break;

		case COLUMN_WHO:
		{
			return (pMR1->m_sWho.CompareNoCase(pMR2->m_sWho));
		}
		break;

		case COLUMN_DATE:
		{
			const long nSec1 = pMR1->m_DateSecs;
			const long nSec2 = pMR2->m_DateSecs;

			return ((nSec1 == nSec2) ? 0 : (nSec1 < nSec2 ? (-1) : (+1)));
		}
		break;

		case COLUMN_SUBJECT:
		{
			LPCSTR pSub1 = (LPCSTR) pMR1->m_sSubject;
			LPCSTR pSub2 = (LPCSTR) pMR2->m_sSubject;

			return (stricmp(pSub1, pSub2));
		}
		break;
	}

	ASSERT(0);
	return (0);
}


// --------------------------------------------------------------------------
// CSearchView

IMPLEMENT_DYNCREATE(CSearchView, C3DFormView)

// --------------------------------------------------------------------------

BEGIN_MESSAGE_MAP(CSearchView, C3DFormView)
	//{{AFX_MSG_MAP(CSearchView)
	ON_MESSAGE(msgFindMsgReloadCriteria, OnMsgReloadCriteria)
	ON_MESSAGE(msgFindMsgMaiboxSel, OnMsgMailboxSelect)
	ON_MESSAGE(msgFindMsgParentFolderSel, OnMsgParentFolderSelect)
	ON_MESSAGE(msgFindMsgAllMailboxesSel, OnMsgAllMailboxesSelect)
	ON_UPDATE_COMMAND_UI(ID_EDIT_FIND_FINDTEXT, OnUpdateEditFindFindText)
	ON_UPDATE_COMMAND_UI(ID_EDIT_FIND_FINDTEXTAGAIN, OnUpdateEditFindFindTextAgain)
	ON_REGISTERED_MESSAGE(WM_FINDREPLACE, OnFindReplace)
	ON_BN_CLICKED(IDC_SEARCHWND_BEGIN_BTN, OnOk)
	ON_BN_CLICKED(IDC_SEARCHWND_MORE_BTN, OnMoreBtn)
	ON_BN_CLICKED(IDC_SEARCHWND_LESS_BTN, OnLessBtn)
	ON_WM_DESTROY()
	ON_WM_SIZE()
	ON_NOTIFY(TCN_SELCHANGE, IDC_SEARCHWND_RESULTS_TAB, OnTabSelchange)
	ON_REGISTERED_MESSAGE(msgListCtrlEx_LBtnDblClk, OnMsgListDblClk)
	ON_REGISTERED_MESSAGE(msgListCtrlEx_ReturnKey, OnMsgListReturnKey)
	ON_REGISTERED_MESSAGE(msgListCtrlEx_DeleteKey, OnMsgListDeleteKey)
	ON_UPDATE_COMMAND_UI(ID_OPEN_TOC, OnUpdateNeedsSelection)
	ON_UPDATE_COMMAND_UI(ID_OPEN_MSG, OnUpdateNeedsSelection)
	ON_UPDATE_COMMAND_UI(ID_MESSAGE_FORWARD, OnUpdateNeedsSelection)
	ON_UPDATE_COMMAND_UI(ID_MESSAGE_DELETE, OnUpdateDelete)
	ON_UPDATE_COMMAND_UI(ID_JUNK, OnUpdateJunk)
	ON_UPDATE_COMMAND_UI(ID_NOT_JUNK, OnUpdateNotJunk)
	ON_UPDATE_COMMAND_UI(ID_EDIT_CLEAR, OnUpdateDelete)
	ON_UPDATE_COMMAND_UI(ID_MESSAGE_REPLY, OnUpdateResponse)
	ON_UPDATE_COMMAND_UI(ID_MESSAGE_REPLY_ALL, OnUpdateResponse)
	ON_UPDATE_COMMAND_UI(ID_MESSAGE_REDIRECT, OnUpdateResponse)
	ON_UPDATE_COMMAND_UI_RANGE(QC_FIRST_COMMAND_ID, QC_LAST_COMMAND_ID, OnUpdateDynamicCommand)
	ON_COMMAND_EX_RANGE(ID_OPEN_TOC, ID_OPEN_TOC, OnOpenCommand)
	ON_COMMAND_EX_RANGE(ID_OPEN_MSG, ID_OPEN_MSG, OnOpenCommand)
	ON_COMMAND(ID_MESSAGE_DELETE, OnDeleteMessages)
	ON_COMMAND_EX_RANGE(ID_JUNK, ID_NOT_JUNK, OnJunkCommand)
	ON_COMMAND(ID_EDIT_CLEAR, OnDeleteMessages)
	ON_COMMAND_EX_RANGE(ID_MESSAGE_FORWARD, ID_MESSAGE_FORWARD, OnComposeMessage)
	ON_COMMAND_EX_RANGE(ID_MESSAGE_REPLY, ID_MESSAGE_REPLY, OnComposeMessage)
	ON_COMMAND_EX_RANGE(ID_MESSAGE_REPLY_ALL, ID_MESSAGE_REPLY_ALL, OnComposeMessage)
	ON_COMMAND_EX_RANGE(ID_MESSAGE_REDIRECT, ID_MESSAGE_REDIRECT, OnComposeMessage)
	ON_COMMAND_EX_RANGE(QC_FIRST_COMMAND_ID, QC_LAST_COMMAND_ID, OnDynamicCommand)
	ON_REGISTERED_MESSAGE(msgListCtrlEx_RBtn, OnMsgListRightClick)
	ON_REGISTERED_MESSAGE(msgMBoxTreeCtrlCheck_CheckChange, OnMsgTreeCheckChange)
	ON_CBN_SELCHANGE(IDC_CRITERIA_CATEGORY_COMBO_1, OnSelchangeCriteriaCategoryCombo1)
	ON_CBN_SELCHANGE(IDC_CRITERIA_CATEGORY_COMBO_2, OnSelchangeCriteriaCategoryCombo2)
	ON_CBN_SELCHANGE(IDC_CRITERIA_CATEGORY_COMBO_3, OnSelchangeCriteriaCategoryCombo3)
	ON_CBN_SELCHANGE(IDC_CRITERIA_CATEGORY_COMBO_4, OnSelchangeCriteriaCategoryCombo4)
	ON_CBN_SELCHANGE(IDC_CRITERIA_CATEGORY_COMBO_5, OnSelchangeCriteriaCategoryCombo5)
	ON_EN_CHANGE(IDC_CRITERIA_TEXT_EDIT_1, OnChangeTextEdit1)
	ON_EN_CHANGE(IDC_CRITERIA_TEXT_EDIT_2, OnChangeTextEdit2)
	ON_EN_CHANGE(IDC_CRITERIA_TEXT_EDIT_3, OnChangeTextEdit3)
	ON_EN_CHANGE(IDC_CRITERIA_TEXT_EDIT_4, OnChangeTextEdit4)
	ON_EN_CHANGE(IDC_CRITERIA_TEXT_EDIT_5, OnChangeTextEdit5)
	ON_COMMAND(ID_EDIT_SELECT_ALL, OnEditSelectAll)
	ON_UPDATE_COMMAND_UI(ID_EDIT_SELECT_ALL, OnUpdateEditSelectAll)
	ON_UPDATE_COMMAND_UI(ID_SEARCH_OPTIONS_BTNMENU_USE_INDEXED_SEARCH, OnUpdateUseIndexedSearch)
	ON_UPDATE_COMMAND_UI(ID_SEARCH_OPTIONS_BTNMENU_FIND_DELETED_IMAP_MESSAGES, OnUpdateFindDeletedIMAPMessages)
	ON_COMMAND(ID_SEARCH_OPTIONS_BTNMENU_USE_INDEXED_SEARCH, OnUseIndexedSearch)
	ON_COMMAND(ID_SEARCH_OPTIONS_BTNMENU_FIND_DELETED_IMAP_MESSAGES, OnFindDeletedIMAPMessages)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

// --------------------------------------------------------------------------
CSearchView::CSearchView()
	: C3DFormView(CSearchView::IDD), m_bInitilized(FALSE), m_nCheckCount(0), m_nFoundCount(0), m_nIMAPSkippedCount(0), m_bShowFoundCount(FALSE),
	  m_CurCritCount(0), m_CriteriaBottomPos(0), m_bInitdOffsets(false), m_bInitFinal(false),
	  m_MsgResultArr(NULL), m_MsgResultArrCount(0), m_UseFastWay(true),
	  m_bUseIndexedSearchIfAvailable(true), m_bSearchButtonMenuInitialized(false)
{
	m_bShouldIncludeDeletedIMAPMessages = ( GetIniShort(IDS_INI_FIND_DELETED_IMAP_MESSAGES) != 0 );

//	if (RightShift())
		m_UseFastWay = false; // Fast way may be unsafe; use slow way for now

#ifdef _MY_TICK_TRACE_
	if (m_UseFastWay)
		::MyTrace("CSearchView initializing with FAST code.\n");
	else
		::MyTrace("CSearchView initializing with SLOW code.\n");
#endif // _MY_TICK_TRACE_

	m_CritState = DEBUG_NEW SingleCritState[MAX_CONTROLS_CRITERIA];

	for (int idx=0; idx < MAX_CONTROLS_CRITERIA; idx++)
	{
		m_bCritCreated[idx] = false;
		m_bCritInitd[idx] = false;
	}

	m_pNewMboxCommand = NULL;

	QCSharewareManager *pSWM = GetSharewareManager();
	if (pSWM)
		pSWM->Register(this);
}

// --------------------------------------------------------------------------

CSearchView::~CSearchView()
{
	delete [] m_CritState;
	m_CritState = NULL;

	QCSharewareManager *pSWM = GetSharewareManager();
	if (pSWM)
		pSWM->UnRegister(this);
}

// --------------------------------------------------------------------------

/*virtual*/ void CSearchView::Notify(QCCommandObject*, COMMAND_ACTION_TYPE theAction, void* pData /*= NULL*/)
{
	if (theAction != CA_SWM_CHANGE_FEATURE)
	{
		ASSERT(0);
		return;
	}

	const bool bCurrentModeIsFull = UsingFullFeatureSet();
	const bool bOldModeWasFull = pData? (*(SharewareModeType*)pData != SWM_MODE_LIGHT) : !bCurrentModeIsFull;

	if (bCurrentModeIsFull != bOldModeWasFull)
		ReloadCriteria();
}


void CSearchView::NotifyIndexedSearchAvailabilityChanged()
{
	if ( gSearchDoc && gSearchDoc->GetFirstViewPosition() )
	{
		POSITION		pos = gSearchDoc->GetFirstViewPosition();

		if (pos)
		{
			CSearchView *	pSearchView = (CSearchView *) gSearchDoc->GetNextView(pos);

			if (pSearchView)
			{
				for (int i = 0; i < pSearchView->m_CurCritCount; i++)
					pSearchView->InitializeTextCompareCombo(i);

				pSearchView->UpdateSearchBtn();
			}
		}
	}
}


void CSearchView::UpdatePathnamesInAllSearchResults(
	const CString &			in_szOldPathname, 
	const CString &			in_szNewPathname)
{
	if ( gSearchDoc && gSearchDoc->GetFirstViewPosition() )
	{
		POSITION		pos = gSearchDoc->GetFirstViewPosition();

		if (pos)
		{
			CSearchView *	pSearchView = (CSearchView *) gSearchDoc->GetNextView(pos);

			if (pSearchView)
				pSearchView->UpdatePathnamesInSearchResults(in_szOldPathname, in_szNewPathname);
		}
	}
}


void CSearchView::UpdatePathnamesInSearchResults(
	const CString &			in_szOldPathname, 
	const CString &			in_szNewPathname)
{
	int				nItem = -1;
	int				nOldPathLength = in_szOldPathname.GetLength();
	
	//	Find update all search result items that are affected by the pathname change. 
	while ( (nItem = m_ResultsList.GetNextItem(nItem, LVNI_ALL)) != -1 )
	{
		CMsgResult *	pMsgResult = reinterpret_cast<CMsgResult *>( m_ResultsList.GetItemData(nItem) );

		if (pMsgResult)
		{
			//	Be sure to just compare the part of the path that might match
			//	* Check to see that the message result partial path matches the old path
			//	  (comparing just the part of the path that might match)
			//	* Make sure that the message result partial path is long enough for us to
			//	  safely check for a terminating backslash
			//	* Check for a terminating backslash (i.e. so that c:\somepath\foo doesn't
			//	  incorrectly match c:\somepath\foobar\test.mbx)
			if ( (in_szOldPathname.CompareNoCase(pMsgResult->m_sPath.Left(nOldPathLength)) == 0) &&
				 (pMsgResult->m_sPath.GetLength() > nOldPathLength) &&
				 (pMsgResult->m_sPath[nOldPathLength] == '\\') )
			{
				CString		szRemainingPathName = pMsgResult->m_sPath.Right(pMsgResult->m_sPath.GetLength() - in_szOldPathname.GetLength() - 1);

				pMsgResult->m_sPath = in_szNewPathname + "\\" + szRemainingPathName;
			}
		}
	}
}


void CSearchView::UpdateMailboxNamesInAllSearchResults(
	const CString &			in_szOldMailboxPath, 
	const char *			in_szNewMailboxPath,
	const char *			in_szNewMailboxName)
{
	if ( gSearchDoc && gSearchDoc->GetFirstViewPosition() )
	{
		POSITION		pos = gSearchDoc->GetFirstViewPosition();

		if (pos)
		{
			CSearchView *	pSearchView = (CSearchView *) gSearchDoc->GetNextView(pos);

			if (pSearchView)
			{
				pSearchView->UpdateMailboxNamesInSearchResults( in_szOldMailboxPath,
																in_szNewMailboxPath,
																in_szNewMailboxName );
			}
		}
	}
}


void CSearchView::UpdateMailboxNamesInSearchResults(
	const CString &			in_szOldMailboxPath, 
	const char *			in_szNewMailboxPath,
	const char *			in_szNewMailboxName)
{
	int				nItem = -1;
	
	//	Find and update all search results for the indicated mailbox.
	while ( (nItem = m_ResultsList.GetNextItem(nItem, LVNI_ALL)) != -1 )
	{
		CMsgResult *	pMsgResult = reinterpret_cast<CMsgResult *>( m_ResultsList.GetItemData(nItem) );

		if ( pMsgResult && (in_szOldMailboxPath.CompareNoCase(pMsgResult->m_sPath) == 0) )
		{
			pMsgResult->m_sMbxName = in_szNewMailboxPath;
			pMsgResult->m_sPath = in_szNewMailboxPath;

			m_ResultsList.SetItemText(nItem, COLUMN_MAILBOX, in_szNewMailboxName);
		}
	}
}


// --------------------------------------------------------------------------
// I am leaving the lines commented in this function so that in case we need to use
// classwizard to change/add stuff, we should be able to do it quite easily.
// Since initially all the controls were created statically, these entries were made
// using classwizard. To optimize resource usage, i have changed the static creation
// to dynamic creation.
// If we need to add/change functions/variables using classwizard, one will have to fool 
// the wizard to first recognize the commented lines, add/modify the changes & then 
// comment the lines again  - 04/25/2000
// --------------------------------------------------------------------------

void 
CSearchView::DoDataExchange(CDataExchange* pDX)
{
	C3DFormView::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CSearchView)
	DDX_Control(pDX, IDC_SEARCHWND_RESULTS_LIST, m_ResultsList);
	DDX_Control(pDX, IDC_SEARCHWND_RESULTS_TAB, m_ResultsMbxTabCtrl);
	DDX_Control(pDX, IDC_SEARCHWND_MAILBOX_TREE, m_MBoxTree);
	DDX_Control(pDX, IDC_SEARCHWND_RESULTS_STATIC, m_ResultsStatic);
    DDX_Control(pDX, IDC_SEARCHWND_X1_STATIC, m_PoweredByX1Static);
    DDX_Control(pDX, IDC_SEARCHWND_X1_ICON, m_PoweredByX1Icon);
	DDX_Control(pDX, IDC_SEARCHWND_BEGIN_BTN, m_BeginBtn);
	DDX_Control(pDX, IDC_SEARCHWND_MORE_BTN, m_MoreBtn);
	DDX_Control(pDX, IDC_SEARCHWND_LESS_BTN, m_LessBtn);
	DDX_Control(pDX, IDC_SEARCHWND_OPTIONS_BUTTON, m_OptionsBtn);
	DDX_Control(pDX, IDC_SEARCHWND_AND_RADIO, m_AndRadioBtn);
	DDX_Control(pDX, IDC_SEARCHWND_OR_RADIO, m_OrRadioBtn);

	/*DDX_Control(pDX, IDC_CRITERIA_NUM_UNITS_STATIC_1, m_NumStatic[0]);
	DDX_Control(pDX, IDC_CRITERIA_NUM_EDIT_1, m_NumEdit[0]);
	DDX_Control(pDX, IDC_CRITERIA_NUM_SPIN_1, m_NumSpin[0]);
	DDX_Control(pDX, IDC_CRITERIA_CATEGORY_COMBO_1, m_CategoryCombo[0]);
	DDX_Control(pDX, IDC_CRITERIA_TEXT_COMPARE_COMBO_1, m_TextCompareCombo[0]);
	DDX_Control(pDX, IDC_CRITERIA_EQUAL_COMBO_1, m_EqualCompareCombo[0]);
	DDX_Control(pDX, IDC_CRITERIA_NUM_COMPARE_COMBO_1, m_NumCompareCombo[0]);
	DDX_Control(pDX, IDC_CRITERIA_DATE_COMPARE_COMBO_1, m_DateCompareCombo[0]);
	DDX_Control(pDX, IDC_CRITERIA_TEXT_EDIT_1, m_TextEdit[0]);
	DDX_Control(pDX, IDC_CRITERIA_STATUS_COMBO_1, m_StatusCombo[0]);
	DDX_Control(pDX, IDC_CRITERIA_LABEL_COMBO_1, m_LabelCombo[0]);
	DDX_Control(pDX, IDC_CRITERIA_PERSONA_COMBO_1, m_PersonaCombo[0]);
	DDX_Control(pDX, IDC_CRITERIA_PRIORITY_COMBO_1, m_PriorityCombo[0]);

	DDX_Control(pDX, IDC_CRITERIA_NUM_UNITS_STATIC_2, m_NumStatic[1]);
	DDX_Control(pDX, IDC_CRITERIA_NUM_EDIT_2, m_NumEdit[1]);
	DDX_Control(pDX, IDC_CRITERIA_NUM_SPIN_2, m_NumSpin[1]);
	DDX_Control(pDX, IDC_CRITERIA_CATEGORY_COMBO_2, m_CategoryCombo[1]);
	DDX_Control(pDX, IDC_CRITERIA_TEXT_COMPARE_COMBO_2, m_TextCompareCombo[1]);
	DDX_Control(pDX, IDC_CRITERIA_EQUAL_COMBO_2, m_EqualCompareCombo[1]);
	DDX_Control(pDX, IDC_CRITERIA_NUM_COMPARE_COMBO_2, m_NumCompareCombo[1]);
	DDX_Control(pDX, IDC_CRITERIA_DATE_COMPARE_COMBO_2, m_DateCompareCombo[1]);
	DDX_Control(pDX, IDC_CRITERIA_TEXT_EDIT_2, m_TextEdit[1]);
	DDX_Control(pDX, IDC_CRITERIA_STATUS_COMBO_2, m_StatusCombo[1]);
	DDX_Control(pDX, IDC_CRITERIA_LABEL_COMBO_2, m_LabelCombo[1]);
	DDX_Control(pDX, IDC_CRITERIA_PERSONA_COMBO_2, m_PersonaCombo[1]);
	DDX_Control(pDX, IDC_CRITERIA_PRIORITY_COMBO_2, m_PriorityCombo[1]);

	DDX_Control(pDX, IDC_CRITERIA_NUM_UNITS_STATIC_3, m_NumStatic[2]);
	DDX_Control(pDX, IDC_CRITERIA_NUM_EDIT_3, m_NumEdit[2]);
	DDX_Control(pDX, IDC_CRITERIA_NUM_SPIN_3, m_NumSpin[2]);
	DDX_Control(pDX, IDC_CRITERIA_CATEGORY_COMBO_3, m_CategoryCombo[2]);
	DDX_Control(pDX, IDC_CRITERIA_TEXT_COMPARE_COMBO_3, m_TextCompareCombo[2]);
	DDX_Control(pDX, IDC_CRITERIA_EQUAL_COMBO_3, m_EqualCompareCombo[2]);
	DDX_Control(pDX, IDC_CRITERIA_NUM_COMPARE_COMBO_3, m_NumCompareCombo[2]);
	DDX_Control(pDX, IDC_CRITERIA_DATE_COMPARE_COMBO_3, m_DateCompareCombo[2]);
	DDX_Control(pDX, IDC_CRITERIA_TEXT_EDIT_3, m_TextEdit[2]);
	DDX_Control(pDX, IDC_CRITERIA_STATUS_COMBO_3, m_StatusCombo[2]);
	DDX_Control(pDX, IDC_CRITERIA_LABEL_COMBO_3, m_LabelCombo[2]);
	DDX_Control(pDX, IDC_CRITERIA_PERSONA_COMBO_3, m_PersonaCombo[2]);
	DDX_Control(pDX, IDC_CRITERIA_PRIORITY_COMBO_3, m_PriorityCombo[2]);

	DDX_Control(pDX, IDC_CRITERIA_NUM_UNITS_STATIC_4, m_NumStatic[3]);
	DDX_Control(pDX, IDC_CRITERIA_NUM_EDIT_4, m_NumEdit[3]);
	DDX_Control(pDX, IDC_CRITERIA_NUM_SPIN_4, m_NumSpin[3]);
	DDX_Control(pDX, IDC_CRITERIA_CATEGORY_COMBO_4, m_CategoryCombo[3]);
	DDX_Control(pDX, IDC_CRITERIA_TEXT_COMPARE_COMBO_4, m_TextCompareCombo[3]);
	DDX_Control(pDX, IDC_CRITERIA_EQUAL_COMBO_4, m_EqualCompareCombo[3]);
	DDX_Control(pDX, IDC_CRITERIA_NUM_COMPARE_COMBO_4, m_NumCompareCombo[3]);
	DDX_Control(pDX, IDC_CRITERIA_DATE_COMPARE_COMBO_4, m_DateCompareCombo[3]);
	DDX_Control(pDX, IDC_CRITERIA_TEXT_EDIT_4, m_TextEdit[3]);
	DDX_Control(pDX, IDC_CRITERIA_STATUS_COMBO_4, m_StatusCombo[3]);
	DDX_Control(pDX, IDC_CRITERIA_LABEL_COMBO_4, m_LabelCombo[3]);
	DDX_Control(pDX, IDC_CRITERIA_PERSONA_COMBO_4, m_PersonaCombo[3]);
	DDX_Control(pDX, IDC_CRITERIA_PRIORITY_COMBO_4, m_PriorityCombo[3]);

	DDX_Control(pDX, IDC_CRITERIA_NUM_UNITS_STATIC_5, m_NumStatic[4]);
	DDX_Control(pDX, IDC_CRITERIA_NUM_EDIT_5, m_NumEdit[4]);
	DDX_Control(pDX, IDC_CRITERIA_NUM_SPIN_5, m_NumSpin[4]);
	DDX_Control(pDX, IDC_CRITERIA_CATEGORY_COMBO_5, m_CategoryCombo[4]);
	DDX_Control(pDX, IDC_CRITERIA_TEXT_COMPARE_COMBO_5, m_TextCompareCombo[4]);
	DDX_Control(pDX, IDC_CRITERIA_EQUAL_COMBO_5, m_EqualCompareCombo[4]);
	DDX_Control(pDX, IDC_CRITERIA_NUM_COMPARE_COMBO_5, m_NumCompareCombo[4]);
	DDX_Control(pDX, IDC_CRITERIA_DATE_COMPARE_COMBO_5, m_DateCompareCombo[4]);
	DDX_Control(pDX, IDC_CRITERIA_TEXT_EDIT_5, m_TextEdit[4]);
	DDX_Control(pDX, IDC_CRITERIA_STATUS_COMBO_5, m_StatusCombo[4]);
	DDX_Control(pDX, IDC_CRITERIA_LABEL_COMBO_5, m_LabelCombo[4]);
	DDX_Control(pDX, IDC_CRITERIA_PERSONA_COMBO_5, m_PersonaCombo[4]);
	DDX_Control(pDX, IDC_CRITERIA_PRIORITY_COMBO_5, m_PriorityCombo[4]);*/

	//}}AFX_DATA_MAP
}

// --------------------------------------------------------------------------

void CSearchView::OnInitialUpdate()
{
#ifdef _MY_TRACE_
::MyTrace("CSearchView::OnInitialUpdate()\n");
#endif // _MY_TRACE_

	if (m_bInitilized)
	{
#ifdef _MY_TRACE_
::MyTrace("    CSearchView::OnInitialUpdate() [SKIPPED]\n");
#endif // _MY_TRACE_
		return; // guard against bogus double initializations
	}

	C3DFormView::OnInitialUpdate();

	// This is to fix the bug (#4552) where the focus is in the criteria
	// edit field, but no cursor.
	//
	// Here goes my theory. When the dialog is created, whomever has the focus
	// is not enabled. This is a bad state in MFC: something that cannot get
	// focus has focus. So, we need to put the focus somewhere that will
	// not be disabled during dialog init. Let's try the MORE button.
	//
	// Of course near the end of the init we set the focus to the correct
	// edit field.
	//
	m_MoreBtn.SetFocus();

	m_SaveStrSeperator = CRString(IDS_SEARCH_SAVE_CHR_SEPARATOR).GetAt(0);

	m_ResultsList.SetHighlightType(LVEX_HIGHLIGHT_ROW);

	m_ResultsList.InsertColumn(COLUMN_MAILBOX,   CRString(IDS_SEARCH_COLUMN_HEADER_MAILBOX)); // "Mailbox"
	m_ResultsList.InsertColumn(COLUMN_WHO,       CRString(IDS_SEARCH_COLUMN_HEADER_WHO));     // "Who"
	m_ResultsList.InsertColumn(COLUMN_DATE,      CRString(IDS_SEARCH_COLUMN_HEADER_DATE));    // "Date"
	m_ResultsList.InsertColumn(COLUMN_SUBJECT,   CRString(IDS_SEARCH_COLUMN_HEADER_SUBJECT)); // "Subject"

	m_ResultsList.SetColumnWidth(COLUMN_MAILBOX, GetIniShort(IDS_INI_SEARCH_MBOX_WIDTH));
	m_ResultsList.SetColumnWidth(COLUMN_WHO, GetIniShort(IDS_INI_SEARCH_WHO_WIDTH));
	m_ResultsList.SetColumnWidth(COLUMN_DATE, GetIniShort(IDS_INI_SEARCH_DATE_WIDTH));
	m_ResultsList.SetColumnWidth(COLUMN_SUBJECT, GetIniShort(IDS_INI_SEARCH_SUBJECT_WIDTH));


//	VERIFY(m_ResultsList.SetColumnAlignment(COLUMN_MAILBOX, LVCFMT_RIGHT));

	{
		// This is pretty ugly. The TC_ITEM needs to be filled with the
		// text for the tab titles. Unfortunately MFC insists that the
		// strings be passed as LPSTR via a struct.
		//
		// This means we need to create the strings (on the stack), then
		// cast them to a LPSTR. Yuck.

		CRString sResults(IDS_SEARCH_TAB_TITLE_RESULTS); // "Results"
		CRString sMailboxes(IDS_SEARCH_TAB_TITLE_MAILBOXES); // "Mailboxes"

		TC_ITEM TabCtrlItem;
		TabCtrlItem.mask = TCIF_TEXT;
		TabCtrlItem.pszText = (LPSTR) LPCSTR(sResults);
		m_ResultsMbxTabCtrl.InsertItem( TAB_RESULTS_IDX, &TabCtrlItem );

		TabCtrlItem.pszText = (LPSTR) LPCSTR(sMailboxes);
		m_ResultsMbxTabCtrl.InsertItem( TAB_MAILBOXES_IDX, &TabCtrlItem );
	}

	m_AndRadioBtn.SetCheck(1);
	m_OrRadioBtn.SetCheck(0);

//	m_ResultsMbxTabCtrl.ModifyStyle( 0, WS_CLIPCHILDREN);

	VERIFY(m_MBoxTree.Init()); // Initialize the tree control

	// Setup the tree check boxes
	VERIFY(m_MBoxTree.InitStateImageList(IDB_TREECTRL_CHECKMARKS, 16, RGB(0,255,255)));
	m_MBoxTree.SetContainerFlag();

	// Fill the tree control from the mailbox director
	g_theMailboxDirector.InitializeMailboxTreeControl( &m_MBoxTree, 0, NULL );

	// Select the first root
	m_MBoxTree.CheckAll();
//	m_MBoxTree.ExpandAll();

	// We want the list/tree ctrls to be children of the tab
	m_MBoxTree.SetParent(&m_ResultsMbxTabCtrl);
	m_ResultsList.SetParent(&m_ResultsMbxTabCtrl);
//	m_ResultsStatic.SetParent(&m_ResultsMbxTabCtrl); // This doesn't seem to work

	// But we want the dialog to get all the notification msgs
	m_MBoxTree.SetOwner(this);
	m_ResultsList.SetOwner(this);
	m_ResultsList.SetEatReturnKey(true); // Ignore old val

	m_ResultsMbxTabCtrl.SetCurSel(TAB_MAILBOXES_IDX); // Bring the mailboxes tab to the front

	m_BeginBtn.SetButtonIcon(IDR_SEARCH_WND);

	m_OptionsBtn.SetButtonIcon(IDI_OPTIONS);
	m_OptionsBtn.AddMenuItem( ID_SEARCH_OPTIONS_BTNMENU_USE_INDEXED_SEARCH,
							  CRString(ID_SEARCH_OPTIONS_BTNMENU_USE_INDEXED_SEARCH), 0 );
	m_OptionsBtn.AddMenuItem( ID_SEARCH_OPTIONS_BTNMENU_FIND_DELETED_IMAP_MESSAGES,
							  CRString(ID_SEARCH_OPTIONS_BTNMENU_FIND_DELETED_IMAP_MESSAGES), 0 );

	UpdateTabContents();
	UpdateResultsText();
	UpdateMbxText();
	UpdateSearchBtn();

	m_bInitilized = TRUE;

	CMDIChild *pParentFrame = (CMDIChild *) GetParentFrame();
	ASSERT(pParentFrame);
	ASSERT_KINDOF(CMDIChild, pParentFrame);

	pParentFrame->RecalcLayout();

	// From CScrollView::ResizeParentToFit() in viewscrl.cpp
	{
		// determine current size of the client area as if no scrollbars present
		CRect rectClient;
		GetWindowRect(rectClient);
		CRect rect = rectClient;
		CalcWindowRect(rect);
		rectClient.left += rectClient.left - rect.left;
		rectClient.top += rectClient.top - rect.top;
		rectClient.right -= rect.right - rectClient.right;
		rectClient.bottom -= rect.bottom - rectClient.bottom;
		rectClient.OffsetRect(-rectClient.left, -rectClient.top);
		ASSERT(rectClient.left == 0 && rectClient.top == 0);

		// determine desired size of the view
		CRect rectView(0, 0, m_totalDev.cx, m_totalDev.cy);
//		if (bShrinkOnly)
//		{
//			if (rectClient.right <= m_totalDev.cx)
//				rectView.right = rectClient.right;
//			if (rectClient.bottom <= m_totalDev.cy)
//				rectView.bottom = rectClient.bottom;
//		}
		CalcWindowRect(rectView, CWnd::adjustOutside);
		rectView.OffsetRect(-rectView.left, -rectView.top);
		ASSERT(rectView.left == 0 && rectView.top == 0);
//		if (bShrinkOnly)
//		{
//			if (rectClient.right <= m_totalDev.cx)
//				rectView.right = rectClient.right;
//			if (rectClient.bottom <= m_totalDev.cy)
//				rectView.bottom = rectClient.bottom;
//		}

		// dermine and set size of frame based on desired size of view
		CRect rectFrame;
		CFrameWnd* pFrame = GetParentFrame();
		ASSERT_VALID(pFrame);
		pFrame->GetWindowRect(rectFrame);
		CSize size = rectFrame.Size();
		size.cx += rectView.right - rectClient.right;
		size.cy += rectView.bottom - rectClient.bottom;

		// CHANGE: Instead of moving the window, we want to set the min size
		
//		pFrame->SetWindowPos(NULL, 0, 0, size.cx, size.cy,
//			SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);
		m_MinSize = size;
	}

//	CRect rct;
//	pParentFrame->GetWindowRect(rct);
//	pParentFrame->SetMinTrackSize(CPoint(rct.Width(), rct.Height()));

	pParentFrame->SetMinTrackSize(CPoint(m_MinSize.cx, m_MinSize.cy));

	if (!m_bInitdOffsets)
	{
		int nMinTop = INT_MAX;
		CRect rct;
        CRect rectBeginButton;

		// First get the absolute pos of the top of the ctrls in question
		// At the same time find the min of all the top pos's
		m_MoreBtn.GetWindowRect(rct);
		nMinTop = __min((m_OffsetMoreBtn = rct.top), nMinTop);

		m_LessBtn.GetWindowRect(rct);
		nMinTop = __min((m_OffsetLessBtn = rct.top), nMinTop);

		m_OptionsBtn.GetWindowRect(rct);
		nMinTop = __min((m_OffsetOptionsBtn = rct.top), nMinTop);

		m_AndRadioBtn.GetWindowRect(rct);
		nMinTop = __min((m_OffsetAndRadio = rct.top), nMinTop);

		m_OrRadioBtn.GetWindowRect(rct);
		nMinTop = __min((m_OffsetOrRadio = rct.top), nMinTop);

		m_BeginBtn.GetWindowRect(rectBeginButton);
		nMinTop = __min((m_OffsetBeginBtn = rectBeginButton.top), nMinTop);

        m_PoweredByX1Static.GetWindowRect(rct);
        nMinTop = __min(m_VertOffsetX1Static = rct.top, nMinTop);
        m_HorizOffsetX1Static = rectBeginButton.left - rct.left;

        m_PoweredByX1Icon.GetWindowRect(rct);
        nMinTop = __min(m_VertOffsetX1Icon = rct.top, nMinTop);
        m_HorizOffsetX1Icon = rectBeginButton.left - rct.left;

		m_ResultsMbxTabCtrl.GetWindowRect(rct);
		nMinTop = __min((m_OffsetTabCtrl = rct.top), nMinTop);

		m_ResultsStatic.GetWindowRect(rct);
		nMinTop = __min((m_OffsetResultsStatic = rct.top), nMinTop);
        m_HorizOffsetResultsStatic = rectBeginButton.left - rct.right;

		// Get the bottom of the window for min window size
//		GetWindowRect(rct);
//		m_OffsetMinWinBottom = rct.bottom - nMinTop;

		// Now make everything relative to the minimum
		// One of these will become zero (the min)
		m_OffsetMoreBtn -= nMinTop;
		m_OffsetLessBtn -= nMinTop;
		m_OffsetOptionsBtn -= nMinTop;
		m_OffsetAndRadio -= nMinTop;
		m_OffsetOrRadio -= nMinTop;
		m_OffsetBeginBtn -= nMinTop;
        m_VertOffsetX1Static -= nMinTop;
        m_VertOffsetX1Icon -= nMinTop;
		m_OffsetTabCtrl -= nMinTop;
		m_OffsetResultsStatic -= nMinTop;

		m_bInitdOffsets = true;

		AdjustControlTopPos();
	}

	m_MBoxTree.SelectSetFirstVisible(m_MBoxTree.GetRootItem());

	HideAllControls();	
	LoadCriteria();
	UpdateSearchBtn();

	// Make sure This is a valid criteria and
	// call Function to set focus(again) And highlight
	// an Edit or NumEdit control
	if (m_CurCritCount < 1 || m_CurCritCount > MAX_CONTROLS_CRITERIA)
		ASSERT(0);
	else
	{
		int nNewIdx = (m_CurCritCount - 1);

		SetCriteriaFocus(nNewIdx);
	}

	CRect ClientRct;
	GetClientRect(ClientRct);
	ResizeControls(ClientRct.Width(), ClientRct.Height());
}

// --------------------------------------------------------------------------

void CSearchView::HideAllControls()
{
	for (int idx = 0; idx < MAX_CONTROLS_CRITERIA; idx++)
	{
		if ( m_bCritInitd[idx] )
			HideControls(idx);
	}

	m_CurCritCount = 0;

	UpdateCritBotPos();
	AdjustControlTopPos();
}

// --------------------------------------------------------------------------

void CSearchView::HideControls(int nIdx)
{
	m_CategoryCombo[nIdx].ShowWindow(SW_HIDE);

	m_TextCompareCombo[nIdx].ShowWindow(SW_HIDE);
	m_EqualCompareCombo[nIdx].ShowWindow(SW_HIDE);
	m_NumCompareCombo[nIdx].ShowWindow(SW_HIDE);
	m_DateCompareCombo[nIdx].ShowWindow(SW_HIDE);

	m_NumStatic[nIdx].ShowWindow(SW_HIDE);
	m_NumEdit[nIdx].ShowWindow(SW_HIDE);
	m_NumSpin[nIdx].ShowWindow(SW_HIDE);
	m_TextEdit[nIdx].ShowWindow(SW_HIDE);
	m_StatusCombo[nIdx].ShowWindow(SW_HIDE);
	m_LabelCombo[nIdx].ShowWindow(SW_HIDE);
	m_PersonaCombo[nIdx].ShowWindow(SW_HIDE);
	m_PriorityCombo[nIdx].ShowWindow(SW_HIDE);
	m_DateTimeCtrl[nIdx].ShowWindow(SW_HIDE);
}

// --------------------------------------------------------------------------

void CSearchView::UpdateCritBotPos()
{
#ifdef _MY_TRACE_
::MyTrace("CSearchView::UpdateCritBotPos()\n");
#endif // _MY_TRACE_

	if (m_CurCritCount < 1)
		m_CriteriaBottomPos = 0;
	else
	{
		CRect rct;
		m_CategoryCombo[m_CurCritCount - 1].GetWindowRect(rct);
		ScreenToClient(rct);
		m_CriteriaBottomPos = rct.bottom;
	}
}

// --------------------------------------------------------------------------

bool CSearchView::RemoveCriteria()
{
#ifdef _MY_TRACE_
::MyTrace("CSearchView::RemoveCriteria()\n");
#endif // _MY_TRACE_

	ASSERT(m_CurCritCount > 0);
	ASSERT(m_CurCritCount <= MAX_CONTROLS_CRITERIA);

	if (m_CurCritCount <= 0)
		return (false);

	HideControls(--m_CurCritCount);

	UpdateMoreLessBtn();
	UpdateAndOrBtns();
	UpdateCritBotPos();
	UpdateSearchBtn();
	AdjustControlTopPos();

	return (true);
}

// --------------------------------------------------------------------------

bool CSearchView::AddNewCriteria()
{
#ifdef _MY_TRACE_
::MyTrace("CSearchView::AddNewCriteria()\n");
#endif // _MY_TRACE_

	ASSERT(m_CurCritCount >= 0);
	ASSERT(m_CurCritCount < MAX_CONTROLS_CRITERIA);

	if (m_CurCritCount >= MAX_CONTROLS_CRITERIA)
		return (false);

	int nNewIdx = (m_CurCritCount++);

	//if (nNewIdx > 0)
	CreateCriteriaCtrls(nNewIdx);

	InitializeCriteriaCtrls(nNewIdx);
	m_CategoryCombo[nNewIdx].ShowWindow(SW_SHOW);
	OnCategoryChange(nNewIdx);
	SetCriteriaFocus(nNewIdx);

	UpdateMoreLessBtn();
	UpdateAndOrBtns();
	UpdateCritBotPos();
	AdjustControlTopPos();

	return (true);
}

// --------------------------------------------------------------------------

void CSearchView::OnCategoryChange(int nIdx)
{
	UpdateState(nIdx);

	if (nIdx < m_CurCritCount)
	{
		ResizeCriteriaCtrls(nIdx);
		SetupDynamicCriteriaCtrls(nIdx);
		InitializeCriteriaValues(nIdx);
		UpdateSearchBtn();
	}
}

// --------------------------------------------------------------------------

void CSearchView::ResizeAllCriteriaCtrls()
{
	for (int idx = 0; idx < m_CurCritCount; idx++)
		ResizeCriteriaCtrls(idx);
}

// --------------------------------------------------------------------------

void CSearchView::ResizeCriteriaCtrls(int nIdx)
{
	ASSERT(nIdx < m_CurCritCount);

	CRect rct;
	GetClientRect(rct);

	ResizeCriteriaCtrls(nIdx, rct.Width(), nIdx * 30);
}

// --------------------------------------------------------------------------

void CSearchView::AdjustControlTopPos()
{
#ifdef _MY_TRACE_
::MyTrace("CSearchView::AdjustControlTopPos()\n");
#endif // _MY_TRACE_

	ASSERT(m_bInitdOffsets);
	if (!m_bInitdOffsets)
		return;

	const int nTop = (m_CriteriaBottomPos + 10);

	MoveControl(&m_MoreBtn, -1, -1, nTop + m_OffsetMoreBtn);
	MoveControl(&m_LessBtn, -1, -1, nTop + m_OffsetLessBtn);
	MoveControl(&m_OptionsBtn, -1, -1, nTop + m_OffsetOptionsBtn);
	MoveControl(&m_AndRadioBtn, -1, -1, nTop + m_OffsetAndRadio);
	MoveControl(&m_OrRadioBtn, -1, -1, nTop + m_OffsetOrRadio);
	MoveControl(&m_BeginBtn, -1, -1, nTop + m_OffsetBeginBtn);
    MoveControl(&m_PoweredByX1Static, -1, -1, nTop + m_VertOffsetX1Static);
    MoveControl(&m_PoweredByX1Icon, -1, -1, nTop + m_VertOffsetX1Icon);
	MoveControl(&m_ResultsStatic, -1, -1, nTop + m_OffsetResultsStatic);
	StretchTopControl(&m_ResultsMbxTabCtrl, nTop + m_OffsetTabCtrl);

	// Invalidate "powered by x1" static and icon because otherwise they don't
	// necessarily erase themselves completely after being moved.
	m_PoweredByX1Static.Invalidate();
	m_PoweredByX1Icon.Invalidate();

	AdjustWinMinHeight(nTop + m_MinSize.cy);
	ResizeTabContents();
}

// --------------------------------------------------------------------------

void CSearchView::AdjustWinMinHeight(int nMinWinHeight)
{
#ifdef _MY_TRACE_
::MyTrace("CSearchView::AdjustWinMinHeight(%d)\n", nMinWinHeight);
#endif // _MY_TRACE_

	ASSERT(m_bInitdOffsets);
	if (!m_bInitdOffsets)
		return;

	CMDIChild *pParentFrame = (CMDIChild *) GetParentFrame();
	
	ASSERT(pParentFrame);
	ASSERT_KINDOF(CMDIChild, pParentFrame);
	
	CPoint pt(pParentFrame->GetMinTrackSize());

	if (pt.y != nMinWinHeight)
	{
		pt.y = nMinWinHeight;
		pParentFrame->SetMinTrackSize(pt);

		CRect rct;
		pParentFrame->GetWindowRect(rct);

		if (nMinWinHeight > rct.Height())
		{
#ifdef _MY_TRACE_
::MyTrace("CSearchView::AdjustWinMinHeight: Expanding window to new min height (%d)\n", nMinWinHeight);
#endif // _MY_TRACE_
			pParentFrame->SetWindowPos(NULL, 0, 0, rct.Width(), nMinWinHeight, SWP_NOMOVE | SWP_NOOWNERZORDER | SWP_NOZORDER);
		}
	}
}

// --------------------------------------------------------------------------

void CSearchView::StretchTopControl(CWnd *pWnd, int nTop)
{
#ifdef _MY_TRACE_
::MyTrace("CSearchView::StretchTopControl(%d)\n", nTop);
#endif // _MY_TRACE_

	CRect CtrlRct;

	pWnd->GetWindowRect(CtrlRct);
	ScreenToClient(CtrlRct);

	CtrlRct.top = nTop;

	pWnd->MoveWindow(CtrlRct);
}

// ---------------------------------------------------------------------------------------
// Changes - 04/25/2000 Apul
/* This function dynamically creates the controls. This was done to optimize the
 * resource usage, since statically creating all the controls would use significant 
 * amount of it. 
 IMPORTANT NOTE : To add any new control, do as mentioned
	- Increase the dwCtrlID array size appropriately	   
	- Add the Control Identifier at the bottom
	- Add the actual creation code in the same order for ease of understanding later
	- IMPORTANT : Make sure the addition of the newer control DOES NOT BREAK the existing structure 

 This function could have been enhanced to use function pointers & stuff so that it would be still easier
 to manipulate creation/addition of controls, but i guess this is a good compromise for the time being.

 Also i have used the same resource identifiers as it were in resource.h when these controls were
 statically created. I am pasting the same here for reference. These should not be erased from 
 resource.h & if erased should be added in the searchview.h taking care that the values are unique

	#define IDC_CRITERIA_CATEGORY_COMBO_1   7600
	#define IDC_CRITERIA_CATEGORY_COMBO_2   7601
	#define IDC_CRITERIA_CATEGORY_COMBO_3   7602
	#define IDC_CRITERIA_CATEGORY_COMBO_4   7603
	#define IDC_CRITERIA_CATEGORY_COMBO_5   7604
	#define IDC_CRITERIA_TEXT_COMPARE_COMBO_1 7605
	#define IDC_CRITERIA_TEXT_COMPARE_COMBO_2 7606
	#define IDC_CRITERIA_TEXT_COMPARE_COMBO_3 7607
	#define IDC_CRITERIA_TEXT_COMPARE_COMBO_4 7608
	#define IDC_CRITERIA_TEXT_COMPARE_COMBO_5 7609
	#define IDC_CRITERIA_STATUS_COMBO_1     7610
	#define IDC_CRITERIA_STATUS_COMBO_2     7611
	#define IDC_CRITERIA_STATUS_COMBO_3     7612
	#define IDC_CRITERIA_STATUS_COMBO_4     7613
	#define IDC_CRITERIA_STATUS_COMBO_5     7614
	#define IDC_CRITERIA_EQUAL_COMBO_1      7615
	#define IDC_CRITERIA_EQUAL_COMBO_2      7616
	#define IDC_CRITERIA_EQUAL_COMBO_3      7617
	#define IDC_CRITERIA_EQUAL_COMBO_4      7618
	#define IDC_CRITERIA_EQUAL_COMBO_5      7619
	#define IDC_CRITERIA_NUM_COMPARE_COMBO_1 7620
	#define IDC_CRITERIA_NUM_COMPARE_COMBO_2 7621
	#define IDC_CRITERIA_NUM_COMPARE_COMBO_3 7622
	#define IDC_CRITERIA_NUM_COMPARE_COMBO_4 7623
	#define IDC_CRITERIA_NUM_COMPARE_COMBO_5 7624
	#define IDC_CRITERIA_DATE_COMPARE_COMBO_1 7625
	#define IDC_CRITERIA_DATE_COMPARE_COMBO_2 7626
	#define IDC_CRITERIA_DATE_COMPARE_COMBO_3 7627
	#define IDC_CRITERIA_DATE_COMPARE_COMBO_4 7628
	#define IDC_CRITERIA_DATE_COMPARE_COMBO_5 7629
	#define IDC_CRITERIA_LABEL_COMBO_1      7630
	#define IDC_CRITERIA_LABEL_COMBO_2      7631
	#define IDC_CRITERIA_LABEL_COMBO_3      7632
	#define IDC_CRITERIA_LABEL_COMBO_4      7633
	#define IDC_CRITERIA_LABEL_COMBO_5      7634
	#define IDC_CRITERIA_PERSONA_COMBO_1    7635
	#define IDC_CRITERIA_PERSONA_COMBO_2    7636
	#define IDC_CRITERIA_PERSONA_COMBO_3    7637
	#define IDC_CRITERIA_PERSONA_COMBO_4    7638
	#define IDC_CRITERIA_PERSONA_COMBO_5    7639
	#define IDC_CRITERIA_PRIORITY_COMBO_1   7640
	#define IDC_CRITERIA_PRIORITY_COMBO_2   7641
	#define IDC_CRITERIA_PRIORITY_COMBO_3   7642
	#define IDC_CRITERIA_PRIORITY_COMBO_4   7643
	#define IDC_CRITERIA_PRIORITY_COMBO_5   7644
	#define IDC_CRITERIA_TEXT_EDIT_1        7645
	#define IDC_CRITERIA_TEXT_EDIT_2        7646
	#define IDC_CRITERIA_TEXT_EDIT_3        7647
	#define IDC_CRITERIA_TEXT_EDIT_4        7648
	#define IDC_CRITERIA_TEXT_EDIT_5        7649
	#define IDC_CRITERIA_NUM_SPIN_1         7650
	#define IDC_CRITERIA_NUM_SPIN_2         7651
	#define IDC_CRITERIA_NUM_SPIN_3         7652
	#define IDC_CRITERIA_NUM_SPIN_4         7653
	#define IDC_CRITERIA_NUM_SPIN_5         7654
	#define IDC_CRITERIA_NUM_EDIT_1         7655
	#define IDC_CRITERIA_NUM_EDIT_2         7656
	#define IDC_CRITERIA_NUM_EDIT_3         7657
	#define IDC_CRITERIA_NUM_EDIT_4         7658
	#define IDC_CRITERIA_NUM_EDIT_5         7659
	#define IDC_CRITERIA_NUM_UNITS_STATIC_1 7660
	#define IDC_CRITERIA_NUM_UNITS_STATIC_2 7661
	#define IDC_CRITERIA_NUM_UNITS_STATIC_3 7662
	#define IDC_CRITERIA_NUM_UNITS_STATIC_4 7663
	#define IDC_CRITERIA_NUM_UNITS_STATIC_5 7664
	#define IDC_CRITERIA_DATE_CTRL_1        7665
	#define IDC_CRITERIA_DATE_CTRL_2        7666
	#define IDC_CRITERIA_DATE_CTRL_3        7667
	#define IDC_CRITERIA_DATE_CTRL_4        7668
	#define IDC_CRITERIA_DATE_CTRL_5        7669

 */
// ---------------------------------------------------------------------------------------
BOOL CSearchView::CreateCriteriaCtrls(int nIdx)
{
	if (TRUE == m_bCritCreated[nIdx])
		return false;

	CRect CtrlRct(10,10,50,20);
	CFont* pFont;
	
	CWnd *pParentFrame = m_MoreBtn.GetParent();
	pFont = m_MoreBtn.GetFont();	

	DWORD dwCtrlID[MAX_CONTROLS_CRITERIA][14] = 	
						{	{IDC_CRITERIA_NUM_EDIT_1,
							 IDC_CRITERIA_NUM_SPIN_1,
							 IDC_CRITERIA_CATEGORY_COMBO_1,
							 IDC_CRITERIA_TEXT_COMPARE_COMBO_1,
							 IDC_CRITERIA_EQUAL_COMBO_1,
							 IDC_CRITERIA_NUM_COMPARE_COMBO_1,
							 IDC_CRITERIA_DATE_COMPARE_COMBO_1,
							 IDC_CRITERIA_TEXT_EDIT_1,
							 IDC_CRITERIA_STATUS_COMBO_1,
							 IDC_CRITERIA_LABEL_COMBO_1,
							 IDC_CRITERIA_PERSONA_COMBO_1,
							 IDC_CRITERIA_PRIORITY_COMBO_1,
							 IDC_CRITERIA_NUM_UNITS_STATIC_1,
							 IDC_CRITERIA_DATE_CTRL_1},
							
							{IDC_CRITERIA_NUM_EDIT_2,
							 IDC_CRITERIA_NUM_SPIN_2,
							 IDC_CRITERIA_CATEGORY_COMBO_2,
							 IDC_CRITERIA_TEXT_COMPARE_COMBO_2,
							 IDC_CRITERIA_EQUAL_COMBO_2,
							 IDC_CRITERIA_NUM_COMPARE_COMBO_2,
							 IDC_CRITERIA_DATE_COMPARE_COMBO_2,
							 IDC_CRITERIA_TEXT_EDIT_2,
							 IDC_CRITERIA_STATUS_COMBO_2,
							 IDC_CRITERIA_LABEL_COMBO_2,
							 IDC_CRITERIA_PERSONA_COMBO_2,
							 IDC_CRITERIA_PRIORITY_COMBO_2,
							 IDC_CRITERIA_NUM_UNITS_STATIC_2,
							 IDC_CRITERIA_DATE_CTRL_2},

							{IDC_CRITERIA_NUM_EDIT_3,
							 IDC_CRITERIA_NUM_SPIN_3,
							 IDC_CRITERIA_CATEGORY_COMBO_3,
							 IDC_CRITERIA_TEXT_COMPARE_COMBO_3,
							 IDC_CRITERIA_EQUAL_COMBO_4,
							 IDC_CRITERIA_NUM_COMPARE_COMBO_3,
							 IDC_CRITERIA_DATE_COMPARE_COMBO_3,
							 IDC_CRITERIA_TEXT_EDIT_3,
							 IDC_CRITERIA_STATUS_COMBO_3,
							 IDC_CRITERIA_LABEL_COMBO_3,
							 IDC_CRITERIA_PERSONA_COMBO_3,
							 IDC_CRITERIA_PRIORITY_COMBO_3,
							 IDC_CRITERIA_NUM_UNITS_STATIC_3,
							 IDC_CRITERIA_DATE_CTRL_3},

							{IDC_CRITERIA_NUM_EDIT_4,
							 IDC_CRITERIA_NUM_SPIN_4,
							 IDC_CRITERIA_CATEGORY_COMBO_4,
							 IDC_CRITERIA_TEXT_COMPARE_COMBO_4,
							 IDC_CRITERIA_EQUAL_COMBO_4,
							 IDC_CRITERIA_NUM_COMPARE_COMBO_4,
							 IDC_CRITERIA_DATE_COMPARE_COMBO_4,
							 IDC_CRITERIA_TEXT_EDIT_4,
							 IDC_CRITERIA_STATUS_COMBO_4,
							 IDC_CRITERIA_LABEL_COMBO_4,
							 IDC_CRITERIA_PERSONA_COMBO_4,
							 IDC_CRITERIA_PRIORITY_COMBO_4,
							 IDC_CRITERIA_NUM_UNITS_STATIC_4,
							 IDC_CRITERIA_DATE_CTRL_4},

							{IDC_CRITERIA_NUM_EDIT_5,
							 IDC_CRITERIA_NUM_SPIN_5,
							 IDC_CRITERIA_CATEGORY_COMBO_5,
							 IDC_CRITERIA_TEXT_COMPARE_COMBO_5,
							 IDC_CRITERIA_EQUAL_COMBO_5,
							 IDC_CRITERIA_NUM_COMPARE_COMBO_5,
							 IDC_CRITERIA_DATE_COMPARE_COMBO_5,
							 IDC_CRITERIA_TEXT_EDIT_5,
							 IDC_CRITERIA_STATUS_COMBO_5,
							 IDC_CRITERIA_LABEL_COMBO_5,
							 IDC_CRITERIA_PERSONA_COMBO_5,
							 IDC_CRITERIA_PRIORITY_COMBO_5,
							 IDC_CRITERIA_NUM_UNITS_STATIC_5,
							 IDC_CRITERIA_DATE_CTRL_5}
						};


	CRect rectSize(10,10,50,30);	
	
	// Control 1 creation
	m_NumEdit[nIdx].Create(WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_BORDER | ES_AUTOHSCROLL | ES_LEFT,
								 rectSize,
								 pParentFrame,
								 dwCtrlID[nIdx][0]);
	m_NumEdit[nIdx].SetFont(pFont);

	// Control 2 creation
	m_NumSpin[nIdx].Create(WS_CHILD | WS_VISIBLE | UDS_ARROWKEYS | UDS_SETBUDDYINT | UDS_NOTHOUSANDS,
								 rectSize,
								 pParentFrame,
								 dwCtrlID[nIdx][1]);	
	m_NumSpin[nIdx].SetFont(pFont);

	// Control 3 creation
	m_CategoryCombo[nIdx].Create(WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_TABSTOP | CBS_DROPDOWNLIST,
								 CtrlRct,
								 pParentFrame,
								 dwCtrlID[nIdx][2]);
	m_CategoryCombo[nIdx].SetFont(pFont);
	

	// Control 4 creation
	m_TextCompareCombo[nIdx].Create(WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_TABSTOP | CBS_DROPDOWNLIST,
								 CtrlRct,
								 pParentFrame,
								 dwCtrlID[nIdx][3]);	
	m_TextCompareCombo[nIdx].SetFont(pFont);

	// Control 5 creation
	m_EqualCompareCombo[nIdx].Create(WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_TABSTOP | CBS_DROPDOWNLIST,
								 CtrlRct,
								 pParentFrame,
								 dwCtrlID[nIdx][4]);
	m_EqualCompareCombo[nIdx].SetFont(pFont);	

	// Control 6 creation
	m_NumCompareCombo[nIdx].Create(WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_TABSTOP | CBS_DROPDOWNLIST,
								 CtrlRct,
								 pParentFrame,
								 dwCtrlID[nIdx][5]);	
	m_NumCompareCombo[nIdx].SetFont(pFont);

	// Control 7 creation
	m_DateCompareCombo[nIdx].Create(WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_TABSTOP | CBS_DROPDOWNLIST,
								 CtrlRct,
								 pParentFrame,
								 dwCtrlID[nIdx][6]);	
	m_DateCompareCombo[nIdx].SetFont(pFont);

	// Control 8 creation
	m_TextEdit[nIdx].Create(WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_BORDER | ES_AUTOHSCROLL | ES_LEFT,
								 rectSize,
								 pParentFrame,
								 dwCtrlID[nIdx][7]);
	m_TextEdit[nIdx].SetFont(pFont);	

	// Control 9 creation
	m_StatusCombo[nIdx].Create(WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_TABSTOP | CBS_DROPDOWNLIST,
								 CtrlRct,
								 pParentFrame,
								 dwCtrlID[nIdx][8]);
	m_StatusCombo[nIdx].SetFont(pFont);
	
	// Control 10 creation
	m_LabelCombo[nIdx].Create(WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_TABSTOP | CBS_DROPDOWNLIST,
								 CtrlRct,
								 pParentFrame,
								 dwCtrlID[nIdx][9]);
	m_LabelCombo[nIdx].SetFont(pFont);
		
	// Control 11 creation
	m_PersonaCombo[nIdx].Create(WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_TABSTOP | CBS_DROPDOWNLIST,
								 CtrlRct,
								 pParentFrame,
								 dwCtrlID[nIdx][10]);	
	m_PersonaCombo[nIdx].SetFont(pFont);

	// Control 12 creation
	m_PriorityCombo[nIdx].Create(WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_TABSTOP | CBS_DROPDOWNLIST,
								 CtrlRct,
								 pParentFrame,
								 dwCtrlID[nIdx][11]);
	m_PriorityCombo[nIdx].SetFont(pFont);	

	// Control 13 creation
	m_NumStatic[nIdx].Create(NULL,
							 WS_CHILD | WS_VISIBLE | SS_LEFT | WS_GROUP,
							 rectSize,
							 pParentFrame,
							 dwCtrlID[nIdx][12]);
	m_NumStatic[nIdx].SetFont(pFont);

	
	// Control 14 creation
	// We are required to create this control so that the DateTime Control could attach to it	
	CEdit TempEdit;
	TempEdit.Create(WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_BORDER | ES_AUTOHSCROLL | ES_LEFT,
								 rectSize,
								 pParentFrame,
								 dwCtrlID[nIdx][13]);
	TempEdit.SetFont(pFont);

	// Now create the date time control & attach it to the edit control created above
	m_DateTimeCtrl[nIdx].SetFormat(_T(CRString(IDS_SEARCH_DATE_CTRL_DISPLAY_FORMAT))); // "MMMM d, yyyy"
	VERIFY(m_DateTimeCtrl[nIdx].AttachDateTimeCtrl(dwCtrlID[nIdx][13], this, SEC_DTS_CALENDAR));
	m_DateTimeCtrl->SetFont(pFont);

	// Code for creating any more new controls goes here


	//...

	// We are all done creating controls & hence ..
	m_bCritCreated[nIdx] = TRUE;
	
	return true;
}


void CSearchView::InitializeCriteriaCtrls(int nIdx)
{
#ifdef _MY_TRACE_
::MyTrace("CSearchView::InitializeCriteriaCtrls(%d)\n", nIdx);
#endif // _MY_TRACE_

	if (m_bCritInitd[nIdx])
		return; // Only need to init each criteria once

	m_CategoryCombo[nIdx].ResetContent();

	int ret;
	const bool bFullFeatured = UsingFullFeatureSet();

	VERIFY((ret = m_CategoryCombo[nIdx].AddString(CRString(IDS_SEARCH_CATEGORYSTR_ANYWHERE))) >= 0); // "Anywhere"
	VERIFY(m_CategoryCombo[nIdx].SetItemData(ret, CRITERIA_OBJECT_ANYWHERE) != CB_ERR);

	if (bFullFeatured)
	{
		VERIFY((ret = m_CategoryCombo[nIdx].AddString(CRString(IDS_SEARCH_CATEGORYSTR_HEADERS))) >= 0); // "Headers"
		VERIFY(m_CategoryCombo[nIdx].SetItemData(ret, CRITERIA_OBJECT_HEADERS) != CB_ERR);
		VERIFY((ret = m_CategoryCombo[nIdx].AddString(CRString(IDS_SEARCH_CATEGORYSTR_BODY))) >= 0); // "Body"
		VERIFY(m_CategoryCombo[nIdx].SetItemData(ret, CRITERIA_OBJECT_BODY) != CB_ERR);
		VERIFY((ret = m_CategoryCombo[nIdx].AddString(CRString(IDS_SEARCH_CATEGORYSTR_ATTACHNAMES))) >= 0); // "Attachment Name(s)"
		VERIFY(m_CategoryCombo[nIdx].SetItemData(ret, CRITERIA_OBJECT_ATTACHNAMES) != CB_ERR);
	}

	VERIFY((ret = m_CategoryCombo[nIdx].AddString(CRString(IDS_SEARCH_CATEGORYSTR_SUMMARY))) >= 0); // "Summary"
	VERIFY(m_CategoryCombo[nIdx].SetItemData(ret, CRITERIA_OBJECT_SUMMARY) != CB_ERR);

	if (bFullFeatured)
	{
		VERIFY((ret = m_CategoryCombo[nIdx].AddString(CRString(IDS_SEARCH_CATEGORYSTR_STATUS))) >= 0); // "Status"
		VERIFY(m_CategoryCombo[nIdx].SetItemData(ret, CRITERIA_OBJECT_STATUS) != CB_ERR);
		VERIFY((ret = m_CategoryCombo[nIdx].AddString(CRString(IDS_SEARCH_CATEGORYSTR_PRIORITY))) >= 0); // "Priority"
		VERIFY(m_CategoryCombo[nIdx].SetItemData(ret, CRITERIA_OBJECT_PRIORITY) != CB_ERR);
		VERIFY((ret = m_CategoryCombo[nIdx].AddString(CRString(IDS_SEARCH_CATEGORYSTR_ATTACHCOUNT))) >= 0); // "Attachment Count"
		VERIFY(m_CategoryCombo[nIdx].SetItemData(ret, CRITERIA_OBJECT_ATTACHCOUNT) != CB_ERR);
		VERIFY((ret = m_CategoryCombo[nIdx].AddString(CRString(IDS_SEARCH_CATEGORYSTR_JUNKSCORE))) >= 0); // "Junk Score"
		VERIFY(m_CategoryCombo[nIdx].SetItemData(ret, CRITERIA_OBJECT_JUNKSCORE) != CB_ERR);
		VERIFY((ret = m_CategoryCombo[nIdx].AddString(CRString(IDS_SEARCH_CATEGORYSTR_LABEL))) >= 0); // "Label"
		VERIFY(m_CategoryCombo[nIdx].SetItemData(ret, CRITERIA_OBJECT_LABEL) != CB_ERR);
		VERIFY((ret = m_CategoryCombo[nIdx].AddString(CRString(IDS_SEARCH_CATEGORYSTR_DATE))) >= 0); // "Date"
		VERIFY(m_CategoryCombo[nIdx].SetItemData(ret, CRITERIA_OBJECT_DATE) != CB_ERR);
		VERIFY((ret = m_CategoryCombo[nIdx].AddString(CRString(IDS_SEARCH_CATEGORYSTR_SIZE))) >= 0); // "Size"
		VERIFY(m_CategoryCombo[nIdx].SetItemData(ret, CRITERIA_OBJECT_SIZE) != CB_ERR);
		VERIFY((ret = m_CategoryCombo[nIdx].AddString(CRString(IDS_SEARCH_CATEGORYSTR_AGE))) >= 0); // "Age"
		VERIFY(m_CategoryCombo[nIdx].SetItemData(ret, CRITERIA_OBJECT_AGE) != CB_ERR);
		VERIFY((ret = m_CategoryCombo[nIdx].AddString(CRString(IDS_SEARCH_CATEGORYSTR_PERSONA))) >= 0); // "Personality"
		VERIFY(m_CategoryCombo[nIdx].SetItemData(ret, CRITERIA_OBJECT_PERSONA) != CB_ERR);
		VERIFY((ret = m_CategoryCombo[nIdx].AddString(CRString(IDS_SEARCH_CATEGORYSTR_MBXNAME))) >= 0); // "Mailbox Name"
		VERIFY(m_CategoryCombo[nIdx].SetItemData(ret, CRITERIA_OBJECT_MAILBOXNAME) != CB_ERR);

		VERIFY((ret = m_CategoryCombo[nIdx].AddString(CRString(IDS_SEARCH_CATEGORYSTR_TO))) >= 0); // "To"
		VERIFY(m_CategoryCombo[nIdx].SetItemData(ret, CRITERIA_OBJECT_TO) != CB_ERR);
		VERIFY((ret = m_CategoryCombo[nIdx].AddString(CRString(IDS_SEARCH_CATEGORYSTR_FROM))) >= 0); // "From"
		VERIFY(m_CategoryCombo[nIdx].SetItemData(ret, CRITERIA_OBJECT_FROM) != CB_ERR);
		VERIFY((ret = m_CategoryCombo[nIdx].AddString(CRString(IDS_SEARCH_CATEGORYSTR_SUBJECT))) >= 0); // "Subject"
		VERIFY(m_CategoryCombo[nIdx].SetItemData(ret, CRITERIA_OBJECT_SUBJECT) != CB_ERR);
		VERIFY((ret = m_CategoryCombo[nIdx].AddString(CRString(IDS_SEARCH_CATEGORYSTR_CC))) >= 0); // "CC"
		VERIFY(m_CategoryCombo[nIdx].SetItemData(ret, CRITERIA_OBJECT_CC) != CB_ERR);
		VERIFY((ret = m_CategoryCombo[nIdx].AddString(CRString(IDS_SEARCH_CATEGORYSTR_BCC))) >= 0); // "BCC"
		VERIFY(m_CategoryCombo[nIdx].SetItemData(ret, CRITERIA_OBJECT_BCC) != CB_ERR);
		VERIFY((ret = m_CategoryCombo[nIdx].AddString(CRString(IDS_SEARCH_CATEGORYSTR_ANYRECIPIENT))) >= 0); // "Any recipient"
		VERIFY(m_CategoryCombo[nIdx].SetItemData(ret, CRITERIA_OBJECT_ANYRECIP) != CB_ERR);
	}

	VERIFY(m_CategoryCombo[nIdx].SetCurSel(0) != CB_ERR);

	// --------------------------------------------------

	InitializeTextCompareCombo(nIdx);

	// --------------------------------------------------

	m_EqualCompareCombo[nIdx].ResetContent();

	if (bFullFeatured)
	{
		VERIFY((ret = m_EqualCompareCombo[nIdx].AddString(CRString(IDS_SEARCH_COMPARESTR_IS))) >= 0); // "is"
		VERIFY(m_EqualCompareCombo[nIdx].SetItemData(ret, CRITERIA_EQUAL_COMPARE_IS) != CB_ERR);
		VERIFY((ret = m_EqualCompareCombo[nIdx].AddString(CRString(IDS_SEARCH_COMPARESTR_ISNOT))) >= 0); // "is not"
		VERIFY(m_EqualCompareCombo[nIdx].SetItemData(ret, CRITERIA_EQUAL_COMPARE_ISNOT) != CB_ERR);

		VERIFY(m_EqualCompareCombo[nIdx].SetCurSel(0) != CB_ERR);
	}

	// --------------------------------------------------

	m_NumCompareCombo[nIdx].ResetContent();

	if (bFullFeatured)
	{
		VERIFY((ret = m_NumCompareCombo[nIdx].AddString(CRString(IDS_SEARCH_COMPARESTR_IS))) >= 0); // "is"
		VERIFY(m_NumCompareCombo[nIdx].SetItemData(ret, CRITERIA_NUM_COMPARE_IS) != CB_ERR);
		VERIFY((ret = m_NumCompareCombo[nIdx].AddString(CRString(IDS_SEARCH_COMPARESTR_ISNOT))) >= 0); // "is not"
		VERIFY(m_NumCompareCombo[nIdx].SetItemData(ret, CRITERIA_NUM_COMPARE_ISNOT) != CB_ERR);
		VERIFY((ret = m_NumCompareCombo[nIdx].AddString(CRString(IDS_SEARCH_COMPARESTR_ISGREATERTHAN))) >= 0); // "is greater than"
		VERIFY(m_NumCompareCombo[nIdx].SetItemData(ret, CRITERIA_NUM_COMPARE_ISGREATERTHAN) != CB_ERR);
		VERIFY((ret = m_NumCompareCombo[nIdx].AddString(CRString(IDS_SEARCH_COMPARESTR_ISLESSTHAN))) >= 0); // "is less than"
		VERIFY(m_NumCompareCombo[nIdx].SetItemData(ret, CRITERIA_NUM_COMPARE_ISLESSTHAN) != CB_ERR);

		VERIFY(m_NumCompareCombo[nIdx].SetCurSel(0) != CB_ERR);
	}

	// --------------------------------------------------

	m_DateCompareCombo[nIdx].ResetContent();

	if (bFullFeatured)
	{
		VERIFY((ret = m_DateCompareCombo[nIdx].AddString(CRString(IDS_SEARCH_COMPARESTR_IS))) >= 0); // "is"
		VERIFY(m_DateCompareCombo[nIdx].SetItemData(ret, CRITERIA_DATE_COMPARE_IS) != CB_ERR);
		VERIFY((ret = m_DateCompareCombo[nIdx].AddString(CRString(IDS_SEARCH_COMPARESTR_ISNOT))) >= 0); // "is not"
		VERIFY(m_DateCompareCombo[nIdx].SetItemData(ret, CRITERIA_DATE_COMPARE_ISNOT) != CB_ERR);
		VERIFY((ret = m_DateCompareCombo[nIdx].AddString(CRString(IDS_SEARCH_COMPARESTR_ISAFTER))) >= 0); //"is after" 
		VERIFY(m_DateCompareCombo[nIdx].SetItemData(ret, CRITERIA_DATE_COMPARE_ISAFTER) != CB_ERR);
		VERIFY((ret = m_DateCompareCombo[nIdx].AddString(CRString(IDS_SEARCH_COMPARESTR_ISBEFORE))) >= 0); // "is before"
		VERIFY(m_DateCompareCombo[nIdx].SetItemData(ret, CRITERIA_DATE_COMPARE_ISBEFORE) != CB_ERR);

		VERIFY(m_DateCompareCombo[nIdx].SetCurSel(3) != CB_ERR);
	}

	// --------------------------------------------------

	if (bFullFeatured)
		m_LabelCombo[nIdx].InitLabels();

	// --------------------------------------------------

	if (bFullFeatured)
	{
		m_NumEdit[nIdx].SetWindowText(CRString(IDS_SEARCH_NUMEDIT_INIT_VALUE)); // "0"
		m_NumSpin[nIdx].SetBuddy(&m_NumEdit[nIdx]);
		m_NumSpin[nIdx].SetRange(0, UD_MAXVAL);
	}

	// --------------------------------------------------

	m_PersonaCombo[nIdx].ResetContent();

	if (bFullFeatured)
	{
		LPSTR pszList = g_Personalities.List();
		while (pszList && *pszList)
		{
			m_PersonaCombo[nIdx].AddString(pszList);
			pszList += strlen(pszList) + 1;
		}

		VERIFY(m_PersonaCombo[nIdx].SetCurSel(0) != CB_ERR);
	}

	// --------------------------------------------------

	m_PriorityCombo[nIdx].ResetContent();

	if (bFullFeatured)
	{
		VERIFY((ret = m_PriorityCombo[nIdx].AddString(CRString(IDS_PRIORITY_HIGHEST))) >= 0); // "Highest"
		VERIFY(m_PriorityCombo[nIdx].SetItemData(ret, CRITERIA_VALUE_PRIORITY_HIGHEST) != CB_ERR);
		VERIFY((ret = m_PriorityCombo[nIdx].AddString(CRString(IDS_PRIORITY_HIGH))) >= 0); // "High"
		VERIFY(m_PriorityCombo[nIdx].SetItemData(ret, CRITERIA_VALUE_PRIORITY_HIGH) != CB_ERR);
		VERIFY((ret = m_PriorityCombo[nIdx].AddString(CRString(IDS_PRIORITY_NORMAL))) >= 0); // "Normal"
		VERIFY(m_PriorityCombo[nIdx].SetItemData(ret, CRITERIA_VALUE_PRIORITY_NORMAL) != CB_ERR);
		VERIFY((ret = m_PriorityCombo[nIdx].AddString(CRString(IDS_PRIORITY_LOW))) >= 0); // "Low"
		VERIFY(m_PriorityCombo[nIdx].SetItemData(ret, CRITERIA_VALUE_PRIORITY_LOW) != CB_ERR);
		VERIFY((ret = m_PriorityCombo[nIdx].AddString(CRString(IDS_PRIORITY_LOWEST))) >= 0); // "Lowest"
		VERIFY(m_PriorityCombo[nIdx].SetItemData(ret, CRITERIA_VALUE_PRIORITY_LOWEST) != CB_ERR);

		VERIFY(m_PriorityCombo[nIdx].SetCurSel(0) != CB_ERR);
	}

	// --------------------------------------------------

	m_StatusCombo[nIdx].ResetContent();

	if (bFullFeatured)
	{
		static int statusTable[] = {
			IDS_STATUS_UNREAD, CRITERIA_VALUE_STATUS_UNREAD,
			IDS_STATUS_READ, CRITERIA_VALUE_STATUS_READ,
			IDS_STATUS_REPLIED, CRITERIA_VALUE_STATUS_REPLIED,
			IDS_STATUS_FORWARDED, CRITERIA_VALUE_STATUS_FORWARDED,
			IDS_STATUS_REDIRECTED, CRITERIA_VALUE_STATUS_REDIRECTED,
			IDS_STATUS_SENT, CRITERIA_VALUE_STATUS_SENT,
			IDS_STATUS_SENDABLE, CRITERIA_VALUE_STATUS_SENDABLE,
			IDS_STATUS_UNSENT, CRITERIA_VALUE_STATUS_UNSENT,
			IDS_STATUS_QUEUED, CRITERIA_VALUE_STATUS_QUEUED,
			IDS_STATUS_TIME_QUEUED, CRITERIA_VALUE_STATUS_TIME_QUEUED,
			IDS_STATUS_UNSENDABLE, CRITERIA_VALUE_STATUS_UNSENDABLE,
			IDS_STATUS_RECOVERED, CRITERIA_VALUE_STATUS_RECOVERED,
			0,0};
		int i;

		for (i=0;statusTable[i];i+=2)
		{
			VERIFY((ret = m_StatusCombo[nIdx].AddString(CRString(statusTable[i]))) >= 0);
			VERIFY(m_StatusCombo[nIdx].SetItemData(ret, statusTable[i+1]) != CB_ERR);
		}

		VERIFY(m_StatusCombo[nIdx].SetCurSel(0) != CB_ERR);
	}

	// --------------------------------------------------

	m_bCritInitd[nIdx] = true;


	// Code for initializing values in controls ...starts here
	if (bFullFeatured)
	{
		CString str;

		const CString sDefaultStr
		= CRString(IDS_SEARCH_CATEGORYSTR_ANYWHERE)
			+ m_SaveStrSeperator
			+ CRString(IDS_SEARCH_COMPARESTR_CONTAINS)
			+ m_SaveStrSeperator; // "Anywhere;Contains;"

		DWORD dwSearchCriteriaIDs[5] = {IDS_INI_SEARCH_CRITERIA_1,
										IDS_INI_SEARCH_CRITERIA_2,
										IDS_INI_SEARCH_CRITERIA_3,
										IDS_INI_SEARCH_CRITERIA_4,
										IDS_INI_SEARCH_CRITERIA_5};

		GetIniString(dwSearchCriteriaIDs[nIdx], str);
		if (!SetCriteriaIniString(nIdx, str))
			SetCriteriaIniString(0, sDefaultStr);		
	}

	// Code for initializing values in controls ...ends here

	UpdateState(nIdx);
}

void CSearchView::InitializeTextCompareCombo(int nIndex)
{
	int				ret;
	int				nPreviousSelection = m_TextCompareCombo[nIndex].GetCurSel();
	const bool		bFullFeatured = UsingFullFeatureSet();

	if (nPreviousSelection == CB_ERR)
		nPreviousSelection = 0;
	
	m_TextCompareCombo[nIndex].SetRedraw(FALSE);
	m_TextCompareCombo[nIndex].ResetContent();

	VERIFY((ret = m_TextCompareCombo[nIndex].AddString(CRString(IDS_SEARCH_COMPARESTR_CONTAINS))) >= 0); // "contains"
	VERIFY(m_TextCompareCombo[nIndex].SetItemData(ret, CRITERIA_TEXT_COMPARE_CONTAINS) != CB_ERR);

	if (bFullFeatured)
	{
		VERIFY((ret = m_TextCompareCombo[nIndex].AddString(CRString(IDS_SEARCH_COMPARESTR_CONTAINSWORD))) >= 0); // "contains word"
		VERIFY(m_TextCompareCombo[nIndex].SetItemData(ret, CRITERIA_TEXT_COMPARE_CONTAINSWORD) != CB_ERR);
		VERIFY((ret = m_TextCompareCombo[nIndex].AddString(CRString(IDS_SEARCH_COMPARESTR_DOESNOTCONTAIN))) >= 0); // "does not contain"
		VERIFY(m_TextCompareCombo[nIndex].SetItemData(ret, CRITERIA_TEXT_COMPARE_DOESNOTCONTAIN) != CB_ERR);
		
		if ( ShouldUseIndexedSearch() )
		{
			VERIFY((ret = m_TextCompareCombo[nIndex].AddString(CRString(IDS_SEARCH_COMPARESTR_MATCHESX1QUERY))) >= 0); // "matches x1 query"
			VERIFY(m_TextCompareCombo[nIndex].SetItemData(ret, CRITERIA_TEXT_COMPARE_MATCHESX1QUERY) != CB_ERR);
		}
		else
		{
			VERIFY((ret = m_TextCompareCombo[nIndex].AddString(CRString(IDS_SEARCH_COMPARESTR_IS))) >= 0); // "is"
			VERIFY(m_TextCompareCombo[nIndex].SetItemData(ret, CRITERIA_TEXT_COMPARE_IS) != CB_ERR);
			VERIFY((ret = m_TextCompareCombo[nIndex].AddString(CRString(IDS_SEARCH_COMPARESTR_ISNOT))) >= 0); // "is not"
			VERIFY(m_TextCompareCombo[nIndex].SetItemData(ret, CRITERIA_TEXT_COMPARE_ISNOT) != CB_ERR);
			VERIFY((ret = m_TextCompareCombo[nIndex].AddString(CRString(IDS_SEARCH_COMPARESTR_STARTSWITH))) >= 0); // "starts with"
			VERIFY(m_TextCompareCombo[nIndex].SetItemData(ret, CRITERIA_TEXT_COMPARE_STARTSWITH) != CB_ERR);
			VERIFY((ret = m_TextCompareCombo[nIndex].AddString(CRString(IDS_SEARCH_COMPARESTR_ENDSWITH))) >= 0); // "ends with"
			VERIFY(m_TextCompareCombo[nIndex].SetItemData(ret, CRITERIA_TEXT_COMPARE_ENDSWITH) != CB_ERR);
			VERIFY((ret = m_TextCompareCombo[nIndex].AddString(CRString(IDS_SEARCH_COMPARESTR_MATCHESREGEXP_ICASE))) >= 0); // "matches regexp (case insensitive)"
			VERIFY(m_TextCompareCombo[nIndex].SetItemData(ret, CRITERIA_TEXT_COMPARE_MATCHESREGEXP_ICASE) != CB_ERR);
			VERIFY((ret = m_TextCompareCombo[nIndex].AddString(CRString(IDS_SEARCH_COMPARESTR_MATCHESREGEXP))) >= 0); // "matches regexp"
			VERIFY(m_TextCompareCombo[nIndex].SetItemData(ret, CRITERIA_TEXT_COMPARE_MATCHESREGEXP) != CB_ERR);
		}

		//	Assume that if we preserved a selection - it's because the user turned
		//	indexed search on or off. Only the first 3 verbs match, so if the user
		//	had picked a later verb bounce the selection back to "contains".
		if (nPreviousSelection > 2)
			nPreviousSelection = 0;
	}
	else
	{
		//	Light mode - only one option allowed
		nPreviousSelection = 0;
	}

	VERIFY(m_TextCompareCombo[nIndex].SetCurSel(nPreviousSelection) != CB_ERR);

	m_TextCompareCombo[nIndex].SetRedraw();
	m_TextCompareCombo[nIndex].Invalidate(FALSE);
}

// --------------------------------------------------------------------------

int CSearchView::MoveControl(CWnd *pWnd, int nLeft, int nRight, int nTop)
{
	CRect CtrlRct;

	pWnd->GetWindowRect(CtrlRct);
	ScreenToClient(CtrlRct);
	int nWidth = 0;

	if ((nLeft < 0) || (nRight < 0)) // One or both < 0
	{
		nWidth = CtrlRct.Width();

		if ((nLeft >= 0) || (nRight >= 0)) // One or both >= 0
		{
			if (nLeft < 0)
			{
				CtrlRct.left = nRight - nWidth; // Left < 0 && right >= 0
				CtrlRct.right = nRight;
			}
			else
			{
				CtrlRct.right = nLeft + nWidth; // Left >= 0 && right < 0
				CtrlRct.left = nLeft;
			}
		}
	}
	else
	{
		// Left >= 0 && right >= 0
		CtrlRct.right = nRight;
		CtrlRct.left = nLeft;
	}

	ASSERT(CtrlRct.Width() > 0);

	const int nOffsetY = nTop - CtrlRct.top;
	CtrlRct.OffsetRect(0, nOffsetY);

	pWnd->MoveWindow(CtrlRct);

	if (pWnd->IsKindOf( RUNTIME_CLASS( CComboBox ) ) )
		SetComboDropDownSize((CComboBox *)pWnd);

	if (nRight < 0)
		return (CtrlRct.right);

	return (CtrlRct.left);
}



void CSearchView::SetComboDropDownSize(CComboBox *pComboBox)
{

	ASSERT(IsWindow(*pComboBox));  // Window must exist or SetWindowPos won't work

	CRect cbSize;       
	int Height; 

	// Get the current size
	pComboBox->GetClientRect(cbSize);
	Height = pComboBox->GetItemHeight(-1);  // start with size of the edit-box portion
	Height += pComboBox->GetItemHeight(0) * pComboBox->GetCount();  // add height of lines of text
	
	// Note: The use of SM_CYEDGE assumes that we're using Windows '95
	//       SM_CYBORDER is appropriate for NT 3.51
	//       not sure what to use for NT 4.0 or Win 3.x + Win32s 1.3c
	// Now add on the height of the border of the edit box
	Height += GetSystemMetrics(SM_CYEDGE) * 2; // top & bottom edges

	// The height of the border of the drop-down box
	Height += GetSystemMetrics(SM_CYEDGE) * 2; // top & bottom edges

	// now set the size of the window
	pComboBox->SetWindowPos(NULL,              // not relative to any other windows
	0, 0,                           // TopLeft corner doesn't change
	cbSize.right, Height,           // existing width, new height
	SWP_NOMOVE | SWP_NOZORDER       // don't move box or change z-ordering.
	);
}



// --------------------------------------------------------------------------

void CSearchView::SetCriteriaFocus(int nIdx)
{
	ASSERT(nIdx < m_CurCritCount);

	if (nIdx >= m_CurCritCount)
		return;

	CWnd *pWnd = NULL;

	switch (m_CritState[nIdx].m_CurValueType)
	{
		case CRITERIA_VALUE_TEXT_TYPE:
		{
			pWnd = &m_TextEdit[nIdx];
		}
		break;

		case CRITERIA_VALUE_AGE_TYPE:
		case CRITERIA_VALUE_ATTACHCOUNT_TYPE:
		case CRITERIA_VALUE_SIZE_TYPE:
		case CRITERIA_VALUE_JUNKSCORE_TYPE:
		{
			pWnd = &m_NumEdit[nIdx];
		}
		break;

		case CRITERIA_VALUE_STATUS_TYPE:
		{
			pWnd = &m_StatusCombo[nIdx];
		}
		break;

		case CRITERIA_VALUE_LABEL_TYPE:
		{
			pWnd = &m_LabelCombo[nIdx];
		}
		break;

		case CRITERIA_VALUE_PERSONA_TYPE:
		{
			pWnd = &m_PersonaCombo[nIdx];
		}
		break;

		case CRITERIA_VALUE_PRIORITY_TYPE:
		{
			pWnd = &m_PriorityCombo[nIdx];
		}
		break;

		case CRITERIA_VALUE_DATE_TYPE:
		{
			pWnd = &m_DateTimeCtrl[nIdx];
		}
		break;

		default: ASSERT(0); break;
	}

	pWnd->SetFocus();
	
	// Highlight Edit Controls to allow type-over
	if (m_CritState[nIdx].m_CurValueType == CRITERIA_VALUE_TEXT_TYPE)	
	{
		m_TextEdit[nIdx].SetSel(0, -1, FALSE);
	}
	else if (m_CritState[nIdx].m_CurValueType == CRITERIA_VALUE_AGE_TYPE
			|| m_CritState[nIdx].m_CurValueType == CRITERIA_VALUE_ATTACHCOUNT_TYPE
			|| m_CritState[nIdx].m_CurValueType == CRITERIA_VALUE_SIZE_TYPE
			|| m_CritState[nIdx].m_CurValueType == CRITERIA_VALUE_JUNKSCORE_TYPE)
	{
		m_NumEdit[nIdx].SetSel(0, -1, FALSE);
	}
}

// --------------------------------------------------------------------------

void CSearchView::ResizeCriteriaCtrls(int nIdx, int cx, int top)
{
	ASSERT(nIdx < m_CurCritCount);

	const int nFirstThird = cx / 3;
	const int nSecondThird = (cx * 2) / 3;

	MoveControl(&m_CategoryCombo[nIdx],     CONTROL_EDGE_SPACING, nFirstThird - CONTROL_EDGE_SPACING, CONTROL_EDGE_SPACING + top);

	switch (m_CritState[nIdx].m_CurVerbType)
	{
		case CRITERIA_VERB_TEXT_COMPARE_TYPE:
		{
			MoveControl(&m_TextCompareCombo[nIdx],  nFirstThird + CONTROL_EDGE_SPACING, nSecondThird - CONTROL_EDGE_SPACING, CONTROL_EDGE_SPACING + top);
		}
		break;

		case CRITERIA_VERB_EQUAL_COMPARE_TYPE:
		{
			MoveControl(&m_EqualCompareCombo[nIdx], nFirstThird + CONTROL_EDGE_SPACING, nSecondThird - CONTROL_EDGE_SPACING, CONTROL_EDGE_SPACING + top);
		}
		break;

		case CRITERIA_VERB_NUM_COMPARE_TYPE:
		{
			MoveControl(&m_NumCompareCombo[nIdx],   nFirstThird + CONTROL_EDGE_SPACING, nSecondThird - CONTROL_EDGE_SPACING, CONTROL_EDGE_SPACING + top);
		}
		break;

		case CRITERIA_VERB_DATE_COMPARE_TYPE:
		{
			MoveControl(&m_DateCompareCombo[nIdx],  nFirstThird + CONTROL_EDGE_SPACING, nSecondThird - CONTROL_EDGE_SPACING, CONTROL_EDGE_SPACING + top);
		}
		break;

		default: ASSERT(0); break;
	}

	switch (m_CritState[nIdx].m_CurValueType)
	{
		case CRITERIA_VALUE_TEXT_TYPE:
		{
			MoveControl(&m_TextEdit[nIdx],          nSecondThird + CONTROL_EDGE_SPACING, cx - CONTROL_EDGE_SPACING, CONTROL_EDGE_SPACING + top);
		}
		break;

		case CRITERIA_VALUE_AGE_TYPE:
		case CRITERIA_VALUE_ATTACHCOUNT_TYPE:
		case CRITERIA_VALUE_SIZE_TYPE:
		case CRITERIA_VALUE_JUNKSCORE_TYPE:
		{
			if (m_CritState[nIdx].m_bCurShowUnits)
			{
				// The static text should be right-edge flush and 4 pixels down from top of other controls
				int nLeft = MoveControl(&m_NumStatic[nIdx], -1, cx - CONTROL_EDGE_SPACING, CONTROL_EDGE_SPACING + top + 4); // Unknown LEFT input

				nLeft -= CONTROL_EDGE_SPACING;

				nLeft = MoveControl(&m_NumSpin[nIdx], -1, nLeft, CONTROL_EDGE_SPACING + top); // Unknown LEFT input
				MoveControl(&m_NumEdit[nIdx], nSecondThird + CONTROL_EDGE_SPACING, nLeft, CONTROL_EDGE_SPACING + top);
			}
			else
			{
				const int nLeft = MoveControl(&m_NumSpin[nIdx], -1, cx - CONTROL_EDGE_SPACING, CONTROL_EDGE_SPACING + top); // Unknown LEFT input
				MoveControl(&m_NumEdit[nIdx], nSecondThird + CONTROL_EDGE_SPACING, nLeft, CONTROL_EDGE_SPACING + top);
			}
		}
		break;

		case CRITERIA_VALUE_STATUS_TYPE:
		{
			MoveControl(&m_StatusCombo[nIdx],       nSecondThird + CONTROL_EDGE_SPACING, cx - CONTROL_EDGE_SPACING, CONTROL_EDGE_SPACING + top);
		}
		break;

		case CRITERIA_VALUE_LABEL_TYPE:
		{
			MoveControl(&m_LabelCombo[nIdx],        nSecondThird + CONTROL_EDGE_SPACING, cx - CONTROL_EDGE_SPACING, CONTROL_EDGE_SPACING + top);
		}
		break;

		case CRITERIA_VALUE_PERSONA_TYPE:
		{
			MoveControl(&m_PersonaCombo[nIdx],      nSecondThird + CONTROL_EDGE_SPACING, cx - CONTROL_EDGE_SPACING, CONTROL_EDGE_SPACING + top);
		}
		break;

		case CRITERIA_VALUE_PRIORITY_TYPE:
		{
			MoveControl(&m_PriorityCombo[nIdx],     nSecondThird + CONTROL_EDGE_SPACING, cx - CONTROL_EDGE_SPACING, CONTROL_EDGE_SPACING + top);
		}
		break;

		case CRITERIA_VALUE_DATE_TYPE:
		{
			MoveControl(&m_DateTimeCtrl[nIdx],     nSecondThird + CONTROL_EDGE_SPACING, cx - CONTROL_EDGE_SPACING, CONTROL_EDGE_SPACING + top);
		}
		break;

		default: ASSERT(0); break;
	}
}


// --------------------------------------------------------------------------

void CSearchView::InitializeCriteriaValues(int nIdx)
{
	ASSERT(nIdx < m_CurCritCount);
	CString scTemp;

	switch (m_CritState[nIdx].m_CurValueType)
	{
		case CRITERIA_VALUE_AGE_TYPE:
			GetIniString(IDS_INI_SEARCH_AGE_DEFAULT,scTemp);
			m_NumEdit[nIdx].SetWindowText(scTemp);
			GetIniString(IDS_INI_SEARCH_AGE_COMPARE_DEFAULT,scTemp);
			m_NumCompareCombo[nIdx].SelectString(-1,scTemp);
			break;
		case CRITERIA_VALUE_ATTACHCOUNT_TYPE:
			GetIniString(IDS_INI_SEARCH_ATTC_DEFAULT,scTemp);
			m_NumEdit[nIdx].SetWindowText(scTemp);
			GetIniString(IDS_INI_SEARCH_ATTC_COMPARE_DEFAULT,scTemp);
			m_NumCompareCombo[nIdx].SelectString(-1,scTemp);
			break;
	}
}

// --------------------------------------------------------------------------

void CSearchView::UpdateState(int nIdx) // Called when category changes
{
	ASSERT(m_bCritInitd[nIdx]);

	// Take current selection from category and setup state for other controls

	const int nSel = m_CategoryCombo[nIdx].GetCurSel();

	ASSERT(CB_ERR != nSel); // no selection

	if (nSel >= 0)
	{
		DWORD dwData = m_CategoryCombo[nIdx].GetItemData(nSel);
		CriteriaObject obj(static_cast<CriteriaObject>(dwData));
		m_CritState[nIdx].UpdateObj(obj);
	}
}

// --------------------------------------------------------------------------

void CSearchView::SetupDynamicCriteriaCtrls(int nIdx) // Called when category changes
{
	ASSERT(nIdx < m_CurCritCount);

	// Verbs
	CriteriaVerbType verb = m_CritState[nIdx].m_CurVerbType;

	m_TextCompareCombo[nIdx].ShowWindow((CRITERIA_VERB_TEXT_COMPARE_TYPE == verb) ? SW_SHOW : SW_HIDE);
	m_EqualCompareCombo[nIdx].ShowWindow((CRITERIA_VERB_EQUAL_COMPARE_TYPE == verb) ? SW_SHOW : SW_HIDE);
	m_NumCompareCombo[nIdx].ShowWindow((CRITERIA_VERB_NUM_COMPARE_TYPE == verb) ? SW_SHOW : SW_HIDE);
	m_DateCompareCombo[nIdx].ShowWindow((CRITERIA_VERB_DATE_COMPARE_TYPE == verb) ? SW_SHOW : SW_HIDE);

	// Values
	CriteriaValueType value = m_CritState[nIdx].m_CurValueType;

	m_TextEdit[nIdx].ShowWindow((CRITERIA_VALUE_TEXT_TYPE == value) ? SW_SHOW : SW_HIDE);

	{
		const bool bShowInt = (CRITERIA_VALUE_ATTACHCOUNT_TYPE == value) ||
							  (CRITERIA_VALUE_SIZE_TYPE == value) ||
							  (CRITERIA_VALUE_AGE_TYPE == value) ||
							  (CRITERIA_VALUE_JUNKSCORE_TYPE == value);

		m_NumEdit[nIdx].ShowWindow((bShowInt) ? SW_SHOW : SW_HIDE);
		m_NumSpin[nIdx].ShowWindow((bShowInt) ? SW_SHOW : SW_HIDE);
	}

	if (m_CritState[nIdx].m_bCurShowUnits)
	{
		ASSERT(!m_CritState[nIdx].m_UnitsStr.IsEmpty());
		m_NumStatic[nIdx].SetWindowText(m_CritState[nIdx].m_UnitsStr);
	}
	m_NumStatic[nIdx].ShowWindow(m_CritState[nIdx].m_bCurShowUnits ? SW_SHOW : SW_HIDE);

	m_StatusCombo[nIdx].ShowWindow((CRITERIA_VALUE_STATUS_TYPE == value) ? SW_SHOW : SW_HIDE);
	m_LabelCombo[nIdx].ShowWindow((CRITERIA_VALUE_LABEL_TYPE == value) ? SW_SHOW : SW_HIDE);
	m_PersonaCombo[nIdx].ShowWindow((CRITERIA_VALUE_PERSONA_TYPE == value) ? SW_SHOW : SW_HIDE);
	m_PriorityCombo[nIdx].ShowWindow((CRITERIA_VALUE_PRIORITY_TYPE == value) ? SW_SHOW : SW_HIDE);
	m_DateTimeCtrl[nIdx].ShowWindow((CRITERIA_VALUE_DATE_TYPE == value) ? SW_SHOW : SW_HIDE);
}

// --------------------------------------------------------------------------

bool CSearchView::IsCriteriaValid(int nIdx)
{
	ASSERT(nIdx < m_CurCritCount);

	return (true);
}

// --------------------------------------------------------------------------

bool CSearchView::GetCtrlCurItemData(CComboBox &cb, DWORD &dw)
{
	int nSel = cb.GetCurSel();

	ASSERT(CB_ERR != nSel);
	if (CB_ERR == nSel)
		return (false);

	DWORD dwVal = cb.GetItemData(nSel);
	ASSERT(CB_ERR != dwVal);

	if (CB_ERR == dwVal)
		return (false);

	dw = dwVal;
	return (true);
}

bool CSearchView::GetCtrlCurItemText(CComboBox &cb, CString &str)
{
	int nSel = cb.GetCurSel();

	ASSERT(CB_ERR != nSel);
	if (CB_ERR == nSel)
		return (false);

	cb.GetLBText(nSel, str);

	return (true);
}

bool CSearchView::GetCriteria(int nIdx, SearchCriteria &criteria)
{
	ASSERT(nIdx < m_CurCritCount);

	SearchCriteria TmpCriteria(criteria);

	VERIFY(TmpCriteria.SetCurObj(m_CritState[nIdx].m_CurObj));

	switch (m_CritState[nIdx].m_CurVerbType)
	{
		case CRITERIA_VERB_TEXT_COMPARE_TYPE:
		{
			DWORD dwVal = 0;
			if (!GetCtrlCurItemData(m_TextCompareCombo[nIdx], dwVal))
				return (false);

			VERIFY(TmpCriteria.SetCurVerbTextCmp(static_cast<CriteriaVerbTextCompare>(dwVal)));
		}
		break;

		case CRITERIA_VERB_EQUAL_COMPARE_TYPE:
		{
			DWORD dwVal = 0;
			if (!GetCtrlCurItemData(m_EqualCompareCombo[nIdx], dwVal))
				return (false);

			VERIFY(TmpCriteria.SetCurVerbEqualCmp(static_cast<CriteriaVerbEqualCompare>(dwVal)));
		}
		break;

		case CRITERIA_VERB_NUM_COMPARE_TYPE:
		{
			DWORD dwVal = 0;
			if (!GetCtrlCurItemData(m_NumCompareCombo[nIdx], dwVal))
				return (false);

			VERIFY(TmpCriteria.SetCurVerbNumCmp(static_cast<CriteriaVerbNumCompare>(dwVal)));
		}
		break;

		case CRITERIA_VERB_DATE_COMPARE_TYPE:
		{
			DWORD dwVal = 0;
			if (!GetCtrlCurItemData(m_DateCompareCombo[nIdx], dwVal))
				return (false);

			VERIFY(TmpCriteria.SetCurVerbDateCmp(static_cast<CriteriaVerbDateCompare>(dwVal)));
		}
		break;

		default: ASSERT(0); break;
	}

	switch (m_CritState[nIdx].m_CurValueType)
	{
		case CRITERIA_VALUE_TEXT_TYPE:
		{
			CString str;
			m_TextEdit[nIdx].GetWindowText(str);

			if (str.IsEmpty())
				return (false);

			VERIFY(TmpCriteria.SetCurValueText(static_cast<CriteriaValueText>(str)));
		}
		break;

		case CRITERIA_VALUE_AGE_TYPE:
		{
			CString str;
			m_NumEdit[nIdx].GetWindowText(str);

			if (str.IsEmpty())
				return (false);

			int nVal = atoi(str);

			VERIFY(TmpCriteria.SetCurValueAge(static_cast<CriteriaValueAge>(nVal)));
		}
		break;

		case CRITERIA_VALUE_ATTACHCOUNT_TYPE:
		{
			CString str;
			m_NumEdit[nIdx].GetWindowText(str);

			if (str.IsEmpty())
				return (false);

			int nVal = atoi(str);

			VERIFY(TmpCriteria.SetCurValueAttachCount(static_cast<CriteriaValueAttachCount>(nVal)));
		}
		break;

		case CRITERIA_VALUE_SIZE_TYPE:
		{
			CString str;
			m_NumEdit[nIdx].GetWindowText(str);

			if (str.IsEmpty())
				return (false);

			int nVal = atoi(str);

			VERIFY(TmpCriteria.SetCurValueSize(static_cast<CriteriaValueSize>(nVal)));
		}
		break;

		case CRITERIA_VALUE_JUNKSCORE_TYPE:
		{
			CString str;
			m_NumEdit[nIdx].GetWindowText(str);

			if (str.IsEmpty())
				return (false);

			int nVal = atoi(str);

			VERIFY(TmpCriteria.SetCurValueJunkScore(static_cast<CriteriaValueJunkScore>(nVal)));
		}
		break;

		case CRITERIA_VALUE_STATUS_TYPE:
		{
			DWORD dwVal = 0;
			if (!GetCtrlCurItemData(m_StatusCombo[nIdx], dwVal))
				return (false);

			VERIFY(TmpCriteria.SetCurValueStatus(static_cast<CriteriaValueStatus>(dwVal)));
		}
		break;

		case CRITERIA_VALUE_LABEL_TYPE:
		{
			const unsigned int nLabelIdx = m_LabelCombo[nIdx].GetCurLabel();

			VERIFY(TmpCriteria.SetCurValueLabel(static_cast<CriteriaValueLabel>(nLabelIdx)));
		}
		break;

		case CRITERIA_VALUE_PERSONA_TYPE:
		{
			CString str;
			if (!GetCtrlCurItemText(m_PersonaCombo[nIdx], str))
				return (false);

			VERIFY(TmpCriteria.SetCurValuePersona(static_cast<CriteriaValuePersona>(str)));
		}
		break;

		case CRITERIA_VALUE_PRIORITY_TYPE:
		{
			DWORD dwVal = 0;
			if (!GetCtrlCurItemData(m_PriorityCombo[nIdx], dwVal))
				return (false);

			VERIFY(TmpCriteria.SetCurValuePriority(static_cast<CriteriaValuePriority>(dwVal)));
		}
		break;

		case CRITERIA_VALUE_DATE_TYPE:
		{
			COleDateTime TmpDate = m_DateTimeCtrl[nIdx].GetDateTime();
			TmpDate.SetDate(TmpDate.GetYear(), TmpDate.GetMonth(), TmpDate.GetDay()); // Zero time

			VERIFY(TmpCriteria.SetCurValueDate(static_cast<CriteriaValueDate>(TmpDate)));
		}
		break;

		default: ASSERT(0); break;
	}

	criteria = TmpCriteria;

	return (true);
}

/////////////////////////////////////////////////////////////////////////////
// CSearchView message handlers

void CSearchView::OnOk() // Search BTN
{
	//	CWaitCursor waiting; // Show wait cursor until scope ends

	m_BeginBtn.EnableWindow(FALSE); // Disable the "Search" button for visual feedback

	m_ResultsMbxTabCtrl.SetCurSel(TAB_RESULTS_IDX); // Bring the results tab to the front
	UpdateTabContents();

	MainProgress(CRString(IDS_SEARCH_FINDING_MESSAGES));
	Progress( 0, CRString(IDS_SEARCH_PROGRESS_CLEARINGRESULTS), 100 ); // "Clearing results..."
	m_ResultsStatic.SetWindowText(CRString(IDS_SEARCH_PROGRESS_CLEARINGRESULTS)); // "Clearing results..."
	ClearResults();
	
	m_ResultsList.UpdateWindow(); // Update results list window, which should be empty -- visual feedback
	
	Progress(CRString(IDS_SEARCH_PROGRESS_SEARCHING)); // "Searching..."
	m_ResultsStatic.SetWindowText(CRString(IDS_SEARCH_PROGRESS_SEARCHING)); // "Searching..."
	if (!DoSearch())
		ErrorDialog(IDS_SEARCH_INTERNAL_ERROR);

	CloseProgress();

	UpdateResultsText();
	m_ResultsList.UpdateWindow(); // Update the results list window, which now has the results
	m_BeginBtn.EnableWindow(TRUE); // Re-Enable "Search" button
}

// --------------------------------------------------------------------------

void CSearchView::OnMoreBtn()
{
	const bool bHadFocus = (GetFocus() == (&m_MoreBtn));

	AddNewCriteria();

	if ((bHadFocus) && (!m_MoreBtn.IsWindowEnabled()))
		m_LessBtn.SetFocus();
}

// --------------------------------------------------------------------------

void CSearchView::OnLessBtn()
{
	const bool bHadFocus = (GetFocus() == (&m_LessBtn));

	RemoveCriteria();

	if ((bHadFocus) && (!m_LessBtn.IsWindowEnabled()))
		m_MoreBtn.SetFocus();
}

// --------------------------------------------------------------------------

void CSearchView::OnDestroy()
{
	SaveCriteria();

	LV_COLUMN col;
	col.mask = LVCF_WIDTH;

	for (int iCOL = 0; m_ResultsList.GetColumn(iCOL, &col); iCOL++)
	{
		ASSERT(col.cx > 0 && col.cx < 30000);  // Make sure it isn't some bizarre size
		switch (iCOL)
		{
		case COLUMN_DATE:
			// Date Column
			SetIniShort(IDS_INI_SEARCH_DATE_WIDTH, short(col.cx));
			break;
		case COLUMN_WHO:
			// Who Column
			SetIniShort(IDS_INI_SEARCH_WHO_WIDTH, short(col.cx));
			break;
		case COLUMN_SUBJECT:
			// Subject Column
			SetIniShort(IDS_INI_SEARCH_SUBJECT_WIDTH, short(col.cx));
			break;
		case COLUMN_MAILBOX:
			// Mailbox Column
			SetIniShort(IDS_INI_SEARCH_MBOX_WIDTH, short(col.cx));
			break;
		default:
			ASSERT(0);
			break;
		}
	}

	ClearResults(false);

	if (m_UseFastWay)
	{
		if (m_MsgResultArr)
		{
			free(m_MsgResultArr);
			m_MsgResultArr = NULL;
		}
	}

	C3DFormView::OnDestroy();
}

// --------------------------------------------------------------------------

bool CSearchView::DoSearch()
{
	// Build the list of criteria
	//MultSearchCriteria m_MSC;
	m_MSC.GetCriteriaList()->clear(); // erase(msc.GetCriteriaList()->begin(), msc.GetCriteriaList()->end());
	SearchCriteria criteria;

	if (m_AndRadioBtn.GetCheck() == 1)
	{
		m_MSC.SetOpAND();
	}
	else if (m_OrRadioBtn.GetCheck() == 1)
	{
		m_MSC.SetOpOR();
	}
	else
	{
		ASSERT(0);
		return (false);
	}

	for (int idx = 0; idx < m_CurCritCount; idx++)
	{
		if (GetCriteria(idx, criteria))
			m_MSC.Add(criteria); // Only add the valid criteria lines
	}

	// Do a sanity check. We disable the search btn when there are zero
	// valid criteria entries. So there should always be at least one
	// criteria when we get to this point.
	ASSERT(m_MSC.GetCriteriaList()->size() > 0);

	if (m_MSC.GetCriteriaList()->size() == 0)
		return false;

	// Get the list of checked mailbox names from the tree control
	std::deque<CString>		strlstCheckedMailboxes;

	if ( ShouldUseIndexedSearch() )
	{
		std::deque<CString>		strlstUncheckMailboxes;
		
		m_MBoxTree.GetListMailboxPathnamesWithSelectedState(true, CMBoxTreeCtrlCheck::TCS_CHECKED, strlstCheckedMailboxes);
		m_MBoxTree.GetListMailboxPathnamesWithSelectedState(true, CMBoxTreeCtrlCheck::TCS_UNCHECKED, strlstUncheckMailboxes);
		
		//	Do search manager method
		SearchManager::Instance()->DoSearch( m_MSC, strlstCheckedMailboxes, strlstUncheckMailboxes,
											 this, TICK_DELTA, m_bShouldIncludeDeletedIMAPMessages );
	}
	else
	{
		m_MBoxTree.GetListMailboxPathnamesWithSelectedState(false, CMBoxTreeCtrlCheck::TCS_CHECKED, strlstCheckedMailboxes);
		
		SearchEngine search_eng(&m_MSC, &strlstCheckedMailboxes);
		ResultsCBType cbfn = makeCallback( (ResultsCBType*)0, (*this), &CSearchView::ProcessResults);

#ifdef _MY_TICK_TRACE_
		DWORD nStartTick = GetTickCount();
#endif // _MY_TICK_TRACE_

		if (!search_eng.Start(cbfn, TICK_DELTA))
		{
			ASSERT(0);
			return (false);
		}

#ifdef _MY_TICK_TRACE_
		DWORD nEndTick = GetTickCount();
		DWORD nDelta = nEndTick -  nStartTick;

		::MyTrace("CSearchView::DoSearch: Search completed in %lu ticks, finding %d matches.\n", nDelta, m_nFoundCount);
#endif // _MY_TICK_TRACE_

		m_nIMAPSkippedCount = search_eng.GetImapSkipCount();
	}

	return (true);
}

// --------------------------------------------------------------------------

bool CSearchView::ProcessResults(std::deque<SearchResult> &results)
{
//	::Beep(2500, 10);

	bool bEsc = false;

	if (results.size() > 0)
	{
		m_ResultsList.SetRedraw(FALSE); // Turn off redraw for the results list

		std::deque<SearchResult>::iterator itr;
		for (itr = results.begin(); itr != results.end(); itr++)
		{
			AddResult(*itr);
			if (bEsc = (PumpEsc(TICK_DELTA) == TRUE))
				break;
		}

		m_ResultsList.SetRedraw(TRUE); // Turn redraw back on for the results list

		UpdateResultsText();
	}

	return (!bEsc);
}

// --------------------------------------------------------------------------

void CSearchView::AddResult(const SearchResult &result)
{
	const int nInsertIdx = m_ResultsList.InsertItem(m_ResultsList.GetItemCount(), NULL);

	ASSERT(nInsertIdx != (-1));
	if (nInsertIdx != (-1))
	{
		CMsgResult *pMR = NewMsgResult(result);

		ASSERT(pMR);
		if (!pMR)
			return;

		ASSERT(result.GetMsgID() != (-1));

		// Set the column text
		VERIFY(UpdateListText(result, nInsertIdx));

		// Set the item data
		VERIFY(m_ResultsList.SetItemData(nInsertIdx, (DWORD) pMR));
	}
}

// --------------------------------------------------------------------------

void CSearchView::InitMem()
{
	if (!m_MsgResultArr)
	{
		m_MsgResultArr = (CMsgResult *) malloc(MAX_LISTCOUNT * sizeof(CMsgResult));
		m_MsgResultArrCount = 0;
	}
}

// --------------------------------------------------------------------------

CMsgResult *CSearchView::NewMsgResult(const SearchResult &result)
{
	CMsgResult *pMsg = NULL;

	if (m_UseFastWay)
	{
		ASSERT(m_MsgResultArrCount < MAX_LISTCOUNT);

		if (m_MsgResultArrCount >= MAX_LISTCOUNT)
			return (NULL);

		if (!m_MsgResultArr)
		{
			InitMem();
		}
		
		pMsg = (CMsgResult *) ((m_MsgResultArr) + (m_MsgResultArrCount++));

//		CMsgResult *pPlaceNew = new(pMsg) CMsgResult(result);
		//	CMsgResult msg(result);
		
		//	(*pMsg) = msg;
//		memcpy(pMsg, &msg, sizeof(CMsgResult));
	}
	else
	{
		pMsg = DEBUG_NEW CMsgResult(result);
		m_MsgResultList.push_back(pMsg);
	}

	return (pMsg);
}

// --------------------------------------------------------------------------

void CSearchView::ClearResults(bool bPump /* = true */)
{
#ifdef _MY_TICK_TRACE_
	::MyTrace("CSearchView::ClearResults: Starting to clear %lu msgs...\n", m_MsgResultList.size());
	DWORD nStartTick = GetTickCount();
#endif // _MY_TICK_TRACE_

	if (m_UseFastWay)
	{
		m_MsgResultArrCount = 0;
	}
	else
	{
		// Use STL list
		std::list<CMsgResult *>::iterator itr;
		for (itr = m_MsgResultList.begin(); itr != m_MsgResultList.end(); ++itr)
		{
			ASSERT(*itr);
			delete (*itr);

			if (bPump)
				PumpEsc(TICK_DELTA); // Pump the msg queue to avoid app lockup
		}

		m_MsgResultList.clear();
	}

#ifdef _MY_TICK_TRACE_
	DWORD nMidTick = GetTickCount();
#endif // _MY_TICK_TRACE_

	m_nIMAPSkippedCount = 0;

	ClearList();
	UpdateResultsText();
//	ResizeColumns();

#ifdef _MY_TICK_TRACE_
	DWORD nEndTick = GetTickCount();

	DWORD nDeltaTick = nEndTick - nStartTick;
	DWORD nEraseDelta = nMidTick - nStartTick;

	::MyTrace("CSearchView::ClearResults: Overall %lu ticks of which %lu ticks to clean mem.\n", nDeltaTick, nEraseDelta);
#endif // _MY_TICK_TRACE_
}

// --------------------------------------------------------------------------

void CSearchView::ClearList()
{
	m_ResultsList.DeleteAllItems();
}

// --------------------------------------------------------------------------

void CSearchView::ResizeColumns()
{
	// Resize the last column to fill width of list

	CRect ClientRct;
	m_ResultsList.GetClientRect(ClientRct);

	const int nMbxColWidth = m_ResultsList.GetColumnWidth(COLUMN_MAILBOX);
	const int nWhoColWidth = m_ResultsList.GetColumnWidth(COLUMN_WHO);
	const int nDateColWidth = m_ResultsList.GetColumnWidth(COLUMN_DATE);
	const int nSubjectColWidth = m_ResultsList.GetColumnWidth(COLUMN_SUBJECT);

	const int nClientRectWidth = ClientRct.Width();
	const int nRemainWidth = nClientRectWidth - (nMbxColWidth + nWhoColWidth + nDateColWidth);

	if ((nRemainWidth > 0) && (nRemainWidth > nSubjectColWidth))
		m_ResultsList.SetColumnWidth(COLUMN_SUBJECT, nRemainWidth);
}

// --------------------------------------------------------------------------

void CSearchView::ResizeTabContents()
{
#ifdef _MY_TRACE_
::MyTrace("CSearchView::ResizeTabContents()\n");
#endif // _MY_TRACE_

	CRect DisplayRct;
	m_ResultsMbxTabCtrl.GetWindowRect(DisplayRct);
	ScreenToClient(DisplayRct);
	m_ResultsMbxTabCtrl.AdjustRect(FALSE, DisplayRct);

	DisplayRct.top += 2;

	ClientToScreen(DisplayRct);
	m_ResultsMbxTabCtrl.ScreenToClient(DisplayRct);

	// These are children of the tab control
	m_ResultsList.MoveWindow(DisplayRct);
	m_MBoxTree.MoveWindow(DisplayRct);
}

// --------------------------------------------------------------------------

void CSearchView::ResizeControls(int cx, int cy)
{
	// Resize the tab rect to the window
	{
		CRect TabRct;	
		m_ResultsMbxTabCtrl.GetWindowRect(TabRct);
		ScreenToClient(TabRct);
		TabRct.bottom = cy - CONTROL_EDGE_SPACING;
		TabRct.right = cx - CONTROL_EDGE_SPACING;
		m_ResultsMbxTabCtrl.MoveWindow(TabRct);
	}

	// Place the controls in the tab display area
	ResizeTabContents();

	// Move the search button right-aligned
	CRect   rectBeginButton;
	m_BeginBtn.GetWindowRect(rectBeginButton);
	ScreenToClient(rectBeginButton);

	int     nWidth = rectBeginButton.Width();
	rectBeginButton.right = cx - CONTROL_EDGE_SPACING;
	rectBeginButton.left = rectBeginButton.right - nWidth;

	m_BeginBtn.MoveWindow(rectBeginButton);

    // Move the "powered by X1" badge keeping its right aligned with the begin button
    CRect   rectX1Static;
    m_PoweredByX1Static.GetWindowRect(rectX1Static);
    ScreenToClient(rectX1Static);

    nWidth = rectX1Static.Width();
    rectX1Static.left = rectBeginButton.left - m_HorizOffsetX1Static;
    rectX1Static.right = rectX1Static.left + nWidth;

    m_PoweredByX1Static.MoveWindow(rectX1Static);

    CRect   rectX1Icon;
    m_PoweredByX1Icon.GetWindowRect(rectX1Icon);
    ScreenToClient(rectX1Icon);

    rectX1Icon.left = rectBeginButton.left - m_HorizOffsetX1Icon;
    rectX1Icon.right = rectX1Icon.left + kX1IconWidth;
	rectX1Icon.bottom = rectX1Icon.top + kX1IconHeight;

    m_PoweredByX1Icon.MoveWindow(rectX1Icon);

	// Align the results text
	CRect      rectTabs(0,0,0,0);

	if (m_ResultsMbxTabCtrl.GetItemCount() > 0)
		m_ResultsMbxTabCtrl.GetItemRect(m_ResultsMbxTabCtrl.GetItemCount() - 1, rectTabs);

	CRect       rectStatusText;	
	m_ResultsStatic.GetWindowRect(rectStatusText);
	ScreenToClient(rectStatusText);
	rectStatusText.left = rectTabs.right + CONTROL_EDGE_SPACING * 4;
	rectStatusText.right = rectBeginButton.left - m_HorizOffsetResultsStatic;

	if (rectStatusText.Width() < STATUS_TEXT_MIN_WIDTH) // Minimum width
		rectStatusText.left = rectStatusText.right - STATUS_TEXT_MIN_WIDTH;

	m_ResultsStatic.MoveWindow(rectStatusText);

//	ResizeColumns();
}

// --------------------------------------------------------------------------

void CSearchView::OnSize(UINT nType, int cx, int cy)
{
#ifdef _MY_TRACE_
::MyTrace("CSearchView::OnSize(%d, %d)\n", cx, cy);
#endif // _MY_TRACE_

	C3DFormView::OnSize(nType, cx, cy);

	// Don't bother if the controls haven't been initialized.
	if (!m_bInitilized)
	{
#ifdef _MY_TRACE_
::MyTrace("    CSearchView::OnSize(%d, %d) [SKIPPED]\n", cx, cy);
#endif // _MY_TRACE_
		return;
	}

	ResizeControls(cx, cy); // Non-criteria controls
	ResizeAllCriteriaCtrls();
}

// --------------------------------------------------------------------------

BOOL CSearchView::OpenMsg(CMsgResult *pMR)
{
	ASSERT(pMR);

	if (pMR)
	{
		CTocDoc *pTocDoc = ::GetToc(pMR->m_sPath, NULL, FALSE, FALSE);

		ASSERT(pTocDoc);
		if (!pTocDoc)
			return (FALSE);

		CSumList &		listSums = pTocDoc->GetSumList();

		CSummary *pSum = listSums.GetByMessageId(pMR->m_nMsgID);

		ASSERT(pSum);
		if (!pSum)
			return (FALSE);

		CMessageDoc *pDoc = pSum->GetMessageDoc();

		ASSERT(pDoc);
		if (!pDoc)
			return (FALSE);

		//Try to hilite the search string to show the search context in the message
		bool bWholeWord = false;
		CString strSearch = "";
		//Show the message
		pSum->Display();	
		MultSearchCriteria::eStart start = MultSearchCriteria::FIRST;
		while(m_MSC.GetSearchString(strSearch, &bWholeWord, start))
		{
			//search engine doesnt support case-sensitive searches, now; may change later
			// Get the view so we can search in it		
			POSITION poss = pDoc->GetFirstViewPosition();

			CView * pView = pDoc->GetNextView(poss);
			ASSERT(pView);

			QCProtocol*	view = QCProtocol::QueryProtocol( QCP_FIND, ( CObject* )pView );
			ASSERT(view);

	
			if(view->DoFindFirst(strSearch, false, bWholeWord, TRUE))
			{
				QCFindMgr *pFindMgr = QCFindMgr::GetFindMgr();
				ASSERT(pFindMgr);

				//update global find text, so F3 works
				pFindMgr->UpdateLastFindState(strSearch, false, bWholeWord);
				break;
			}
			else // also not found in header
			if(pView->IsKindOf(RUNTIME_CLASS(CHeaderView)))
			{
				if(pDoc == NULL)
					break;

				ASSERT_KINDOF(CCompMessageDoc, pDoc);
				if(!pDoc->IsKindOf(RUNTIME_CLASS(CCompMessageDoc)))
					break;

				CView * p_compVw = ((CCompMessageDoc *) pDoc)->GetCompView();

				ASSERT(p_compVw);
				if(p_compVw == NULL)
					break;

				QCProtocol*	bodyview = QCProtocol::QueryProtocol( QCP_FIND, ( CObject* )p_compVw);
				ASSERT(bodyview);

				if(bodyview->DoFindFirst(strSearch, false, bWholeWord, TRUE))
				{
					QCFindMgr *pFindMgr = QCFindMgr::GetFindMgr();
					ASSERT(pFindMgr);

					//update global find text, so F3 works
					pFindMgr->UpdateLastFindState(strSearch, false, bWholeWord);
					break;
				}


			}
			start = MultSearchCriteria::NEXT;
		}

		// Make sure window is shown, just in case above selection
		// of found text doesn't cause the message window to open.
		if (!pSum->m_FrameWnd || pSum->m_FrameWnd->IsWindowVisible() == FALSE)
			pSum->Display();
	
		return (TRUE);
	}

	return (FALSE);
}

// --------------------------------------------------------------------------

BOOL CSearchView::UpdateListInfo(CSummary *pSum, int nIdx)
{
	SearchResult TmpResult(pSum);
	VERIFY(UpdateListText(TmpResult, nIdx));

	CMsgResult *pMR = (CMsgResult *) m_ResultsList.GetItemData(nIdx);
	ASSERT(pMR);

	if (pMR)
	{
		// Copy the new data into the old result location
		CMsgResult NewResult(TmpResult);
		(*pMR) = NewResult;

		return (TRUE);
	}

	return (FALSE);
}

// --------------------------------------------------------------------------

BOOL CSearchView::UpdateListText(const SearchResult &result, int nIdx)
{
	m_ResultsList.SetItemText(nIdx, COLUMN_MAILBOX, result.GetMbxName() );
	m_ResultsList.SetItemText(nIdx, COLUMN_WHO, result.GetWho());
	m_ResultsList.SetItemText(nIdx, COLUMN_DATE, result.GetDate());
	m_ResultsList.SetItemText(nIdx, COLUMN_SUBJECT, result.GetSubject());

	return (TRUE);
}

// --------------------------------------------------------------------------

BOOL CSearchView::OpenTOC(CMsgResult * pMR, CObArray * pArrayOpenedTOCs)
{
	ASSERT(pMR);

	if (pMR)
	{
		CTocDoc *pTocDoc = ::GetToc(pMR->m_sPath, NULL, FALSE, FALSE);

		ASSERT(pTocDoc);
		if (!pTocDoc)
			return (FALSE);

		CSumList &		listSums = pTocDoc->GetSumList();

		CSummary *pSum = listSums.GetByMessageId(pMR->m_nMsgID);

		ASSERT(pSum);
		if (!pSum)
			return (FALSE);

		const BOOL bDisplayed = pTocDoc->Display();

		// Open the view if it isn't open already and do the appropriate
		// message selecting.
		CTocView* pTocView = pTocDoc->GetView();

		ASSERT(bDisplayed);
		ASSERT(pTocView);
		
		if (bDisplayed && pTocView)
		{
			//	See if we should reset the selection of the tocview.
			//	Are we selecting just one message (i.e. pArrayOpenedTOCs is NULL)?
			//	Have we just opened a TOC when selecting multiple messages?
			bool		bAlreadyOpened = false;

			if (pArrayOpenedTOCs)
			{
				for (int i = 0; (i < pArrayOpenedTOCs->GetSize()); ++i)
				{
					if ( pTocView == (CTocView*) pArrayOpenedTOCs->GetAt(i) )
					{
						bAlreadyOpened = true;
						break;
					}
				}
			}

			// Clear the selection the first time we open a toc in each operation
			// so that all relevant messages are selected in the toc, and only those
			// messages are selected.
			if (!bAlreadyOpened)
			{
				pTocView->SelectAll(FALSE, TRUE);

				if (pArrayOpenedTOCs)
					pArrayOpenedTOCs->Add(pTocView);
			}

			// Select the current message - pass in true for the second parameter
			// because this is the result of a direct user action and we want
			// the auto-mark-as-read behavior to apply.
			pSum->Select(TRUE, true);
		}
	}

	return (TRUE);
}

// --------------------------------------------------------------------------

bool CSearchView::GetSumList(std::list<CSummary *> &SumList, /* const */ std::list<int> &IdxList,
							 bool bAllowTrash, bool bAllowOut, bool bAllowJunk, int *iNumRemoved)
{
	SumList.clear();

	CMsgResult *pMR = NULL;
	CTocDoc *pTocDoc = NULL;
	CSummary *pSum = NULL;

	*iNumRemoved = 0;

	std::list<int>::iterator itr = IdxList.begin();
	while (itr != IdxList.end())
	{
		pMR = (CMsgResult *) m_ResultsList.GetItemData(*itr);
		ASSERT(pMR);
		if (!pMR)
			continue;

		pTocDoc = ::GetToc(pMR->m_sPath, NULL, FALSE, FALSE, FALSE);
		ASSERT(pTocDoc);
		if (!pTocDoc)
		{
			++itr;
			continue;
		}

		CSumList &		listSums = pTocDoc->GetSumList();

		pSum = listSums.GetByMessageId(pMR->m_nMsgID);
		
		//	Message gone?
		if (!pSum)
		{
			//	Remove it and skip it (without ASSERT because this is perfectly normal -
			//	the Search window is not notified when other mailboxes make changes).
			IdxList.erase(itr++);
			continue;
		}

		const int MBType = pTocDoc->m_Type;
		if ((!bAllowTrash && (MBType == MBT_TRASH)) ||
			(!bAllowOut && (MBType == MBT_OUT)) ||
			(!bAllowJunk && (MBType == MBT_JUNK)))
		{
			IdxList.erase(itr++);
			++(*iNumRemoved);
		}
		else
		{
			SumList.push_back(pSum);
			++itr;
		}
	}

	return (true);
}

// --------------------------------------------------------------------------

bool CSearchView::GetJunkSumList(std::list<CSummary *> &SumList,
								 /* const */ std::list<int> &IdxList,
								 int *iNumRemoved)
{
	SumList.clear();

	CMsgResult *pMR = NULL;
	CTocDoc *pTocDoc = NULL;
	CSummary *pSum = NULL;

	*iNumRemoved = 0;

	std::list<int>::iterator itr = IdxList.begin();
	while (itr != IdxList.end())
	{
		pMR = (CMsgResult *) m_ResultsList.GetItemData(*itr);
		ASSERT(pMR);
		if (!pMR)
			continue;

		pTocDoc = ::GetToc(pMR->m_sPath, NULL, FALSE, FALSE);
		ASSERT(pTocDoc);
		if (!pTocDoc)
			continue;

		CSumList &		listSums = pTocDoc->GetSumList();

		pSum = listSums.GetByMessageId(pMR->m_nMsgID);
		
		//	Message gone?
		if (!pSum)
		{
			//	Remove it and skip it (without ASSERT because this is perfectly normal -
			//	the Search window is not notified when other mailboxes make changes).
			IdxList.erase(itr++);
			continue;
		}

		if (pTocDoc->m_Type != MBT_JUNK)
		{
			IdxList.erase(itr++);
			++(*iNumRemoved);
		}
		else
		{
			SumList.push_back(pSum);
			++itr;
		}
	}

	return (true);
}

BOOL CSearchView::OnCmdMsg(UINT nID, int nCode, void* pExtra, AFX_CMDHANDLERINFO* pHandlerInfo)
{ 
	return C3DFormView::OnCmdMsg(nID, nCode, pExtra, pHandlerInfo);
}

// --------------------------------------------------------------------------

void CSearchView::OnUpdateNeedsSelection(CCmdUI* pCmdUI)
{
	std::list<int>	SelList;
	
	//	Ignore result of GetCurSel because it always returns true (provided
	//	our parameter to it is non-NULL). Check empty() instead.
	m_ResultsList.GetCurSel(&SelList);

	pCmdUI->Enable( !SelList.empty() );
}

// --------------------------------------------------------------------------

void CSearchView::OnUpdateJunk(CCmdUI* pCmdUI)
{
	OnUpdateFullFeatureSet(pCmdUI);

	if (!UsingFullFeatureSet())
		return;
	
	std::list<int>			SelList;
	
	//	Ignore result of GetCurSel because it always returns true (provided
	//	our parameter to it is non-NULL). Check empty() instead.
	m_ResultsList.GetCurSel(&SelList);

	std::list<CSummary *>	SumList;
	int						iNumRemoved = 0;

	// If we are selectively enabling Junk and Junk items are selected, don't enable Junk menu.
	if (!GetIniShort(IDS_INI_ALWAYS_ENABLE_JUNK))
	{
		std::list<CSummary *>	SumList;
		int						iNumRemoved = 0;
		GetJunkSumList(SumList, SelList, &iNumRemoved);
		if (!SumList.empty())
		{
			pCmdUI->Enable(FALSE);
			return;
		}
		m_ResultsList.GetCurSel(&SelList);
	}

	GetSumList(SumList,
				SelList,
				true/*bAllowTrash*/,
				false/*bAllowOut*/,
				(GetIniShort(IDS_INI_ALWAYS_ENABLE_JUNK) != 0)/*bAllowJunk*/,
				&iNumRemoved);

	// Enable if we have valid items.
	pCmdUI->Enable( !SumList.empty() );
}

// --------------------------------------------------------------------------

void CSearchView::OnUpdateNotJunk(CCmdUI* pCmdUI)
{
	OnUpdateFullFeatureSet(pCmdUI);

	if (!UsingFullFeatureSet())
		return;
	
	std::list<int>			SelList;
	
	//	Ignore result of GetCurSel because it always returns true (provided
	//	our parameter to it is non-NULL). Check empty() instead.
	m_ResultsList.GetCurSel(&SelList);

	std::list<CSummary *>	SumList;
	int						iNumRemoved = 0;
	unsigned int			iJunkSums = 0;

	if (!GetIniShort(IDS_INI_ALWAYS_ENABLE_JUNK))
	{
		// If we are selectively enabling Not Junk and non-junk items are selected, don't enable Not Junk menu.
		std::list<CSummary *>	SumList;
		int						iNumRemoved = 0;
		GetJunkSumList(SumList, SelList, &iNumRemoved);
		iJunkSums = SumList.size();
		m_ResultsList.GetCurSel(&SelList);
		GetSumList(SumList,
					SelList,
					true/*bAllowTrash*/,
					false/*bAllowOut*/,
					true/*bAllowJunk*/,
					&iNumRemoved);

		// Enable if we have valid items.
		pCmdUI->Enable(SumList.size() == iJunkSums);
	}
	else
	{
		// If we are not selectively enabling Not Junk enable Not Junk menu if anything is selected.
		GetSumList(SumList,
					SelList,
					true/*bAllowTrash*/,
					false/*bAllowOut*/,
					true/*bAllowJunk*/,
					&iNumRemoved);

		// Enable if we have valid items.
		pCmdUI->Enable(!SumList.empty());
	}
}

// --------------------------------------------------------------------------

void CSearchView::OnUpdateDelete(CCmdUI* pCmdUI)
{
	std::list<int>			SelList;
	
	//	Ignore result of GetCurSel because it always returns true (provided
	//	our parameter to it is non-NULL). Check empty() instead.
	m_ResultsList.GetCurSel(&SelList);

	std::list<CSummary *>	SumList;
	int						iNumRemoved = 0;

	GetSumList(SumList,
				SelList,
				false/*bAllowTrash*/,
				true/*bAllowOut*/,
				true/*bAllowJunk*/,
				&iNumRemoved);

	// Delete is not enabled for the Trash mailbox
	pCmdUI->Enable( !SumList.empty() );
}

// --------------------------------------------------------------------------

void CSearchView::OnUpdateResponse(CCmdUI* pCmdUI)
{
	std::list<int>			SelList;
	
	//	Ignore result of GetCurSel because it always returns true (provided
	//	our parameter to it is non-NULL). Check empty() instead.
	m_ResultsList.GetCurSel(&SelList);

	std::list<CSummary *>	SumList;
	int						iNumRemoved = 0;

	GetSumList(SumList,
				SelList,
				true/*bAllowTrash*/,
				(pCmdUI->m_nID == ID_MESSAGE_REPLY_ALL)/*only allow out mailbox with reply to all*/,
				true,/*bAllowJunk*/
				&iNumRemoved);

	// Reply and redirect are not enabled for the Out mailbox
	pCmdUI->Enable( !SumList.empty() );

	// Reply and Reply All menu items get changed by the main window depending on
	// which one gets mapped to Ctrl+R, so we need to continue routing
	if (pCmdUI->m_nID == ID_MESSAGE_REPLY || pCmdUI->m_nID == ID_MESSAGE_REPLY_ALL)
		pCmdUI->ContinueRouting();
}

// --------------------------------------------------------------------------

void CSearchView::OnUpdateDynamicCommand(CCmdUI* pCmdUI)
{
	QCCommandObject*	pCommand;
	COMMAND_ACTION_TYPE	theAction;	

	if( pCmdUI->m_pSubMenu == NULL )
	{
		if( g_theCommandStack.Lookup( (WORD) (pCmdUI->m_nID), &pCommand, &theAction ) )
		{
			std::list<int>	SelList;

			//	Ignore result of GetCurSel because it always returns true (provided
			//	our parameter to it is non-NULL). Check empty() instead.
			m_ResultsList.GetCurSel(&SelList);

			switch (theAction)
			{
				case CA_REPLY_WITH:
				case CA_REPLY_TO_ALL_WITH:
				case CA_REDIRECT_TO:
					{
						std::list<CSummary *>	SumList;
						int						iNumRemoved = 0;

						GetSumList(SumList,
									SelList,
									true/*bAllowTrash*/,
									false/*bAllowOut*/,
									true/*bAllowJunk*/,
									&iNumRemoved);
						
						// Reply and redirect are not enabled for the Out mailbox
						pCmdUI->Enable( !SumList.empty() );
					}
					return;

				case CA_TRANSFER_TO:
				case CA_TRANSFER_NEW:
				case CA_FORWARD_TO:
					pCmdUI->Enable( !SelList.empty() );
					return;

				case CA_SAVED_SEARCH:
					pCmdUI->Enable( UsingFullFeatureSet() ? TRUE : FALSE );
					return;
			}
		}
	}

	pCmdUI->ContinueRouting();
}

// --------------------------------------------------------------------------

BOOL CSearchView::OnComposeMessage(UINT nID)
{
	std::list<int>			SelList;
	
	m_ResultsList.GetCurSel(&SelList);

	std::list<CSummary *>	SumList;
	int						iNumRemoved = 0;

	GetSumList( SumList,
				SelList,
				true/*bAllowTrash*/,
				((nID == ID_MESSAGE_FORWARD) || (nID == ID_MESSAGE_REPLY_ALL))/*allow out mailbox only for forwarding messages and reply to all*/,
				true/*bAllowJunk*/,
				&iNumRemoved );

	if ( !SumList.empty() )
	{
		CCursor cursor;
		
		StartGroup();

		MessageCascadeSpot = 0;
		::AsyncEscapePressed(TRUE);				// reset Escape key logic

		for (std::list<CSummary *>::iterator itr = SumList.begin(); itr != SumList.end(); ++itr)
		{
			if (::AsyncEscapePressed())
				break;
		
			(*itr)->ComposeMessage(nID);
		}
		MessageCascadeSpot = -1;

		EndGroup();
	}
	
	return TRUE;
}

// --------------------------------------------------------------------------

BOOL CSearchView::OnOpenCommand(UINT nID)
{
	CMsgResult *		pMR = NULL;
	int					nItem = -1;
	CObArray			arrayOpenedTOCs;

	while ( (nItem = m_ResultsList.GetNextItem(nItem, LVNI_SELECTED)) != -1 )
	{
		pMR = (CMsgResult *) m_ResultsList.GetItemData(nItem);

		if (nID == ID_OPEN_TOC)
			OpenTOC(pMR, &arrayOpenedTOCs);
		else	//	nID == ID_OPEN_MSG
			OpenMsg(pMR);
	}

	return TRUE;
}

// --------------------------------------------------------------------------

void CSearchView::OnDeleteMessages()
{
	std::list<int>		SelIdxList;

	m_ResultsList.GetCurSel(&SelIdxList);

	if (SelIdxList.size() > 0)
	{
		int						iNumRemoved = 0;
		std::list<CSummary *>	SumList;

		GetSumList(SumList,
					SelIdxList,
					false/*bAllowTrash*/,
					true/*bAllowOut*/,
					true/*bAllowJunk*/,
					&iNumRemoved);

		//	Check to see if there are any items left to process
		if (SelIdxList.size() > 0)
		{
			//	Ask user if they want to delete
			if ( VerifyDelete(SumList) )
			{
				CWaitCursor wait_cursor;

				Progress( 0, CRString(IDS_SEARCH_PROGRESS_DELETING), SumList.size() ); // "Deleting..."
				
				bool								bEsc = false;
				std::list<int>::iterator			selItr = SelIdxList.begin();
				std::list<CSummary *>::iterator		selSumItr = SumList.begin();
		
				for ( ; selSumItr != SumList.end(); ++selSumItr )
				{
					if ( selItr == SelIdxList.end() )
					{
						//	SelIdxList is supposed to be the same length as SumList
						ASSERT(0);
						break;
					}
					
					VERIFY( DoDelete(*selSumItr, *selItr) );

					ProgressAdd(1);

					if (bEsc = (PumpEsc(TICK_DELTA) == TRUE))
						break;

					++selItr;
				}

				//	SelIdxList is supposed to be the same length as SumList
				VERIFY( bEsc || (selItr == SelIdxList.end()) );

				CloseProgress();
			}
		}

		//	Warn the user that you can't trash an item already in the Trash mailbox
		if (iNumRemoved > 0)
			ErrorDialog(IDS_SEARCH_RESULTS_TRASH_ERROR);
	}
}

// --------------------------------------------------------------------------

BOOL CSearchView::OnJunkCommand(UINT nID)
{
	std::list<int>		SelIdxList;

	m_ResultsList.GetCurSel(&SelIdxList);

	if (SelIdxList.size() > 0)
	{
		int						iNumRemoved = 0;
		std::list<CSummary *>	SumList;

		if (GetIniShort(IDS_INI_ALWAYS_ENABLE_JUNK))
		{
			GetSumList(SumList,
						SelIdxList,
						true/*bAllowTrash*/,
						false/*bAllowOut*/,
						true/*bAllowJunk*/,
						&iNumRemoved);
		}
		else if (nID == ID_NOT_JUNK)
		{
			GetJunkSumList(SumList,
						SelIdxList,
						&iNumRemoved);
		}
		else
		{
			GetSumList(SumList,
						SelIdxList,
						true/*bAllowTrash*/,
						false/*bAllowOut*/,
						false/*bAllowJunk*/,
						&iNumRemoved);
		}

		//	Check to see if there are any items left to process
		if (SelIdxList.size() > 0)
		{
			CWaitCursor wait_cursor;

			if (nID == ID_JUNK)
			{
				Progress( 0, CRString(IDS_SEARCH_PROGRESS_JUNKING), SumList.size() ); // "Junking..."
			}
			else
			{
				Progress( 0, CRString(IDS_SEARCH_PROGRESS_UNJUNKING), SumList.size() ); // "Unjunking..."
			}
			
			bool								bEsc = false;
			std::list<int>::iterator			selItr = SelIdxList.begin();
			std::list<CSummary *>::iterator		selSumItr = SumList.begin();
			CSummary							*pSum = NULL;
			CTocDoc								*pTocDoc = NULL;
			int									nItem = 0;
			CMsgResult							*pMR = NULL;
			CFilterActions						filt;

			if (nID == ID_JUNK)
			{
				if (!filt.StartFiltering())
				{
					return FALSE;
				}
			}

			// Hash the address book for translator use and potentially
			// determining if we need to add not junked senders to the AB.
			CObArray	oaABHashes;

			CFilter::GenerateHashes(&oaABHashes);

			// Determine if we're adding Not Junk'ed senders to the address book.
			bool		bAddNotJunkedSendersToAB = (nID == ID_NOT_JUNK) &&
												   GetIniShort(IDS_INI_ADDBOOK_IS_WHITELIST) &&
												   GetIniShort(IDS_INI_ADD_NONJUNK_TO_ADDBOOK);

			for ( ; selSumItr != SumList.end(); ++selSumItr )
			{
				nItem = *selItr;
				
				pMR = (CMsgResult *) m_ResultsList.GetItemData(nItem);
				ASSERT(pMR);
				if (!pMR)
					continue;

				pSum = *selSumItr;

				if ( selItr == SelIdxList.end() )
				{
					//	SelIdxList is supposed to be the same length as SumList
					ASSERT(0);
					break;
				}
				
				pSum = CJunkMail::DeclareJunk( pSum, (nID == ID_JUNK), bAddNotJunkedSendersToAB,
											   &oaABHashes, NULL/*&filt*/ );

				// If we have a new summary that was the result junking or not junking
				// then update the item data and display.
				pTocDoc = pSum->m_TheToc;
				if (pTocDoc)
				{
					// Update the data for the item in the results list to reflect its new
					// mailbox location.
					QCMailboxCommand	*pCommand = g_theMailboxDirector.FindByPathname((const char *)pTocDoc->GetMBFileName());
					if (pCommand)
					{
						CString				 strMailboxName = pCommand->GetName();
						CString				 strMailboxPath = pCommand->GetPathname();
						m_ResultsList.SetItemText(nItem, COLUMN_MAILBOX, strMailboxName);
						pMR->m_sMbxName = strMailboxPath;
						pMR->m_sPath = strMailboxPath;
						pMR->m_nMsgID = pSum->GetUniqueMessageId();
					}
				}

				ProgressAdd(1);

				if (bEsc = (PumpEsc(TICK_DELTA) == TRUE))
					break;

				++selItr;
			}

			filt.EndFiltering();

			//	SelIdxList is supposed to be the same length as SumList
			VERIFY( bEsc || (selItr == SelIdxList.end()) );

			CloseProgress();
		}

		//	Warn the user that you can't trash an item already in the Trash mailbox
		if (iNumRemoved > 0)
		{
			if (nID == ID_JUNK)
			{
				ErrorDialog(IDS_SEARCH_RESULTS_JUNK_ERROR);
			}
			else
			{
				ErrorDialog(IDS_SEARCH_RESULTS_NOT_JUNK_ERROR);
			}
		}
	}
	return TRUE;
}


// --------------------------------------------------------------------------

BOOL CSearchView::OnDynamicCommand(UINT uID)
{
	//	This code is as similar as possible to CTocView::OnDynamicCommand.
	QCCommandObject*	pCommand;
	COMMAND_ACTION_TYPE	theAction;	
	
	if( !g_theCommandStack.GetCommand( ( WORD ) uID, &pCommand, &theAction ) )
	{
		return FALSE;
	}

	if( ( pCommand == NULL ) || !theAction )
	{
		return FALSE;
	}

	CMsgResult *	pMR = NULL;
	CTocDoc *		pTocDoc = NULL;
	CSummary *		pSum = NULL;

	if( theAction == CA_TRANSFER_NEW )
	{
		ASSERT_KINDOF( QCMailboxCommand, pCommand );
		pCommand = g_theMailboxDirector.CreateTargetMailbox( ( QCMailboxCommand* ) pCommand, TRUE );
		if ( NULL == pCommand )
			return TRUE;		// user didn't want to transfer after all

		ASSERT_KINDOF( QCMailboxCommand, pCommand );

		ASSERT( ( ( QCMailboxCommand* ) pCommand)->GetType() == MBT_REGULAR ||
				( ( QCMailboxCommand* ) pCommand)->GetType() == MBT_IMAP_MAILBOX );

		theAction = CA_TRANSFER_TO;
	}

	if ( pCommand->IsKindOf( RUNTIME_CLASS( QCMailboxCommand ) ) )
	{
		if ( theAction == CA_TRANSFER_TO )
		{
			//	Mechanism of transfering differs from CTocView::OnDynamicCommand
			CSummary *		pSumNew = NULL;
			CString			strMailboxName = ((QCMailboxCommand*)pCommand)->GetName();
			CString			strMailboxPath = ((QCMailboxCommand*)pCommand)->GetPathname();
			
			if (!strMailboxName.IsEmpty() && !strMailboxPath.IsEmpty())
			{
				CTocDoc *			pDestTocDoc = ::GetToc(strMailboxPath, strMailboxName, FALSE, FALSE);
				std::list<int>		SelIdxList;

				m_ResultsList.GetCurSel(&SelIdxList);

				if ( pDestTocDoc && (SelIdxList.size() > 0) )
				{
					CString			strLastFromTOC;
					CWaitCursor		wait_cursor;
					int				nItem;

					Progress( 0, CRString(IDS_SEARCH_PROGRESS_TRANSFERING), SelIdxList.size() ); // "Transfering..."
			
					for ( std::list<int>::iterator selItr = SelIdxList.begin();
						  selItr != SelIdxList.end();
						  ++selItr )
					{
						nItem = *selItr;
						
						pMR = (CMsgResult *) m_ResultsList.GetItemData(nItem);
						ASSERT(pMR);
						if (!pMR)
							continue;

						//	Check to see if we already found the associated destination TOC
						if ( !pTocDoc || (pMR->m_sPath != strLastFromTOC) )
						{
							pTocDoc = ::GetToc(pMR->m_sPath, NULL, FALSE, FALSE);
							strLastFromTOC = pMR->m_sPath;
						}
						ASSERT(pTocDoc);
						if (!pTocDoc)
							continue;

						CSumList &		listSums = pTocDoc->GetSumList();

						pSum = listSums.GetByMessageId(pMR->m_nMsgID);
						ASSERT(pSum);
						if (!pSum)
							continue;
						
						if (theAction == CA_TRANSFER_TO && ((QCMailboxCommand*)pCommand)->m_theType == MBT_TRASH)
							pSumNew = pTocDoc->Xfer(pDestTocDoc, pSum);
						else
							pSumNew = pTocDoc->Xfer(pDestTocDoc, pSum, TRUE, ShiftDown());

						//	If we have a new summary that was the result of a transfer
						//	(not a copy - i.e. not with ShiftDown) then update the item data and display.
						if ( pSumNew && !ShiftDown() )
						{
							// Update the data for the item in the results list to reflect its new
							// mailbox location.
							m_ResultsList.SetItemText(nItem, COLUMN_MAILBOX, strMailboxName);
							pMR->m_sMbxName = strMailboxPath;
							pMR->m_sPath = strMailboxPath;
							pMR->m_nMsgID = pSumNew->GetUniqueMessageId();
						}

						if (PumpEsc(TICK_DELTA) == TRUE)
							break;

						ProgressAdd(1);
					}

					CloseProgress();
				}
			}

			return TRUE;
		}
	}

	if( ( theAction == CA_REPLY_WITH ) ||
		( theAction == CA_REPLY_TO_ALL_WITH ) ||
		( theAction == CA_FORWARD_TO ) ||
		( theAction == CA_REDIRECT_TO ) )
	{
		CCursor			theCursor;
		
		StartGroup();

		MessageCascadeSpot = 0;
	
		::AsyncEscapePressed(TRUE);				// reset Escape key logic

		int					nItem = -1;

		while ( (nItem = m_ResultsList.GetNextItem(nItem, LVNI_SELECTED)) != -1 )
		{
			if (::AsyncEscapePressed())
			{
				break;
			}
			
			//	Have to get the pSum differently than CTocView::OnDynamicCommand
			pMR = (CMsgResult *) m_ResultsList.GetItemData(nItem);
			ASSERT(pMR);
			if (!pMR)
				continue;

			pTocDoc = ::GetToc(pMR->m_sPath, NULL, FALSE, FALSE);
			ASSERT(pTocDoc);
			if (!pTocDoc)
				continue;

			CSumList &		listSums = pTocDoc->GetSumList();

			pSum = listSums.GetByMessageId(pMR->m_nMsgID);
			ASSERT(pSum);
			if (!pSum)
				continue;
			
			pCommand->Execute(theAction, pSum);
		}
		
		MessageCascadeSpot = -1;

		EndGroup();
		return TRUE;
	}

	if (theAction == CA_SAVED_SEARCH)
	{
		SearchManager::SavedSearchCommand *		pSavedSearchCommand =
				reinterpret_cast<SearchManager::SavedSearchCommand *>(pCommand);

		LoadCriteriaFromXML( pSavedSearchCommand->GetFileName() );

		return TRUE;
	}

	return FALSE;
}

// --------------------------------------------------------------------------

bool CSearchView::VerifyDelete(bool bUnread, bool bQueued, bool bSendable)
{
	const bool bWarnDeleteSearchResult = (GetIniShort(IDS_INI_WARN_DELETE_SEARCHRESULT) != 0);
	const bool bWarnDeleteUnread = (GetIniShort(IDS_INI_WARN_DELETE_UNREAD) != 0);
	const bool bWarnDeleteQueued = (GetIniShort(IDS_INI_WARN_DELETE_QUEUED) != 0);
	const bool bWarnDeleteUnsent = (GetIniShort(IDS_INI_WARN_DELETE_UNSENT) != 0);

	if (bWarnDeleteSearchResult)
	{
		// Deleting messages from the results list will move the messages to the trash. Continue?
		if (WarnDialog(IDS_INI_WARN_DELETE_SEARCHRESULT, IDS_WARN_DELETE_SEARCHRESULT) != IDOK)
			return (false);
	}
	else if (bUnread && bWarnDeleteUnread)
	{
		if (WarnDialog(IDS_INI_WARN_DELETE_UNREAD, IDS_WARN_DELETE_UNREAD) != IDOK)
			return (false);
	}
	else if (bQueued && bWarnDeleteQueued)
	{
		if (WarnDialog(IDS_INI_WARN_DELETE_QUEUED, IDS_WARN_DELETE_QUEUED) != IDOK)
			return (false);
	}
	else if (bSendable && bWarnDeleteUnsent)
	{
		if (WarnDialog(IDS_INI_WARN_DELETE_UNSENT, IDS_WARN_DELETE_UNSENT) != IDOK)
			return (false);
	}

	return (true);
}

// --------------------------------------------------------------------------

bool CSearchView::VerifyDelete(CSummary *pSum)
{
	const bool bUnread = (pSum->m_State == MS_UNREAD);
	const bool bQueued = (pSum->IsQueued() == TRUE);
	const bool bSendable = (pSum->IsSendable() == TRUE);

	return (VerifyDelete(bUnread, bQueued, bSendable));
}

// --------------------------------------------------------------------------

// VerifyDelete [PROTECTED]
//
// Return true if delete should happen; false cancels delete action

bool CSearchView::VerifyDelete(/* const */ std::list<CSummary *> &SumList)
{
	bool bUnread = false, bQueued = false, bSendable = false;

	for (std::list<CSummary *>::iterator itr = SumList.begin(); itr != SumList.end(); ++itr)
	{
		if (!bUnread)
			bUnread = ((*itr)->m_State == MS_UNREAD);

		if (!bQueued)
			bQueued = (((*itr)->IsQueued()) == TRUE);

		if (!bSendable)
			bSendable = (((*itr)->IsSendable()) == TRUE);

		if (bUnread && bQueued && bSendable)
			break; // Optimization -- stop looking if everything is true
	}

	return (VerifyDelete(bUnread, bQueued, bSendable));
}

// --------------------------------------------------------------------------

bool CSearchView::RemoveFromResultsList(int nIdx)
{
	return (m_ResultsList.DeleteItem(nIdx) == TRUE);
}

// --------------------------------------------------------------------------

bool CSearchView::RemoveFromResultsList(/* const */ std::list<int> &IdxList)
{
	Progress(0, CRString(IDS_SEARCH_PROGRESS_EMPTYINGLIST), IdxList.size() ); // "Emptying list..."

	int nOffset = 0;
	bool bEsc = false;

	for (std::list<int>::iterator itr = IdxList.begin(); itr != IdxList.end(); ++itr)
	{
		if (!RemoveFromResultsList( (*itr) - (nOffset++) ))
		{
			bEsc = true;
			break;
		}

		ProgressAdd(1);

		if (bEsc = (PumpEsc(TICK_DELTA) == TRUE))
			break;
	}

	CloseProgress();

	return (!bEsc);
}

// --------------------------------------------------------------------------

bool CSearchView::DoDelete(CSummary * pSum, int iMsgIdx)
{
	ASSERT(pSum);

	CTocDoc *pTocDoc = pSum->m_TheToc;
	ASSERT(pTocDoc);
	if (!pTocDoc)
		return false;

	const int MBType = pTocDoc->m_Type;
	ASSERT(MBType != MBT_TRASH);
	if (MBType == MBT_TRASH)
		return (false); // We do not allow search results to delete from TRASH

#ifdef IMAP4
	// If this is an IMAP mailbox, just flag messages and get out.
	if (pTocDoc->IsImapToc())
	{
		// Pass this to the IMAP command object.
		//
		QCMailboxCommand* pImapFolder = g_theMailboxDirector.FindByPathname( (const char *) pTocDoc->GetMBFileName() );
		if( pImapFolder != NULL )
			pImapFolder->Execute( CA_DELETE_MESSAGE, pSum);
		else
		{
			ASSERT(0);
		}

		return (true);
	}
#endif // IMAP4

	CTocDoc *TrashToc = GetTrashToc();
	if (TrashToc)
	{
		//	Contrary to past comments here, we should be calling Xfer directly
		//	because we do NOT want the the command director logic for CA_TRANSFER_TO.
		//	The command director would notify the TOC window containing this message
		//	that it is okay to invoke the "auto-mark-as-read" logic.
		//	That makes sense for CTocView, but NOT here.
		//	Another reason to call Xfer directly is that we need the new CSummary
		//	so that we can update our information appropriately.
		CSummary *		pSumNew = pTocDoc->Xfer(TrashToc, pSum);

		// Update the data for the item in the results list to reflect its new
		// mailbox location.
		if (iMsgIdx != -1)
		{
			CMsgResult *	pMR = (CMsgResult *) m_ResultsList.GetItemData(iMsgIdx);
			VERIFY(pMR);
			
			if (pMR)
			{
				m_ResultsList.SetItemText( iMsgIdx, COLUMN_MAILBOX, TrashToc->Name() );
				pMR->m_sMbxName = TrashToc->GetMBFileName();
				pMR->m_sPath = TrashToc->GetMBFileName();
				pMR->m_nMsgID = pSumNew->GetUniqueMessageId();
			}
		}
	}

	return (true);
}

// --------------------------------------------------------------------------

LONG CSearchView::OnMsgListDeleteKey(WPARAM /*wParam*/, LPARAM lParam)
{
	CListCtrlEx *pList = (CListCtrlEx *) lParam;

	ASSERT(pList == (&m_ResultsList));

	if (pList == (&m_ResultsList))
		OnDeleteMessages();

	return (0); // Ignored
}




// --------------------------------------------------------------------------

LONG CSearchView::OnMsgListRightClick(WPARAM wParam, LPARAM lParam)
{
	CPoint point((DWORD)lParam); // screen-based point of click

	CPoint clientPt = point;
	m_ResultsList.ScreenToClient(&clientPt);
	int idx = m_ResultsList.HitTest(clientPt);

	// Ensure we right-clicked on a valid item. Although the ListCtrl should
	// never tell us to handle an invalid right-click, this is a safety check.

	ASSERT(idx != (-1));
	if (idx == (-1))
		return (0);

	// Popup menu stuff
	CMenu menu;
	menu.CreatePopupMenu();

	menu.AppendMenu(MF_STRING, ID_OPEN_MSG, LPCSTR(CRString(IDS_SEARCH_RESULTS_POPUPMENU_OPENMSG)));
	menu.AppendMenu(MF_STRING, ID_OPEN_TOC, LPCSTR(CRString(IDS_SEARCH_RESULTS_POPUPMENU_OPENMBX)));
	menu.AppendMenu(MF_SEPARATOR);

	menu.InsertMenu(3, MF_BYPOSITION | MF_POPUP,
					(UINT) CMainFrame::QCGetMainFrame()->GetTransferMenu()->GetSafeHmenu(),
					CRString(IDS_TRANSFER_NAME));

	menu.AppendMenu(MF_STRING, ID_MESSAGE_REPLY, LPCSTR(CRString(IDS_SEARCH_RESULTS_POPUPMENU_REPLYMSG)));
	menu.AppendMenu(MF_STRING, ID_MESSAGE_REPLY_ALL, LPCSTR(CRString(IDS_SEARCH_RESULTS_POPUPMENU_REPLYALLMSG)));
	menu.AppendMenu(MF_STRING, ID_MESSAGE_FORWARD, LPCSTR(CRString(IDS_SEARCH_RESULTS_POPUPMENU_FORWARDMSG)));
	menu.AppendMenu(MF_STRING, ID_MESSAGE_REDIRECT, LPCSTR(CRString(IDS_SEARCH_RESULTS_POPUPMENU_REDIRECTMSG)));
	menu.AppendMenu(MF_SEPARATOR);
	menu.AppendMenu(MF_STRING, ID_MESSAGE_DELETE, LPCSTR(CRString(IDS_SEARCH_RESULTS_POPUPMENU_DELMSG)));
	menu.AppendMenu(MF_SEPARATOR);
	menu.AppendMenu(MF_STRING, ID_JUNK, LPCSTR(CRString(IDS_SEARCH_RESULTS_POPUPMENU_JUNK)));
	menu.AppendMenu(MF_STRING, ID_NOT_JUNK, LPCSTR(CRString(IDS_SEARCH_RESULTS_POPUPMENU_NOT_JUNK)));

	CContextMenu::MatchCoordinatesToWindow(GetSafeHwnd(), point);
	CContextMenu(&menu, point.x, point.y);

	VERIFY(menu.Detach());

	menu.DestroyMenu();

	return (0);
}

// --------------------------------------------------------------------------

LONG CSearchView::OnMsgListDblClk(WPARAM /*wParam*/, LPARAM lParam)
{
	const BOOL bControlDown = ControlDown();

	CPoint point((DWORD)lParam); // screen-based point of click

	CPoint clientPt = point;
	m_ResultsList.ScreenToClient(&clientPt);
	int idx = m_ResultsList.HitTest(clientPt);

	ASSERT(idx != (-1));
	if (idx == (-1))
		return (0);

	CMsgResult *pMR = (CMsgResult *) m_ResultsList.GetItemData(idx);

	if (bControlDown)
		VERIFY( OpenTOC(pMR, NULL) );
	else
		VERIFY(OpenMsg(pMR));

	return (0); // Ignored
}

// --------------------------------------------------------------------------

LONG CSearchView::OnMsgListReturnKey(WPARAM /*wParam*/, LPARAM lParam)
{
	const BOOL bControlDown = ControlDown();

	const CListCtrlEx *pList = (const CListCtrlEx *) lParam;

	ASSERT(pList == (&m_ResultsList));

	if (pList == (&m_ResultsList))
	{
		std::list<int>		SelList;
		if (!m_ResultsList.GetCurSel(&SelList))
			return (0);

		for (std::list<int>::iterator iter = SelList.begin(); iter != SelList.end(); iter++)
		{
			int nIdx = (*iter);
			CMsgResult *pMR = (CMsgResult *) m_ResultsList.GetItemData(nIdx);

			if (bControlDown)
				VERIFY( OpenTOC(pMR, NULL) );
			else
				VERIFY(OpenMsg(pMR));
		}
	}

	return (0); // Ignored
}

// --------------------------------------------------------------------------

void CSearchView::OnTabSelchange(NMHDR* pNMHDR, LRESULT* pResult) // TCN_SELCHANGE from IDC_SEARCHWND_RESULTS_TAB
{
	UpdateTabContents();
}

// --------------------------------------------------------------------------

void CSearchView::UpdateTabContents()
{
	int nCurTab = m_ResultsMbxTabCtrl.GetCurFocus();

	ASSERT( (TAB_RESULTS_IDX == nCurTab) | (TAB_MAILBOXES_IDX == nCurTab) );

	switch (nCurTab)
	{
		case (TAB_RESULTS_IDX): // Results Tab
		{
			m_ResultsList.ShowWindow(SW_SHOW);
			m_MBoxTree.ShowWindow(SW_HIDE);
		}
		break;

		case (TAB_MAILBOXES_IDX): // Mailboxes Tab
		{
			m_ResultsList.ShowWindow(SW_HIDE);
			m_MBoxTree.ShowWindow(SW_SHOW);
		}
		break;
	};
}

// --------------------------------------------------------------------------

void CSearchView::UpdateStatusText()
{
	CString txt = "";

	if (m_bShowFoundCount)
	{
		CString str;

		// First do num of matches
		if (0 == m_nFoundCount)
			str = LPCTSTR(CRString(IDS_SEARCH_STATUS_NOMATCHES)); // "No matches"
		else if (1 == m_nFoundCount)
			str = LPCTSTR(CRString(IDS_SEARCH_STATUS_ONEMATCH)); // "One match"
		else
			str.Format(CRString(IDS_SEARCH_STATUS_N_MATCHES_FMT), m_nFoundCount); // "%d matches"
		str += ' ';

		// Add to main text
		txt += str;

		// Do skipped count, if any
		if (m_nIMAPSkippedCount > 0)
		{
			
			str.Format(CRString(IDS_SEARCH_STATUS_SKIPPED_FMT), m_nIMAPSkippedCount); // "(%d skipped)"
			str += ' ';
			txt += str;
		}

		// Do mbx count
		str.Format(CRString(IDS_SEARCH_STATUS_MBXCOUNT_FMT), m_nCheckCount); // "in %u mailbox"

		if (m_nCheckCount > 1)
			str += CRString(IDS_SEARCH_STATUS_MBXCOUNT_PLURAL); // "es"

		txt += str;
	}
	else
	{
		if (0 == m_nCheckCount)
			txt = CRString(IDS_SEARCH_MBXSEL_NOMBX); // "No mailboxes selected"
		else if (1 == m_nCheckCount)
			txt = CRString(IDS_SEARCH_MBXSEL_ONEMBX); // "One mailbox selected"
		else
			txt.Format(CRString(IDS_SEARCH_MBXSEL_ONEMBX_N_MBX_FMT), m_nCheckCount); // "%u mailboxes selected"
	}

	m_ResultsStatic.SetWindowText(txt);
}

// --------------------------------------------------------------------------

void CSearchView::UpdateMbxText()
{
	m_nCheckCount = m_MBoxTree.GetCheckCount();
	m_bShowFoundCount = FALSE; // Lose found info when you change the mailbox checks

	UpdateStatusText();
}

// --------------------------------------------------------------------------

void CSearchView::UpdateResultsText()
{
	m_bShowFoundCount = TRUE;
	m_nFoundCount = m_ResultsList.GetItemCount();
	UpdateStatusText();
}

// --------------------------------------------------------------------------

LONG CSearchView::OnMsgTreeCheckChange(WPARAM wParam, LPARAM lParam)
{
	UpdateMbxText();
	UpdateSearchBtn();

	return (0); // ignored
}

// --------------------------------------------------------------------------

void CSearchView::UpdateAndOrBtns()
{
	m_AndRadioBtn.EnableWindow(m_CurCritCount > 1);
	m_OrRadioBtn.EnableWindow(m_CurCritCount > 1);
}

// --------------------------------------------------------------------------

void CSearchView::UpdateMoreLessBtn()
{
	m_LessBtn.EnableWindow(m_CurCritCount > 1);
	m_MoreBtn.EnableWindow(m_CurCritCount < MAX_CONTROLS_CRITERIA);
}

// --------------------------------------------------------------------------

void CSearchView::UpdateSearchBtn()
{
	bool bEnabled = false;
	SearchCriteria criteria;

	for (int idx = 0; (!bEnabled) && (idx < m_CurCritCount); idx++)
	{
		bEnabled = GetCriteria(idx, criteria);

		if (bEnabled)
		{
			CriteriaObject		curObjectType;

			criteria.GetCurObj(curObjectType);

			if (SearchCriteria::GetValueType(curObjectType) == CRITERIA_VALUE_TEXT_TYPE)
			{
				// Only enable button with text criteria when it contains
				// something other than just whitespace.
				CString		strFindText = criteria.GetValueText();

				strFindText.Trim();
				bEnabled = !strFindText.IsEmpty();
			}
		}
	}

	m_BeginBtn.EnableWindow(bEnabled ? TRUE : FALSE);

	//	If this is the first time, then initialize the search button
	//	if we're using the full feature set.
	if ( !m_bSearchButtonMenuInitialized && UsingFullFeatureSet() )
	{
		UpdateSearchBtnMenu();
		m_bSearchButtonMenuInitialized = true;
	}

	if ( ShouldUseIndexedSearch() )
	{
		m_PoweredByX1Static.ShowWindow(SW_SHOW);
		m_PoweredByX1Icon.ShowWindow(SW_SHOW);
	}
	else
	{
		m_PoweredByX1Static.ShowWindow(SW_HIDE);
		m_PoweredByX1Icon.ShowWindow(SW_HIDE);
	}
}


void CSearchView::UpdateSearchBtnMenu()
{
	//	Reset the menu
	m_BeginBtn.ResetMenu();
	
	if (m_bSearchButtonMenuInitialized)
	{
		//	Refresh the list of saved searches if we were already initialized
		//	(i.e. if we're being called from WriteCurrentSearchCriteria).
		SearchManager::Instance()->UpdateSavedSearchList();
	}

	//	Rebuild the Edit->Find->Find Messages Using menu
	CMainFrame *	pMainFrame = CMainFrame::QCGetMainFrame();
	ASSERT(pMainFrame);
	if (pMainFrame)
	{
		CDynamicSavedSearchMenu *	pSavedSearchMenu = pMainFrame->GetSavedSearchMenu();
		ASSERT(pSavedSearchMenu);
		if (pSavedSearchMenu)
			pSavedSearchMenu->RebuildMenu();
	}
	
	if ( !UsingFullFeatureSet() )
	{
		//	Only allow saved search feature with full feature set
		m_BeginBtn.SetIsSplit(false);
		return;
	}

	//	Right align the menu for the begin search button because the button is at
	//	the far right of the window. Unfortunately when left aligning TrackPopupMenu
	//	will decide to right align the menu with the *left* of the button if the menu
	//	wouldn't fit on the screen. Avoid this by right aligning the menu with the
	//	*right* of the button.
	m_BeginBtn.SetLeftAlignMenu(false);

	//	Get the list of saved searches. The list of saved searches is refreshed
	//	above if we were already initialized. It's first initialized inside
	//	GetSavedSearchesList as necessary.
	SearchManager::SavedSearchList &	listSavedSearches = SearchManager::Instance()->GetSavedSearchesList();

	//	If we're going to be adding saved searches, split the button so that the
	//	user can initiate search without loading a saved search.
	m_BeginBtn.SetIsSplit( !listSavedSearches.empty() );

	for ( SearchManager::SavedSearchList::iterator i = listSavedSearches.begin();
		  i != listSavedSearches.end();
		  i++ )
	{
		SearchManager::SavedSearchCommand *		pSavedSearchCommand = *i;

		if (pSavedSearchCommand)
		{
			m_BeginBtn.AddMenuItem( pSavedSearchCommand->GetCommandID(),
									pSavedSearchCommand->GetFileTitle(), 0 );
		}
	}
}

// --------------------------------------------------------------------------

void CSearchView::OnSelchangeCriteriaCategoryCombo1() // IDC_CRITERIA_CATEGORY_COMBO_1
{
	OnCategoryChange(0);
}

// --------------------------------------------------------------------------

void CSearchView::OnSelchangeCriteriaCategoryCombo2() // IDC_CRITERIA_CATEGORY_COMBO_2
{
	OnCategoryChange(1);
}

// --------------------------------------------------------------------------

void CSearchView::OnSelchangeCriteriaCategoryCombo3() // IDC_CRITERIA_CATEGORY_COMBO_3
{
	OnCategoryChange(2);
}

// --------------------------------------------------------------------------

void CSearchView::OnSelchangeCriteriaCategoryCombo4() // IDC_CRITERIA_CATEGORY_COMBO_4
{
	OnCategoryChange(3);
}

// --------------------------------------------------------------------------

void CSearchView::OnSelchangeCriteriaCategoryCombo5() // IDC_CRITERIA_CATEGORY_COMBO_5
{
	OnCategoryChange(4);
}

// --------------------------------------------------------------------------

void CSearchView::OnChangeTextEdit1() // IDC_CRITERIA_TEXT_EDIT_1
{
	UpdateSearchBtn();
}

// --------------------------------------------------------------------------

void CSearchView::OnChangeTextEdit2() // IDC_CRITERIA_TEXT_EDIT_2
{
	UpdateSearchBtn();
}

// --------------------------------------------------------------------------

void CSearchView::OnChangeTextEdit3() // IDC_CRITERIA_TEXT_EDIT_3
{
	UpdateSearchBtn();
}

// --------------------------------------------------------------------------

void CSearchView::OnChangeTextEdit4() // IDC_CRITERIA_TEXT_EDIT_4
{
	UpdateSearchBtn();
}

// --------------------------------------------------------------------------

void CSearchView::OnChangeTextEdit5() // IDC_CRITERIA_TEXT_EDIT_5
{
	UpdateSearchBtn();
}

//
// PumpEsc
//
// Returns TRUE if ESC has been pressed; FALSE otherwise
//

BOOL CSearchView::PumpEsc(DWORD nDeltaTick)
{
	static DWORD nLastTick = 0;

	//
	// Never pump more than once every nDeltaTick ticks
	//

	if ((GetTickCount() - nLastTick) < nDeltaTick)
		return (FALSE);

	//
	// Retrieve and dispatch any waiting messages, looking for ESC
	//

	BOOL bESC = ::EscapePressed();

	nLastTick = GetTickCount();

	return bESC;
}
// --------------------------------------------------------------------------

//
// FIND TEXT
//

void CSearchView::OnUpdateEditFindFindText(CCmdUI* pCmdUI) // Find (Ctrl-F)
{
	pCmdUI->Enable(TRUE);
}

void CSearchView::OnUpdateEditFindFindTextAgain(CCmdUI* pCmdUI) // Find Again (F3)
{
	QCFindMgr *pFindMgr = QCFindMgr::GetFindMgr();
	ASSERT(pFindMgr);

	if ((pFindMgr) && (pFindMgr->CanFindAgain()))
		pCmdUI->Enable(TRUE);
	else
		pCmdUI->Enable(FALSE);
}

LONG CSearchView::OnFindReplace(WPARAM wParam, LPARAM lParam) // WM_FINDREPLACE
{
	// Pass the message to either the resutls list or the mbx tree control
	// whichever is currently shown.

	CWnd *pWnd = NULL;
	int nCurTab = m_ResultsMbxTabCtrl.GetCurFocus();

	ASSERT( (TAB_RESULTS_IDX == nCurTab) | (TAB_MAILBOXES_IDX == nCurTab) );

	switch (nCurTab)
	{
		case (TAB_RESULTS_IDX): // Results Tab
		{
			pWnd = (&m_ResultsList);
		}
		break;

		case (TAB_MAILBOXES_IDX): // Mailboxes Tab
		{
			pWnd = (&m_MBoxTree);
		}
		break;
	};

	ASSERT(pWnd);
	if (!pWnd)
		return (0);

	return (pWnd->SendMessage(WM_FINDREPLACE, wParam, lParam));
}

// --------------------------------------------------------------------------

// 
// ParseStringList
//
// Given a string which contains a list of items, this func will parse the list
// and return a pointer to each item; optionally you can replace each delim char
// with another char. 
//
//

int ParseStringList(LPSTR pStr, char cDelimChar, std::vector<LPCSTR> &vParseList, char cReplaceChar)
{
	const bool bReplace = (cDelimChar != cReplaceChar);
	LPSTR pLastStart, pCurPos;

	vParseList.clear();

	pLastStart = pCurPos = pStr;

	while (*pCurPos)
	{
		if ((*pCurPos) != cDelimChar)
		{
			++pCurPos;
		}
		else
		{
			// We're at the separator character
			if (bReplace)
				(*pCurPos) = cReplaceChar;

			vParseList.push_back(pLastStart);
			pLastStart = (++pCurPos);
		}
	}

	vParseList.push_back(pLastStart);

	return (vParseList.size());
}

// --------------------------------------------------------------------------

bool StrIsDigit(LPCTSTR pStr)
{
	while (*pStr)
		if (!isdigit((unsigned char)*pStr))
			return (false);

	return (true);
}


void CSearchView::ReloadCriteria()
{
	HideAllControls();
	
	for (int idx = 0; idx < MAX_CONTROLS_CRITERIA; idx++)
		m_bCritInitd[idx] = false;

	LoadCriteria();
}


// --------------------------------------------------------------------------

// IDS_INI_SEARCH_CRITERIA_COUNT

//
// LoadCriteria [PROTECTED]
//

bool CSearchView::LoadCriteria()
{
	// Force the count to be between 1 and 5
	const bool bFullFeatured = UsingFullFeatureSet();
	const int nCount = bFullFeatured? __min(__max(((int) GetIniShort(IDS_INI_SEARCH_CRITERIA_COUNT)), 1), 5) : 1;
	ASSERT(nCount >= 1);
	ASSERT(nCount <= 5);

	while (m_CurCritCount > nCount)
		RemoveCriteria();

	while (m_CurCritCount < nCount)
		AddNewCriteria();

	ASSERT(m_CurCritCount == nCount);

	const CString sDefaultStr
		= CRString(IDS_SEARCH_CATEGORYSTR_ANYWHERE)
			+ m_SaveStrSeperator
			+ CRString(IDS_SEARCH_COMPARESTR_CONTAINS)
			+ m_SaveStrSeperator; // "Anywhere;Contains;"

	CString str;
	GetIniString(IDS_INI_SEARCH_CRITERIA_1, str);
	InitializeCriteriaCtrls(0);
	if (!SetCriteriaIniString(0, str))
		SetCriteriaIniString(0, sDefaultStr);


	// Moved this to InitializeCriteriaControls .. TBD : Delete the commented code once confirmed that this change has no ripple effects

	/*if (bFullFeatured)
	{
		GetIniString(IDS_INI_SEARCH_CRITERIA_2, str);
		InitializeCriteriaCtrls(1);
		if (!SetCriteriaIniString(1, str))
			SetCriteriaIniString(0, sDefaultStr);

		GetIniString(IDS_INI_SEARCH_CRITERIA_3, str);
		InitializeCriteriaCtrls(2);
		if (!SetCriteriaIniString(2, str))
			SetCriteriaIniString(0, sDefaultStr);

		GetIniString(IDS_INI_SEARCH_CRITERIA_4, str);
		InitializeCriteriaCtrls(3);
		if (!SetCriteriaIniString(3, str))
			SetCriteriaIniString(0, sDefaultStr);

		GetIniString(IDS_INI_SEARCH_CRITERIA_5, str);
		InitializeCriteriaCtrls(4);
		if (!SetCriteriaIniString(4, str))
			SetCriteriaIniString(0, sDefaultStr);
	}*/

	if (bFullFeatured)
	{
		m_AndRadioBtn.ShowWindow(SW_SHOW);
		m_OrRadioBtn.ShowWindow(SW_SHOW);
		m_MoreBtn.ShowWindow(SW_SHOW);
		m_LessBtn.ShowWindow(SW_SHOW);
	}
	else
	{
		m_AndRadioBtn.ShowWindow(SW_HIDE);
		m_OrRadioBtn.ShowWindow(SW_HIDE);
		m_MoreBtn.ShowWindow(SW_HIDE);
		m_LessBtn.ShowWindow(SW_HIDE);
	}

	return (true);
}

// --------------------------------------------------------------------------

//
// SaveCriteria [PROTECTED]
//

bool CSearchView::SaveCriteria()
{
	VERIFY(SetIniShort(IDS_INI_SEARCH_CRITERIA_COUNT, (short) m_CurCritCount));

	CString str;
	if (GetCriteriaIniString(0, str))
		SetIniString(IDS_INI_SEARCH_CRITERIA_1, str);

	if (GetCriteriaIniString(1, str))
		SetIniString(IDS_INI_SEARCH_CRITERIA_2, str);

	if (GetCriteriaIniString(2, str))
		SetIniString(IDS_INI_SEARCH_CRITERIA_3, str);

	if (GetCriteriaIniString(3, str))
		SetIniString(IDS_INI_SEARCH_CRITERIA_4, str);

	if (GetCriteriaIniString(4, str))
		SetIniString(IDS_INI_SEARCH_CRITERIA_5, str);

	return (true);
}

void CSearchView::GetSuggestedFileName(CString & out_strSuggestedFileName)
{
	//	Get the first criteria INI string because it's very close to a
	//	nice text version of the search.
	GetCriteriaIniString(0, out_strSuggestedFileName);

	//	Replace the first two semi-colons, which separate the category, verb,
	//	and value, with spaces.
	for (int i = 0; i < 2; i++)
	{
		int		nIndex = out_strSuggestedFileName.Find(';');
		if (nIndex >= 0)
			out_strSuggestedFileName.SetAt(nIndex, ' ');
	}

	//	Replace slashes with dashes to make the file name legal, while still
	//	keeping dates readable.
	out_strSuggestedFileName.Replace('/', '-');

	//	Remove characters that aren't allowed in Windows file names
	static const char *		kIllegalCharacters = "/\\:*?\"<>|";

	for ( const char * pIllegalChar = kIllegalCharacters; *pIllegalChar; pIllegalChar++ )
	{
		out_strSuggestedFileName.Remove(*pIllegalChar);
	}
}

void CSearchView::LoadCriteriaFromXML(const char * in_pszPathName)
{
	//	Check to see if our file exists
	if ( !::FileExistsMT(in_pszPathName) )
	{
		ASSERT(!"Search saved criteria file is missing");
		
		//	Log the error
		CString		szLogEntry;

		szLogEntry.Format( "Search saved criteria file %s does not exist.",
							in_pszPathName );
		PutDebugLog(DEBUG_MASK_SEARCH, szLogEntry);
				
		return;
	}

	//	Open our file
	int		hFile = open(in_pszPathName, _O_RDONLY | _O_TEXT);
	if (hFile == -1)
	{
		ASSERT(!"Could not open saved criteria file");
		
		//	Log the error
		CString		szLogEntry;

		szLogEntry.Format( "Search saved criteria file %s could not be opened.",
							in_pszPathName );
		PutDebugLog(DEBUG_MASK_SEARCH, szLogEntry);
		
		return;
	}

	bool	bLoadGood = false;

	//	Allocated a buffer big enough for the entire file contents (only a few K)
	long					nLength = lseek(hFile, 0, SEEK_END);
	std::auto_ptr<char>		szFileBuf( DEBUG_NEW_NOTHROW char [nLength+1] );

	if ( szFileBuf.get() )
	{		
		//	Load the entire file contents into our buffer
		lseek(hFile, 0, SEEK_SET);
		nLength = read(hFile, szFileBuf.get(), nLength);
		szFileBuf.get()[nLength] = 0;

		//	Parse the entire file in one fell swoop
		XMLParser		xmlParser;
		bLoadGood = (xmlParser.Parse(szFileBuf.get(), nLength, true) == 0);

		if (bLoadGood)
		{
			long		nCount = xmlParser.GetSearchCriteriaCount();
			ASSERT(nCount >= 1);
			ASSERT(nCount <= MAX_CONTROLS_CRITERIA);

			//	Use wait cursor in case this takes a moment or two
			CCursor		waitCursor;

			//	Stop drawing until we're done loading the search criteria
			SetRedraw(FALSE);

			while (m_CurCritCount > nCount)
				RemoveCriteria();

			while (m_CurCritCount < nCount)
				AddNewCriteria();

			for (short i = 0; i < nCount; i++)
				SetCriteriaIniString( i, xmlParser.GetSearchCriterion(i) );

			//	Allow drawing now that we're done loading the search criteria
			SetRedraw(TRUE);

			//	Invalidate and update the window
			Invalidate();
			UpdateWindow();

			//	Initiate the search with the newly loaded criteria
			OnOk();
		}
	}

	close(hFile);
}

void CSearchView::WriteCurrentSearchCriteria(const char * in_pszPathName)
{
	JJFile		file;
	CString		szDirectory = in_pszPathName;
	int			nLastBackslash = szDirectory.ReverseFind('\\');

	if (nLastBackslash == -1)
	{
		ASSERT(!"Failed to find saved search directory");
		
		CString		szLogEntry;

		szLogEntry.Format("Failed to find saved search directory: %s", in_pszPathName);

		PutDebugLog(DEBUG_MASK_SEARCH, szLogEntry);

		return;
	}

	szDirectory = szDirectory.Left(nLastBackslash + 1);

	CString		szTempFilePathName;

	HRESULT		hr = GetTempFileName( szDirectory, "ISM", 0, szTempFilePathName.GetBuffer(MAX_PATH + 1) );
	if ( SUCCEEDED(hr) )
	{
		szTempFilePathName.ReleaseBuffer();
	}
	else
	{
		szTempFilePathName = szDirectory;
		szTempFilePathName += "ISM-temp.tmp";
	}

	hr = file.Open(szTempFilePathName, O_CREAT | O_TRUNC | O_WRONLY);

	if ( SUCCEEDED(hr) )
	{
		//	Open succeeded - write out the XML
		XMLWriter		xmlWriter(file);

		//	Start <SavedSearchInfo>
		xmlWriter.WriteTagStart(XMLParser::kXMLBaseContainer, true);

		//	Write <SearchCriteriaCount>
		xmlWriter.WriteTaggedData(XMLParser::kKeySearchCriteriaCount, true, "%d", m_CurCritCount);

		//	Start <SearchCriteria>
		xmlWriter.WriteTagStart(XMLParser::kKeySearchCriteria, true);

		CString			szCriteriaString;
		for (int i = 0; i < MAX_CONTROLS_CRITERIA; i++)
		{
			//	Write <SearchCriterion>
			if ( GetCriteriaIniString(i, szCriteriaString) )
				xmlWriter.WriteTaggedData(XMLParser::kKeySearchCriterion, true, "%s", szCriteriaString);
		}

		//	Close </SearchCriteria>
		xmlWriter.WriteTagEnd(XMLParser::kKeySearchCriteria, true);

		//	Close </SavedSearchInfo>
		xmlWriter.WriteTagEnd(XMLParser::kXMLBaseContainer, true);

		//	Close the file
		hr = file.Close();
		ASSERT( SUCCEEDED(hr) );

		//	Rename the temp file to our final name
		hr = file.Rename(in_pszPathName);
		ASSERT( SUCCEEDED(hr) );

		//	If renaming failed, clean up the temp file
		if ( FAILED(hr) )
			FileRemoveMT(szTempFilePathName);
	}
	else
	{
		ASSERT(!"Failed to open indexed search temp file");
		
		CString		szLogEntry;

		szLogEntry.Format("Failed to open saved search temp file: %s", szTempFilePathName);

		PutDebugLog(DEBUG_MASK_SEARCH, szLogEntry);
		FileRemoveMT(szTempFilePathName);
	}

	//	Update the search button menu so that we list the newly saved search
	UpdateSearchBtnMenu();
}

bool CSearchView::GetCriteriaIniString(int nIdx, CString &str)
{
	str.Empty();

	if (!m_bCritInitd[nIdx])
	{
		str = CRString(IDS_SEARCH_CATEGORYSTR_ANYWHERE)
			+ m_SaveStrSeperator
			+ CRString(IDS_SEARCH_COMPARESTR_CONTAINS)
			+ m_SaveStrSeperator; // "Anywhere;Contains;"

		return (true);
	}

	ASSERT(m_bCritInitd[nIdx]);

	CString tmpStr;

	m_CategoryCombo[nIdx].GetWindowText(tmpStr);

	ASSERT(!tmpStr.IsEmpty());

	str += tmpStr;
	str += m_SaveStrSeperator;

	tmpStr.Empty();
	switch (m_CritState[nIdx].m_CurVerbType)
	{
		case CRITERIA_VERB_TEXT_COMPARE_TYPE:
		{
			m_TextCompareCombo[nIdx].GetWindowText(tmpStr);
		}
		break;

		case CRITERIA_VERB_EQUAL_COMPARE_TYPE:
		{
			m_EqualCompareCombo[nIdx].GetWindowText(tmpStr);
		}
		break;

		case CRITERIA_VERB_NUM_COMPARE_TYPE:
		{
			m_NumCompareCombo[nIdx].GetWindowText(tmpStr);
		}
		break;

		case CRITERIA_VERB_DATE_COMPARE_TYPE:
		{
			m_DateCompareCombo[nIdx].GetWindowText(tmpStr);
		}
		break;

		default: ASSERT(0); break;
	}

	ASSERT(!tmpStr.IsEmpty());

	str += tmpStr;
	str += m_SaveStrSeperator;

	tmpStr.Empty();
	switch (m_CritState[nIdx].m_CurValueType)
	{
		case CRITERIA_VALUE_TEXT_TYPE:
		{
			m_TextEdit[nIdx].GetWindowText(tmpStr);
		}
		break;

		case CRITERIA_VALUE_AGE_TYPE:
		{
			m_NumEdit[nIdx].GetWindowText(tmpStr);
		}
		break;

		case CRITERIA_VALUE_ATTACHCOUNT_TYPE:
		{
			m_NumEdit[nIdx].GetWindowText(tmpStr);
		}
		break;

		case CRITERIA_VALUE_SIZE_TYPE:
		{
			m_NumEdit[nIdx].GetWindowText(tmpStr);
		}
		break;

		case CRITERIA_VALUE_JUNKSCORE_TYPE:
		{
			m_NumEdit[nIdx].GetWindowText(tmpStr);
		}
		break;

		case CRITERIA_VALUE_STATUS_TYPE:
		{
			m_StatusCombo[nIdx].GetWindowText(tmpStr);
		}
		break;

		case CRITERIA_VALUE_LABEL_TYPE:
		{
			m_LabelCombo[nIdx].GetWindowText(tmpStr);
		}
		break;

		case CRITERIA_VALUE_PERSONA_TYPE:
		{
			m_PersonaCombo[nIdx].GetWindowText(tmpStr);
		}
		break;

		case CRITERIA_VALUE_PRIORITY_TYPE:
		{
			m_PriorityCombo[nIdx].GetWindowText(tmpStr);
		}
		break;

		case CRITERIA_VALUE_DATE_TYPE:
		{
			COleDateTime TmpDate = m_DateTimeCtrl[nIdx].GetDateTime();
			tmpStr = TmpDate.Format(CRString(IDS_SEARCH_SAVEDATE_FMT)); // "%m/%d/%Y"
		}
		break;

		default: ASSERT(0); break;
	}

	str += tmpStr;

	return (true);
}

bool CSearchView::SetCriteriaIniString(int nIdx, LPCSTR pIniStr)
{
	ASSERT(m_bCritInitd[nIdx]);
	if (!m_bCritInitd[nIdx])
		return (false);

	CString CategoryStr, VerbStr, ValueStr;

	{
		LPSTR str = DEBUG_NEW char[strlen(pIniStr) + 1];

		strcpy(str, pIniStr);

		std::vector<LPCSTR> vParseList;

		const int nCount = ParseStringList(str, m_SaveStrSeperator, vParseList, '\0');

		ASSERT(3 == nCount);

		if (3 == nCount)
		{
			CategoryStr = vParseList[0];
			VerbStr = vParseList[1];
			ValueStr = vParseList[2];
		}

		delete[] str;
	}

	ASSERT(!CategoryStr.IsEmpty());
	ASSERT(!VerbStr.IsEmpty());

	if ((CategoryStr.IsEmpty()) || (VerbStr.IsEmpty()))
		return (false);

	bool bSuccess = (m_CategoryCombo[nIdx].SelectString(-1, CategoryStr) != CB_ERR);
	ASSERT(bSuccess);

	if (!bSuccess)
		return (false);

	OnCategoryChange(nIdx);

	switch (m_CritState[nIdx].m_CurVerbType)
	{
		case CRITERIA_VERB_TEXT_COMPARE_TYPE:
		{
			bSuccess = (m_TextCompareCombo[nIdx].SelectString(-1, VerbStr) != CB_ERR);
		}
		break;

		case CRITERIA_VERB_EQUAL_COMPARE_TYPE:
		{
			bSuccess = (m_EqualCompareCombo[nIdx].SelectString(-1, VerbStr) != CB_ERR);
		}
		break;

		case CRITERIA_VERB_NUM_COMPARE_TYPE:
		{
			bSuccess = (m_NumCompareCombo[nIdx].SelectString(-1, VerbStr) != CB_ERR);
		}
		break;

		case CRITERIA_VERB_DATE_COMPARE_TYPE:
		{
			bSuccess = (m_DateCompareCombo[nIdx].SelectString(-1, VerbStr) != CB_ERR);
		}
		break;

		default: ASSERT(0); bSuccess = false; break;
	}

	ASSERT(bSuccess);

	if (!bSuccess)
		return (false);

	switch (m_CritState[nIdx].m_CurValueType)
	{
		case CRITERIA_VALUE_TEXT_TYPE:
		{
			m_TextEdit[nIdx].SetWindowText(ValueStr);
		}
		break;

		case CRITERIA_VALUE_AGE_TYPE:
		case CRITERIA_VALUE_ATTACHCOUNT_TYPE:
		case CRITERIA_VALUE_SIZE_TYPE:
		case CRITERIA_VALUE_JUNKSCORE_TYPE:
		{
			m_NumEdit[nIdx].SetWindowText(ValueStr);
		}
		break;

		case CRITERIA_VALUE_STATUS_TYPE:
		{
			bSuccess = (m_StatusCombo[nIdx].SelectString(-1, ValueStr) != CB_ERR);
		}
		break;

		case CRITERIA_VALUE_LABEL_TYPE:
		{
			bSuccess = (m_LabelCombo[nIdx].SelectString(-1, ValueStr) != CB_ERR);
		}
		break;

		case CRITERIA_VALUE_PERSONA_TYPE:
		{
			bSuccess = (m_PersonaCombo[nIdx].SelectString(-1, ValueStr) != CB_ERR);
		}
		break;

		case CRITERIA_VALUE_PRIORITY_TYPE:
		{
			bSuccess = (m_PriorityCombo[nIdx].SelectString(-1, ValueStr) != CB_ERR);
		}
		break;

		case CRITERIA_VALUE_DATE_TYPE:
		{
			COleDateTime TmpDate;
			bSuccess = (TmpDate.ParseDateTime(ValueStr, VAR_DATEVALUEONLY) == TRUE);

			if (bSuccess)
				m_DateTimeCtrl[nIdx].SetDateTime(TmpDate);
		}
		break;

		default: ASSERT(0); bSuccess = false; break;
	}

	ASSERT(bSuccess);

	if (!bSuccess)
		return (false);

	return (true);
}

void CSearchView::HandleSelectMessage(QCMailboxCommand * in_pMbxCmd, bool in_bSelectParentFolder)
{
	ASSERT(in_pMbxCmd);
	
	if (in_pMbxCmd)
	{
		// Check this MBX and uncheck everything else
		HTREEITEM		hItem = m_MBoxTree.FindCheck(in_pMbxCmd, true, in_bSelectParentFolder);

		// Now select that item, which will also scroll it in to view
		ASSERT(hItem);
		if (hItem)
		{
			if ( m_MBoxTree.ItemHasChildren(hItem) )
				VERIFY( m_MBoxTree.Expand(hItem, TVE_EXPAND) );
			VERIFY( m_MBoxTree.Select(hItem, TVGN_CARET) );
		}
	}
}

LONG CSearchView::OnMsgReloadCriteria(WPARAM wParam, LPARAM lParam)
{
	ReloadCriteria();

	return 0;
}

LONG CSearchView::OnMsgMailboxSelect(WPARAM wParam, LPARAM lParam)
{
	HandleSelectMessage(reinterpret_cast<QCMailboxCommand *>(wParam), false);

	return 0;
}

LONG CSearchView::OnMsgParentFolderSelect(WPARAM wParam, LPARAM lParam)
{
	HandleSelectMessage(reinterpret_cast<QCMailboxCommand *>(wParam), true);

	return 0;
}

LONG CSearchView::OnMsgAllMailboxesSelect(WPARAM wParam, LPARAM lParam)
{
	m_MBoxTree.CheckAll(true);

	return 0;
}

// ==========================================================================
//
// SingleCritState
//
// ==========================================================================


SingleCritState::SingleCritState()
{ }

SingleCritState::SingleCritState(const SingleCritState &copy) // Copy constructor
	: m_CurObj(copy.m_CurObj),
	  m_CurVerbType(copy.m_CurVerbType),
	  m_CurValueType(copy.m_CurValueType),
	  m_bCurShowUnits(copy.m_bCurShowUnits),
	  m_UnitsStr(copy.m_UnitsStr)
{ }

SingleCritState::SingleCritState(const CriteriaObject &obj) // Init constructor
	: m_CurObj(obj)
{
	UpdateObj(m_CurObj);
}

SingleCritState::~SingleCritState()
{ }


SingleCritState &SingleCritState::operator=(const SingleCritState &copy)
{
	m_CurObj = copy.m_CurObj;
	m_CurVerbType = copy.m_CurVerbType;
	m_CurValueType = copy.m_CurValueType;
	m_bCurShowUnits = copy.m_bCurShowUnits;
	m_UnitsStr = copy.m_UnitsStr;

	return (*this);
}

bool SingleCritState::UpdateObj(const CriteriaObject &obj)
{
	m_CurObj = obj;

	if (!SearchCriteria::GetVerbValPair(m_CurObj, m_CurVerbType, m_CurValueType))
		return (false);

	if (CRITERIA_OBJECT_SIZE == m_CurObj)
	{
		m_bCurShowUnits = true;
		m_UnitsStr = CRString(IDS_SEARCH_SIZE_UNITS); // "K"
	}
	else if (CRITERIA_OBJECT_AGE == m_CurObj)
	{
		m_bCurShowUnits = true;
		m_UnitsStr = CRString(IDS_SEARCH_DATE_UNITS); // "Days"
	}
	else
	{
		m_bCurShowUnits = false;
		m_UnitsStr.Empty();
	}

	return (true);
}

void CSearchView::OnEditSelectAll()
{
	int nCount = m_ResultsList.GetItemCount();

	for (int i = 0; i < nCount; i++)
	{
		m_ResultsList.SetItemState(i, (UINT)LVIS_SELECTED, (UINT)LVIS_SELECTED);
	}

	//set the focu on the first item.
	m_ResultsList.SetItemState(0, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED |LVIS_FOCUSED);
}

void CSearchView::OnUpdateEditSelectAll(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(TRUE);
}


void CSearchView::OnUpdateUseIndexedSearch(CCmdUI* pCmdUI) 
{
	if ( SearchManager::Instance()->ShouldUseIndexedSearch() )
	{
		pCmdUI->Enable(TRUE);
		pCmdUI->SetCheck( m_bUseIndexedSearchIfAvailable ? 1 : 0 );
	}
	else
	{
		pCmdUI->Enable(FALSE);
		pCmdUI->SetCheck(0);
	}
}


void CSearchView::OnUpdateFindDeletedIMAPMessages(CCmdUI* pCmdUI) 
{
	if ( ShouldUseIndexedSearch() )
	{
		pCmdUI->Enable(TRUE);
		pCmdUI->SetCheck( m_bShouldIncludeDeletedIMAPMessages ? 1 : 0 );
	}
	else
	{
		pCmdUI->Enable(FALSE);
		pCmdUI->SetCheck(0);
	}
}


void CSearchView::OnUseIndexedSearch()
{
	if ( SearchManager::Instance()->ShouldUseIndexedSearch() )
	{
		m_bUseIndexedSearchIfAvailable = !m_bUseIndexedSearchIfAvailable;

		for (int i = 0; i < m_CurCritCount; i++)
			InitializeTextCompareCombo(i);

		UpdateSearchBtn();
	}
}


void CSearchView::OnFindDeletedIMAPMessages() 
{
	if ( ShouldUseIndexedSearch() )
	{
		m_bShouldIncludeDeletedIMAPMessages = !m_bShouldIncludeDeletedIMAPMessages;
		
		if ( m_BeginBtn.IsWindowEnabled() )
		{
			//	Re-do the search to get the new results list
			OnOk();
		}
	}
}
