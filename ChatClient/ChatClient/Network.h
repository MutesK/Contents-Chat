#pragma once

#pragma comment(lib, "ws2_32")

#include <WinSock2.h>
#include <WS2tcpip.h>
#include "Protocol.h"
#include "RingBuffer.h"
#include "SerializeBuffer.h"
#include <list>
using namespace std;
#define PORT 6000
#define WM_NETWORK WM_USER + 1

struct st_CLIENT
{
	DWORD clientIDNum;
	WCHAR szNickName[16];

	int roomNum;
};

struct st_ROOM
{
	int roomNum;
	WCHAR szTitle[256];

	list<st_CLIENT *> userClient;
};

BYTE makeCheckSum(WORD MsgType, BYTE* PayLoad, int PayLoadBYTESize);
bool InitNetwork(WCHAR* szIP, HWND hWnd);
void RemoveSocket();




void ReqLogin(WCHAR *szNick);
void ReqRoomList();
void ReqRoomCreate(WCHAR *szRoomName, WORD RoomSize);
void ReqRoomEnter(DWORD RoomNumber);
void ReqChat(WCHAR *szChatMessage, WORD ChatSize);
void ReqRoomLeave();
void MakeSendPacket(WORD MsgType, CSerializeBuffer* pBuffer);
void SendEvent();




void RecvEvent();
void PacketProc();
void ResLogin(CSerializeBuffer &Buffer, BYTE CheckSum, WORD PayloadSize);
void ResRoomList(CSerializeBuffer &Buffer, BYTE CheckSum, WORD PayloadSize);
void ResRoomCreate(CSerializeBuffer &Buffer, BYTE CheckSum, WORD PayloadSize);
void ResRoomEnter(CSerializeBuffer &Buffer, BYTE CheckSum, WORD PayloadSize);
void ResUserRoomEnter(CSerializeBuffer &Buffer, BYTE CheckSum, WORD PayloadSize);
void ResChat(CSerializeBuffer &Buffer, BYTE CheckSum, WORD PayloadSize);
void ResRoomLeave(CSerializeBuffer &Buffer, BYTE CheckSum, WORD PayloadSize);
void ResRoomDelete(CSerializeBuffer &Buffer, BYTE CheckSum, WORD PayloadSize);