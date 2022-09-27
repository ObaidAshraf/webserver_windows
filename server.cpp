#ifndef UNICODE
#define UNICODE
#endif

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <http.h>
#include <stdio.h>

#pragma comment(lib, "httpapi.lib")

using namespace std;

#define INITIALIZE_HTTP_RESPONSE( resp, status, reason )    \
    do                                                      \
    {                                                       \
        RtlZeroMemory( (resp), sizeof(*(resp)) );           \
        (resp)->StatusCode = (status);                      \
        (resp)->pReason = (reason);                         \
        (resp)->ReasonLength = (USHORT) strlen(reason);     \
    } while (FALSE)

#define ADD_KNOWN_HEADER(Response, HeaderId, RawValue)               \
    do                                                               \
    {                                                                \
        (Response).Headers.KnownHeaders[(HeaderId)].pRawValue =      \
                                                          (RawValue);\
        (Response).Headers.KnownHeaders[(HeaderId)].RawValueLength = \
            (USHORT) strlen(RawValue);                               \
    } while(FALSE)

#define ALLOC_MEM(cb) HeapAlloc(GetProcessHeap(), 0, (cb))

#define FREE_MEM(ptr) HeapFree(GetProcessHeap(), 0, (ptr))

DWORD DoReceiveRequests(HANDLE hReqQueue);
DWORD SendHttpResponse(IN HANDLE hReqQueue, IN PHTTP_REQUEST pRequest, IN USHORT StatusCode, IN PSTR pReason, IN PSTR pEntity);
DWORD RenderHomePage(IN HANDLE hReqQueue, IN PHTTP_REQUEST pRequest);
DWORD RenderPrintResponse(IN HANDLE hReqQueue, IN PHTTP_REQUEST pRequest, bool success);

int main(int argc, char** argv) {
    ULONG           retCode;
    HANDLE          hReqQueue = NULL;
    HTTPAPI_VERSION HttpApiVersion = HTTPAPI_VERSION_2;
    HTTP_SERVER_SESSION_ID ssID = HTTP_NULL_ID;

    retCode = HttpInitialize(HttpApiVersion, HTTP_INITIALIZE_SERVER, NULL);
    if (retCode != NO_ERROR) {
        wprintf(L"Failed to initialize HTTP. %lu\n", retCode);
        return retCode;
    }

    // Create a Request Queue Handle
    retCode = HttpCreateHttpHandle( &hReqQueue, 0);
    if (retCode != NO_ERROR) {
        wprintf(L"HttpCreateHttpHandle failed with %lu \n", retCode);
        goto CleanUp;
    }

    retCode = HttpAddUrl(hReqQueue, L"http://localhost:8080/", NULL);
    if (retCode != NO_ERROR) {
        wprintf(L"HttpAddUrl failed with %lu \n", retCode);
        goto CleanUp;
    }

    DoReceiveRequests(hReqQueue);

CleanUp:
    HttpRemoveUrl(hReqQueue, L"http://localhost:8080/");
    HttpTerminate(HTTP_INITIALIZE_SERVER, NULL);

	return 0;
}

DWORD DoReceiveRequests(IN HANDLE hReqQueue) {
    ULONG              result, resultPrinter;
    HTTP_REQUEST_ID    requestId;
    DWORD              bytesRead;
    PHTTP_REQUEST      pRequest;
    PCHAR              pRequestBuffer;
    ULONG              RequestBufferLength;

    RequestBufferLength = sizeof(HTTP_REQUEST) + 2048;
    pRequestBuffer = (PCHAR)ALLOC_MEM(RequestBufferLength);

    if (pRequestBuffer == NULL)
    {
        return ERROR_NOT_ENOUGH_MEMORY;
    }

    pRequest = (PHTTP_REQUEST)pRequestBuffer;

    HTTP_SET_NULL_ID(&requestId);

    for (;;) {
        RtlZeroMemory(pRequest, RequestBufferLength);

        result = HttpReceiveHttpRequest(
            hReqQueue,          // Req Queue
            requestId,          // Req ID
            0,                  // Flags
            pRequest,           // HTTP request buffer
            RequestBufferLength,// req buffer length
            &bytesRead,         // bytes received
            NULL                // LPOVERLAPPED
        );

        if (result == NO_ERROR) {
            switch (pRequest->Verb)
            {
                case HttpVerbGET: {
                    if (pRequest->CookedUrl.pQueryString == NULL)
                        RenderHomePage(hReqQueue, pRequest);
                    else {
                        // TODO: Thermal Printer code goes here!
                        resultPrinter = true;
                        if (resultPrinter)
                            RenderPrintResponse(hReqQueue, pRequest, true);
                        else
                            RenderPrintResponse(hReqQueue, pRequest, false);
                    }
                    break;
                }
                default: {
                    result = SendHttpResponse(hReqQueue, pRequest, 200, PSTR("OK"), PSTR("Unknown\r\n"));
                    break;
                }
            }
        }
    }

    if (pRequestBuffer)
    {
        FREE_MEM(pRequestBuffer);
    }

    return result;
}

