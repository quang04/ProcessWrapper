//--------------------------------------------
// The references
// https://learn.microsoft.com/en-us/windows/win32/procthread/creating-a-child-process-with-redirected-input-and-output?source=recommendations
//---------------------------------------------


#include <iostream>
#include <format>
#include "QProcess.h"
#include <memory>
#include <tlhelp32.h>

extern std::string utf8_encode(const std::wstring& wstr);
extern std::wstring utf8_decode(const std::string& str);

void TraceW(const std::string& data)
{
	std::wstring dataW = std::move(utf8_decode(data));
	OutputDebugStringW(dataW.c_str());
}

void TraceA(const std::string& data)
{
	OutputDebugStringA(data.c_str());
}

QProcess::QProcess(QPROCESSCONFIG config)
	: m_strFileName(std::move(config.strFileName))
	, m_strCurrentDirectory(std::move(config.strCurrentDirectory))
	, m_funcDataOut(std::move(config.stdOutFunc))
	, m_funcErrorOut(std::move(config.stdErrFunc))
	, m_bIsRedirectStdOutput(config.isRedirectStdOutput)
	, m_bIsRedirectStdError(config.isRedirectStdError)
	, m_bIsRedirectStdInput(config.isRedirectStdInput)
	, m_bIsCreateNoWindow(config.isCreateNoWindow)
	, m_strEnvironment(std::move(config.strEnvironment))
	, m_bufferSize(4096)
	, m_hChildProcess(INVALID_HANDLE_VALUE)
	, m_eventThreadStop(false)
	, m_dwChildProcessID(0)
	, m_bIsClosed(false)
{
	Open();
	AsyncRead();
}


QProcess::~QProcess()
{
	Close();
}

bool QProcess::CreateChildProcess(HANDLE hStdOut, HANDLE hStdIn, HANDLE hStdErr)
{
	PROCESS_INFORMATION pi;
	STARTUPINFO si;

	ZeroMemory(&pi, sizeof(PROCESS_INFORMATION));
	ZeroMemory(&si, sizeof(STARTUPINFO));
	si.cb = sizeof(STARTUPINFO);
	si.hStdOutput = hStdOut;
	si.hStdInput = hStdIn;
	si.hStdError = hStdErr;

	if (m_bIsRedirectStdInput ||
		m_bIsRedirectStdOutput ||
		m_bIsRedirectStdError)
	{
		si.dwFlags |= STARTF_USESTDHANDLES;
	}

	DWORD creationFlags = 0;
	if (m_bIsCreateNoWindow)
		creationFlags |= CREATE_NO_WINDOW;

#ifdef UNICODE
	creationFlags |= CREATE_UNICODE_ENVIRONMENT;
#endif


	if (!CreateProcess(nullptr,
		m_strFileName.data(),
		nullptr,
		nullptr,
		TRUE,
		creationFlags,
		m_strEnvironment.empty() ? nullptr : m_strEnvironment.data(),
		nullptr,
		&si,
		&pi))
	{
		PrintError("CreateProcess");
		return false;
	}

	//Store value
	m_hChildProcess.store(pi.hProcess);
	m_dwChildProcessID = pi.dwProcessId;
	CloseHandle(pi.hThread);

	return true;
}

bool QProcess::Write(const char* byte, const size_t& length) const
{
	DWORD dwWritten = 0;
	BOOL bSucess = FALSE;

	bSucess = WriteFile(m_hStdinWrite(),
		byte,
		static_cast<DWORD>(length),
		&dwWritten,
		nullptr);

	if (!bSucess || dwWritten == 0)
		return false;

	return true;
}

void QProcess::AsyncRead()
{
	if (!m_bIsRedirectStdOutput &&
		!m_bIsRedirectStdError) return;

	if (m_hStdoutRead() == INVALID_HANDLE_VALUE &&
		m_hStdErrRead() == INVALID_HANDLE_VALUE) return;

	//Check function empty or not
	if (m_funcDataOut == nullptr &&
	    m_funcErrorOut == nullptr) return;

	m_threadStdOut = std::thread([this]() {

			int nRet;

			for (;;)
			{
				nRet = Read();

				if (nRet <= 0) break;

				//Event thread stop was signalled
				if (m_eventThreadStop)
				{
					break;
				}

				//The child process ended
				if (m_hChildProcess == INVALID_HANDLE_VALUE)
				{
					//Read the left byte
					nRet = Read();
					if (nRet > 0) nRet = 0;
					break;
				}

				//Avoid overheat
				Sleep(5);

			}
		});

}

