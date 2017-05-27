#include "Network.h"
#include <Windows.h>
#include "resource.h"
#include <list>
#include "Chat.h"
using namespace std;

HINSTANCE g_hInst;
HWND g_hDlgMain, g_hDlgRoom, g_hDlgLobby;
WCHAR g_szIP[16], g_szNickName[16], g_RoomName[200];
st_CLIENT* p_gClient;
bool g_bConnect = false;
bool g_bSendFlag = false;
bool g_bOpenChat = false;

list<st_ROOM *> RoomList;

int g_iSelectedRoomIndex = -1;

BOOL CALLBACK DialogProc(HWND hWnd, UINT iMsg, WPARAM wParam, LPARAM lParam);
BOOL CALLBACK LobbyDialogProc(HWND hWnd, UINT iMsg, WPARAM wParam, LPARAM lParam);
BOOL CALLBACK ChatDialogProc(HWND hWnd, UINT iMsg, WPARAM wParam, LPARAM lParam);

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpszCmdParma, int nCmdShow)
{
	g_hInst = hInstance;

	MSG msg = { 0 };
	BOOL bRet;

	 DialogBox(g_hInst, MAKEINTRESOURCE(IDD_DIALOG_IP), NULL, (DLGPROC)DialogProc);


	while ((bRet = GetMessage(&msg, NULL, 0, 0)) != 0)
	{
		if (bRet == -1)
			break;

		bRet = FALSE;
	
		if (IsWindow(g_hDlgRoom))
		{
			bRet != IsDialogMessage(g_hDlgRoom, &msg);
			OutputDebugString(L"Room\n");
		}
		if (IsWindow(g_hDlgLobby))
		{
			bRet != IsDialogMessage(g_hDlgLobby, &msg);
			OutputDebugString(L"Lobby\n");
		}
		if (!bRet)
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}
	
	RemoveSocket();


	return (int)msg.wParam;

}

void LobbyDialogShowWindow(HWND ParentHwnd)
{
	g_hDlgLobby = CreateDialog(g_hInst, MAKEINTRESOURCE(IDD_DIALOG_LOBBY), ParentHwnd, (DLGPROC)LobbyDialogProc);
	ShowWindow(g_hDlgLobby, SW_SHOW);
}

void ChatRoomDialogShowWindow(HWND ParentHwnd)
{
	g_bOpenChat = true;
	g_hDlgRoom = CreateDialog(g_hInst, MAKEINTRESOURCE(IDD_DIALOG_CHATROOM), ParentHwnd, (DLGPROC)ChatDialogProc);
	ShowWindow(g_hDlgRoom, SW_SHOW);
}

