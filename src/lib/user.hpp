#ifndef CP_USER_HPP
#define CP_USER_HPP


#include <string>
#include <cstdint>
#include <utility>


struct User {
    int32_t id{};
    std::string username{};

    User() = default;

    User(int32_t id, std::string username) : id(id), username(std::move(username)) {}

    auto operator<(const User &rhs) const -> bool {
        return id < rhs.id;
    }
};


#endif //CP_USER_HPP
