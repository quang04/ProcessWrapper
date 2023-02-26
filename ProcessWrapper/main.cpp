#include <iostream>
#include "QProcess.h"

void DataOut(const char* data, const size_t& size)
{
	std::cout << "Data Out: " << std::string(data, size) << std::endl;
}
void ErrorOut(const char* data, const size_t& size)
{
	std::cout << "Error Out: " << std::string(data, size) << std::endl;
}

void Test1()
{
	QPROCESSCONFIG config = QPROCESSCONFIG("cmd", "", DataOut,
		ErrorOut);

	QProcess* pythonProcess = new QProcess(config);

	pythonProcess->WriteCommand("python --version");
	// Do your stuff
	Sleep(1000);

	//
	pythonProcess->Close();
	delete pythonProcess;
}

void Test2()
{
	QPROCESSCONFIG config = QPROCESSCONFIG("cmd");

	QProcess* cmdProcess = new QProcess(config);
	std::string dataOut = "";

	dataOut = cmdProcess->ReadLineDataOut();
	std::cout << dataOut << std::endl;

	//cd to C drive. need /d in case from another drive
	cmdProcess->WriteCommand("cd /d C:");
	dataOut = cmdProcess->ReadLineDataOut();
	std::cout << dataOut << std::endl;

	cmdProcess->WriteCommand("dir");
	dataOut = cmdProcess->ReadLineDataOut();
	std::cout << dataOut << std::endl;

	cmdProcess->Close();
	delete cmdProcess;
}

void Test3()
{
	QPROCESSCONFIG config = QPROCESSCONFIG("cmd", "", DataOut,
		ErrorOut);

	QProcess* cmdProcess = new QProcess(config);
	cmdProcess->WriteCommand("cd /d D:\\Venv_Folder\\virtual_env_icodi");
	cmdProcess->WriteCommand("Scripts\\activate");
	cmdProcess->WriteCommand("pip freeze");

	//When done your work
	//Delete at here
	//cmdProcess->Close();
	//delete cmdProcess;
}
void Test4()
{
	QPROCESSCONFIG config = QPROCESSCONFIG("cmd",
		"",
		[](const char* data, const size_t& size) {

			std::cout << "Lambda Data Out: " << std::string(data, size) << std::endl;
		},
		[](const char* data, const size_t& size) {
			std::cout << "Lambda Error Out: " << std::string(data, size) << std::endl;
		});

	QProcess* cmdProcess = new QProcess(config);
	cmdProcess->WriteCommand("cd /d D:\\Venv_Folder\\virtual_env_icodi");
	cmdProcess->WriteCommand("Scripts\\activate");
	cmdProcess->WriteCommand("pip freeze");

	//When done your work
	//Delete at here
	//cmdProcess->Close();
	//delete cmdProcess;
}
int main(void)
{
	Test1();
	Test2();
	Test3();
	Test4();

	std::getchar();
	return 0;
}