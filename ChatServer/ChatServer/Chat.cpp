#include "Chat.h"

list <st_CLIENT *> g_ClientList;
list <st_ROOM *> g_RoomList;

SOCKET g_ListenSocket;

unsigned int CntClient = 1;
unsigned int CntRoom = 1;

void NetworkInit()
{
	WSADATA wsaData;
	WSAStartup(MAKEWORD(2, 2), &wsaData);

	g_ListenSocket = socket(AF_INET, SOCK_STREAM, 0);

	SOCKADDR_IN servAddr;
	ZeroMemory(&servAddr, sizeof(servAddr));
	servAddr.sin_family = AF_INET;
	InetPton(AF_INET, L"0.0.0.0", &servAddr.sin_addr);
	servAddr.sin_port = htons(6000);

	
	if (bind(g_ListenSocket, (SOCKADDR *)&servAddr, sizeof(SOCKADDR_IN)) == SOCKET_ERROR)
	{
		wprintf(L"bind Error() \n");
		return;
	}

	// 포트 오픈
	if (listen(g_ListenSocket, SOMAXCONN))
	{
		wprintf(L"Listen error \n");
		return;
	}

	u_long on = 1;
	ioctlsocket(g_ListenSocket, FIONBIO, &on);

	wprintf(L"Server Open.! \n");
}

void NetworkClear()
{
	closesocket(g_ListenSocket);

	auto iter = g_ClientList.begin();

	for (; iter != g_ClientList.end();)
	{
		st_CLIENT *pClient = (*iter);
		closesocket(pClient->_clientSock);
		g_ClientList.erase(iter);
		delete pClient;
	}

	WSACleanup();
}



