#include <tuple>
#include <utility>


#include "../database.hpp"


auto bind(sqlite3_stmt *&sqlite3Stmt, int32_t index, const char *value) noexcept -> bool {
    return sqlite3_bind_text(sqlite3Stmt, index, value, -1, nullptr) == SQLITE_OK;
}


auto bind(sqlite3_stmt *&sqlite3Stmt, int32_t index, int32_t value) noexcept -> bool {
    return sqlite3_bind_int(sqlite3Stmt, index, value) == SQLITE_OK;
}


template<class T, size_t index = 0>
auto bindTuple(
        sqlite3_stmt *&sqlite3Stmt,
        const T &tuple
) noexcept -> typename std::enable_if<index >= std::tuple_size<T>::value, bool>::type {
    return true;
}


template<class T, size_t index = 0>
auto bindTuple(
        sqlite3_stmt *stmt,
        const T &tuple
) noexcept -> typename std::enable_if<index < std::tuple_size<T>::value, bool>::type {
    auto value = std::get<index>(tuple);
    return bind(stmt, index + 1, value) && bindTuple<T, index + 1>(stmt, tuple);
}


auto Database::executeSqlQuery(const std::string &sql) noexcept -> bool {
    return sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &err_msg) == SQLITE_OK;
}


auto Database::getFormattedDatetime(const time_t rawTime) noexcept -> std::string {
    time_t _rawTime = rawTime;
    struct tm *currentTime;
    time(&_rawTime);
    currentTime = localtime(&_rawTime);

    const int TIME_STRING_LENGTH = 20;
    char buffer[TIME_STRING_LENGTH];

    strftime(buffer, TIME_STRING_LENGTH, "%Y-%m-%d %H:%M:%S", currentTime);
    return std::string(buffer);
}


auto Database::prepareStatement(const char *sqlQuery) noexcept -> bool {
    return sqlite3_prepare_v2(db, sqlQuery, -1, &stmt, nullptr) == SQLITE_OK;
}


template<class... Args>
auto Database::bindStatement(Args... args) noexcept -> bool {
    return bindTuple(stmt, std::make_tuple(args...));
}


auto Database::createChat(
        const std::string &chatName,
        const int32_t &adminId,
        const std::vector<int32_t> &userIds
) -> bool {
    if (isChatExists(chatName)) {
        return false;
    }

    const auto creationRawTime = time(nullptr);

    if (adminId == -1) {
        return false;
    }

    const auto sqlChatsQuery = "INSERT INTO Chats(Name, AdminId, CreationRawTime) VALUES(?, ?, ?);";

    mutex.lock();
    if (!prepareStatement(sqlChatsQuery)) {
        throw std::runtime_error("sqlite3_prepare_v2 error");
    }

    if (!bindStatement(chatName.c_str(), adminId, creationRawTime)) {
        throw std::runtime_error("sqlite3_bind error");
    }

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        throw std::runtime_error("sqlite3_step error");
    }

    mutex.unlock();
    const auto chatId = getChatId(chatName);
    mutex.lock();

    for (const auto &userId : userIds) {
        const auto sqlChatsInfoQuery = "INSERT INTO ChatsInfo(ChatId, UserId, AllowedRawTime) VALUES(?, ?, ?);";
        if (userId == -1) {
            break;
        }
        if (!prepareStatement(sqlChatsInfoQuery)) {
            throw std::runtime_error("sqlite3_prepare_v2 error");
        }

        if (!bindStatement(chatId, userId, creationRawTime)) {
            throw std::runtime_error("sqlite3_bind error");
        }
        if (sqlite3_step(stmt) != SQLITE_DONE) {
            throw std::runtime_error("sqlite3_step error");
        }
    }

    mutex.unlock();

    return true;
}


auto Database::getUserPassword(const std::string &username) -> std::string {
    const auto sqlQuery = "SELECT Password FROM Users WHERE Username = ?";

    std::lock_guard lockGuard(mutex);
    if (!prepareStatement(sqlQuery)) {
        throw std::runtime_error("sqlite3_prepare_v2 error");
    }

    if (!bindStatement(username.c_str())) {
        throw std::runtime_error("sqlite3_bind_text error");
    }

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        return reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
    } else {
        throw std::runtime_error("sqlite3_step error");
    }
}


auto Database::getChatId(const std::string &chatName) -> int32_t {
    const auto sqlQuery = "SELECT Id FROM Chats WHERE Name = ?";

    std::lock_guard lockGuard(mutex);
    if (!prepareStatement(sqlQuery)) {
        throw std::runtime_error("sqlite3_prepare_v2 error");
    }

    if (!bindStatement(chatName.c_str())) {
        throw std::runtime_error("sqlite3_bind_int error");
    }

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        return sqlite3_column_int(stmt, 0);
    } else {
        return -1;
    }
}