BOOL CALLBACK DialogProc(HWND hWnd, UINT iMsg, WPARAM wParam, LPARAM lParam)
{
	HWND hIPEditBox = 0, hNickEditBox = 0;

	switch (iMsg)
	{
	case WM_DESTROY:
		PostQuitMessage(0);
		return true;
	case WM_INITDIALOG:
		g_hDlgMain = hWnd;
		ZeroMemory(g_szIP, 0, sizeof(WCHAR) * 16);
		hIPEditBox = GetDlgItem(hWnd, IDC_EDIT_IP);
		SetWindowText(hIPEditBox, L"127.0.0.1");

		ZeroMemory(g_szNickName, 0, sizeof(WCHAR) * 50);
		hNickEditBox = GetDlgItem(hWnd, IDC_EDIT_NICKNAME);
		SetWindowText(hNickEditBox, L"손님");
		break;
	case WM_CLOSE:
		PostQuitMessage(0);
		break;
	case WM_COMMAND:
		switch (wParam)
		{
		case IDOK:
			GetDlgItemText(hWnd, IDC_EDIT_IP, g_szIP, 16);
			GetDlgItemText(hWnd, IDC_EDIT_NICKNAME, g_szNickName, 50);
			InitNetwork(g_szIP, hWnd);
			LobbyDialogShowWindow(hWnd);
			ShowWindow(hWnd, FALSE);
		}
		break;

	case WM_NETWORK:
		if (WSAGETSELECTERROR(lParam))
		{
			MessageBox(g_hDlgMain, L"네트워크 초기화 실패(소켓 에러)", L"Error", MB_OK);
			RemoveSocket();
			PostQuitMessage(0);
		}

		switch (WSAGETSELECTEVENT(lParam))
		{
		case FD_CONNECT:
			g_bConnect = true;

			// 닉네임을 서버로 전송
			// 연결되면 자동으로 로곤 신청과 그후 룸 전송 요청한다.
			ReqLogin(g_szNickName);
			
			break;
		case FD_READ:
			RecvEvent();
			break;
		case FD_WRITE:
			g_bSendFlag = true;
			SendEvent();
			break;
		case FD_CLOSE:
			if (g_bConnect)
			{
				RemoveSocket();
			}
			break;
		}
	}

	return false;
}
BOOL CALLBACK LobbyDialogProc(HWND hWnd, UINT iMsg, WPARAM wParam, LPARAM lParam)
{
	switch (iMsg)
	{
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	case WM_CLOSE:
		DestroyWindow(g_hDlgMain);
		DestroyWindow(g_hDlgLobby);
		break;
	case WM_COMMAND:
		switch (wParam)
		{
		case IDOK:
			RequestMakeRoom();
			break;
		}

		switch (LOWORD(wParam))
		{
		case IDC_LISTROOM:
			switch (HIWORD(wParam))
			{
			case LBN_DBLCLK:
				if (!g_bOpenChat)
				{
					RequestEnterRoom();
				}
				break;
			case LBN_SELCHANGE:
				g_iSelectedRoomIndex = SendMessage((HWND)lParam, LB_GETCURSEL, 0, 0);
				break;
			}
			break;
		}

		break;
	}
	return false;
}
BOOL CALLBACK ChatDialogProc(HWND hWnd, UINT iMsg, WPARAM wParam, LPARAM lParam)
{
	switch (iMsg)
	{
	case WM_COMMAND:
		switch (wParam)
		{
		case IDSEND:
			RequestChat();
			break;
		}
		break;
	case WM_KEYDOWN:
		switch (wParam)
		{
		case VK_RETURN:
			RequestChat();
			break;
		}
		break;
	case WM_DESTROY:
		g_bOpenChat = false;
		RequestLeaveRoom();
		EndDialog(hWnd, TRUE);
		break;
	case WM_CLOSE:
		g_bOpenChat = false;
		RequestLeaveRoom();
		EndDialog(hWnd, TRUE);
		break;
	}
	return false;
}