void NetworkProc()
{
	FD_SET RSet, WSet;  // 64인분
	timeval time;
	time.tv_sec = 1;
	time.tv_usec = 0;

	/////////////////////////////////////////////////////////
	// 64명 마다 소켓 Set, Select를 해야 된다. (FD_SETSIZE)
	/////////////////////////////////////////////////////////
	int iCount = 0;
	list <st_CLIENT *>::iterator Iter;
	list <st_CLIENT *>::iterator beginIter;
	Iter = g_ClientList.begin();

	while (1)
	{
		if(Iter == g_ClientList.end())
			Iter = g_ClientList.begin();

		beginIter = Iter;

		FD_ZERO(&RSet);
		FD_ZERO(&WSet);
		FD_SET(g_ListenSocket, &RSet);

		for (int i = 0; i < FD_SETSIZE; i++)
		{
			if (Iter == g_ClientList.end())
				break;

			if ((*Iter) == nullptr)
				Iter = g_ClientList.erase(Iter);

			st_CLIENT *pClient = (*Iter);
			FD_SET(pClient->_clientSock, &RSet);

			if (pClient->SendQ.GetDataSize() > 0)
				FD_SET(pClient->_clientSock, &WSet);

			Iter++;
		}

		int retval = select(0, &RSet, &WSet, NULL, &time);

		if (retval == SOCKET_ERROR)
		{
			wprintf(L"Select Error \n");
			return;
		}

		// 연결 수립
		if (FD_ISSET(g_ListenSocket, &RSet))
			NewUser();

		Iter = beginIter;

		// 데이터 송수신
		for (int i = 0; i < FD_SETSIZE; i++)
		{
			if (Iter == g_ClientList.end())
				break;

			st_CLIENT *pClient = (*Iter);

			// 받기파트
			if (FD_ISSET(pClient->_clientSock, &RSet))
			{
				if (RecvPacketProc(pClient->_clientSock, pClient) == SOCKET_ERROR)
				{
					// 만약 이 클라이언트가 다른 방에 이미 들어갔다면?
					Disconnect(pClient);
					//wprintf(L"Disconnected - UserNo[%d] \n", pClient->clientIDNum);
					//delete pClient;
					Iter = g_ClientList.erase(Iter);
					

					if (Iter == g_ClientList.end())
						break;
				}

				pClient->RecvQ.Clear();
			}

			if (FD_ISSET(pClient->_clientSock, &WSet))
			{
				if (SendPacketProc(pClient->_clientSock, pClient) == SOCKET_ERROR)
				{
					// 만약 이 클라이언트가 다른 방에 이미 들어갔다면?
					Disconnect(pClient);
					//wprintf(L"Disconnected - UserNo[%d] \n", pClient->clientIDNum);
					//delete pClient;
					Iter = g_ClientList.erase(Iter);


					if (Iter == g_ClientList.end())
						break;
				}

				pClient->SendQ.Clear();
			}

			Iter++;
		}

		
	}

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

int RecvPacketProc(SOCKET sock, st_CLIENT *pClient)
{
	CSerializeBuffer* RecvQ = &pClient->RecvQ;
	
	int retval = recv(sock, RecvQ->GetBufferPtr(),
		RecvQ->GetBufferSize() - RecvQ->GetDataSize(), 0);

	if (retval == SOCKET_ERROR)
		return retval;

	RecvQ->MoveWritePos(retval);

	while (1)
	{
		// 데이터가 헤더 사이즈 이상인지 확인
		if (RecvQ->GetDataSize() < 6)
			return false;

		// 패킷코드 가져와서 확인
		BYTE PacketCode;
		BYTE CheckSum;
		WORD MsgType;
		WORD PayloadSize;

		*RecvQ >> (BYTE)PacketCode >> (BYTE)CheckSum >> (WORD)MsgType >> (WORD)PayloadSize;

		if (PacketCode != dfPACKET_CODE)
			return false;


		switch (MsgType)
		{
		case df_REQ_LOGIN:
			//로그인
			RecvLogin(CheckSum, pClient, PayloadSize);
			break;
		case df_REQ_ROOM_LIST:
			//대화방 리스트
			RecvRoomList(CheckSum, pClient, PayloadSize);
			break;
		case df_REQ_ROOM_CREATE:
			//대화방 생성
			RecvRoomCreate(CheckSum, pClient, PayloadSize);
			break;
		case df_REQ_ROOM_ENTER:
			//대화방 입장
			RecvRoomEnter(CheckSum, pClient, PayloadSize);
			break;
		case df_REQ_CHAT:
			RecvChat(CheckSum, pClient, PayloadSize);
			//채팅송신
			break;
		case df_REQ_ROOM_LEAVE:
			RecvRoomLeave(CheckSum, pClient, PayloadSize);
			break;
		}
	}
}

void NewUser()
{
	st_CLIENT *pUser = new st_CLIENT;
	int addrlen = sizeof(pUser->_clientAddr);
	pUser->clientIDNum = -1;

	pUser->_clientSock = accept(g_ListenSocket, (SOCKADDR *)&pUser->_clientAddr, &addrlen);
	if (pUser->_clientSock == INVALID_SOCKET)
	{
		delete pUser;
		return;
	}


	WCHAR szIP[20];
	g_ClientList.push_back(pUser);
	InetNtop(AF_INET, &pUser->_clientAddr.sin_addr, szIP, sizeof(szIP));
	wprintf(L"Accept IP[%s] \n", szIP);
}

void RecvLogin(BYTE bCheckSum, st_CLIENT *pClient, WORD PayloadSize)
{
	WCHAR szNicName[16];
	CSerializeBuffer* RecvQ = &pClient->RecvQ;

	// 체크섬 체크
	if (bCheckSum != makeCheckSum(df_REQ_LOGIN, (BYTE *)RecvQ->GetReadBufferPtr(), PayloadSize))
	{
		wprintf(L"Req Login CheckSum  isn't Matched - UserNo[%d] \n", pClient->clientIDNum);
		// 4번 오류 
		 SendLogin(pClient, df_RESULT_LOGIN_ETC);
		 return;
	}
	///////////////////////

	RecvQ->GetData((char *)szNicName, PayloadSize);

	// 중복 체크
	list <st_CLIENT *>::iterator iter = g_ClientList.begin();
	for (; iter != g_ClientList.end(); ++iter)
	{
		st_CLIENT *pClient = (*iter);
		if (lstrcmpW(pClient->szNickName, szNicName) == 0)
		{
			// 2번 오류 
			SendLogin(pClient, df_RESULT_LOGIN_DNICK);
			return;
		}
	}

	pClient->clientIDNum = CntClient++;
	lstrcpyW(pClient->szNickName, szNicName);
	wprintf(L"%s[%d] Connected  \n", szNicName, pClient->clientIDNum);
	// 1번
	 SendLogin(pClient, df_RESULT_LOGIN_OK);
}
void RecvRoomList(BYTE bCheckSum, st_CLIENT *pClient, WORD PayloadSize)
{
	// PayLoad 존재하지 않음.

	// 현재 모든 대회방 리스트를 전송하게 한다. -> SendRoomList로 보낼꺼니 신호만 넘긴다.

	// 1. 체크섬 확인
	if (bCheckSum != makeCheckSum(df_REQ_ROOM_LIST, nullptr, 0))
	{
		wprintf(L"Req Room List CheckSum Error - UserNo[%d]  \n", pClient->clientIDNum);
		return;
	}

	return SendRoomList(pClient);

}
// Req 대화방 생성
void RecvRoomCreate(BYTE bCheckSum, st_CLIENT *pClient, WORD PayloadSize)
{
	// 1. 방제목 Size, 2. 방제목(유니코드)
	CSerializeBuffer* RecvQ = &pClient->RecvQ;

	// 체크섬 확인
	if (bCheckSum != makeCheckSum(df_REQ_ROOM_CREATE, (BYTE *)RecvQ->GetReadBufferPtr(), PayloadSize))
	{
		wprintf(L"Req Room Create CheckSum Error - UserNo[%d]  \n", pClient->clientIDNum);
		// 4번
		 SendRoomCreate(pClient, nullptr, df_RESULT_ROOM_CREATE_ETC);
		 return;
	}

	WORD roomNameSize;
	*RecvQ >> roomNameSize;

	WCHAR szTest[15] = L"Test";
	// 방이름 중복 확인
	WCHAR *szRoomName = new WCHAR[(roomNameSize / 2)];
	memset(szRoomName, L'\0', roomNameSize + 1);
	RecvQ->GetData((char *)szRoomName, roomNameSize);
	szRoomName[roomNameSize / 2] =  L'\0';

	list <st_ROOM *>::iterator iter;
	for (iter = g_RoomList.begin(); iter != g_RoomList.end(); ++iter)
	{
		if (lstrcmpW((*iter)->szTitle, szRoomName) == 0)
		{
			// 2번 
			wprintf(L"Req Room Create 중복 Error - UserNo[%d]  \n", pClient->clientIDNum);
			SendRoomCreate(pClient, nullptr, df_RESULT_ROOM_CREATE_DNICK);
			return;
		}
	}
	st_ROOM *newRoom = new st_ROOM;
	newRoom->roomNum = CntRoom++;
	wprintf(L"Req Room Create Success - UserNo[%d] - Room Name - [%s]  \n", pClient->clientIDNum, szRoomName);
	lstrcpyW(newRoom->szTitle, szRoomName);
	g_RoomList.push_back(newRoom);
	
	list <st_CLIENT *>::iterator cIter;
	for (cIter = g_ClientList.begin(); cIter != g_ClientList.end(); ++cIter)
		SendRoomCreate(*cIter, newRoom, df_RESULT_ROOM_CREATE_OK);
	

}
//  Req 대화방 입장
void RecvRoomEnter(BYTE bCheckSum, st_CLIENT *pClient, WORD PayloadSize)
{
	CSerializeBuffer* RecvQ = &pClient->RecvQ;
	
	// 체크섬 체크
	if (bCheckSum != makeCheckSum(df_REQ_ROOM_ENTER, (BYTE *)RecvQ->GetReadBufferPtr(), PayloadSize))
	{
		wprintf(L"Req RoomEnter CheckSum Error - UserNo[%d] \n", pClient->clientIDNum);
		// 4번
		SendRoomEnter(pClient, nullptr, df_RESULT_ROOM_ENTER_ETC);
		return;
	}

	int RoomNum;
	*RecvQ >> RoomNum;

	// 들어갈 방을 찾는다.
	list<st_ROOM *>::iterator iter = g_RoomList.begin();
	for (; iter != g_RoomList.end(); ++iter)
	{
		st_ROOM *pRoom = (*iter);

		if (pRoom->roomNum == RoomNum)
		{
			// 입장 처리한다.
			pClient->roomNum = RoomNum;
			pRoom->userClient.push_back(pClient);
			// 1번 
			SendRoomEnter(pClient, pRoom, df_RESULT_ROOM_ENTER_OK);
			SendAnotherRoomEnter(pClient, pRoom);
		}
	}
	// 2
	SendRoomEnter(pClient, nullptr, df_RESULT_ROOM_ENTER_NOT);
	
}
// Req 채팅 송신
void RecvChat(BYTE bCheckSum, st_CLIENT *pClient, WORD PayloadSize)
{
	CSerializeBuffer* RecvQ = &pClient->RecvQ;

	if (bCheckSum != makeCheckSum(df_REQ_CHAT, (BYTE *)RecvQ->GetReadBufferPtr(), PayloadSize))
	{
		wprintf(L"Req Chat CheckSum Error - UserNo[%d] \n", pClient->clientIDNum);
		return;
	}

	WORD MessageSize;
	*RecvQ >> MessageSize;

	WCHAR* szMessage = new WCHAR[(MessageSize + 1) / 2];
	RecvQ->GetData((char *)szMessage, MessageSize);

	SendChat(pClient, MessageSize + 1, szMessage);

}
// 11 Req 방퇴장 
void RecvRoomLeave(BYTE bCheckSum, st_CLIENT *pClient, WORD PayloadSize)
{
	// 페이로드 없음.
	if (bCheckSum != makeCheckSum(df_REQ_ROOM_LEAVE, nullptr, 0))
	{
		wprintf(L"Req RoomLeave CheckSum Error - UserNo[%d] \n", pClient->clientIDNum);
		return;
	}

	int RoomNumber = pClient->roomNum;
	list<st_ROOM *>::iterator iter = g_RoomList.begin();

	// 방을 찾는다.
	for (; iter != g_RoomList.end(); ++iter)
	{
		st_ROOM *pRoom = (*iter);

		if (pRoom->roomNum == RoomNumber)
		{
			
			 SendRoomLeave(pClient, pRoom);
			 pRoom->userClient.remove(pClient);

			// 현재 해당 룸의 유저수가 1인지?
			if (pRoom->userClient.size() == 0)
			{
				// 방 삭제
				SendRoomDelete(RoomNumber);
				delete pRoom;
				g_RoomList.erase(iter);
				return;
			}
		}
	}
}


void SendLogin(st_CLIENT *pClient, BYTE ErrorCode)
{
	CSerializeBuffer Buffer;
	
	// 헤더 만든다.

	Buffer << ErrorCode;
	Buffer << pClient->clientIDNum;
	// 헤더생성 지점
	MakeSendPacket(Buffer, pClient, df_RES_LOGIN);
}

void SendRoomList(st_CLIENT *pClient)
{
	// 현재 가지고 있는 모든 Room에 대해 전송한다.
	CSerializeBuffer Buffer;
	
	list <st_ROOM *>::iterator iter;

	WORD RoomCount = g_RoomList.size();
	Buffer << RoomCount;

	for (iter = g_RoomList.begin(); iter != g_RoomList.end(); ++iter)
	{
		st_ROOM *pRoom = (*iter);

		Buffer << pRoom->roomNum;
		
		WCHAR* szRoomName = pRoom->szTitle;
		WORD roomSize = lstrlenW(szRoomName) * 2;

		Buffer << roomSize;


		Buffer.PutData((char *)szRoomName, lstrlenW(szRoomName) * 2);

		BYTE joined = pRoom->userClient.size();
		Buffer << joined; // 참여 인원

		WCHAR szNicName[30];
		list<st_CLIENT *> ::iterator iter = pRoom->userClient.begin();

		for (; iter != pRoom->userClient.end(); ++iter)
		{
			Buffer.PutData((char *)(*iter)->szNickName, lstrlenW((*iter)->szNickName) * 2);
		}
	}

	MakeSendPacket(Buffer, pClient, df_RES_ROOM_LIST);

}

void SendRoomCreate(st_CLIENT *pClient, st_ROOM *pRoom, BYTE ErrorCode)
{
	CSerializeBuffer Buffer;
	
	Buffer << (BYTE)ErrorCode;


	Buffer << pRoom->roomNum;
	
	WORD size = lstrlenW(pRoom->szTitle);
	Buffer << (WORD)(size * 2);

	for (int i = 0; i < size; i++)
		Buffer << LOBYTE(pRoom->szTitle[i]) << HIBYTE(pRoom->szTitle[i]);

	// 헤더생성 지점
	MakeSendPacket(Buffer, pClient, df_RES_ROOM_CREATE);
}

void SendRoomEnter(st_CLIENT *pClient, st_ROOM *pRoom, BYTE ErrorCode)
{
	// 방의 정보를 전송한다.
	CSerializeBuffer Buffer;
	
	Buffer << ErrorCode;

	if (ErrorCode != df_RESULT_ROOM_ENTER_OK)
		return;

	// 방번호
	Buffer << pRoom->roomNum;
	int Size = lstrlenW(pRoom->szTitle);
	Buffer << (WORD)(Size * 2);
	// 방제목
	
	Buffer.PutData((char *)pRoom->szTitle, Size * 2);

	// 참가 인원
	Buffer << (BYTE)pRoom->userClient.size();

	WCHAR szNicName[15];
	memset(szNicName, L'\0', 30);
	list<st_CLIENT *>::iterator iter = pRoom->userClient.begin();
	for (; iter != pRoom->userClient.end(); ++iter)
	{
		Buffer.PutData((char *)(*iter)->szNickName, 30);
		Buffer << (*iter)->clientIDNum;
	}

	// 헤더생성 지점
	MakeSendPacket(Buffer, pClient, df_RES_ROOM_ENTER);
	
}

void SendAnotherRoomEnter(st_CLIENT *pExceptClient, st_ROOM *pRoom)
{
	list<st_CLIENT *>::iterator iter;
	list<DWORD>::iterator iters;

	DWORD UserNo = pExceptClient->clientIDNum;
	list<st_CLIENT *> *pUserClient = &pRoom->userClient;

	
	for (iter = pUserClient->begin(); iter != pUserClient->end(); ++iter)
	{
		st_CLIENT *pClient = (*iter);

		if (pClient == pExceptClient)
			continue;
	
		CSerializeBuffer Buffer;
		

		Buffer.PutData((char *)pExceptClient->szNickName, 30);
		Buffer << UserNo;

		MakeSendPacket(Buffer, pClient, df_RES_USER_ENTER);
	}

}
// 10 Res 채팅수신 (아무때나 올 수 있음)  (나에겐 오지 않음)
//
// 4Byte : 송신자 No
//
// 2Byte : 메시지 Size
// Size  : 대화내용(유니코드)
//------------------------------------------------------------
void SendChat(st_CLIENT *pClient, WORD MessageSize, WCHAR *szMessage)
{
	// pClient를 제외하고 룸안에 있는 클라이언트에게 전송한다.
	list <st_ROOM *>::iterator Roomiter;
	int roomNum = pClient->roomNum;
	st_ROOM *pRoom = nullptr;

	int SenderNo = pClient->clientIDNum;

	for (Roomiter = g_RoomList.begin(); Roomiter != g_RoomList.end(); ++Roomiter)
	{
		// 방을 찾는다.
		if ((*Roomiter)->roomNum == roomNum)
		{
			pRoom = (*Roomiter);
			break;
		}
	}

	if (pRoom == nullptr)
		return;

	// 방안에 있는 클라이언트들에게 전송해야 됨.
	list<st_CLIENT *> *pUserList = &pRoom->userClient;
	list<st_CLIENT *>::iterator iter;

	for (iter = pUserList->begin(); iter != pUserList->end(); ++iter)
	{
		st_CLIENT* pUserClient = (*iter);

		if (pUserClient == pClient)
			continue;

		// 전송 대상
		CSerializeBuffer Buffer;
		

		Buffer << SenderNo;
		Buffer << (WORD)(MessageSize);
		Buffer.PutData((char *)szMessage, MessageSize );

		MakeSendPacket(Buffer, pUserClient, df_RES_CHAT);
	}



}

void SendRoomLeave(st_CLIENT *pClient, st_ROOM *pRoom)
{
	// 방안에 있는 클라이언트들에게 전송한다.
	int leaveNo = pClient->clientIDNum;
	list<st_CLIENT *> *pUserList = &pRoom->userClient;
	list<st_CLIENT *>::iterator iter;

	for (iter = pUserList->begin(); iter != pUserList->end(); ++iter)
	{
		st_CLIENT *pUserClient = (*iter);
		CSerializeBuffer Buffer;
		

		Buffer << leaveNo;
		MakeSendPacket(Buffer, pUserClient, df_RES_ROOM_LEAVE);
	}
}

void SendRoomDelete(DWORD roomNo)
{
	// 모든 클라이언트들에게 전송한다.
	list<st_CLIENT *>::iterator iter;

	for (iter = g_ClientList.begin(); iter != g_ClientList.end(); ++iter)
	{
		st_CLIENT *pClient = (*iter);

		CSerializeBuffer Buffer;
		

		Buffer << roomNo;
		MakeSendPacket(Buffer, pClient, df_RES_ROOM_DELETE);
	}
}

void MakeSendPacket(CSerializeBuffer &Buffer, st_CLIENT *pClient, WORD MsgType)
{
	BYTE PacketCode = dfPACKET_CODE;
	WORD PayloadSize = Buffer.GetDataSize();
	BYTE *PayLoad = (BYTE *)Buffer.GetBufferPtr();

	BYTE CheckSum = makeCheckSum(MsgType, PayLoad, PayloadSize);

	CSerializeBuffer &pSendQ = pClient->SendQ;
	pSendQ << PacketCode;
	pSendQ << CheckSum;
	pSendQ << MsgType;
	pSendQ << PayloadSize;

	pSendQ.PutData((char *)PayLoad, PayloadSize);
}

void  Disconnect(st_CLIENT *pClient)
{
	// 방에 들어가 있다면 퇴장처리
	int RoomNumber = pClient->roomNum;
	list<st_ROOM *>::iterator iter = g_RoomList.begin();

	// 방을 찾는다.
	for (; iter != g_RoomList.end(); ++iter)
	{
		st_ROOM *pRoom = (*iter);

		if (pRoom->roomNum == RoomNumber)
		{

			SendRoomLeave(pClient, pRoom);
			pRoom->userClient.remove(pClient);

			// 현재 해당 룸의 유저수가 1인지?
			if (pRoom->userClient.size() == 0)
			{
				// 방 삭제
				SendRoomDelete(RoomNumber);
				delete pRoom;
				g_RoomList.erase(iter);
				break;
			}
		}
	}


	wprintf(L"Disconnected - UserNo[%d] \n", pClient->clientIDNum);
	closesocket(pClient->_clientSock);
	delete pClient;

}

int SendPacketProc(SOCKET sock, st_CLIENT *pClient)
{
	int iSendSize = pClient->SendQ.GetDataSize();

	if (iSendSize <= 0)
		return false;

	int iResult = send(sock, pClient->SendQ.GetReadBufferPtr(), iSendSize, 0);

	if (iResult == SOCKET_ERROR)
	{
		DWORD dwError = WSAGetLastError();

		if (WSAEWOULDBLOCK == dwError)
		{
			wprintf(L"SOCKET WOULDBLOCK ERROR USERNO - %d \n", pClient->clientIDNum);
			
		}
		wprintf(L"SOCKET ERROR USERNO - %d \n", pClient->clientIDNum);
		
		
		return dwError;
	}
}