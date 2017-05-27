#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include "Network.h"
#include "Chat.h"

SOCKET g_ClientSocket;
// 자기 자신 객체
extern st_CLIENT* p_gClient;

HWND g_MainHWnd;

CRingBuffer SendQ;
CRingBuffer RecvQ;

extern bool g_bSendFlag;
extern list<st_ROOM *> RoomList;

bool InitNetwork(WCHAR* szIP, HWND hWnd)
{
// 네트워크 초기화
	WSADATA wsaData;
	g_MainHWnd = hWnd;

	WSAStartup(MAKEWORD(2, 2), &wsaData);

	g_ClientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (g_ClientSocket == INVALID_SOCKET)
		return false;

	SOCKADDR_IN servAddr;
	ZeroMemory(&servAddr, sizeof(servAddr));
	servAddr.sin_family = AF_INET;
	InetPton(AF_INET, szIP, &servAddr.sin_addr);
	servAddr.sin_port = htons(PORT);

	
	WSAAsyncSelect(g_ClientSocket, g_MainHWnd,
		WM_NETWORK, FD_CONNECT | FD_READ | FD_WRITE | FD_CLOSE);


	int err = connect(g_ClientSocket, (SOCKADDR *)&servAddr, sizeof(SOCKADDR_IN));
	if (err == SOCKET_ERROR)
	{
		if (WSAGetLastError() != WSAEWOULDBLOCK)
		{
			return false;
		}
	}

	return true;
}

void RemoveSocket()
{
	// 방에 들어가 있다면 확인한다.

	closesocket(g_ClientSocket);
	g_ClientSocket = INVALID_SOCKET;
}


// Request 클라이언트 -> 서버 
void ReqLogin(WCHAR *szNick)
{
	CSerializeBuffer buffer;
	
	buffer.PutData((char *)szNick, 30);

	MakeSendPacket(df_REQ_LOGIN, &buffer);
	SendEvent();
}
void ReqRoomList()
{
	CSerializeBuffer buffer;
	MakeSendPacket(df_REQ_ROOM_LIST, &buffer);
	SendEvent();
}
void ReqRoomCreate(WCHAR *szRoomName, WORD RoomSize)
{
	CSerializeBuffer buffer;

	buffer << RoomSize;
	buffer.PutData((char *)szRoomName, RoomSize);
	MakeSendPacket(df_REQ_ROOM_CREATE, &buffer);
	SendEvent();
}
void ReqRoomEnter(DWORD RoomNumber)
{
	CSerializeBuffer buffer;
	buffer << RoomNumber;

	MakeSendPacket(df_REQ_ROOM_ENTER, &buffer);
	SendEvent();
}
void ReqChat(WCHAR *szChatMessage, WORD ChatSize)
{
	CSerializeBuffer buffer;

	buffer << ChatSize;
	buffer.PutData((char *)szChatMessage, ChatSize);

	MakeSendPacket(df_REQ_CHAT, &buffer);
	SendEvent();
}
void ReqRoomLeave()
{
	CSerializeBuffer buffer;
	MakeSendPacket(df_REQ_ROOM_LEAVE, &buffer);
	SendEvent();
}
void MakeSendPacket(WORD MsgType, CSerializeBuffer* pBuffer)
{
	BYTE Code = dfPACKET_CODE;
	WORD PayloadSize = pBuffer->GetDataSize();
	BYTE CheckSum = makeCheckSum(MsgType, (BYTE *)pBuffer->GetReadBufferPtr(), PayloadSize);

	SendQ.Put((char *)&Code, 1);
	SendQ.Put((char *)&CheckSum, 1);
	SendQ.Put((char *)&MsgType, 2);
	SendQ.Put((char *)&PayloadSize, 2);

	SendQ.Put((char *)pBuffer->GetReadBufferPtr(), PayloadSize);
}
void SendEvent()
{
	if (!g_bSendFlag)
		return;

	if (SendQ.GetUseSize() < 6)
		return;

	WSABUF wsabuf[2];
	int bufcount = 1;

	wsabuf[0].len = SendQ.GetNotBrokenGetSize();
	wsabuf[0].buf = SendQ.GetReadBufferPtr();
	if (SendQ.GetUseSize() > SendQ.GetNotBrokenGetSize())
	{
		bufcount++;
		wsabuf[1].len = SendQ.GetUseSize() - SendQ.GetNotBrokenGetSize();
		wsabuf[1].buf = SendQ.GetBufferPtr();
	}

	DWORD SendSize = 0;
	DWORD Flag = 0;

	if (WSASend(g_ClientSocket, wsabuf, bufcount, &SendSize, Flag, NULL, NULL) == SOCKET_ERROR)
	{
		if (WSAGetLastError() == WSAEWOULDBLOCK)
		{
			g_bSendFlag = false;
			return;
		}
		else
		{
			MessageBox(g_MainHWnd, L"소켓 전송 에러(Send)", L"Error", MB_OK);
			return;
		}
	}

	SendQ.RemoveData(SendSize);

}

 
void RecvEvent()
{
	WSABUF wsabuf[2];
	int bufcount = 1;

 	wsabuf[0].len = RecvQ.GetNotBrokenPutSize();
	wsabuf[0].buf = RecvQ.GetReadBufferPtr();

	if (RecvQ.GetNotBrokenPutSize() < RecvQ.GetFreeSize())
	{
		// 아직 공간이 남아 있다면
		wsabuf[1].len = RecvQ.GetFreeSize() - wsabuf[0].len;
		wsabuf[1].buf = RecvQ.GetBufferPtr();
		bufcount++;
	}

	DWORD RecvSize = 0;
	DWORD Flag = 0;

	if (WSARecv(g_ClientSocket, wsabuf, bufcount, &RecvSize, &Flag, NULL, NULL) == SOCKET_ERROR)
	{
		if (WSAGetLastError() != WSAEWOULDBLOCK)
		{
			MessageBox(g_MainHWnd, L"소켓 수신 에러(Recv)", L"Error", MB_OK);
			return;
		}
	}

	RecvQ.MoveWritePos(RecvSize);
	PacketProc();
}

