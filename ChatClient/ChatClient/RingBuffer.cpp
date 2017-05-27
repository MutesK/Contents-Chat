#include <iostream>
#include "RingBuffer.h"



CRingBuffer::CRingBuffer()
:m_front(0), m_rear(0), m_bufferSize(10000)
{
	m_buffer = new char[10000];
	memset(m_buffer, 0, 10000);
}

CRingBuffer::CRingBuffer(int iSize)
	:m_bufferSize(iSize), m_front(0), m_rear(0)
{
	m_buffer = new char[iSize];
	memset(m_buffer, 0, iSize);
}


CRingBuffer::~CRingBuffer()
{
	delete[] m_buffer;
}

int	CRingBuffer::GetBufferSize(void)
{
	return m_bufferSize;
}

/////////////////////////////////////////////////////////////////////////
// 현재 사용중인 용량 얻기.
//
// Parameters: 없음.
// Return: (int)사용중인 용량.
/////////////////////////////////////////////////////////////////////////
int	CRingBuffer::GetUseSize(void)
{
	if (m_rear >= m_front)
		return m_rear - m_front;
	else
		return m_rear + (m_bufferSize - 2) - m_front;
}

/////////////////////////////////////////////////////////////////////////
// 현재 버퍼에 남은 용량 얻기.
//
// Parameters: 없음.
// Return: (int)남은용량.
/////////////////////////////////////////////////////////////////////////
int	CRingBuffer::GetFreeSize(void)
{
	return (m_bufferSize - 2) - GetUseSize();
}

/////////////////////////////////////////////////////////////////////////
// 버퍼 포인터로 외부에서 한방에 읽고, 쓸 수 있는 길이.
// (끊기지 않은 길이)
//
// Parameters: 없음.
// Return: (int)사용가능 용량.
////////////////////////////////////////////////////////////////////////
int	CRingBuffer::GetNotBrokenGetSize(void)
{
	if (m_front >= m_rear)
	{
		int endPointIndex = m_bufferSize - 2;

		return endPointIndex - m_front;
	}
	else 
		return m_rear - m_front;
}
int	CRingBuffer::GetNotBrokenPutSize(void)
{
	if (m_front <= m_rear)
	{
		int endPointIndex = m_bufferSize - 2;

		return endPointIndex - m_rear;
	}
	else
		return m_front - m_rear;
}

/////////////////////////////////////////////////////////////////////////
// WritePos 에 데이타 넣음.
//
// Parameters: (char *)데이타 포인터. (int)크기. 
// Return: (int)넣은 크기.
/////////////////////////////////////////////////////////////////////////
int	CRingBuffer::Put(char *chpData, int iSize)
{
	int inputSize = 0;

	if (m_front <= m_rear)
	{
		int pSize = GetNotBrokenPutSize();

		if (pSize < iSize)
		{
			memcpy(m_buffer + m_rear, chpData, pSize);
			chpData += pSize;
			inputSize += pSize;

			if (iSize - pSize > m_front)
			{
				// iSize - pSize = 새로 0부터 넣어야 할 데이터 크기
				memcpy(m_buffer, chpData, m_front);
				inputSize += m_front;
				m_rear = m_front;
			}
			else
			{
				memcpy(m_buffer, chpData, iSize - pSize);
				inputSize += iSize - pSize;
				m_rear = iSize - pSize;
			}
			return inputSize;
		}
		else
		{
			// 넣을수 있는 공간 >= 넣어야 하는 공간
			memcpy(m_buffer + m_rear, chpData, iSize);
			m_rear += iSize;
			return iSize;
		}
	}
	else
	{
		// rear보다 front가 큰 경우
		int pSize = m_front - m_rear;

		if (pSize < iSize)
		{
			// pSize만큼만 넣고 리턴한다.
			memcpy(m_buffer + m_rear, chpData, pSize);
			m_rear += pSize;
			return pSize;
		}
		else
		{
			// iSize 만큼 다 넣는다.
			memcpy(m_buffer + m_rear, chpData, iSize);
			m_rear += iSize;
			return iSize;
		}

	}
}

