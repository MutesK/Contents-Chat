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

	// ��Ʈ ����
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
	FD_SET RSet, WSet;  // 64�κ�
	timeval time;
	time.tv_sec = 1;
	time.tv_usec = 0;

	/////////////////////////////////////////////////////////
	// 64�� ���� ���� Set, Select�� �ؾ� �ȴ�. (FD_SETSIZE)
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

		// ���� ����
		if (FD_ISSET(g_ListenSocket, &RSet))
			NewUser();

		Iter = beginIter;

		// ������ �ۼ���
		for (int i = 0; i < FD_SETSIZE; i++)
		{
			if (Iter == g_ClientList.end())
				break;

			st_CLIENT *pClient = (*Iter);

			// �ޱ���Ʈ
			if (FD_ISSET(pClient->_clientSock, &RSet))
			{
				if (RecvPacketProc(pClient->_clientSock, pClient) == SOCKET_ERROR)
				{
					// ���� �� Ŭ���̾�Ʈ�� �ٸ� �濡 �̹� ���ٸ�?
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
					// ���� �� Ŭ���̾�Ʈ�� �ٸ� �濡 �̹� ���ٸ�?
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
		// �����Ͱ� ��� ������ �̻����� Ȯ��
		if (RecvQ->GetDataSize() < 6)
			return false;

		// ��Ŷ�ڵ� �����ͼ� Ȯ��
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
			//�α���
			RecvLogin(CheckSum, pClient, PayloadSize);
			break;
		case df_REQ_ROOM_LIST:
			//��ȭ�� ����Ʈ
			RecvRoomList(CheckSum, pClient, PayloadSize);
			break;
		case df_REQ_ROOM_CREATE:
			//��ȭ�� ����
			RecvRoomCreate(CheckSum, pClient, PayloadSize);
			break;
		case df_REQ_ROOM_ENTER:
			//��ȭ�� ����
			RecvRoomEnter(CheckSum, pClient, PayloadSize);
			break;
		case df_REQ_CHAT:
			RecvChat(CheckSum, pClient, PayloadSize);
			//ä�ü۽�
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

	// üũ�� üũ
	if (bCheckSum != makeCheckSum(df_REQ_LOGIN, (BYTE *)RecvQ->GetReadBufferPtr(), PayloadSize))
	{
		wprintf(L"Req Login CheckSum  isn't Matched - UserNo[%d] \n", pClient->clientIDNum);
		// 4�� ���� 
		 SendLogin(pClient, df_RESULT_LOGIN_ETC);
		 return;
	}
	///////////////////////

	RecvQ->GetData((char *)szNicName, PayloadSize);

	// �ߺ� üũ
	list <st_CLIENT *>::iterator iter = g_ClientList.begin();
	for (; iter != g_ClientList.end(); ++iter)
	{
		st_CLIENT *pClient = (*iter);
		if (lstrcmpW(pClient->szNickName, szNicName) == 0)
		{
			// 2�� ���� 
			SendLogin(pClient, df_RESULT_LOGIN_DNICK);
			return;
		}
	}

	pClient->clientIDNum = CntClient++;
	lstrcpyW(pClient->szNickName, szNicName);
	wprintf(L"%s[%d] Connected  \n", szNicName, pClient->clientIDNum);
	// 1��
	 SendLogin(pClient, df_RESULT_LOGIN_OK);
}
void RecvRoomList(BYTE bCheckSum, st_CLIENT *pClient, WORD PayloadSize)
{
	// PayLoad �������� ����.

	// ���� ��� ��ȸ�� ����Ʈ�� �����ϰ� �Ѵ�. -> SendRoomList�� �������� ��ȣ�� �ѱ��.

	// 1. üũ�� Ȯ��
	if (bCheckSum != makeCheckSum(df_REQ_ROOM_LIST, nullptr, 0))
	{
		wprintf(L"Req Room List CheckSum Error - UserNo[%d]  \n", pClient->clientIDNum);
		return;
	}

	return SendRoomList(pClient);

}
// Req ��ȭ�� ����
void RecvRoomCreate(BYTE bCheckSum, st_CLIENT *pClient, WORD PayloadSize)
{
	// 1. ������ Size, 2. ������(�����ڵ�)
	CSerializeBuffer* RecvQ = &pClient->RecvQ;

	// üũ�� Ȯ��
	if (bCheckSum != makeCheckSum(df_REQ_ROOM_CREATE, (BYTE *)RecvQ->GetReadBufferPtr(), PayloadSize))
	{
		wprintf(L"Req Room Create CheckSum Error - UserNo[%d]  \n", pClient->clientIDNum);
		// 4��
		 SendRoomCreate(pClient, nullptr, df_RESULT_ROOM_CREATE_ETC);
		 return;
	}

	WORD roomNameSize;
	*RecvQ >> roomNameSize;

	WCHAR szTest[15] = L"Test";
	// ���̸� �ߺ� Ȯ��
	WCHAR *szRoomName = new WCHAR[(roomNameSize / 2)];
	memset(szRoomName, L'\0', roomNameSize + 1);
	RecvQ->GetData((char *)szRoomName, roomNameSize);
	szRoomName[roomNameSize / 2] =  L'\0';

	list <st_ROOM *>::iterator iter;
	for (iter = g_RoomList.begin(); iter != g_RoomList.end(); ++iter)
	{
		if (lstrcmpW((*iter)->szTitle, szRoomName) == 0)
		{
			// 2�� 
			wprintf(L"Req Room Create �ߺ� Error - UserNo[%d]  \n", pClient->clientIDNum);
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
//  Req ��ȭ�� ����
void RecvRoomEnter(BYTE bCheckSum, st_CLIENT *pClient, WORD PayloadSize)
{
	CSerializeBuffer* RecvQ = &pClient->RecvQ;
	
	// üũ�� üũ
	if (bCheckSum != makeCheckSum(df_REQ_ROOM_ENTER, (BYTE *)RecvQ->GetReadBufferPtr(), PayloadSize))
	{
		wprintf(L"Req RoomEnter CheckSum Error - UserNo[%d] \n", pClient->clientIDNum);
		// 4��
		SendRoomEnter(pClient, nullptr, df_RESULT_ROOM_ENTER_ETC);
		return;
	}

	int RoomNum;
	*RecvQ >> RoomNum;

	// �� ���� ã�´�.
	list<st_ROOM *>::iterator iter = g_RoomList.begin();
	for (; iter != g_RoomList.end(); ++iter)
	{
		st_ROOM *pRoom = (*iter);

		if (pRoom->roomNum == RoomNum)
		{
			// ���� ó���Ѵ�.
			pClient->roomNum = RoomNum;
			pRoom->userClient.push_back(pClient);
			// 1�� 
			SendRoomEnter(pClient, pRoom, df_RESULT_ROOM_ENTER_OK);
			SendAnotherRoomEnter(pClient, pRoom);
		}
	}
	// 2
	SendRoomEnter(pClient, nullptr, df_RESULT_ROOM_ENTER_NOT);
	
}
// Req ä�� �۽�
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
// 11 Req ������ 
void RecvRoomLeave(BYTE bCheckSum, st_CLIENT *pClient, WORD PayloadSize)
{
	// ���̷ε� ����.
	if (bCheckSum != makeCheckSum(df_REQ_ROOM_LEAVE, nullptr, 0))
	{
		wprintf(L"Req RoomLeave CheckSum Error - UserNo[%d] \n", pClient->clientIDNum);
		return;
	}

	int RoomNumber = pClient->roomNum;
	list<st_ROOM *>::iterator iter = g_RoomList.begin();

	// ���� ã�´�.
	for (; iter != g_RoomList.end(); ++iter)
	{
		st_ROOM *pRoom = (*iter);

		if (pRoom->roomNum == RoomNumber)
		{
			
			 SendRoomLeave(pClient, pRoom);
			 pRoom->userClient.remove(pClient);

			// ���� �ش� ���� �������� 1����?
			if (pRoom->userClient.size() == 0)
			{
				// �� ����
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
	
	// ��� �����.

	Buffer << ErrorCode;
	Buffer << pClient->clientIDNum;
	// ������� ����
	MakeSendPacket(Buffer, pClient, df_RES_LOGIN);
}

void SendRoomList(st_CLIENT *pClient)
{
	// ���� ������ �ִ� ��� Room�� ���� �����Ѵ�.
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
		Buffer << joined; // ���� �ο�

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

	// ������� ����
	MakeSendPacket(Buffer, pClient, df_RES_ROOM_CREATE);
}

void SendRoomEnter(st_CLIENT *pClient, st_ROOM *pRoom, BYTE ErrorCode)
{
	// ���� ������ �����Ѵ�.
	CSerializeBuffer Buffer;
	
	Buffer << ErrorCode;

	if (ErrorCode != df_RESULT_ROOM_ENTER_OK)
		return;

	// ���ȣ
	Buffer << pRoom->roomNum;
	int Size = lstrlenW(pRoom->szTitle);
	Buffer << (WORD)(Size * 2);
	// ������
	
	Buffer.PutData((char *)pRoom->szTitle, Size * 2);

	// ���� �ο�
	Buffer << (BYTE)pRoom->userClient.size();

	WCHAR szNicName[15];
	memset(szNicName, L'\0', 30);
	list<st_CLIENT *>::iterator iter = pRoom->userClient.begin();
	for (; iter != pRoom->userClient.end(); ++iter)
	{
		Buffer.PutData((char *)(*iter)->szNickName, 30);
		Buffer << (*iter)->clientIDNum;
	}

	// ������� ����
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
// 10 Res ä�ü��� (�ƹ����� �� �� ����)  (������ ���� ����)
//
// 4Byte : �۽��� No
//
// 2Byte : �޽��� Size
// Size  : ��ȭ����(�����ڵ�)
//------------------------------------------------------------
void SendChat(st_CLIENT *pClient, WORD MessageSize, WCHAR *szMessage)
{
	// pClient�� �����ϰ� ��ȿ� �ִ� Ŭ���̾�Ʈ���� �����Ѵ�.
	list <st_ROOM *>::iterator Roomiter;
	int roomNum = pClient->roomNum;
	st_ROOM *pRoom = nullptr;

	int SenderNo = pClient->clientIDNum;

	for (Roomiter = g_RoomList.begin(); Roomiter != g_RoomList.end(); ++Roomiter)
	{
		// ���� ã�´�.
		if ((*Roomiter)->roomNum == roomNum)
		{
			pRoom = (*Roomiter);
			break;
		}
	}

	if (pRoom == nullptr)
		return;

	// ��ȿ� �ִ� Ŭ���̾�Ʈ�鿡�� �����ؾ� ��.
	list<st_CLIENT *> *pUserList = &pRoom->userClient;
	list<st_CLIENT *>::iterator iter;

	for (iter = pUserList->begin(); iter != pUserList->end(); ++iter)
	{
		st_CLIENT* pUserClient = (*iter);

		if (pUserClient == pClient)
			continue;

		// ���� ���
		CSerializeBuffer Buffer;
		

		Buffer << SenderNo;
		Buffer << (WORD)(MessageSize);
		Buffer.PutData((char *)szMessage, MessageSize );

		MakeSendPacket(Buffer, pUserClient, df_RES_CHAT);
	}



}

void SendRoomLeave(st_CLIENT *pClient, st_ROOM *pRoom)
{
	// ��ȿ� �ִ� Ŭ���̾�Ʈ�鿡�� �����Ѵ�.
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
	// ��� Ŭ���̾�Ʈ�鿡�� �����Ѵ�.
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
	// �濡 �� �ִٸ� ����ó��
	int RoomNumber = pClient->roomNum;
	list<st_ROOM *>::iterator iter = g_RoomList.begin();

	// ���� ã�´�.
	for (; iter != g_RoomList.end(); ++iter)
	{
		st_ROOM *pRoom = (*iter);

		if (pRoom->roomNum == RoomNumber)
		{

			SendRoomLeave(pClient, pRoom);
			pRoom->userClient.remove(pClient);

			// ���� �ش� ���� �������� 1����?
			if (pRoom->userClient.size() == 0)
			{
				// �� ����
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