void PacketProc()
{
	while (1)
	{
		if (RecvQ.GetUseSize() < 6)
			break;

		CSerializeBuffer Buffer;
		Buffer.PutData(RecvQ.GetReadBufferPtr(), Buffer.GetBufferSize());
		RecvQ.RemoveData(Buffer.GetBufferSize());

		BYTE PacketCode;
		BYTE CheckSum;
		WORD MsgType;
		WORD PayloadSize;

		Buffer >> PacketCode;
		Buffer >> CheckSum;
		Buffer >> MsgType;
		Buffer >> PayloadSize;

		if (PacketCode != dfPACKET_CODE)
		{
			MessageBox(g_MainHWnd, L"Packet Code ERROR", L"Error", MB_OK);
			exit(1);
		}

		switch (MsgType)
		{
		case df_RES_LOGIN:
			ResLogin(Buffer, CheckSum, PayloadSize);
			break;
		case df_RES_ROOM_LIST:
			ResRoomList(Buffer, CheckSum, PayloadSize);
			break;
		case df_RES_ROOM_CREATE:
			ResRoomCreate(Buffer, CheckSum, PayloadSize);
			break;
		case df_RES_ROOM_ENTER:
			ResRoomEnter(Buffer, CheckSum, PayloadSize);
			break;
		case df_RES_USER_ENTER:
			ResUserRoomEnter(Buffer, CheckSum, PayloadSize);
			break;
		case df_RES_CHAT:
			ResChat(Buffer, CheckSum, PayloadSize);
			break;
		case df_RES_ROOM_LEAVE:
			ResRoomLeave(Buffer, CheckSum, PayloadSize);
			break;
		case df_RES_ROOM_DELETE:
			ResRoomDelete(Buffer, CheckSum, PayloadSize);
			break;
		}

	}
}