/////////////////////////////////////////////////////////////////////////
// ReadPos 에서 데이타 가져옴. ReadPos 이동.
//
// Parameters: (char *)데이타 포인터. (int)크기.
// Return: (int)가져온 크기.
/////////////////////////////////////////////////////////////////////////
int	CRingBuffer::Get(char *chpDest, int iSize)
{
	int getSize = 0;
	memset(chpDest, 0, sizeof(iSize) + 1);

	if (m_front <= m_rear)
	{
		int pSize = m_rear - m_front;

		if (pSize < iSize)
		{
			memcpy(chpDest, m_buffer + m_front, pSize);
			m_front += pSize;
			return pSize;
		}
		else
		{
			memcpy(chpDest, m_buffer + m_front, iSize);
			m_front += iSize;
			return iSize;
		}
	}
	else
	{
		int pSize = GetNotBrokenGetSize();
		int frontIndex = m_front;
		if (pSize < iSize)
		{
			memcpy(chpDest, m_buffer + m_front, pSize);
			frontIndex += pSize;
			getSize += pSize;

			if (iSize - pSize > m_rear)
			{
				// iSize - pSize = 새로 0부터 넣어야 할 데이터 크기
				memcpy(chpDest + pSize, m_buffer, m_rear);
				getSize += m_rear;
				m_front = m_rear;
			}
			else
			{
				memcpy(chpDest + pSize, m_buffer, iSize - pSize);
				getSize += iSize - pSize;
				m_front = iSize - pSize;
			}
			return getSize;
		}
		else
		{
			memcpy(chpDest, m_buffer + m_front, iSize);
			m_front += iSize;
			return iSize;
		}
	}
}

int	CRingBuffer::Peek(char *chpDest, int iSize)
{
	int getSize = 0;


	if (m_front <= m_rear)
	{
		int pSize = m_rear - m_front;

		if (pSize < iSize)
		{
			memcpy(chpDest, m_buffer + m_front, pSize);
			return pSize;
		}
		else
		{
			memcpy(chpDest, m_buffer + m_front, iSize);
			return iSize;
		}
	}
	else
	{
		int pSize = GetNotBrokenGetSize();
		int frontIndex = m_front;
		if (pSize < iSize)
		{
			memcpy(chpDest, m_buffer + m_front, pSize);
			frontIndex += pSize;
			getSize += pSize;
			chpDest += pSize;

			if (iSize - pSize > m_rear)
			{
				// iSize - pSize = 새로 0부터 넣어야 할 데이터 크기
				memcpy(chpDest, m_buffer, m_rear);
				getSize += m_rear;
			}
			else
			{
				memcpy(chpDest, m_buffer, iSize - pSize);
				getSize += iSize - pSize;
			}
			return getSize;
		}
		else
		{
			memcpy(chpDest, m_buffer + m_front, iSize);
			return iSize;
		}
	}
}

void CRingBuffer::RemoveData(int iSize)
{
	if (m_front <= m_rear)
	{
		int pSize = m_rear - m_front;

		if (pSize < iSize)
		{
			m_front += pSize;
		}
		else
		{
			m_front += iSize;
		}
	}
	else
	{
		int pSize = GetNotBrokenGetSize();
		int frontIndex = m_front;
		if (pSize < iSize)
		{
			frontIndex += pSize;

			if (iSize - pSize > m_rear)
			{
				m_front = m_rear;
			}
			else
			{
				m_front = iSize - pSize;
			}
		}
	}
}

void CRingBuffer::MoveWritePos(int iSize)
{

	if (m_front <= m_rear)
	{
		int pSize = GetNotBrokenPutSize();

		if (pSize < iSize)
		{

			if (iSize - pSize > m_front)
				m_rear = m_front;
			
			else
				m_rear = iSize - pSize;
			
		}
		else
			m_rear += iSize;
		
	}
	else
	{
		// rear보다 front가 큰 경우
		int pSize = m_front - m_rear;

		if (pSize < iSize)
			m_rear += pSize;
		else
			m_rear += iSize;
		

	}
}

void CRingBuffer::ClearBuffer(void)
{
	m_front = 0;
	m_rear = 0;
}

char* CRingBuffer::GetBufferPtr(void)
{
	return m_buffer;
}

char* CRingBuffer::GetReadBufferPtr(void)
{
	return m_buffer + m_front;
}


char* CRingBuffer::GetWriteBufferPtr(void)
{
	return m_buffer + m_rear;
}