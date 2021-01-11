#ifndef CP_MESSAGING_HPP
#define CP_MESSAGING_HPP


#include <string>
#include <cstddef>
#include <zmqpp/zmqpp.hpp>
#include <msgpack.hpp>
#include <utility>


#include "auth.hpp"
#include "chatMessage.hpp"


enum class MessageType {
    CreateMessage,
    Update,
    SignIn,
    SignUp,
    CreateChat,
    UpdateChats,
    GetAllMessagesFromChat,
    InviteUserToChat,
    ClientError,
    ServerError
};


struct MessageData {
    int32_t time{};
    std::string name{};
    std::string buffer{};
    bool flag{};
    std::vector<std::string> vector{};
    std::vector<ChatMessage> chatMessages{};

    MessageData() = default;

    MessageData(std::string buffer) : buffer(std::move(buffer)) {}

    MessageData(time_t time, std::string username, std::string data) : time(time), name(std::move(username)),
                                                                       buffer(std::move(data)) {}

    MessageData(std::string username, std::string buffer) : name(std::move(username)),
                                                            buffer(std::move(buffer)) {}

    MSGPACK_DEFINE (time, name, buffer, flag, vector, chatMessages)
};


struct Message {
    MessageType type{};
    AuthenticationStatus authenticationStatus{};
    MessageData data{};

    Message() = default;

    explicit Message(MessageType messageType) : type(messageType) {}

    Message(MessageType messageType, MessageData message) : type(messageType), data(std::move(message)) {}

    MSGPACK_DEFINE (type, authenticationStatus, data);
};


auto sendMessage(zmqpp::socket &socket, const Message &message) -> void;

auto receiveMessage(zmqpp::socket &socket, Message &message) -> void;


MSGPACK_ADD_ENUM(MessageType)
MSGPACK_ADD_ENUM(AuthenticationStatus)


#endif //CP_MESSAGING_HPP