void ResLogin(CSerializeBuffer &Buffer, BYTE CheckSum, WORD PayloadSize)
{
	BYTE BCheckSum = makeCheckSum(df_RES_LOGIN, (BYTE *)Buffer.GetReadBufferPtr(), PayloadSize);

	if (CheckSum != BCheckSum)
	{
		MessageBox(g_MainHWnd, L"CheckSum Error", L"Error", MB_OK);
		exit(1);
	}

	BYTE Result;
	WORD UserNo;

	Buffer >> Result;
	if (Result != df_RESULT_LOGIN_OK)
	{
		MessageBox(g_MainHWnd, L"로그인 실패", L"Error", MB_OK);
		exit(1);
	}

	Buffer >> UserNo;

	ResponseNickNameUserNumber(UserNo);
}

void ResRoomList(CSerializeBuffer &Buffer, BYTE CheckSum, WORD PayloadSize)
{
	BYTE BCheckSum = makeCheckSum(df_RES_ROOM_LIST, (BYTE *)Buffer.GetReadBufferPtr(), PayloadSize);

	if (CheckSum != BCheckSum)
	{
		MessageBox(g_MainHWnd, L"CheckSum Error", L"Error", MB_OK);
		exit(1);
	}


	WORD RoomCount;
	Buffer >> RoomCount;


	for (BYTE i = 0; i < RoomCount; i++)
	{
		st_ROOM *pRoom = new st_ROOM;

		DWORD RoomNumber;
		WORD RoomByteSize;
		BYTE JoinClient;

		Buffer >> RoomNumber;
		Buffer >> RoomByteSize;


		Buffer.GetData((char *)pRoom->szTitle, RoomByteSize);
		pRoom->szTitle[RoomByteSize / 2] = L'\0';

		Buffer >> JoinClient;
		pRoom->roomNum = RoomNumber;
		

		for (BYTE j = 0; j < JoinClient; j++)
		{
			st_CLIENT *newClient = new st_CLIENT;
			Buffer.GetData((char *)newClient->szNickName, 30);

			pRoom->userClient.push_back(newClient);
		}

		RoomList.push_back(pRoom);
	}

	ResponseRoomList();

}
void ResRoomCreate(CSerializeBuffer &Buffer, BYTE CheckSum, WORD PayloadSize)
{
	BYTE BCheckSum = makeCheckSum(df_RES_ROOM_CREATE, (BYTE *)Buffer.GetReadBufferPtr(), PayloadSize);

	if (CheckSum != BCheckSum)
	{
		MessageBox(g_MainHWnd, L"CheckSum Error", L"Error", MB_OK);
		exit(1);
	}

	BYTE Result;
	Buffer >> Result;
	if (Result != df_RESULT_ROOM_CREATE_OK)
	{
		MessageBox(g_MainHWnd, L"방 생성 실패", L"Error", MB_OK);
		exit(1);
	}

	st_ROOM *pRoom = new st_ROOM;

	DWORD RoomNumber;
	WORD RoomByteSize;

	Buffer >> RoomNumber;
	Buffer >> RoomByteSize;
	
	pRoom->roomNum = RoomNumber;
	Buffer.GetData((char *)pRoom->szTitle, RoomByteSize);
	pRoom->szTitle[RoomByteSize / 2] = L'\0';

	RoomList.push_back(pRoom);

	ResponseRoomCreate(pRoom);
	
}
void ResRoomEnter(CSerializeBuffer &Buffer, BYTE CheckSum, WORD PayloadSize)
{
	BYTE BCheckSum = makeCheckSum(df_RES_ROOM_ENTER, (BYTE *)Buffer.GetReadBufferPtr(), PayloadSize);

	if (CheckSum != BCheckSum)
	{
		MessageBox(g_MainHWnd, L"CheckSum Error", L"Error", MB_OK);
		exit(1);
	}

	BYTE Result;
	Buffer >> Result;
	if (Result != df_RESULT_ROOM_ENTER_OK)
	{
		MessageBox(g_MainHWnd, L"방 입장 실패", L"Error", MB_OK);
		exit(1);
	}

	ChatRoomDialogShowWindow(g_MainHWnd);


	int RoomNumber;
	WORD RoomSize;
	WCHAR *szRoomName;

	Buffer >> RoomNumber;
	Buffer >> RoomSize;
	szRoomName = new WCHAR[RoomSize / 2];
	Buffer.GetData((char *)szRoomName, RoomSize);
	st_ROOM *pRoom = nullptr;
	// 이 정보를 바탕으로 룸을 찾는다.
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
		return;


	BYTE JoinClient;
	Buffer >> JoinClient;
	pRoom->userClient.clear();


	for (auto i = 0; i < JoinClient; i++)
	{
		st_CLIENT *pClient = new st_CLIENT;
		Buffer.GetData((char *)pClient->szNickName, 30);
		pClient->szNickName[16] = L'\0';

		Buffer >> pClient->clientIDNum;
		if (pClient->clientIDNum == p_gClient->clientIDNum)
		{
			p_gClient->roomNum = RoomNumber;
			pRoom->userClient.push_back(p_gClient);
		}
		else
			pRoom->userClient.push_back(pClient);
	}

	ResponseEnterRoom(pRoom);
}
void ResUserRoomEnter(CSerializeBuffer &Buffer, BYTE CheckSum, WORD PayloadSize)
{
	BYTE BCheckSum = makeCheckSum(df_RES_USER_ENTER, (BYTE *)Buffer.GetReadBufferPtr(), PayloadSize);

	if (CheckSum != BCheckSum)
	{
		MessageBox(g_MainHWnd, L"CheckSum Error", L"Error", MB_OK);
		exit(1);
	}

	// 플레이어가 있는방에 추가
	int RoomNumber = p_gClient->roomNum;
	auto iter = RoomList.begin();
	st_ROOM *pRoom = nullptr;

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
	
	st_CLIENT *pNewClient = new st_CLIENT;
	Buffer.GetData((char *)pNewClient->szNickName, 30);
	pNewClient->szNickName[15] = L'\0';

	Buffer >> pNewClient->clientIDNum;

	pRoom->userClient.push_back(pNewClient);
	
	ResponseUserEnter(pNewClient->szNickName);
}

