#include <mutex>
#include <chrono>
#include <string>
#include <thread>
#include <random>
#include <sstream>
#include <utility>
#include <iostream>
#include <zmqpp/zmqpp.hpp>


#include "lib/messaging.hpp"
#include "lib/networking.hpp"


#define RESET   "\033[0m"
#define RED     "\033[31m"


std::string username;


std::vector<std::string> chats;
std::mutex mutex;


constexpr int32_t sendTimeout = 3 * 1000;
constexpr int32_t receiveTimeout = 3 * 1000;


auto updater(zmqpp::socket &clientSocket) -> void {
    static time_t lastChatsUpdateTime{0};

    try {
        while (true) {
            auto message = Message(MessageType::UpdateChats, MessageData(lastChatsUpdateTime, username, ""));
            mutex.lock();
            sendMessage(clientSocket, message);
            receiveMessage(clientSocket, message);
            mutex.unlock();

            for (const auto &chat: message.data.vector) {
                chats.push_back(chat);
            }

            if (message.data.vector.empty()) {
                lastChatsUpdateTime == 0 ? lastChatsUpdateTime = 0 : lastChatsUpdateTime = message.data.time;
            } else {
                lastChatsUpdateTime = message.data.time;
            }

            std::this_thread::sleep_for(std::chrono::seconds(2));
        }
    } catch (...) {}

    std::cout << "updater stopped" << std::endl;
}


auto connectToServer(zmqpp::socket &serverSocket, zmqpp::socket &clientSocket) -> void {

    const std::string serverEndPoint("tcp://192.168.1.2:4506");
    std::string clientEndPoint("tcp://" + getIP() + ":");


    clientSocket.set(zmqpp::socket_option::send_timeout, sendTimeout);
    clientSocket.set(zmqpp::socket_option::receive_timeout, receiveTimeout);

    serverSocket.set(zmqpp::socket_option::send_timeout, sendTimeout);
    serverSocket.set(zmqpp::socket_option::receive_timeout, receiveTimeout);

    serverSocket.connect(serverEndPoint);

    // if testing on same machine with server
    for (int i = 0; i < 5; i++) {
        try {
            std::random_device randomDevice;
            std::mt19937 randomEngine(randomDevice());
            std::uniform_int_distribution distribution(4000, 9999);

            uint32_t port = distribution(randomEngine);
            clientSocket.bind(clientEndPoint + std::to_string(port));
            clientEndPoint += std::to_string(port);
            break;
        } catch (zmqpp::exception&) {
            if (i == 4) {
                std::cerr << "can't find appropriate port" << std::endl;
                exit(1);
            }
            continue;
        }
    }

    std::string password;
    int command;
    std::cout << "Choose:\n    1.Sign in\n    2.Sign up\nEnter number: ";
    if (!(std::cin >> command)) {
        throw std::runtime_error("invalid input");
    }
    MessageType requestType;
    if (command == 1) {
        requestType = MessageType::SignIn;
    } else if (command == 2) {
        requestType = MessageType::SignUp;
    } else {
        throw std::runtime_error("invalid command");
    }


    std::cout << "username: ";
    std::cin >> username;
    std::cout << "password: ";
    std::cin >> password;


    zmqpp::message connectMessage;
    connectMessage << clientEndPoint;
    if (!serverSocket.send(connectMessage, true)) {
        throw std::runtime_error("send error");
    }

    if (requestType == MessageType::SignIn) {
        auto request = Message(MessageType::SignIn, MessageData(username, password));
        sendMessage(clientSocket, request);

        Message response;
        receiveMessage(clientSocket, response);

        if (response.authenticationStatus == AuthenticationStatus::NotExists) {
            throw std::runtime_error("user not exists");
        } else if (response.authenticationStatus == AuthenticationStatus::InvalidPassword) {
            throw std::runtime_error("invalid password");
        } else if (response.authenticationStatus == AuthenticationStatus::Success) {
            std::cout << "sing in succeeded" << std::endl;
        }
    } else {
        auto request = Message(MessageType::SignUp, MessageData(username, password));
        sendMessage(clientSocket, request);

        Message response;
        receiveMessage(clientSocket, response);

        if (response.authenticationStatus == AuthenticationStatus::Exists) {
            throw std::runtime_error("user exists");
        } else if (response.authenticationStatus == AuthenticationStatus::Success) {
            std::cout << "sing up succeeded" << std::endl;
        }
    }
}

