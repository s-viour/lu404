#undef UNICODE

#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <iostream>
#include <thread>

// Need to link with Ws2_32.lib
#pragma comment (lib, "Ws2_32.lib")
// #pragma comment (lib, "Mswsock.lib")

// large buffer to allow long URLs to be read in one-go
// do we even need to read stuff?
// is the URL even part of what we read?
#define DEFAULT_BUFLEN 2048
#define DEFAULT_PORT "80"
const char* RESPONSE = "HTTP/1.1 404 Not Found";

SOCKET create_listen_socket();
void client_thread(SOCKET);
void listen_thread(SOCKET);

int main() {
  char buffer[256];
  if (GetCurrentDirectory(256, buffer) == 0) {
    std::cerr << "failed to get current directory! code: "
      << GetLastError() << '\n';
    return 1;
  }

  STARTUPINFO start_info = {0,};
  PROCESS_INFORMATION process_info = {0,};
  auto result = CreateProcess(
    NULL, "legouniverse.exe",
    NULL, NULL, false, NULL,
    NULL, NULL, &start_info, &process_info
  );
  if (!result) {
    std::cerr << "failed to start lego universe executable! code: "
      << GetLastError() << '\n';
    return 1;
  }

  WSADATA wsa_data;
  // Initialize Winsock
  auto i_result = WSAStartup(MAKEWORD(2,2), &wsa_data);
  if (i_result != 0) {
    std::cerr << "WSAStartup failed with error: " << i_result << '\n';
    return 1;
  }
  std::clog << "initialized winsock!\n";

  auto listen_socket = create_listen_socket();

  // create the listen thread
  std::thread listen(listen_thread, listen_socket);

  // wait for the process to close before cleaning up
  WaitForSingleObject(process_info.hProcess, INFINITE);
  // then close the socket, join the thread, and clean up WSA
  // this cannot be done in any other order!
  closesocket(listen_socket);
  listen.join();
  WSACleanup();
  return 0;
}

/// creates and binds a listen socket on DEFAULT_PORT
/// most of the code taken from
/// https://docs.microsoft.com/en-us/windows/win32/winsock/complete-server-code
///
SOCKET create_listen_socket() {
  int i_result;
  struct addrinfo *result = NULL;
  struct addrinfo hints;
  int send_result;

  ZeroMemory(&hints, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = IPPROTO_TCP;
  hints.ai_flags = AI_PASSIVE;

  // resolve the server address and port
  i_result = getaddrinfo(NULL, DEFAULT_PORT, &hints, &result);
  if ( i_result != 0 ) {
    std::cerr << "getaddrinfo failed with error: " << i_result << '\n';
    return NULL;
  }
  std::clog << "resolved server addr and port!\n";

  // create a SOCKET for connecting to server
  auto listen_socket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
  if (listen_socket == INVALID_SOCKET) {
    std::cerr << "socket failed with error: " << WSAGetLastError() << '\n';
    freeaddrinfo(result);
    return NULL;
  }
  std::clog << "socket created!\n";

  // Setup the TCP listening socket
  i_result = bind( listen_socket, result->ai_addr, (int)result->ai_addrlen);
  if (i_result == SOCKET_ERROR) {
    std::cerr << "bind failed with error: " << WSAGetLastError() << '\n';
    freeaddrinfo(result);
    closesocket(listen_socket);
    return NULL;
  }
  freeaddrinfo(result);
  return listen_socket;
}

/// main function for the listen thread
/// will exit on an 10004 error from WSAGetLastError() as that indicates shutdown
/// 
void listen_thread(SOCKET listen_socket) {
  int i_result;

  i_result = listen(listen_socket, SOMAXCONN);
  if (i_result == SOCKET_ERROR) {
    std::cerr << "listen failed with error: " << WSAGetLastError() << '\n';
    closesocket(listen_socket);
    return;
  }
  std::clog << "listening on port " << DEFAULT_PORT << '\n';

  // accept client sockets and spawn threads to handle them
  while (1) {
    auto client_socket = accept(listen_socket, NULL, NULL);
    if (client_socket == INVALID_SOCKET) {
      auto err = WSAGetLastError();
      if (err == 10004) {
        std::clog << "detected socket close!\n";
        break;
      }
      std::cerr << "accept failed with error: " << WSAGetLastError() << '\n';
    }
    // detach so client threads can clean up themselves
    std::thread(client_thread, client_socket).detach();
  }
  return;
}

/// main function for client threads
/// spawned by the listen thread to send and receive data ONCE
/// this function will clean itself up if detached!
///
void client_thread(SOCKET client_socket) {
  int recv_result;
  int send_result;
  char recvbuf[DEFAULT_BUFLEN];
  int recvbuflen = DEFAULT_BUFLEN;

  // it might be unnecessary to receive anything here
  // we might just be able to send the 404 response raw
  recv_result = recv(client_socket, recvbuf, recvbuflen, 0);
  if (recv_result > 0) {
    
    // send RESPONSE no matter what request we get
    send_result = send( client_socket, RESPONSE, recv_result, 0 );
    if (send_result == SOCKET_ERROR) {
      std::cerr << "send failed with error: " << WSAGetLastError() << '\n';
      closesocket(client_socket);
      return;
    }
  } else  {
    std::cerr << "recv failed with error: " << WSAGetLastError() << '\n';
    closesocket(client_socket);
    return;
  }

  // shutdown the connection since we're done
  recv_result = shutdown(client_socket, SD_SEND);
  if (recv_result == SOCKET_ERROR) {
    std::cerr << "shutdown failed with error: " << WSAGetLastError() << '\n';
  }

  // cleanup
  closesocket(client_socket);
}
