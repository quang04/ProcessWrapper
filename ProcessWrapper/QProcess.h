#pragma once
#include <string>
#include <functional>
#include <atomic>
#include <thread>
#include <source_location>
#include <Windows.h>
#include "QHandle.h"

void TraceW(const std::string& data);
void TraceA(const std::string& data);

#ifdef  UNICODE
typedef std::wstring QString;
#define TRACE_ERROR TraceW
#else
typedef std::string QString;
#define TRACE_ERROR TraceA
#endif

typedef std::function<void(const char* byteData, const size_t& sizeData)> processFuncDataOutCallBack;


typedef struct _QPROCESSCONFIG {
	QString strFileName;
	QString strCurrentDirectory;
	processFuncDataOutCallBack stdOutFunc;
	processFuncDataOutCallBack stdErrFunc;
	bool isRedirectStdOutput;
	bool isRedirectStdError;
	bool isRedirectStdInput;
	bool isCreateNoWindow;
	QString strEnvironment;

public:
#ifdef UNICODE
	_QPROCESSCONFIG(QString fileName,
		QString currentDirectory = L"",
		processFuncDataOutCallBack outFunc = nullptr,
		processFuncDataOutCallBack errFunc = nullptr,
		bool isRedirectStdOutput = true,
		bool isRedirectStdError = true,
		bool isRedirectStdInput = true,
		bool isCreateNoWindow = true,
		QString strEnvironment = L"")
		: strFileName(fileName)
		, strCurrentDirectory(currentDirectory)
		, stdOutFunc(std::move(outFunc))
		, stdErrFunc(std::move(errFunc))
		, isRedirectStdOutput(isRedirectStdOutput)
		, isRedirectStdError(isRedirectStdError)
		, isRedirectStdInput(isRedirectStdInput)
		, isCreateNoWindow(isCreateNoWindow)
		, strEnvironment(strEnvironment)
	{
	}
#else
	_QPROCESSCONFIG(QString fileName,
		QString currentDirectory = "",
		processFuncDataOutCallBack outFunc = nullptr,
		processFuncDataOutCallBack errFunc = nullptr,
		bool isRedirectStdOutput = true,
		bool isRedirectStdError = true,
		bool isRedirectStdInput = true,
		bool isCreateNoWindow = true,
		QString strEnvironment = "")
		: strFileName(fileName)
		, strCurrentDirectory(currentDirectory)
		, stdOutFunc(std::move(outFunc))
		, stdErrFunc(std::move(errFunc))
		, isRedirectStdOutput(isRedirectStdOutput)
		, isRedirectStdError(isRedirectStdError)
		, isRedirectStdInput(isRedirectStdInput)
		, isCreateNoWindow(isCreateNoWindow)
		, strEnvironment(strEnvironment)
	{
	}
#endif



}QPROCESSCONFIG, *PQPROCESSCONFIG;

class QProcess
{
public:
	QProcess(QPROCESSCONFIG config);
	//Rule of five
	QProcess(const QProcess& other) = delete;
	const QProcess operator=(const QProcess& other) = delete;
	QProcess(QProcess&& other) = delete;
	const QProcess operator=(QProcess&& other) = delete;
	virtual ~QProcess();
protected:
	QHandle m_hStdinWrite;
	QHandle m_hStdoutRead;
	QHandle m_hStdErrRead;
private:
	/// <summary>
	/// Callback dataout function
	/// </summary>
	processFuncDataOutCallBack m_funcDataOut;
	processFuncDataOutCallBack m_funcErrorOut;

	/// <summary>
	/// Process configuration
	/// </summary>
	bool m_bIsRedirectStdOutput;
	bool m_bIsRedirectStdError;
	bool m_bIsRedirectStdInput;
	bool m_bIsCreateNoWindow;
	QString m_strFileName;
	QString m_strCurrentDirectory;
	QString m_strEnvironment;
	/// <summary>
	/// Buffer receive from pipe. Default 4096
	/// </summary>
	const std::size_t m_bufferSize;

	/// <summary>
	/// Event notify thread
	/// </summary>
	std::atomic_bool m_eventThreadStop;	//Notify to stop thread
	std::thread m_threadStdOut;			//Thread handler

	/// <summary>
	/// Handle of child process
	/// </summary>
	std::atomic<HANDLE> m_hChildProcess;
	DWORD m_dwChildProcessID;

	/// <summary>
	/// Indicate process closed or not
	/// Set only at constructor and Close() function
	/// </summary>
	bool m_bIsClosed;
private:
	/// <summary>
	/// Close handler
	/// </summary>
	/// <param name="rhObject"></param>
	void DestroyHandle(HANDLE&& rhObject);

	/// <summary>
	/// Print error utility
	/// </summary>
	/// <param name="mess"></param>
	/// <param name="location"></param>
	void PrintError(const char* mess, const std::source_location& location = std::source_location::current());

	/// <summary>
	/// Create child process
	/// </summary>
	/// <param name="hStdOut"></param>
	/// <param name="hStdIn"></param>
	/// <param name="hStdErr"></param>
	/// <returns></returns>
	bool CreateChildProcess(HANDLE hStdOut, HANDLE hStdIn, HANDLE hStdErr);

	/// <summary>
	/// Write to pipe
	/// </summary>
	/// <param name="byte"></param>
	/// <param name="length"></param>
	/// <returns></returns>
	bool Write(const char* byte, const size_t& length) const;

	/// <summary>
	/// Start thread reading data out from pipe
	/// </summary>
	void AsyncRead();

	/// <summary>
	/// Read data
	/// </summary>
	/// <returns></returns>
	int Read();

	/// <summary>
	/// Entry point
	/// </summary>
	bool Open();


public:

	/// <summary>
	/// End point
	/// </summary>
	void Close();

	/// <summary>
	/// Force kill process
	/// </summary>
	void Kill() const;

	/// <summary>
	/// Write to process
	/// </summary>
	void WriteCommand(const std::string& strCommand);

	std::string ReadLineDataOut();
};