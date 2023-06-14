#include <iostream>
#include <thread>
#include <vector>
#include <mutex>
#include <chrono>
#include <cstring>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <algorithm>

#define MAX_CLIENTS 20
#define BUFFER_SIZE 1024

std::mutex mutex;

struct Client {
    int socket;
    std::string name;
    int score;
};


struct Question {
    int level;
    std::string content;
    std::vector<std::string> answerList;
    std::string correctAnswer;
};


std::vector<Question> questions = {
    {1, "What is the capital of France?", {"London", "Paris", "Berlin", "Madrid"}, "Paris"},
    {2, "Who wrote the novel 'Pride and Prejudice'?", {"Jane Austen", "Charles Dickens", "Mark Twain", "Leo Tolstoy"}, "Jane Austen"},
    {3, "What is the chemical symbol for the element Gold?", {"Go", "Gd", "Au", "Ag"}, "Au"},
    {4, "Which planet is known as the Red Planet?", {"Mars", "Venus", "Jupiter", "Saturn"}, "Mars"},
    {5, "What is the largest ocean on Earth?", {"Atlantic Ocean", "Indian Ocean", "Arctic Ocean", "Pacific Ocean"}, "Pacific Ocean"}
};


std::vector<Client> clients;


void handleClient(int clientSocket) {
    if (send(clientSocket, "Welcome Client", sizeof("Welcome Client"), 0) <= 0) {
        std::cerr << "Error sending welcome message" << std::endl;
        close(clientSocket);
        return;
    }


    char buffer[BUFFER_SIZE];
    memset(buffer, 0, BUFFER_SIZE);


    // Receive client's name
    if (recv(clientSocket, buffer, BUFFER_SIZE, 0) <= 0) {
        std::cout << "Error receiving client name" << std::endl;
        close(clientSocket);
        return;
    }


    // Add the client to the list
    mutex.lock();
    Client client;
    client.socket = clientSocket;
    client.name = buffer;
    client.score = 0;
    clients.push_back(client);
    mutex.unlock();


    std::cout << "Client connected: " << client.name << std::endl;


    // Game loop
    while (true) {
        memset(buffer, 0, BUFFER_SIZE);
        std::cout << buffer << std::endl;
        if (recv(clientSocket, buffer, BUFFER_SIZE, 0) <= 0) {
            std::cout << "Error receiving client message" << std::endl;
            close(clientSocket);
            return;
        }
        else if (strcmp(buffer, "START_GAME") == 0)
        {
            
            int questionIndex = 0;

            while (true) {
                // Check if all questions have been sent
                if (questionIndex >= questions.size()) {
                    break;
                }


                // Get the current question
                Question currentQuestion = questions[questionIndex];


                // Construct the message containing the question content and answer options
                std::string message = "Question Level: " + std::to_string(currentQuestion.level) + "\n";
                message += currentQuestion.content + "\n";


                for (const std::string& answer : currentQuestion.answerList) {
                    message += answer + "\n";
                }


                // Send the message to the client
                if (send(clientSocket, message.c_str(), message.length(), 0) <= 0) {
                    std::cout << "Error sending question" << std::endl;
                    break;
                }


                // Receive client's answer
                char buffer[BUFFER_SIZE];
                memset(buffer, 0, BUFFER_SIZE);
                if (recv(clientSocket, buffer, BUFFER_SIZE, 0) <= 0) {
                    std::cout << "Error receiving answer" << std::endl;
                    break;
                }


                // Check client's answer
                std::string clientAnswer = buffer;
                if (clientAnswer == currentQuestion.correctAnswer) {
                    std::cout << "Client answered correctly" << std::endl;
                    client.score++;
                } else {
                    std::cout << "Client answered incorrectly" << std::endl;

                    // Send the GAME_OVER message to the client
                    if (send(clientSocket, "GAME_OVER", strlen("GAME_OVER"), 0) <= 0) {
                        std::cout << "Error sending GAME_OVER message" << std::endl;
                        break;
                    }

                    std::string score_msg = "SCORE: " + std::to_string(client.score);
                    // Send the score message to the client
                    if (send(clientSocket, score_msg.c_str(), score_msg.length(), 0) <= 0) {
                        std::cout << "Error sending SCORE message" << std::endl;
                        break;
                    }
                    break;
                }


                // Move to the next question
                questionIndex++;
            }
        }
       
    }


    // Wait for a short duration before closing the socket
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    close(clientSocket);

    // Remove the client from the list
    mutex.lock();
    clients.erase(std::remove_if(clients.begin(), clients.end(),[clientSocket](const Client& c) { return c.socket == clientSocket; }), clients.end());
    mutex.unlock();
}


int main() {
    int serverSocket;
    int port = 5555;
    struct sockaddr_in serverAddr {};


    // Create socket
    if ((serverSocket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        std::cerr << "Socket creation failed" << std::endl;
        return -1;
    }


    // Set up server address
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    serverAddr.sin_addr.s_addr = INADDR_ANY;


    // Bind the socket to the specified IP and port
    if (bind(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        std::cerr << "Binding failed" << std::endl;
        return -1;
    }


    // Listen for incoming connections
    if (listen(serverSocket, MAX_CLIENTS) < 0) {
        std::cerr << "Listening failed" << std::endl;
        return -1;
    }


    std::cout << "Server started. Listening on port " << port << std::endl;


    // Accept client connections and handle them in separate threads
    while (true) {
        struct sockaddr_in clientAddr {};
        socklen_t clientAddrSize = sizeof(clientAddr);


        // Accept a client connection
        int clientSocket = accept(serverSocket, (struct sockaddr*)&clientAddr, &clientAddrSize);
        if (clientSocket < 0) {
            std::cerr << "Failed to accept client connection" << std::endl;
            continue;
        }


        // Handle the client in a separate thread
        std::thread clientThread(handleClient, clientSocket);
        clientThread.detach();
    }


    // Close the server socket
    close(serverSocket);


    return 0;
}