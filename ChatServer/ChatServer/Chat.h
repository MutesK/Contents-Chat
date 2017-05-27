#pragma once


#include "Common.h"
#include "Protocol.h"
#include "SerializeBuffer.h"
#include "RingBuffer.h"
#include <list>
using namespace std;
struct st_CLIENT
{
	SOCKET _clientSock;
	SOCKADDR_IN _clientAddr;

	CSerializeBuffer SendQ;
	CSerializeBuffer RecvQ;

	DWORD clientIDNum;
	DWORD roomNum;

	WCHAR szNickName[15];
};

struct st_ROOM
{
	int roomNum;
	WCHAR szTitle[256];

	list<st_CLIENT *> userClient;
};

void NetworkInit();

void NetworkClear();
void NetworkProc();
void NewUser();

BYTE makeCheckSum(WORD MsgType, BYTE* PayLoad, int PayLoadBYTESize);
/////////////////////////////////////
// 송신 함수 리스트
// Returns : True 성공, False 실패(소켓 연결 끊기)
int RecvPacketProc(SOCKET sock, st_CLIENT *pClient);
void RecvLogin(BYTE bCheckSum, st_CLIENT *pClient, WORD PayloadSize);
void RecvRoomList(BYTE bCheckSum, st_CLIENT *pClient, WORD PayloadSize);
void RecvRoomCreate(BYTE bCheckSum, st_CLIENT *pClient, WORD PayloadSize);
void RecvRoomEnter(BYTE bCheckSum, st_CLIENT *pClient , WORD PayloadSize);

// Need to Test
void RecvChat(BYTE bCheckSum, st_CLIENT *pClient, WORD PayloadSize);
void RecvRoomLeave(BYTE bCheckSum, st_CLIENT *pClient, WORD PayloadSize);


// 수신 함수는 SendQ에 데이터만 넣기만 한다.
void SendLogin(st_CLIENT *pClient, BYTE ErrorCode);
void SendRoomList(st_CLIENT *pClient);
void SendRoomCreate(st_CLIENT *pClient, st_ROOM *pRoom, BYTE ErrorCode);
void SendRoomEnter(st_CLIENT *pClient, st_ROOM *pRoom, BYTE ErrorCode);
void SendAnotherRoomEnter(st_CLIENT *pExceptClient, st_ROOM *pRoom);

// Need to Test
void SendChat(st_CLIENT *pExcetionClient, WORD MessageSize, WCHAR *szMessage);
void SendRoomLeave(st_CLIENT *pClient, st_ROOM *pRoom);
void SendRoomDelete(DWORD roomNo);

void MakeSendPacket(CSerializeBuffer &Buffer, st_CLIENT *pClient, WORD MsgType);

void Disconnect(st_CLIENT *pClient);

int SendPacketProc(SOCKET sock, st_CLIENT *pClient);