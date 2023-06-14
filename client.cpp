#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <vector>
#include <sstream>

#define BUFFER_SIZE 1024

struct Question {
    int level;
    std::string content;
    std::vector<std::string> answerList;
};


Question decodeQuestion(const std::string& message) {
    Question question;
    std::istringstream iss(message);
    std::string line;


    // Read the question level
    std::getline(iss, line);
    question.level = std::stoi(line.substr(line.find(":") + 1));


    // Read the question content
    std::getline(iss, question.content);


    // Read the answer options
    while (std::getline(iss, line)) {
        question.answerList.push_back(line);
    }


    return question;
}


int main() {
    int clientSocket;
    int port = 5555;
    std::string serverAddress = "127.0.0.1";


    // Create socket
    if ((clientSocket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        std::cerr << "Socket creation failed" << std::endl;
        return -1;
    }


    struct sockaddr_in serverAddr {};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);


    // Convert server address from string to network address structure
    if (inet_pton(AF_INET, serverAddress.c_str(), &(serverAddr.sin_addr)) <= 0) {
        std::cerr << "Invalid server address" << std::endl;
        return -1;
    }


    // Connect to the server
    if (connect(clientSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        std::cerr << "Connection failed" << std::endl;
        return -1;
    }


    std::cout << "Connected to the server" << std::endl;


    // Receive welcome message from the server
    char buffer[BUFFER_SIZE];
    memset(buffer, 0, BUFFER_SIZE);
    if (recv(clientSocket, buffer, BUFFER_SIZE, 0) <= 0) {
        std::cerr << "Error receiving welcome message" << std::endl;
        close(clientSocket);
        return -1;
    }


    std::cout << buffer << std::endl;


    // Send client's name to the server
    std::string name;
    std::cout << "Enter your name: ";
    std::getline(std::cin, name);
    if (send(clientSocket, name.c_str(), name.length(), 0) <= 0) {
        std::cerr << "Error sending name" << std::endl;
        close(clientSocket);
        return -1;
    }


    // Game loop
    while (true) {
        int choice;
        std::cout << "Who want to be a millionaire" << std::endl;
        std::cout << "1. Start Game" << std::endl;
        std::cout << "2. Challenge another player" << std::endl;
        std::cout << "Other to quit" << std::endl;
        std::cin >> choice;
        std::cin.ignore(); // Add this line to discard the newline character

        switch (choice)
        {
        case 1:
            if (send(clientSocket, "START_GAME", strlen("START_GAME"), 0) <= 0) {
                std::cerr << "Error sending START_GAME message" << std::endl;
                close(clientSocket);
                return -1;
            }


            while (true)
            {
                memset(buffer, 0, BUFFER_SIZE);
                if (recv(clientSocket, buffer, BUFFER_SIZE, 0) <= 0) {
                    std::cerr << "Error receiving server message" << std::endl;
                    close(clientSocket);
                    return -1;
                }
                else if(strcmp(buffer, "GAME_OVER") == 0){
                    std::cout << "You answered incorrectly. " << buffer << std::endl;
                    memset(buffer, 0, BUFFER_SIZE);
                    if (recv(clientSocket, buffer, BUFFER_SIZE, 0) <= 0) {
                        std::cerr << "Error receiving score message" << std::endl;
                        close(clientSocket);
                        return -1;
                    }
                    std::cout << buffer << std::endl;
                    break;
                }
                else{
                    // Decode the server's question message
                    Question question = decodeQuestion(buffer);


                    // Print the question
                    std::cout << "Level: " << question.level << std::endl;
                    std::cout << "Question: " << question.content << std::endl;
                    std::cout << "Options:" << std::endl;
                    for (const std::string& option : question.answerList) {
                        std::cout << option << std::endl;
                    }

                    std::string clientAnswer;
                    std::getline(std::cin, clientAnswer);
                    if (send(clientSocket, clientAnswer.c_str(), clientAnswer.length(), 0) <= 0) {
                        std::cerr << "Error sending answer" << std::endl;
                        close(clientSocket);
                        return -1;
                    }
                }   
            }
           
            break;
        case 2:
            break;


        default:
            close(clientSocket);
            return 0;
        }


    }


    // Close the client socket
    close(clientSocket);


    return 0;
}