void ResChat(CSerializeBuffer &Buffer, BYTE CheckSum, WORD PayloadSize)
{
	BYTE BCheckSum = makeCheckSum(df_RES_CHAT, (BYTE *)Buffer.GetReadBufferPtr(), PayloadSize);

	if (CheckSum != BCheckSum)
	{
		MessageBox(g_MainHWnd, L"CheckSum Error", L"Error", MB_OK);
		exit(1);
	}

	int SenderNo;
	WORD MessageSize;
	WCHAR *pMessage;

	Buffer >> SenderNo;
	Buffer >> MessageSize;
	pMessage = new WCHAR[MessageSize / 2];
	Buffer.GetData((char *)pMessage, MessageSize);
	pMessage[MessageSize / 2] = L'\0';

	ResponseChat(SenderNo, pMessage, MessageSize);
}

void ResRoomLeave(CSerializeBuffer &Buffer, BYTE CheckSum, WORD PayloadSize)
{
	BYTE BCheckSum = makeCheckSum(df_RES_ROOM_LEAVE, (BYTE *)Buffer.GetReadBufferPtr(), PayloadSize);

	if (CheckSum != BCheckSum)
	{
		MessageBox(g_MainHWnd, L"CheckSum Error", L"Error", MB_OK);
		exit(1);
	}

	int LeaveUser;
	Buffer >> LeaveUser;

	ResonseLeaveRoom(LeaveUser);
}
void ResRoomDelete(CSerializeBuffer &Buffer, BYTE CheckSum, WORD PayloadSize)
{
	BYTE BCheckSum = makeCheckSum(df_RES_ROOM_DELETE, (BYTE *)Buffer.GetReadBufferPtr(), PayloadSize);

	if (CheckSum != BCheckSum)
	{
		MessageBox(g_MainHWnd, L"CheckSum Error", L"Error", MB_OK);
		exit(1);
	}

	int RoomNo;
	Buffer >> RoomNo;

	ResonseRoomDelete(RoomNo);

}
BYTE makeCheckSum(WORD MsgType, BYTE* PayLoad, int PayLoadSize)
{
	BYTE checkSum;

	checkSum = HIBYTE(MsgType) + LOBYTE(MsgType);

	for (int i = 0; i < PayLoadSize; i++)
		checkSum += PayLoad[i];

	checkSum %= 256;

	return checkSum;
}