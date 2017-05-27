#include "Common.h"
#include "Chat.h"
#include <locale>
int main()
{
	setlocale(LC_ALL, "");
	NetworkInit();

	NetworkProc();

	NetworkClear();
}