auto main() -> int {
    try {
        zmqpp::context context;

        zmqpp::socket serverSocket(context, zmqpp::socket_type::push);
        zmqpp::socket clientSocket(context, zmqpp::socket_type::request);

        connectToServer(serverSocket, clientSocket);

        std::thread updaterThread(updater, std::ref(clientSocket));
        int32_t command;
        while (true) {
            std::cout << "Choose:\n"
                         "    1. Show chats\n"
                         "    2. Create chat\n"
                         "    3. Enter chat\n"
                         "    4. Quit\n"
                         "Enter num: ";
            std::cin >> command;

            if (command == 1) {
                for (const auto &chat: chats) {
                    std::cout << "    " << chat << std::endl;
                }
            } else if (command == 2) {
                std::string chatName;
                std::cout << "Enter chat name: ";
                std::cin.ignore();
                std::getline(std::cin, chatName);

                std::string line;
                std::cout << "Enter usernames: ";
                std::getline(std::cin, line);
                std::stringstream ss(line);

                MessageData msgData;
                msgData.name = username;
                msgData.buffer = chatName;
                msgData.vector.push_back(username);

                for (std::string s; ss >> s;) {
                    msgData.vector.push_back(s);
                }

                auto message = Message(MessageType::CreateChat, msgData);

                mutex.lock();
                sendMessage(clientSocket, message);
                receiveMessage(clientSocket, message);
                mutex.unlock();

                if (message.type == MessageType::ClientError) {
                    std::cout << RED << message.data.buffer << RESET << std::endl;
                } else if (message.type == MessageType::ServerError) {
                    std::cout << RED << "Server error" << RESET << std::endl;
                }
            } else if (command == 3) {
                std::string chatName;
                std::cout << "Enter chat name: ";
                std::cin >> chatName;

                while (true) {
                    std::cout << "Choose:\n"
                                 "    1. Send message\n"
                                 "    2. Show messages\n"
                                 "    3. Invite user\n"
                                 "    4. Exit menu\n"
                                 "Enter num: ";
                    std::cin >> command;

                    if (command == 1) {
                        std::string data;
                        std::cout << "Enter message:" << std::endl;
                        std::cin.ignore();
                        std::getline(std::cin, data);

                        MessageData msgData;
                        msgData.name = chatName;
                        msgData.buffer = data;
                        auto message = Message(MessageType::CreateMessage, msgData);

                        mutex.lock();
                        sendMessage(clientSocket, message);
                        receiveMessage(clientSocket, message);
                        mutex.unlock();

                        if (message.type == MessageType::ClientError) {
                            std::cout << RED << message.data.buffer << RESET << std::endl;
                        } else if (message.type == MessageType::ServerError) {
                            std::cout << RED << "Server error" << RESET << std::endl;
                        }
                    } else if (command == 2) {
                        MessageData msgData;
                        msgData.name = chatName;
                        auto message = Message(MessageType::GetAllMessagesFromChat, msgData);


                        mutex.lock();
                        sendMessage(clientSocket, message);
                        receiveMessage(clientSocket, message);
                        mutex.unlock();
                        if (message.type == MessageType::ClientError) {
                            std::cout << RED << message.data.buffer << RESET << std::endl;
                        } else if (message.type == MessageType::ServerError) {
                            std::cout << "Server error" << std::endl;
                        } else {
                            for (const auto &chatMessage: message.data.chatMessages) {
                                std::cout << chatMessage << std::endl;
                            }
                        }
                    } else if (command == 3) {
                        std::string user;
                        std::cout << "Enter username: ";
                        std::cin >> user;

                        std::string value;
                        std::cout << "Share history with user? (y/n): ";
                        std::cin >> value;

                        MessageData msgData;
                        msgData.name = chatName;
                        msgData.buffer = user;

                        if (value == "y" || value == "Y") {
                            msgData.flag = true;
                        } else if (value == "n" || value == "N") {
                            msgData.flag = false;
                        } else {
                            std::cout << "invalid command" << std::endl;
                            break;
                        }
                        auto message = Message(MessageType::InviteUserToChat, msgData);


                        mutex.lock();
                        sendMessage(clientSocket, message);
                        receiveMessage(clientSocket, message);
                        mutex.unlock();

                        if (message.type == MessageType::ClientError) {
                            std::cout << RED << message.data.buffer << RESET << std::endl;
                        } else if (message.type == MessageType::ServerError) {
                            std::cout << RED << "Server error" << RESET << std::endl;
                        }
                    } else if (command == 4) {
                        break;
                    } else {
                        std::cout << "Invalid command" << std::endl;
                    }
                }
            } else if (command == 4) {
                break;
            } else {
                std::cout << "Invalid command" << std::endl;
            }

        }

    } catch (zmqpp::exception &exception) {
        std::cerr << "caught zmq exception: " << exception.what() << std::endl;
        exit(1);
    } catch (std::runtime_error &exception) {
        std::cerr << exception.what() << std::endl;
        exit(2);
    }

    return 0;
}