void ResponseNickNameUserNumber(int UserNo)
{
	HWND hStaticNickName, hStaticUserNo;
	HWND hWnd = g_hDlgLobby;

	p_gClient = new st_CLIENT;
	p_gClient->clientIDNum = UserNo;
	lstrcpyW(p_gClient->szNickName, g_szNickName);


	hStaticNickName = GetDlgItem(hWnd, IDC_NICKNAME);
	SetWindowText(hStaticNickName, p_gClient->szNickName);

	WCHAR szNumber[10];

	_itow_s(p_gClient->clientIDNum, szNumber, 10);

	hStaticUserNo = GetDlgItem(hWnd, IDC_USERNUMBER);
	SetWindowText(hStaticUserNo, szNumber);

	ReqRoomList();
}
void ResponseRoomList()
{
	// RoomList에 들어간 정보를 Lobby의 리스트에 출력한다.
	HWND hWnd = g_hDlgLobby;
	HWND hListBox = GetDlgItem(hWnd, IDC_LISTROOM);

	list<st_ROOM *>::iterator iter;

	int iindex = 0;

	for (iter = RoomList.begin(); iter != RoomList.end(); ++iter)
	{
		// 현재로썬 방제목, 방 구조체 포인터를 동시에 입력시키면 된다.
		st_ROOM *pRoom = (*iter);
		iindex = SendMessage(hListBox, LB_ADDSTRING, 0, (LPARAM)pRoom->szTitle);

		if (iindex == LB_ERR)
			continue;

		SendMessage(hListBox, LB_SETITEMDATA, (WPARAM)iindex, (LPARAM)(*iter));

	}

}
void ResponseRoomCreate(st_ROOM *pRoom)
{
	HWND hWnd = g_hDlgLobby;
	HWND hListBox = GetDlgItem(hWnd, IDC_LISTROOM);

	int iIndex = SendMessage(hListBox, LB_ADDSTRING, 0, (LPARAM)pRoom->szTitle);
	if (iIndex == LB_ERR)
		return;

	SendMessage(hListBox, LB_SETITEMDATA, (WPARAM)iIndex, (LPARAM)pRoom);

}
void ResponseEnterRoom(st_ROOM *pRoom)
{
	HWND hWnd = g_hDlgRoom;
	HWND hListBox = GetDlgItem(hWnd, IDC_LISTID);
	HWND hStatic = GetDlgItem(hWnd, IDC_STATICROOMNAME);

	SendMessage(hListBox, LB_RESETCONTENT, 0, 0);

	auto iter = pRoom->userClient.begin();

	for (; iter != pRoom->userClient.end(); ++iter)
	{
		st_CLIENT *pClient = (*iter);
		SendMessage(hListBox, LB_ADDSTRING, 0, (LPARAM)pClient->szNickName);
	}


	SetWindowText(hStatic, pRoom->szTitle);

}
void ResponseUserEnter(WCHAR *szMessage)
{
	HWND hWnd = g_hDlgRoom;
	HWND hListBox = GetDlgItem(hWnd, IDC_LISTID);

	st_ROOM *pRoom = nullptr;

	auto iter = RoomList.begin();
	int RoomNumber = p_gClient->roomNum;

	for (; iter != RoomList.end(); ++iter)
	{
		if ((*iter)->roomNum == RoomNumber)
		{
			pRoom = (*iter);
			break;
		}
	}

	if (pRoom == nullptr)
	{
		p_gClient->roomNum = -1;
		return;
	}

	SendMessage(hListBox, LB_ADDSTRING, 0, (LPARAM)szMessage);

}
void ResonseLeaveRoom(int UserNo)
{
	int roomNum = p_gClient->roomNum;
	HWND hWnd = g_hDlgRoom;
	HWND hListBox = GetDlgItem(hWnd, IDC_LISTID);


	st_CLIENT *pClient = nullptr;
	st_ROOM *pRoom = nullptr;

	auto iter = RoomList.begin();
	for (; iter != RoomList.end(); ++iter)
	{
		if ((*iter)->roomNum == roomNum)
		{
			pRoom = (*iter);
			break;
		}
	}

	if (pRoom == nullptr)
		return;
	

	// 룸에서 없앤다.
	
	auto Useriter = pRoom->userClient.begin();

	for (; Useriter != pRoom->userClient.end(); ++Useriter)
	{
		 pClient = (*Useriter);

		if (pClient->clientIDNum == UserNo)
		{
			if (UserNo == p_gClient->clientIDNum)
				p_gClient->roomNum = -1;
			pRoom->userClient.erase(Useriter);
			break;
		}

	}

	if (pClient == nullptr)
		return;

	int iIndex = SendMessage(hListBox, LB_FINDSTRING, -1, (LPARAM)pClient->szNickName);
	if (iIndex == LB_ERR)
		return;

	SendMessage(hListBox, LB_DELETESTRING, iIndex, 0);

	if(pClient != p_gClient)
		delete pClient;

	if (pRoom->userClient.size() == 0)
		ResonseRoomDelete(pRoom->roomNum);
}