int QProcess::Read()
{
	DWORD dwAvail = 0;
	size_t dwDataSize = 0;

	for (;;)
	{
		//Read dataout
		if (m_bIsRedirectStdOutput)
		{
			dwAvail = 0;
			dwDataSize = 0;

			//Check how many byte left in pipe
			if (!::PeekNamedPipe(m_hStdoutRead(), NULL, 0, NULL,
				&dwAvail, NULL))
			{
				PrintError("PeekNamedPipe");
				break;
			}

			if (!dwAvail)
				return 1; // Not data available

			dwDataSize = min(m_bufferSize, dwAvail);
			std::unique_ptr<char[]> buffer(new char[dwDataSize]);

			DWORD dwRead = 0;
			if (!ReadFile(m_hStdoutRead(),
				static_cast<char*>(buffer.get()),
				static_cast<DWORD>(dwDataSize),
				&dwRead,
				nullptr))
			{
				PrintError("ReadFile");
				break;
			}

			//Call back
			if(m_funcDataOut != nullptr)
				m_funcDataOut(buffer.get(), static_cast<size_t>(dwDataSize));
		}

		//Read errorout
		if (m_bIsRedirectStdError)
		{
			dwAvail = 0;
			dwDataSize = 0;

			//Check how many byte left in pipe
			if (!::PeekNamedPipe(m_hStdErrRead(), NULL, 0, NULL,
				&dwAvail, NULL))
			{
				PrintError("PeekNamedPipe");
				break;
			}

			if (!dwAvail)
				return 1; // Not data available

			dwDataSize = min(m_bufferSize, dwAvail);
			std::unique_ptr<char[]> buffer(new char[dwDataSize]);

			DWORD dwRead = 0;
			if (!ReadFile(m_hStdErrRead(),
				static_cast<char*>(buffer.get()),
				static_cast<DWORD>(dwDataSize),
				&dwRead,
				nullptr))
			{
				PrintError("ReadFile");
				break;
			}

			//Call back
			if(m_funcErrorOut != nullptr)
				m_funcErrorOut(buffer.get(), static_cast<size_t>(dwDataSize));
		}
	}

	DWORD dwError = ::GetLastError();
	if (dwError == ERROR_BROKEN_PIPE ||	// pipe has been ended
		dwError == ERROR_NO_DATA)		// pipe closing in progress
	{
		PrintError("Child Process End");
		return 0;	// child process ended
	}

	PrintError("Read stdout pipe error");
	return -1;
}

bool QProcess::Open()
{
	//Create 3 anonymous pipe.
	//Pipe In, Out and Err
	HANDLE hParentStdInWrite = INVALID_HANDLE_VALUE;	//Parent stdin write handle
	HANDLE hParentStdOutRead = INVALID_HANDLE_VALUE;	//Parent stdout read handle
	HANDLE hParentStdErrRead = INVALID_HANDLE_VALUE;	//Parent stderr read handle

	HANDLE hChildStdInRead	 = INVALID_HANDLE_VALUE;	//Child stdin read handle
	HANDLE hChildStdOutWrite = INVALID_HANDLE_VALUE;	//Child stdout write handle
	HANDLE hChildStdErrWrite = INVALID_HANDLE_VALUE;	//Child stderr write handle


	SECURITY_ATTRIBUTES sa;

	// Set up the security attributes struct.
	sa.nLength = sizeof(SECURITY_ATTRIBUTES);
	sa.lpSecurityDescriptor = nullptr;
	sa.bInheritHandle = TRUE;


	BOOL bOK = FALSE;

	//Create pipe
	//using __try __finally for stack unwinding
	__try
	{
		//Pipe Out
		if (m_bIsRedirectStdOutput)
		{
			if (!CreatePipe(&hParentStdOutRead, &hChildStdOutWrite, &sa, 0))
			{
				PrintError("CreatePipe");
				__leave;
			}


			// Set the inheritance properties to FALSE. Otherwise, the child
			// inherits the these handles; resulting in non-closeable
			// handles to the pipes being created.
			if (!DuplicateHandle(GetCurrentProcess(),
				hParentStdOutRead,
				GetCurrentProcess(),
				&m_hStdoutRead,
				0,
				FALSE,
				DUPLICATE_SAME_ACCESS))
			{
				PrintError("DuplicateHandle");
				__leave;
			}

			// Close inheritable copies of the handles we do not want to
			// be inherited.
			DestroyHandle(std::move(hParentStdOutRead));

		}

		//Pipe In
		if (m_bIsRedirectStdInput)
		{
			if (!CreatePipe(&hChildStdInRead, &hParentStdInWrite, &sa, 0))
			{
				PrintError("CreatePipe");
				__leave;
			}


			// Set the inheritance properties to FALSE. Otherwise, the child
			// inherits the these handles; resulting in non-closeable
			// handles to the pipes being created.
			if (!DuplicateHandle(GetCurrentProcess(),
				hParentStdInWrite,
				GetCurrentProcess(),
				&m_hStdinWrite,
				0,
				FALSE,
				DUPLICATE_SAME_ACCESS))
			{
				PrintError("DuplicateHandle");
				__leave;
			}

			// Close inheritable copies of the handles we do not want to
			// be inherited.
			DestroyHandle(std::move(hParentStdInWrite));
		}

		//Pipe Error
		if (m_bIsRedirectStdError)
		{
			if (!CreatePipe(&hParentStdErrRead, &hChildStdErrWrite, &sa, 0))
			{
				PrintError("CreatePipe");
				__leave;
			}


			// Set the inheritance properties to FALSE. Otherwise, the child
			// inherits the these handles; resulting in non-closeable
			// handles to the pipes being created.
			if (!DuplicateHandle(GetCurrentProcess(),
				hParentStdErrRead,
				GetCurrentProcess(),
				&m_hStdErrRead,
				0,
				FALSE,
				DUPLICATE_SAME_ACCESS))
			{
				PrintError("DuplicateHandle");
				__leave;
			}

			// Close inheritable copies of the handles we do not want to
			// be inherited.
			DestroyHandle(std::move(hParentStdErrRead));
		}

		if (!CreateChildProcess(hChildStdOutWrite, hChildStdInRead, hChildStdErrWrite))
		{
			PrintError("CreateChild");
			__leave;
		}

		//// Child is created. Close the parents copy of those pipe
		//// handles that only the child should have open.
		//// Make sure that no handles to the write end of the stdout pipe
		//// are maintained in this process or else the pipe will not
		//// close when the child process exits and ReadFile will hang.
		DestroyHandle(std::move(hChildStdOutWrite));
		DestroyHandle(std::move(hChildStdInRead));
		DestroyHandle(std::move(hChildStdErrWrite));

		bOK = TRUE;

	}
	__finally
	{
		//Error destroy everything
		if (!bOK)
		{
			DestroyHandle(std::move(hParentStdOutRead));
			DestroyHandle(std::move(hParentStdErrRead));
			DestroyHandle(std::move(hParentStdInWrite));

			DestroyHandle(std::move(hChildStdInRead));
			DestroyHandle(std::move(hChildStdOutWrite));
			DestroyHandle(std::move(hChildStdErrWrite));
			Close();
			return false;
		}
	}
	return true;
}

