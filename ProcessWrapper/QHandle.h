#pragma once
#include <Windows.h>


class QHandle
{
public:
	QHandle() noexcept;
	virtual ~QHandle();
	constexpr QHandle(const QHandle& other) noexcept;
	constexpr QHandle(QHandle&& other) noexcept;
	constexpr QHandle& operator=(const QHandle& other) noexcept;
	HANDLE operator()() const;
	HANDLE* operator&();
public:
	void Close() const noexcept;
	HANDLE Detach() noexcept;
	inline void Set(const HANDLE& h)
	{
		m_hHandle = h;
	}
private:
	HANDLE m_hHandle;
};