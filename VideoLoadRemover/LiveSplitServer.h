#pragma once

#include "pch.h"
#include <cstdio>
#include <exception>
#include <thread>
#include <string>
#include <iostream>
#include <fstream>

#define DEFAULT_BUFLEN 512
class LiveSplitServer
{
private:
	bool isLoading;
	HANDLE pipe;
public:
	LiveSplitServer();
	BOOL open_pipe();
	int send_to_ls(std::string message);
	void close_pipe();
	bool get_isLoading();
	void set_isLoading(bool);

};