void QProcess::Close()
{
	if (m_bIsClosed) return;

	m_bIsClosed = true;
	//Set at atomic for stopping thread
	m_eventThreadStop = true;

	//Wait for thread completely exist
	//Maybe need wait time out at here to avoid deadlock
	if (m_threadStdOut.joinable())
		m_threadStdOut.join();

	m_hStdErrRead.Close();
	m_hStdinWrite.Close();
	m_hStdoutRead.Close();

}

void QProcess::Kill() const
{
	if (m_dwChildProcessID == 0) return;
	if (m_bIsClosed) return;

	auto snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

	if (snapshot != INVALID_HANDLE_VALUE)
	{
		PROCESSENTRY32 process;
		ZeroMemory(&process, sizeof(process));
		process.dwSize = sizeof(process);

		if (Process32First(snapshot, &process))
		{
			do
			{
				if (process.th32ParentProcessID == m_dwChildProcessID)
				{
					HANDLE process_handle = OpenProcess(PROCESS_TERMINATE, FALSE, process.th32ProcessID);
					if (process_handle) {
						TerminateProcess(process_handle, 2);
						CloseHandle(process_handle);
					}
				}
			} while (Process32Next(snapshot, &process));
		}
		CloseHandle(snapshot);
	}
	else
	{
		TerminateProcess(m_hChildProcess, 2);
	}

}

void QProcess::WriteCommand(const std::string& strCommand)
{
	std::string newCommand = strCommand + "\r\n";

	if (!Write(newCommand.c_str(), newCommand.size()))
		PrintError("");
}

std::string QProcess::ReadLineDataOut()
{
	//Sleep Xms at here to make sure "data avaiable at pipe"
	//without sleep can not get data out
	//This number may vary
	//TODO: Need find better solution at here
	Sleep(40);

	DWORD dwAvail = 0;

	//Check how many byte left in pipe
	if (!::PeekNamedPipe(m_hStdoutRead(),
		NULL,
		0,
		NULL,
		&dwAvail,
		NULL))
	{
		PrintError("PeekNamedPipe");
		return "";
	}

	if (!dwAvail)
		return ""; // Not data available

	std::unique_ptr<char[]> buffer(new char[dwAvail]);

	DWORD dwRead = 0;
	if (!ReadFile(m_hStdoutRead(),
		static_cast<char*>(buffer.get()),
		dwAvail,
		&dwRead,
		nullptr))
	{
		PrintError("ReadFile");
		return "";
	}

	return std::string(buffer.get(), static_cast<size_t>(dwAvail));
}

void QProcess::DestroyHandle(HANDLE&& rhObject)
{
	if (rhObject == INVALID_HANDLE_VALUE) return;

	::CloseHandle(rhObject);
	rhObject = INVALID_HANDLE_VALUE;

}

void QProcess::PrintError(const char* mess, const std::source_location& location)
{
	std::string dataFormat = std::format("Error. Message: {}. Function: {}. Line: {}",
		mess,
		location.function_name(),
		location.line());

	std::cout << dataFormat << std::endl;

	TRACE_ERROR(dataFormat);
}
