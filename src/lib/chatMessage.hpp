#ifndef CP_CHAT_MESSAGE_HPP
#define CP_CHAT_MESSAGE_HPP


#include <string>
#include <utility>
#include <iostream>
#include <msgpack.hpp>


struct ChatMessage {
    std::string datetime{};
    std::string username{};
    std::string text{};

    ChatMessage() = default;

    ChatMessage(std::string datetime, std::string username, std::string text) : datetime(std::move(datetime)),
                                                                                username(std::move(username)),
                                                                                text(std::move(text)) {}

    friend auto operator<<(std::ostream &os, const ChatMessage &chatMessage) -> std::ostream& {
        const auto [datetime, username, text] = chatMessage;
        os << "| " << datetime << " / " << username << "> " << text;
        return os;
    }

    MSGPACK_DEFINE (datetime, username, text)
};


#endif //CP_CHAT_MESSAGE_HPP
