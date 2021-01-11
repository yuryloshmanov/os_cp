#include "../messaging.hpp"


auto sendMessage(zmqpp::socket &socket, const Message &message) -> void {
    zmqpp::message zmqMessage;
    msgpack::sbuffer package;

    msgpack::pack(&package, message);
    zmqMessage.add_raw(package.data(), package.size());

    if (!socket.send(zmqMessage)) {
        throw std::runtime_error("send timeout");
    }
}


auto receiveMessage(zmqpp::socket &socket, Message &message) -> void {
    zmqpp::message zmqMessage;
    if (!socket.receive(zmqMessage)) {
        throw std::runtime_error("receive timeout");
    }

    msgpack::unpacked unpackedPackage;
    msgpack::unpack(unpackedPackage, static_cast<const char *>(zmqMessage.raw_data()), zmqMessage.size(0));
    unpackedPackage.get().convert(message);
}

