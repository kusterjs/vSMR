#pragma once
#include <curl\curl.h>
#include <curl\easy.h>
#include "bstrlib\bstrwrap.h"

class HttpHelper
{
private:
	static CBString downloadedContents;
	static size_t handle_data(void *ptr, size_t size, size_t nmemb, void *stream);

public:
	HttpHelper();
	CBString downloadStringFromURL(const char* url);
	~HttpHelper();

};
