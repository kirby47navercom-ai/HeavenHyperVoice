// Run against a local --dev-no-auth FieldServer or InstanceServer.
#define NOMINMAX
#include <winsock2.h>
#include <openssl/ssl.h>
#undef near
#undef far
#include "FieldCodec.h"
#include "TriangleWorld.h"
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <thread>

using namespace hhv::movement;

void require(bool success, const char *message) {
    if (!success) {
        throw std::runtime_error(message);
    }
}

class LocalConnection {
  public:
    explicit LocalConnection(const std::string &port) {
        context = SSL_CTX_new(TLS_client_method());
        require(context != nullptr, "TLS context failed");
        // Only the loopback development endpoint is supported by this test executable.
        SSL_CTX_set_verify(context, SSL_VERIFY_NONE, nullptr);
        connection = BIO_new_ssl_connect(context);
        const std::string endpoint = "127.0.0.1:" + port;
        BIO_set_conn_hostname(connection, endpoint.c_str());
        require(BIO_do_connect(connection) > 0, "Could not connect to local development server");
        BIO_get_ssl(connection, &ssl);
        const DWORD timeout = 10000;
        const SOCKET socket = static_cast<SOCKET>(SSL_get_fd(ssl));
        setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char *>(&timeout),
                   sizeof(timeout));
    }

    ~LocalConnection() {
        BIO_free_all(connection);
        SSL_CTX_free(context);
    }

    void send(const heaven::proto::Bytes &frame) {
        std::size_t offset = 0;
        while (offset < frame.size()) {
            const int written =
                SSL_write(ssl, frame.data() + offset, static_cast<int>(frame.size() - offset));
            require(written > 0, "TLS send failed");
            offset += written;
        }
    }

    bool receive(heaven::proto::Bytes &body) {
        std::uint8_t header[4];
        if (!read(header, 4)) {
            return false;
        }
        const std::uint32_t size =
            header[0] | (header[1] << 8) | (header[2] << 16) | (static_cast<std::uint32_t>(header[3]) << 24);
        require(size > 0 && size < 4 * 1024 * 1024, "Invalid server frame size");
        body.resize(size);
        return read(body.data(), size);
    }

  private:
    bool read(void *buffer, std::size_t size) {
        auto *bytes = static_cast<std::uint8_t *>(buffer);
        std::size_t offset = 0;
        while (offset < size) {
            const int count = SSL_read(ssl, bytes + offset, static_cast<int>(size - offset));
            if (count <= 0) {
                return false;
            }
            offset += count;
        }
        return true;
    }

    SSL_CTX *context = nullptr;
    BIO *connection = nullptr;
    SSL *ssl = nullptr;
};

heaven::proto::Bytes enterFrame(std::uint64_t id, std::uint32_t type) {
    flatbuffers::FlatBufferBuilder builder;
    const auto name = builder.CreateString("SharedCoreNetworkTest");
    const auto enter = HeavenField::CreateEnter(builder, 0, name, id, 0, type, Version);
    return heaven::proto::detail::wrapField(builder, HeavenField::Payload::Enter, enter.Union());
}

heaven::proto::Bytes moveFrame(const std::vector<PredictedInput> &inputs) {
    flatbuffers::FlatBufferBuilder builder;
    const auto frames = wire::encodeInputs(builder, inputs);
    const auto move = HeavenField::CreateMove(builder, frames);
    return heaven::proto::detail::wrapField(builder, HeavenField::Payload::Move, move.Union());
}