DWORD SendHttpResponse(
    IN HANDLE        hReqQueue,
    IN PHTTP_REQUEST pRequest,
    IN USHORT        StatusCode,
    IN PSTR          pReason,
    IN PSTR          pEntityString
)
{
    HTTP_RESPONSE   response;
    HTTP_DATA_CHUNK dataChunk;
    DWORD           result;
    DWORD           bytesSent;

    INITIALIZE_HTTP_RESPONSE(&response, StatusCode, pReason);
    ADD_KNOWN_HEADER(response, HttpHeaderContentType, "text/html");

    if (pEntityString)
    {
        dataChunk.DataChunkType = HttpDataChunkFromMemory;
        dataChunk.FromMemory.pBuffer = pEntityString;
        dataChunk.FromMemory.BufferLength = (ULONG)strlen(pEntityString);
        response.EntityChunkCount = 1;
        response.pEntityChunks = &dataChunk;
    }

    result = HttpSendHttpResponse(
        hReqQueue,           // ReqQueueHandle
        pRequest->RequestId, // Request ID
        0,                   // Flags
        &response,           // HTTP response
        NULL,                // pReserved1
        &bytesSent,          // bytes sent  (OPTIONAL)
        NULL,                // pReserved2  (must be NULL)
        0,                   // Reserved3   (must be 0)
        NULL,                // LPOVERLAPPED(OPTIONAL)
        NULL                 // pReserved4  (must be NULL)
    );

    if (result != NO_ERROR)
    {
        wprintf(L"HttpSendHttpResponse failed with %lu \n", result);
    }

    return result;
}

DWORD RenderHomePage(IN HANDLE hReqQueue, IN PHTTP_REQUEST pRequest) {
    DWORD result = SendHttpResponse(hReqQueue, pRequest, 200, PSTR("OK"), PSTR("\
        <HTML> \
        <HEAD> \
        <TITLE>Receipt</TITLE> \
        <META name = 'description' content = ''> \
        <META name = 'keywords' content = ''> \
        <META name = 'generator' content = 'CuteHTML'></head> \
        <link rel='shortcut icon' href='https://www.eziaccounts.net.au/images/favicon.ico' type='image/x-icon'>\
        <BODY BGCOLOR = '#FFFFFF' TEXT = '#000000' LINK = '#0000FF' VLINK = '#800080' style = 'padding:20px'> \
        <H1>Send Receipt to Print</H1> \
        <form method='get' name = 'form' action='/'> \
        <input type = 'hidden' name = 'account' id = 'account' value = 'CASHS' /> \
        <b>Printer Name : </b><input type = 'text' class = 'input' name = 'PrinterName' id = 'PrinterName' size = '11' maxlength = '11' value = 'Epson 80'> \
        <p> \
        <b>Sale No #  : </b><input type = 'text' class = 'input' name = 'saleno' id = 'saleno' size = '11' maxlength = '11' value = ''> \
        <p> \
        Text \
        <textarea name = 'text' id = 'text'> This is a receipt </textarea> \
        <p> \
        <input class = 'upper but' type = 'submit' id = 'submitbut' value = ' Print Receipt ' title = 'Update'  tabIndex = '1' /> \
        <br><br><hr> \
        Submitting this form to an installed program files 'receipt.exe' file will print receipt using locally installed thermal printer. \
        </form> \
        </BODY> \
        </HTML> \
        \r\n"));

    return result;
}


DWORD RenderPrintResponse(IN HANDLE hReqQueue, IN PHTTP_REQUEST pRequest, bool success) {
    DWORD result;
    if (success) {
        result = SendHttpResponse(hReqQueue, pRequest, 200, PSTR("OK"), PSTR("Success\r\n"));
    }
    else {
        result = SendHttpResponse(hReqQueue, pRequest, 200, PSTR("OK"), PSTR("Failure\r\n"));
    }

    return result;
}