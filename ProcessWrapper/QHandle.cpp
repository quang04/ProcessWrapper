#include "QHandle.h"

QHandle::QHandle() noexcept
	: m_hHandle(INVALID_HANDLE_VALUE)
{
}

QHandle::~QHandle()
{
}

constexpr QHandle::QHandle(const QHandle& other) noexcept
{
	this->m_hHandle = other.m_hHandle;

}

constexpr QHandle::QHandle(QHandle&& other) noexcept
{
	this->m_hHandle = other.m_hHandle;
	other.m_hHandle = INVALID_HANDLE_VALUE;
}

constexpr QHandle& QHandle::operator=(const QHandle& other) noexcept
{
	this->m_hHandle = other.m_hHandle;
	return *this;
}

HANDLE QHandle::operator()() const
{
	return m_hHandle;
}

HANDLE* QHandle::operator&()
{
	return &m_hHandle;
}

void QHandle::Close() const noexcept
{
	if (m_hHandle != INVALID_HANDLE_VALUE)
		CloseHandle(m_hHandle);
}

HANDLE QHandle::Detach() noexcept
{
	HANDLE previousHandle = m_hHandle;
	m_hHandle = INVALID_HANDLE_VALUE;
	return previousHandle;
}