auto Database::isChatExists(const std::string &chatName) -> bool {
    return getChatId(chatName) != -1;
}


auto Database::getAllUsers() -> std::set<User> {
    const auto sqlQuery = "SELECT Id, Username FROM Users";

    std::lock_guard lockGuard(mutex);
    if (!prepareStatement(sqlQuery)) {
        throw std::runtime_error("sqlite3_prepare_v2 error");
    }

    std::set<User> users;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        users.insert(
                User(sqlite3_column_int(stmt, 0), reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1)))
        );
    }

    return users;
}


auto Database::authenticateUser(const std::string &username, const std::string &password) -> AuthenticationStatus {
    if (isUserExist(username)) {
        if (getUserPassword(username) == password) {
            return AuthenticationStatus::Success;
        } else {
            return AuthenticationStatus::InvalidPassword;
        }
    } else {
        return AuthenticationStatus::NotExists;
    }
}


auto Database::getUserAllowedRawTime(int32_t chatId, int32_t userId) -> time_t {
    const auto sqlQuery = "SELECT AllowedRawTime FROM ChatsInfo WHERE ChatId = ? AND UserId = ?";

    std::lock_guard lockGuard(mutex);
    if (!prepareStatement(sqlQuery)) {
        throw std::runtime_error("sqlite3_prepare_v2 error");
    }

    if (!bindStatement(chatId, userId)) {
        throw std::runtime_error("sqlite_bind error");
    }

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        return sqlite3_column_int(stmt, 0);
    } else {
        throw std::runtime_error("sqlite3_step error");
    }
}


auto Database::inviteUserToChat(
        const std::string &chatName,
        const int32_t invitorId,
        const int32_t userId,
        bool allowHistorySharing
) -> void {

    const auto chatId = getChatId(chatName);
    const auto allowedRawTime = (allowHistorySharing) ?
                                (getUserAllowedRawTime(chatId, invitorId)) : (time(nullptr));

    const auto sqlQuery = "INSERT INTO ChatsInfo(ChatId, UserId, AllowedRawTime) VALUES(?, ?, ?);";

    std::lock_guard lockGuard(mutex);
    if (!prepareStatement(sqlQuery)) {
        throw std::runtime_error("sqlite3_prepare_v2 error");
    }

    if (!bindStatement(chatId, userId, allowedRawTime)) {
        throw std::runtime_error("sqlite3_bind_int error");
    }

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        throw std::runtime_error("sqlite3_step error");
    }
}


auto Database::createMessage(
        const std::string &chatName,
        const int32_t senderId,
        const time_t rawTime,
        const std::string &data
) -> bool {

    const auto chatId = getChatId(chatName);
    const auto formattedDatetime = getFormattedDatetime(rawTime);
    const auto sqlQuery = "INSERT INTO Messages(ChatId, SenderId, RawTime, Time, Data) VALUES(?, ?, ?, ?, ?)";

    if (chatId == -1) {
        return false;
    }

    std::lock_guard lockGuard(mutex);
    if (!prepareStatement(sqlQuery)) {
        throw std::runtime_error("sqlite3_prepare_v2 error");
    }
    if (!bindStatement(chatId, senderId, rawTime, formattedDatetime.c_str(), data.c_str())) {
        throw std::runtime_error("sqlite3_bind_int error");
    }

    if (sqlite3_step(stmt) == SQLITE_DONE) {
        return true;
    } else {
        throw std::runtime_error("sqlite3_step error");
    }
}


auto Database::getChatsByTime(const int32_t userId, const time_t rawTime) -> std::vector<std::string> {
    const auto sqlQuery = "SELECT ChatId FROM ChatsInfo WHERE AllowedRawTime > ? AND UserId = ?";

    mutex.lock();

    if (!prepareStatement(sqlQuery)) {
        throw std::runtime_error("sqlite3_prepare_v2 error");
    }

    if (!bindStatement(rawTime, userId)) {
        throw std::runtime_error("sqlite3_bind_int error");
    }

    std::vector<int> chatIds;
    for (int i = 0; sqlite3_step(stmt) == SQLITE_ROW; i++) {
        const auto chatId = sqlite3_column_int(stmt, 0);
        chatIds.push_back(chatId);
    }

    mutex.unlock();

    std::vector<std::string> chats;
    chats.reserve(chatIds.size());
    for (const auto &chatId: chatIds) {
        // getChatName locks
        chats.push_back(getChatName(chatId));
    }

    return chats;
}


