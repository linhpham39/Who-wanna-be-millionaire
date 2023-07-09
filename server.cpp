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
#include <random>
#include <fstream>
#include <unordered_set>
#include "include/json.hpp"


#define MAX_CLIENTS 20
#define BUFFER_SIZE 1024
#define NUM_QUESTIONS_PER_PLAY 5

using json = nlohmann::json;

std::mutex mutex;

struct Client {
    int socket;
    std::string name;
    int score;
    bool fiftyFiftyUsed;
    bool phoneAFriendUsed;
    bool askTheAudienceUsed;
};


struct Game {
    Client client1;
    Client client2;
    int score1;
    int score2;
    std::chrono::system_clock::time_point datetime;

};


struct Question {
    int level;
    std::string content;
    std::unordered_map<std::string, std::string> answerList;
    std::string correctAnswer;
};


std::vector<Question> questions;
std::vector<Client> clients;

void sendQuestion(Client& client, const Question& question) {
    std::string message = "Question Level: " + std::to_string(question.level) + "\n";
    message += question.content + "\n";
    for (const auto& pair : question.answerList) {
        message += pair.first + ": " + pair.second + "\n";
    }

    message += "Or E to use a lifeline.";
    if (send(client.socket, message.c_str(), message.length(), 0) <= 0) {
        std::cerr << "Error sending question" << std::endl;
    }
}


std::vector<int> genRandomQuestionsList(int k, int n) {
    std::vector<int> questionList;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dist(0, n);
    std::unordered_set<int> uniqueSet;

    while (uniqueSet.size() < k) {
        int randomNumber = dist(gen);
        uniqueSet.insert(randomNumber);
    }

    for (int number : uniqueSet) {
        questionList.push_back(number);
    }

    return questionList;
}

void sendPlayerList(Client& client) {
    std::string message = "LIST ONLINE PLAYERS:\n";
    for (const auto& c : clients) {
        message += c.name + "\n";
    }

    if (send(client.socket, message.c_str(), message.length(), 0) <= 0) {
        std::cerr << "Error sending player list" << std::endl;
    }
}

void handleGame(Client& client) {
    int questionCount = 0;
    int score = 0;

    std::vector<int> questionIndices=genRandomQuestionsList(NUM_QUESTIONS_PER_PLAY,questions.size()-1);

    while (true) {
        // Check if all questions have been sent
        if (questionCount >= NUM_QUESTIONS_PER_PLAY) {
            std::string message = "GAME_WON";
            if (send(client.socket, message.c_str(), message.length(), 0) <= 0) {
                std::cerr << "Error sending GAME_WON message" << std::endl;
            }

            // Update score and save it
            client.score += score;
            return;
        }

        // Get the current question
        Question currentQuestion = questions[questionIndices[questionCount]];

        // Send the question to the client
        sendQuestion(client, currentQuestion);

        // Receive client's answer
        char buffer[BUFFER_SIZE];
        memset(buffer, 0, BUFFER_SIZE);
        if (recv(client.socket, buffer, BUFFER_SIZE, 0) <= 0) {
            std::cerr << "Error receiving answer" << std::endl;
            return;
        }

        // Check client's answer
        std::string clientAnswer = buffer;
        if (clientAnswer == currentQuestion.correctAnswer) {
            std::cout << "Client answered correctly" << std::endl;
            score++;
        } else {
            std::cout << "Client answered incorrectly" << std::endl;

            // Send the GAME_OVER message to the client
            std::string gameOverMessage = "GAME_OVER";
            if (send(client.socket, gameOverMessage.c_str(), gameOverMessage.length(), 0) <= 0) {
                std::cerr << "Error sending GAME_OVER message" << std::endl;
            }

            memset(buffer, 0, BUFFER_SIZE);
            if (recv(client.socket, buffer, BUFFER_SIZE, 0) <= 0) {
                std::cerr << "Error receiving RECV_GAME_OVER message" << std::endl;
                return;
            }

            // Send the score message to the client
            std::string scoreMessage = "SCORE: " + std::to_string(score);
            if (send(client.socket, scoreMessage.c_str(), scoreMessage.length(), 0) <= 0) {
                std::cerr << "Error sending SCORE message" << std::endl;
            }

            // Update score and save it
            client.score += score;
            return;
        }

        // Move to the next question
        questionCount++;
    }
}

void deleteClient(Client& client) {
    // Close the client's socket
    close(client.socket);

    // Remove the client from the clients vector
    mutex.lock();
    clients.erase(std::remove_if(clients.begin(), clients.end(), [&client](const Client& c) {
        return c.socket == client.socket;
    }), clients.end());
    mutex.unlock();

    std::cout << "Client disconnected: " << client.name << std::endl;
}


void handleRequest(Client& client, const std::string& request) {
    if (request == "START_GAME") {
        handleGame(client);
    } 
    else if (request == "DISCONNECT")
    {
        deleteClient(client);
    }
    
    else if (request == "GET_PLAYERS") {
        sendPlayerList(client);
    }

    // else if (request == "GET_SCOREBOARD") {
    //     sendScoreboard(client);
    // } else if (request == "CHALLENGE") {
    //     handleChallenge(client);
    // } else {
    //     // Handle other requests
    // }
}

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

    while (true) {
        memset(buffer, 0, BUFFER_SIZE);
        if (recv(clientSocket, buffer, BUFFER_SIZE, 0) <= 0) {
            std::cout << "Error receiving client message" << std::endl;
            close(clientSocket);
            return;
        }

        std::string request = buffer;
        handleRequest(client, request);
    }

    // ...
}

int main() {
    // Read questions file
    std::ifstream f("questions_list.json");
    json questions_data = json::parse(f)["all_questions"];
    for (auto& q: questions_data){
        Question question={
            q["level"],
            q["Question"],
            {{"D",q["D"]},
            {"C",q["C"]},
            {"B",q["B"]},
            {"A",q["A"]}},
            q["correct_answer"]    
        };
        questions.emplace_back(question);
    }

    // Create socket 
    int serverSocket;
    int port = 5555;
    struct sockaddr_in serverAddr {};

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