int main(int argc, char **argv) {
    try {
        require(argc >= 3, "Usage: MovementNetworkTest <local port> <collision file> [instance type]");
        WSADATA sockets;
        require(WSAStartup(MAKEWORD(2, 2), &sockets) == 0, "Socket initialization failed");
        TriangleWorld world;
        std::ifstream geometry(argv[2]);
        require(world.load(geometry), "Cannot load collision fixture");
        LocalConnection connection(argv[1]);
        connection.send(enterFrame(990001, argc > 3 ? static_cast<std::uint32_t>(std::stoul(argv[3])) : 0));

        PredictionQueue prediction;
        heaven::proto::Bytes body;
        std::map<std::uint64_t, Vec3> wildOrigins;
        std::set<std::uint64_t> movingWild;
        const auto observe = [&](const HeavenField::Envelope& envelope) {
            const auto* snapshot = envelope.payload_as_Snapshot();
            if (!snapshot) {
                return;
            }
            for (const auto* entities : {snapshot->spawned(), snapshot->moved()}) {
                if (!entities) {
                    continue;
                }
                for (const auto* entity : *entities) {
                    if (entity->species() != 0 && entity->movement()) {
                        wildOrigins.emplace(entity->entity_id(), wire::decodeState(*entity->movement()).position);
                    }
                    if (wildOrigins.count(entity->entity_id()) != 0 && entity->movement()) {
                        const auto state = wire::decodeState(*entity->movement());
                        require(finite(state.position) && finite(state.velocity), "Invalid wild core state");
                        const auto delta = state.position - wildOrigins.at(entity->entity_id());
                        if (std::hypot(delta.x, delta.y) > 10.f) {
                            movingWild.insert(entity->entity_id());
                        }
                    }
                }
            }
        };
        bool entered = false;
        while (!entered && connection.receive(body)) {
            const auto *envelope = heaven::proto::verifyFieldEnvelope(body);
            require(envelope != nullptr, "Malformed server response");
            observe(*envelope);
            if (const auto *ack = envelope->payload_as_EnterAck()) {
                require(ack->core_version() == Version && ack->collision_hash() == world.hash(),
                        "Server and client collision/core do not match");
                prediction.reset(wire::decodeState(*ack->movement()));
                entered = true;
            }
        }
        require(entered, "No entry acknowledgement");

        std::uint32_t acknowledged = 0;
        std::vector<PredictedInput> lastBatch;
        std::map<std::uint32_t, State> expected;
        const auto start = std::chrono::steady_clock::now();
        for (int batch = 0; batch < 120; ++batch) {
            for (int step = 0; step < 3; ++step) {
                const int tick = batch * 3 + step;
                Input input;
                input.x = tick < 180 ? .7f : -.7f;
                input.y = .15f;
                input.buttons = tick % 90 == 10 ? Jump : (tick % 90 == 55 ? Roll : Run);
                require(prediction.predict(input, Config{}, world), "Prediction stopped");
                expected[prediction.history.back().input.sequence] = prediction.state;
            }
            lastBatch = prediction.takeUnsent();
            if (batch == 60) {
                // Server must ignore the forged position and still reproduce the inputs.
                lastBatch.back().position = {999999, 999999, 999999};
            }
            connection.send(moveFrame(lastBatch));
            const auto target = lastBatch.back().input.sequence;
            while (acknowledged < target && connection.receive(body)) {
                const auto *envelope = heaven::proto::verifyFieldEnvelope(body);
                require(envelope != nullptr, "Malformed movement reply");
                observe(*envelope);
                if (const auto *ack = envelope->payload_as_Correction()) {
                    const auto authoritative = wire::decodeState(*ack->movement());
                    const auto &predicted = expected.at(ack->sequence());
                    require(length(authoritative.position - predicted.position) < .01f,
                            "Network simulation position diverged");
                    require(length(authoritative.velocity - predicted.velocity) < .01f &&
                                authoritative.mode == predicted.mode &&
                                authoritative.rollRemaining == predicted.rollRemaining,
                            "Jump/roll state diverged over the network");
                    require(prediction.acknowledge(ack->sequence(), authoritative, world),
                            "Invalid server sequence");
                    acknowledged = ack->sequence();
                }
            }
            require(acknowledged == target, "Connection closed before movement was verified");
            std::this_thread::sleep_until(start + std::chrono::milliseconds((batch + 1) * 50));
        }
        require(prediction.history.empty(), "Verified inputs remain in queue");
        if (argc > 4) {
            require(movingWild.size() >= std::stoul(argv[4]), "Not enough wild Pokemon moved during the test");
            std::cout << "PASS: " << movingWild.size() << " wild Pokemon moved with finite core states\n";
        }

        connection.send(moveFrame(lastBatch));
        bool closed = false;
        for (int frame = 0; frame < 100 && !closed; ++frame) {
            closed = !connection.receive(body);
        }
        require(closed, "Duplicate input did not terminate the invalid stream");
        std::cout << "PASS: 360 network ticks, jump, roll, forged coordinates, acknowledgements and "
                     "duplicate rejection\n";
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