auto Database::getChatName(const int chatId) -> std::string {
    const auto sqlQuery = "SELECT Name FROM Chats WHERE Id = ?";

    std::lock_guard lockGuard(mutex);
    if (!prepareStatement(sqlQuery)) {
        throw std::runtime_error("sqlite3_prepare_v2 error");
    }

    if (!bindStatement(chatId)) {
        throw std::runtime_error("sqlite3_bind_int error");
    }

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        return reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
    } else {
        return {};
    }
}


auto Database::createUser(const std::string &username, const std::string &password) -> void {
    std::lock_guard lockGuard(mutex);
    if (!executeSqlQuery("INSERT INTO Users(Username, Password) VALUES('" + username + "', '" + password + "');")) {
        throw std::runtime_error("sqlite3_exec error");
    }
}


auto Database::getUserId(const std::string &username) -> int32_t {
    const auto sqlQuery = "SELECT Id FROM Users WHERE Username = ?";

    std::lock_guard lockGuard(mutex);
    if (!prepareStatement(sqlQuery)) {
        throw std::runtime_error("sqlite3_prepare_v2 error");
    }

    if (!bindStatement(username.c_str())) {
        throw std::runtime_error("sqlite3_bind_text error");
    }

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        return sqlite3_column_int(stmt, 0);
    } else {
        return -1;
    }
}


auto Database::isUserExist(const std::string &username) -> bool {
    return getUserId(username) != -1;
}


Database::Database() : Database("database.db") {}


Database::Database(const std::string &path) {
    if (sqlite3_open(path.c_str(), &db) != SQLITE_OK) {
        throw std::runtime_error("sqlite3_open error");
    }

    std::string sql = "CREATE TABLE IF NOT EXISTS Users(Id INTEGER PRIMARY KEY AUTOINCREMENT, Username TEXT, Password TEXT);"
                      "CREATE TABLE IF NOT EXISTS Chats(Id INTEGER PRIMARY KEY AUTOINCREMENT, Name TEXT, AdminId INT, CreationRawTime INT);"
                      "CREATE TABLE IF NOT EXISTS ChatsInfo(ChatId INT, UserId INT, AllowedRawTime INT);"
                      "CREATE TABLE IF NOT EXISTS Messages(Id INTEGER PRIMARY KEY AUTOINCREMENT, ChatId INT, SenderId INT, RawTime INT, Time DATETIME, Data TEXT);";

    if (!executeSqlQuery(sql)) {
        throw std::runtime_error("sqlite3_exec error");
    }
}


Database::~Database() {
    if (err_msg) {
        sqlite3_free(err_msg);
    }
    sqlite3_close(db);
}

auto
Database::getAllMessagesFromChat(const std::string &chatName, int32_t userId) -> std::vector<ChatMessage> {
    const auto chatId = getChatId(chatName);
    const auto sqlQueryForRawTime = "SELECT AllowedRawTime FROM ChatsInfo WHERE ChatId = ? AND UserId = ?";

    std::lock_guard lockGuard(mutex);
    if (!prepareStatement(sqlQueryForRawTime)) {
        throw std::runtime_error("sqlite3_prepare_v2 error");
    }

    if (!bindStatement(chatId, userId)) {
        throw std::runtime_error("sqlite_bind error");
    }

    int32_t allowedRawTime;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        allowedRawTime = sqlite3_column_int(stmt, 0);
    } else {
        throw std::logic_error("Chat don't exists");
    }

    const auto sqlQueryForMessages = "SELECT SenderId, Time, Data FROM Messages WHERE ChatId = ? AND RawTime >= ? ORDER BY RawTime";

    if (!prepareStatement(sqlQueryForMessages)) {
        throw std::runtime_error("sqlite3_prepare_v2 error");
    }

    if (!bindStatement(chatId, allowedRawTime)) {
        throw std::runtime_error("sqlite_bind error");
    }

    std::vector<ChatMessage> messages;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        messages.emplace_back(
                reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1)),
                std::to_string(sqlite3_column_int(stmt, 0)),
                reinterpret_cast<const char *>(sqlite3_column_text(stmt, 2))

        );
    }

    for (auto &chatMessage: messages) {
        chatMessage.username = getUsername(std::stoi(chatMessage.username));
    }

    return messages;
}

auto Database::getUsername(const int id) -> std::string {
    const auto sqlQuery = "SELECT Username FROM Users WHERE Id = ?";

    if (!prepareStatement(sqlQuery)) {
        throw std::runtime_error("sqlite3_prepare_v2 error");
    }
    if (!bindStatement(id)) {
        throw std::runtime_error("sqlite3_bind_text error");
    }
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        return std::string(reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0)));
    } else {
        throw std::runtime_error("sqlite3_step error");
    }
}
