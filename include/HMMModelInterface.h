// #include <iostream>
// #include <cstdio>
// #include <memory>
// #include <stdexcept>
// #include <string>
// // Windows headers must be included in this specific order
// #include <WinSock2.h>
// #include <ws2tcpip.h>
// #include <windows.h>
// #include "Utils.h"

// #pragma comment(lib, "ws2_32.lib")

// namespace HMMModelInterface {
    
//     // Check if the server is running, and if not, start it
//     bool ensure_server_running() {
//         // Check if socket can connect to the server
//         WSADATA wsaData;
//         if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
//             Utils::logMessage("WSAStartup failed");
//             return false;
//         }

//         SOCKET ConnectSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
//         if (ConnectSocket == INVALID_SOCKET) {
//             Utils::logMessage("Socket creation failed");
//             WSACleanup();
//             return false;
//         }

//         sockaddr_in clientService;
//         clientService.sin_family = AF_INET;
//         // Replace deprecated inet_addr with inet_pton
//         inet_pton(AF_INET, "127.0.0.1", &clientService.sin_addr);
//         clientService.sin_port = htons(12345);

//         // Try to connect
//         int result = connect(ConnectSocket, (SOCKADDR*)&clientService, sizeof(clientService));
//         closesocket(ConnectSocket);
        
//         if (result != SOCKET_ERROR) {
//             // Connection successful, server is running
//             // Utils::logMessage("HMM server is already running");
//             WSACleanup();
//             return true;
//         }

//         // Server not running, start it
//         Utils::logMessage("Server not running, starting HMM server");
        
//         STARTUPINFOW si = { sizeof(STARTUPINFOW) };
//         PROCESS_INFORMATION pi;
//         std::wstring cmd = L"E:\\CPP\\v3\\scripts\\start_hmm_server.bat";
        
//         if (!CreateProcessW(NULL, &cmd[0], NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
//             Utils::logMessage("Failed to start server process");
//             WSACleanup();
//             return false;
//         }
        
//         CloseHandle(pi.hProcess);
//         CloseHandle(pi.hThread);
        
//         // Wait for server to start
//         Sleep(3000); // Give it 3 seconds to start up
        
//         WSACleanup();
//         return true;
//     }

//     // Connect to Python server over socket and get prediction
//     std::string communicate_with_server(const std::string& input) {
//         Utils::logMessage("Connecting to HMM server...");
//         std::string result;
        
//         WSADATA wsaData;
//         if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
//             Utils::logMessage("WSAStartup failed");
//             return "";
//         }

//         SOCKET ConnectSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
//         if (ConnectSocket == INVALID_SOCKET) {
//             Utils::logMessage("Socket creation failed");
//             WSACleanup();
//             return "";
//         }

//         sockaddr_in clientService;
//         clientService.sin_family = AF_INET;
//         // Replace deprecated inet_addr with inet_pton
//         inet_pton(AF_INET, "127.0.0.1", &clientService.sin_addr);
//         clientService.sin_port = htons(12345);

//         // Connect to server
//         if (connect(ConnectSocket, (SOCKADDR*)&clientService, sizeof(clientService)) == SOCKET_ERROR) {
//             Utils::logMessage("Failed to connect to server");
//             closesocket(ConnectSocket);
//             WSACleanup();
            
//             // Try to restart server once
//             if (ensure_server_running()) {
//                 // Try connecting again after server restart
//                 WSAStartup(MAKEWORD(2, 2), &wsaData);
//                 ConnectSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
                
//                 sockaddr_in clientService2;
//                 clientService2.sin_family = AF_INET;
//                 inet_pton(AF_INET, "127.0.0.1", &clientService2.sin_addr);
//                 clientService2.sin_port = htons(12345);
                
//                 if (connect(ConnectSocket, (SOCKADDR*)&clientService2, sizeof(clientService2)) == SOCKET_ERROR) {
//                     Utils::logMessage("Failed to connect to server after restart");
//                     closesocket(ConnectSocket);
//                     WSACleanup();
//                     return "";
//                 }
//             } else {
//                 return "";
//             }
//         }

//         Utils::logMessage("Connected to server, sending data: " + input);
        
//         // Send data
//         if (send(ConnectSocket, input.c_str(), (int)input.size(), 0) == SOCKET_ERROR) {
//             Utils::logMessage("Failed to send data");
//             closesocket(ConnectSocket);
//             WSACleanup();
//             return "";
//         }

//         // Receive response
//         char recvbuf[4096];
//         int recvbuflen = 4096;
//         int bytesReceived = 0;
        
//         // Set socket timeout
//         DWORD timeout = 10000; // 10 seconds
//         setsockopt(ConnectSocket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
        
//         Utils::logMessage("Waiting for response...");
        
//         bytesReceived = recv(ConnectSocket, recvbuf, recvbuflen, 0);
//         if (bytesReceived > 0) {
//             recvbuf[bytesReceived] = '\0';
//             result = recvbuf;
//             Utils::logMessage("Received response: " + result);
//         } else {
//             Utils::logMessage("Failed to receive response or timeout");
//         }

//         // Cleanup
//         closesocket(ConnectSocket);
//         WSACleanup();
        
//         return result;
//     }

//     // Use the persistent Python server for prediction
//     // example: input = "0.1,0.3,0.7,0.2";
//     int use(std::string input) {
//         // Ensure server is running (starts it if not)
//         if (!ensure_server_running()) {
//             Utils::logMessage("Failed to ensure server is running");
//             return -1;
//         }
        
//         // Communicate with server
//         std::string output = communicate_with_server(input);
        
//         // Utils::logMessage("Server output: " + output);
        
//         // Handle potential errors
//         if (output.empty()) {
//             Utils::logMessage("Error: Empty output from HMM server");
//             return -1;
//         }
        
//         if (output.find("ERROR:") == 0) {
//             Utils::logMessage("Server error: " + output);
//             return -1;
//         }
        
//         try {
//             return std::stoi(output);
//         }
//         catch (const std::exception& e) {
//             Utils::logMessage("Error parsing server output: " + std::string(e.what()));
//             return -1;
//         }
//     }
// }




#pragma once
#include "ModelInterface.h"
#include <vector>
#include <string>
#include <pybind11/pybind11.h>
#include "FeatureMatrix.h"

class HMMModelInterface : public ModelInterface {
public:
    HMMModelInterface();
    ~HMMModelInterface() override;

    bool LoadModel(const char_T* modelPath) override;
    std::vector<float> Predict(const std::vector<float>& inputData,
                               const std::vector<int64_t>& inputShape) override;
    std::vector<float> Predict(const std::vector<Bar>& bars);

    std::vector<float> predict2D(const std::vector<std::vector<float>> inputData);
  
    void PrintModelInfo() override;

private:
    pybind11::object model_;
};