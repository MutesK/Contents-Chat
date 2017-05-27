#include "SerializeBuffer.h"



CSerializeBuffer::CSerializeBuffer()
{
	m_chpBuffer = new char[eBUFFER_DEFAULT];
	m_iDataSize = 0;
	m_iBufferSize = eBUFFER_DEFAULT;

	m_chpReadPos = m_chpBuffer;
	m_chpWritePos = m_chpBuffer;
}

CSerializeBuffer::CSerializeBuffer(int iBufferSize)
{
	m_chpBuffer = new char[iBufferSize];
	m_iDataSize = 0;
	m_iBufferSize = iBufferSize;


	m_chpReadPos = m_chpBuffer;
	m_chpWritePos = m_chpBuffer;
}

CSerializeBuffer::~CSerializeBuffer()
{
	Release();
}


void CSerializeBuffer::Release(void)
{
	delete[] m_chpBuffer;

	m_chpReadPos = nullptr;
	m_chpWritePos = nullptr;
	m_iDataSize = 0;
	m_iBufferSize = 0;
}
void	CSerializeBuffer::Clear(void)
{
	m_chpReadPos = m_chpBuffer;
	m_chpWritePos = m_chpBuffer;

	m_iDataSize = 0;
}

int	CSerializeBuffer::MoveWritePos(int iSize)
{
	int Usage = m_iBufferSize - (m_chpWritePos - m_chpReadPos);
	int CopiedSize = min(iSize, Usage);
	m_chpWritePos += CopiedSize;
	m_iDataSize += CopiedSize;
	return CopiedSize;
}

int	CSerializeBuffer::MoveReadPos(int iSize)
{
	int Usage = m_chpWritePos - m_chpReadPos;
	int CopiedSize = min(iSize,  Usage);
	m_chpReadPos += CopiedSize;
	m_iDataSize += CopiedSize;

	return CopiedSize;
}

int	CSerializeBuffer::GetData(char *chpDest, int iSize)
{
	int Usage = m_chpWritePos - m_chpReadPos;
	if (Usage == 0)
		return 0;

	int GetDataSize = min(iSize, Usage);

	memcpy_s(chpDest, GetDataSize, m_chpReadPos, GetDataSize);

	m_chpReadPos += GetDataSize;
	m_iDataSize -= GetDataSize;
	return GetDataSize;
}

int	CSerializeBuffer::PutData(char *chpSrc, int iSrcSize)
{
	int Usage = m_iBufferSize - (m_chpWritePos - m_chpReadPos);
	int CopiedSize;

	if (Usage == m_iBufferSize)
		CopiedSize = min(iSrcSize, m_iBufferSize);
	else 
		CopiedSize = min(iSrcSize, Usage);

	memcpy_s(m_chpWritePos, CopiedSize, chpSrc, CopiedSize);
	m_chpWritePos += CopiedSize;
	m_iDataSize += CopiedSize;
	return CopiedSize;
}

CSerializeBuffer& CSerializeBuffer::operator=(CSerializeBuffer &clSrCSerializeBuffer)
{
	this->m_iDataSize = clSrCSerializeBuffer.m_iDataSize;
	this->m_iBufferSize = clSrCSerializeBuffer.m_iBufferSize;

	this->m_chpBuffer = new char[m_iBufferSize];
	
	int ReadIndex = clSrCSerializeBuffer.m_chpReadPos - clSrCSerializeBuffer.m_chpBuffer;
	this->m_chpReadPos = m_chpBuffer + ReadIndex;

	int WriteIndex = clSrCSerializeBuffer.m_chpWritePos - clSrCSerializeBuffer.m_chpBuffer;
	this->m_chpWritePos = m_chpBuffer + WriteIndex;

	return *this;
}

CSerializeBuffer& CSerializeBuffer::operator<<(BYTE byValue)
{
	PutData((char *)&byValue, 1);
	return *this;
}

CSerializeBuffer& CSerializeBuffer::operator<<(char chValue)
{
	PutData((char *)&chValue, 1);
	return *this;
}

CSerializeBuffer	&CSerializeBuffer::operator << (short shValue)
{
	PutData((char *)&shValue, 2);
	return *this;
}
CSerializeBuffer	&CSerializeBuffer::operator << (WORD wValue)
{
	PutData((char *)&wValue, 2);
	return *this;
}

CSerializeBuffer	&CSerializeBuffer::operator << (int iValue)
{
	PutData((char *)&iValue, 4);
	return *this;
}
CSerializeBuffer	&CSerializeBuffer::operator << (DWORD dwValue)
{
	PutData((char *)&dwValue, 4);
	return *this;
}
CSerializeBuffer	&CSerializeBuffer::operator << (float fValue)
{
	floatdata f;
	f.a= fValue;

	PutData(f.value, 4);
	return *this;
}

CSerializeBuffer	&CSerializeBuffer::operator << (__int64 iValue)
{
	PutData((char *)&iValue, 8);
	return *this;
}
CSerializeBuffer	&CSerializeBuffer::operator << (double dValue)
{
	doubledata d;
	d.a = dValue;

	PutData(d.value, 8);
	return *this;
}

CSerializeBuffer	&CSerializeBuffer::operator >> (BYTE &byValue)
{
	GetData((char *)&byValue, 1);
	return *this;
}
CSerializeBuffer	&CSerializeBuffer::operator >> (char &chValue)
{
	GetData((char *)&chValue, 1);
	return *this;
}

CSerializeBuffer	&CSerializeBuffer::operator >> (short &shValue)
{
	GetData((char *)&shValue, 2);
	return *this;
}
CSerializeBuffer	&CSerializeBuffer::operator >> (WORD &wValue)
{
	GetData((char *)&wValue, 2);
	return *this;
}

CSerializeBuffer	&CSerializeBuffer::operator >> (int &iValue)
{
	GetData((char *)&iValue, 4);
	return *this;
}
CSerializeBuffer	&CSerializeBuffer::operator >> (DWORD &dwValue)
{
	GetData((char *)&dwValue, 4);
	return *this;
}
CSerializeBuffer	&CSerializeBuffer::operator >> (float &fValue)
{
	floatdata f;
	GetData(f.value, 4);

	fValue = f.a;
	return *this;
}

CSerializeBuffer	&CSerializeBuffer::operator >> (__int64 &iValue)
{
	GetData((char *)&iValue, 8);
	return *this;
}
CSerializeBuffer	&CSerializeBuffer::operator >> (double &dValue)
{
	doubledata d;
	GetData(d.value, 8);

	dValue = d.a;

	return *this;
}