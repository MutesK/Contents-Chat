#pragma once
void LobbyDialogShowWindow(HWND ParentHwnd);
void ChatRoomDialogShowWindow(HWND ParentHwnd);


// Response 대응 함수
void ResponseNickNameUserNumber(int UserNo);
void ResponseRoomList();
void ResponseRoomCreate(st_ROOM *pRoom);
void ResponseEnterRoom(st_ROOM *pRoom);
void ResponseUserEnter(WCHAR *szMessage);
void ResponseChat(int SenderNo, WCHAR *szMessage, WORD MessageSize);
void ResonseLeaveRoom(int UserNo);
void ResonseRoomDelete(int RoomNo);

// Request 대응 함수
void RequestMakeRoom();
void RequestEnterRoom();
void RequestLeaveRoom();
void RequestChat();