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
#define NUM_QUESTIONS_PER_PLAY 3

using json = nlohmann::json;

std::mutex mutex;

struct Client {
    int socket;
    std::string name;
    int score;
};


struct Question {
    int level;
    std::string content;
    std::unordered_map<std::string,std::string> answerList;
    std::string correctAnswer;
};

std::vector<Question> questions;
std::vector<Client> clients;

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
            std::vector<int> questionIndices=genRandomQuestionsList(NUM_QUESTIONS_PER_PLAY,questions.size()-1);
            int questionCount = 0;
            
            while (true) {
                // Check if all questions have been sent
                if (questionCount >= NUM_QUESTIONS_PER_PLAY) {
                    if (send(clientSocket, "GAME_WON", strlen("GAME_WON"), 0) <= 0) {
                        std::cout << "Error sending GAME_WON message" << std::endl;
                        break;
                    }
                    std::ofstream outputFile("scoreboard.txt", std::ios::app);
                    if (outputFile.is_open()) {
                        std::string content = client.name+","+std::to_string(client.score);
                        outputFile << content << std::endl;
                        outputFile.close();
                    } else {
                    std::cout << "Failed to open the file." << std::endl;
                    }
                    break;
                }


                // Get the current question
                Question currentQuestion = questions[questionIndices[questionCount]];


                // Construct the message containing the question content and answer options
                std::string message = "Question Level: " + std::to_string(currentQuestion.level) + "\n";
                message += currentQuestion.content + "\n";

                for (const auto& pair : currentQuestion.answerList) {
                    message += pair.first +": " +pair.second + "\n";
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
                    std::ofstream outputFile("scoreboard.txt", std::ios::app);
                    if (outputFile.is_open()) {
                        std::string content = client.name+","+std::to_string(client.score);
                        outputFile << content << std::endl;
                        outputFile.close();
                    } else {
                    std::cout << "Failed to open the file." << std::endl;
                    }
                    break;
                }


                // Move to the next question
                questionCount++;
            }
        }
        else if (strcmp(buffer, "GET_SCOREBOARD") == 0)
        {
            std::string message = "USER, SCORE \n";
            std::ifstream inputFile("scoreboard.txt");
            if (inputFile.is_open()) {
                std::string line;
                while (std::getline(inputFile, line)) {
                    message += line +"\n";
                }
                if (send(clientSocket, message.c_str(), message.length(), 0) <= 0) {
                    std::cout << "Error sending SCORE message" << std::endl;
                    break;
                }
                inputFile.close();
            } else {
                std::cout << "Failed to open the file." << std::endl;
            }
        }
        else if (strcmp(buffer, "GET_PLAYERS") == 0)
        {
            std::string message = "LIST ONLINE PLAYERS \n";
            for (const auto& client : clients) {
                message += client.name +"\n";
            }
            if (send(clientSocket, message.c_str(), message.length(), 0) <= 0) {
                std::cout << "Error sending PLAYERS message" << std::endl;
                break;
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