void ResponseChat(int SenderNo, WCHAR *szMessage, WORD MessageSize)
{
	HWND hWnd = g_hDlgRoom;
	HWND hListBox = GetDlgItem(hWnd, IDC_LISTCHAT);

	int RoomNumber = p_gClient->roomNum;
	st_ROOM *pRoom = nullptr;

	auto iter = RoomList.begin();

	for (; iter != RoomList.end(); ++iter)
	{
		if ((*iter)->roomNum == RoomNumber)
		{
			pRoom = (*iter);
			break;
		}
	}

	if (pRoom == nullptr)
		p_gClient->roomNum = -1;

	


	auto Useriter = pRoom->userClient.begin();
	WCHAR ChatMessage[200];

	for (; Useriter != pRoom->userClient.end(); ++Useriter)
	{
		st_CLIENT *pClient = (*Useriter);

		if (pClient->clientIDNum == SenderNo)
		{
			wsprintf(ChatMessage, L"%s:", pClient->szNickName);
			break;
		}
	}

	lstrcatW(ChatMessage, szMessage);
	SendMessage(hListBox, LB_ADDSTRING, 0, (LPARAM)ChatMessage);

}
void ResonseRoomDelete(int RoomNo)
{
	HWND hWnd = g_hDlgLobby;
	HWND hListBox = GetDlgItem(hWnd, IDC_LISTROOM);

	st_ROOM *pRoom = nullptr;
	auto iter = RoomList.begin();

	for (; iter != RoomList.end(); ++iter)
	{
		pRoom = (*iter);

		if (pRoom->roomNum == RoomNo)
		{
			RoomList.erase(iter);
			break;
		}
	}

	if (pRoom == nullptr)
		return;

	int iIndex = SendMessage(hListBox, LB_FINDSTRING, -1, (LPARAM)pRoom->szTitle);
	if (iIndex == LB_ERR)
		return;

	SendMessage(hListBox, LB_DELETESTRING, iIndex, 0);
	delete pRoom;
}
void RequestMakeRoom()
{
	// 만약 접속중인 상태가 아니라면 
	if (!g_bConnect)
		return; // 처리하지 않는다.
	HWND hRoomEdit;
	HWND hWnd = g_hDlgLobby;

	ZeroMemory(g_RoomName, sizeof(WCHAR) * 200);
	hRoomEdit = GetDlgItem(hWnd, IDC_EDIT_ROOMNAME);

	GetDlgItemText(hWnd, IDC_EDIT_ROOMNAME, g_RoomName, 200);
	SetDlgItemText(hWnd, IDC_EDIT_ROOMNAME, L"");

	WORD RoomNameSize = lstrlenW(g_RoomName);
	RoomNameSize = (RoomNameSize * 2);


	if (RoomNameSize <= 0)
		return;

	ReqRoomCreate(g_RoomName, RoomNameSize);
}
void RequestEnterRoom()
{
	// 만약 접속중인 상태가 아니라면 
	if (!g_bConnect)
		return; // 처리하지 않는다.

	if (g_iSelectedRoomIndex == LB_ERR)
		return;

	HWND hWnd = g_hDlgLobby;
	HWND hListBox = GetDlgItem(hWnd, IDC_LISTROOM);

	st_ROOM *pData = (st_ROOM *)SendMessage(hListBox, LB_GETITEMDATA, g_iSelectedRoomIndex, 0);

	if ((LRESULT)pData == LB_ERR)
		return;

	ReqRoomEnter(pData->roomNum);
}
void RequestLeaveRoom()
{
	if (!g_bConnect)
		return; // 처리하지 않는다.

	ReqRoomLeave();
	// 현재 들어가 있는 방에서 나간다.

}
void RequestChat()
{
	if (!g_bConnect)
		return; // 처리하지 않는다.

	HWND hWnd = g_hDlgRoom;
	HWND hChat = GetDlgItem(hWnd, IDC_EDITCHAT);
	HWND hListBox = GetDlgItem(hWnd, IDC_LISTCHAT);

	WCHAR szChat[200];
	ZeroMemory(szChat, sizeof(WCHAR) * 200);

	GetDlgItemText(hWnd, IDC_EDITCHAT, szChat, 200);
	SetDlgItemText(hWnd, IDC_EDITCHAT, L"");

	WORD ChatSize = lstrlenW(szChat) * 2;

	if (ChatSize <= 0)
		return;

	WCHAR ChatMessage[200];
	memset(ChatMessage, L'\0', sizeof(WCHAR) * 200);
	wsprintf(ChatMessage, L"나:");

	lstrcatW(ChatMessage, szChat);
	SendMessage(hListBox, LB_ADDSTRING, 0, (LPARAM)ChatMessage);

	ReqChat(szChat, ChatSize);
}