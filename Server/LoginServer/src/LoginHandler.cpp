#include "LoginHandler.h"

#include <spdlog/spdlog.h>

#include <memory>
#include <utility>

#include "LoginCodec.h"
#include "PasswordHash.h"

namespace heaven::login {

bool LoginHandler::onFrame(TlsSession& session, const proto::Bytes& body) {
    // 핸들러 상태는 IOCP 워커(여기)와 인증 스레드가 같이 만진다. 다른 핸들러와
    // 달리 락이 필요한 이유다 — 클라이언트가 응답을 기다리지 않고 다음 프레임을
    // 밀어 넣으면 두 스레드가 겹친다.
    Stage stage = Stage::Done;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stage = stage_;
        if (stage == Stage::AwaitingLogin || stage == Stage::AwaitingSelection) {
            // 처리하는 동안 들어오는 프레임을 막는다.
            stage_ = Stage::Busy;
        }
    }

    if (stage == Stage::Busy) {
        spdlog::warn("{}: frame arrived while a request was in flight", session.peer());
        return false;
    }
    if (stage == Stage::Done) {
        spdlog::warn("{}: extra frame after the exchange finished", session.peer());
        return false;
    }

    const auto* envelope = proto::verifyLoginEnvelope(body);
    if (envelope == nullptr) {
        spdlog::warn("{}: malformed frame", session.peer());
        fail(session, "잘못된 요청입니다");
        return true;
    }

    const auto payload = envelope->payload_type();

    if (stage == Stage::AwaitingLogin) {
        switch (payload) {
            case HeavenLogin::Payload::LoginRequest:
                return handleLogin(session, *envelope->payload_as_LoginRequest());
            case HeavenLogin::Payload::RegisterRequest:
                return handleRegister(session, *envelope->payload_as_RegisterRequest());
            default:
                spdlog::warn("{}: expected a login or register request", session.peer());
                fail(session, "잘못된 요청입니다");
                return true;
        }
    }

    switch (payload) {
        case HeavenLogin::Payload::CreateCharacterRequest:
            return handleCreateCharacter(session, *envelope->payload_as_CreateCharacterRequest());
        case HeavenLogin::Payload::SelectCharacterRequest:
            return handleSelectCharacter(session, *envelope->payload_as_SelectCharacterRequest());
        case HeavenLogin::Payload::DeleteCharacterRequest:
            return handleDeleteCharacter(session, *envelope->payload_as_DeleteCharacterRequest());
        case HeavenLogin::Payload::ReleasePartnerRequest:
            return handleReleasePartner(session, *envelope->payload_as_ReleasePartnerRequest());
        case HeavenLogin::Payload::SetPartyRequest:
            return handleSetParty(session, *envelope->payload_as_SetPartyRequest());
        default:
            spdlog::warn("{}: expected a character request", session.peer());
            fail(session, "잘못된 요청입니다");
            return true;
    }
}

void LoginHandler::finish(TlsSession& session, const proto::Bytes& frame) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stage_ = Stage::Done;
    }
    session.send(frame);
    session.closeAfterFlush();
}

void LoginHandler::fail(TlsSession& session, std::string_view message) {
    finish(session, proto::encodeLoginFailure(message));
}

void LoginHandler::resume(Stage stage) {
    std::lock_guard<std::mutex> lock(mutex_);
    stage_ = stage;
}

bool LoginHandler::submitCharacterChange(TlsSession& session,
                                         std::function<std::pair<bool, const char*>()> work) {
    auto self = session.shared_from_this();
    const LoginContext* context = &context_;
    LoginHandler* handler = this;
    const std::uint64_t accountId = accountId_;

    // DB 왕복이므로 IOCP 워커에서 하지 않는다.
    const bool queued = context_.authQueue->submit(
        [self, context, handler, accountId, work = std::move(work)] {
            const auto [ok, message] = work();

            // 성공이든 실패든 최신 목록을 같이 보낸다. 클라가 따로 조회할 필요가 없다.
            const std::vector<Character> characters =
                context->characters->listByAccount(accountId);

            // 아직 선택이 남았으므로 연결을 유지한다.
            handler->resume(Stage::AwaitingSelection);
            self->send(proto::encodeCharacterList(ok, message, characters));
        });

    if (!queued) {
        spdlog::warn("{}: auth queue full, refusing request", session.peer());
        resume(Stage::AwaitingSelection);
        session.send(proto::encodeCharacterList(false, "서버가 혼잡합니다", {}));
    }
    return true;
}

// -------------------------------------------------------------------- 가입

bool LoginHandler::handleRegister(TlsSession& session,
                                  const HeavenLogin::RegisterRequest& request) {
    const auto* username = request.username();
    const auto* password = request.password();

    if (username == nullptr || password == nullptr) {
        finish(session, proto::encodeRegisterResult(false, "요청에 빠진 항목이 있습니다"));
        return true;
    }

    const std::string user = username->str();
    const std::string secret = password->str();

    // 형식 검증은 DB 를 건드리기 전에 끝낸다.
    const char* problem = nullptr;
    if (!proto::isValidUsername(user)) {
        problem = "아이디는 영문, 숫자, 밑줄만 쓸 수 있고 3~32자여야 합니다";
    } else if (secret.size() > proto::kMaxPasswordBytes) {
        // 바이트 상한은 프레임 크기를 막기 위한 것이라 그대로 바이트로 본다.
        problem = "비밀번호가 너무 깁니다";
    } else if (const auto length = proto::utf8Length(secret); !length.has_value()) {
        problem = "비밀번호의 문자 인코딩이 올바르지 않습니다";
    } else if (*length < proto::kMinPasswordChars) {
        // 최소 길이는 **글자** 수로 센다. 바이트로 세면 한글 세 글자(9바이트)가
        // "8자 이상" 을 통과한다. 닉네임 검사와 같은 이유다.
        problem = "비밀번호는 8자 이상이어야 합니다";
    }

    if (problem != nullptr) {
        spdlog::info("register rejected: {} ({}) - {}", user, session.peer(), problem);
        finish(session, proto::encodeRegisterResult(false, problem));
        return true;
    }

    session.markAuthenticated();

    // 해싱이 22ms 라 로그인과 마찬가지로 IOCP 워커에서 하면 안 된다.
    auto self = session.shared_from_this();
    const LoginContext* context = &context_;
    LoginHandler* handler = this;

    const bool queued = context_.authQueue->submit([self, context, handler, user, secret] {
        std::string hash;
        try {
            hash = data::hashPassword(secret);
        } catch (const std::exception& e) {
            spdlog::error("failed to hash password for {}: {}", user, e.what());
            handler->finish(*self, proto::encodeRegisterResult(
                                       false, "서버 오류로 가입에 실패했습니다"));
            return;
        }

        const CreateAccountResult result = context->accounts->createAccount(user, hash);

        switch (result) {
            case CreateAccountResult::Created:
                spdlog::info("registered: {} ({})", user, self->peer());
                handler->finish(*self, proto::encodeRegisterResult(
                                           true, "가입이 완료되었습니다. 로그인 후 "
                                                 "캐릭터를 만드세요"));
                break;
            case CreateAccountResult::UsernameTaken:
                spdlog::info("register rejected: {} - username taken", user);
                handler->finish(
                    *self, proto::encodeRegisterResult(false, "이미 사용 중인 아이디입니다"));
                break;
            case CreateAccountResult::NotSupported:
                handler->finish(*self, proto::encodeRegisterResult(
                                           false, "이 서버는 가입을 받지 않습니다"));
                break;
            case CreateAccountResult::Error:
                handler->finish(*self, proto::encodeRegisterResult(
                                           false, "서버 오류로 가입에 실패했습니다"));
                break;
        }
    });

    if (!queued) {
        spdlog::warn("{}: auth queue full, refusing registration", session.peer());
        finish(session, proto::encodeRegisterResult(false, "서버가 혼잡합니다"));
    }
    return true;
}

// ------------------------------------------------------------------ 로그인

bool LoginHandler::handleLogin(TlsSession& session, const HeavenLogin::LoginRequest& request) {
    const auto* username = request.username();
    const auto* password = request.password();

    if (username == nullptr || username->size() == 0 ||
        username->size() > proto::kMaxUsernameChars ||
        (password != nullptr && password->size() > proto::kMaxPasswordBytes)) {
        spdlog::warn("{}: rejected malformed credentials", session.peer());
        fail(session, "아이디 또는 비밀번호 형식이 올바르지 않습니다");
        return true;
    }

    // 첫 유효 프레임을 받았으므로 핸드셰이크 타임아웃 대상에서 제외한다.
    // 여기서부터는 maxSessionLifetime 이 이 연결의 상한이다.
    session.markAuthenticated();

    // 여기서부터는 인증 스레드로 넘긴다. IOCP 워커는 즉시 반환한다.
    // 람다가 세션을 shared_ptr 로 붙잡으므로, 작업이 끝날 때까지 세션과
    // 이 핸들러(세션이 소유한다)가 살아 있다.
    auto self = session.shared_from_this();
    std::string user = username->str();
    std::string secret = password != nullptr ? password->str() : std::string();

    // context 는 main 이 소유하고 서버보다 오래 산다. 포인터를 값으로 복사한다.
    // 지역 참조를 참조로 캡처하면 onFrame 이 반환되는 순간 매달린 참조가 된다.
    const LoginContext* context = &context_;
    LoginHandler* handler = this;

    const bool queued = context_.authQueue->submit([self, context, handler,
                                                    user = std::move(user),
                                                    secret = std::move(secret)] {
        const auto account = context->accounts->authenticate(user, secret);

        if (!account.has_value()) {
            spdlog::info("login rejected: {} ({})", user, self->peer());
            handler->fail(*self, "아이디 또는 비밀번호가 올바르지 않습니다");
            return;
        }

        const std::vector<Character> characters =
            context->characters->listByAccount(account->id);

        {
            std::lock_guard<std::mutex> lock(handler->mutex_);
            handler->accountId_ = account->id;
            handler->stage_ = Stage::AwaitingSelection;
        }

        spdlog::info("login ok: {} (account {}, {}) - {} character(s)", user, account->id,
                     self->peer(), characters.size());

        // 연결을 끊지 않는다. 캐릭터를 고르는 두 번째 왕복이 남아 있다.
        self->send(proto::encodeLoginSuccess(
            characters, static_cast<std::uint8_t>(kMaxCharactersPerAccount)));
    });

    if (!queued) {
        spdlog::warn("{}: auth queue full, refusing login", session.peer());
        fail(session, "서버가 혼잡합니다. 잠시 후 다시 시도해 주세요");
    }
    return true;
}

// ------------------------------------------------------------- 캐릭터 생성

bool LoginHandler::handleCreateCharacter(TlsSession& session,
                                         const HeavenLogin::CreateCharacterRequest& request) {
    const auto* nickname = request.nickname();

    // 클라이언트는 도감번호로 말한다. 내부 id 는 배열 위치라 클라이언트 카탈로그
    // 순서에 묶이는데, 순서가 밀려 저장된 파트너가 다른 종족이 된 적이 있다.
    // species_id 는 도감번호를 모르는 구버전 클라를 위해서만 본다.
    std::uint16_t speciesId = request.species_id();
    if (const std::uint16_t dex = request.dex_number(); dex != 0) {
        const proto::SpeciesBase* byDex = proto::findSpeciesByDex(dex);
        if (byDex == nullptr) {
            resume(Stage::AwaitingSelection);
            session.send(proto::encodeCharacterList(false, "알 수 없는 파트너입니다", {}));
            return true;
        }
        speciesId = byDex->id;
    }

    if (nickname == nullptr || nickname->size() > proto::kMaxNicknameBytes) {
        resume(Stage::AwaitingSelection);
        session.send(proto::encodeCharacterList(false, "닉네임이 올바르지 않습니다", {}));
        return true;
    }

    const std::string nick = nickname->str();
    if (const char* problem = proto::validateNickname(nick)) {
        resume(Stage::AwaitingSelection);
        session.send(proto::encodeCharacterList(false, problem, {}));
        return true;
    }
    // 0 이면 파트너 없이 시작한다. 그 외에는 종족 표에 있어야 한다.
    const proto::SpeciesBase* starter =
        speciesId == 0 ? nullptr : proto::findSpecies(speciesId);
    if (speciesId != 0 && starter == nullptr) {
        resume(Stage::AwaitingSelection);
        session.send(
            proto::encodeCharacterList(false, "알 수 없는 파트너입니다", {}));
        return true;
    }

    // 표에 있다고 다 스타터인 것은 아니다. 클라이언트 위젯에도 같은 목록이
    // 있지만 그건 화면일 뿐이고, 고를 수 있는 것을 정하는 곳은 여기다.
    if (starter != nullptr && !proto::isStarterDex(starter->dex)) {
        resume(Stage::AwaitingSelection);
        session.send(
            proto::encodeCharacterList(false, "처음 고를 수 없는 포켓몬입니다", {}));
        return true;
    }

    // 클라이언트가 보낸 값이므로 범위를 강제한다. 인덱스가 음수면 클라이언트가
    // 배열 밖을 짚고, 부피가 범위를 넘으면 모델이 뒤틀린다.
    proto::AppearanceInfo appearance = proto::detail::readAppearance(request.appearance());

    const LoginContext* context = &context_;
    const std::uint64_t accountId = accountId_;

    return submitCharacterChange(session, [context, accountId, nick, speciesId, appearance] {
        const CreateCharacterResult result =
            context->characters->create(accountId, nick, speciesId, appearance);

        const char* message = nullptr;
        switch (result) {
            case CreateCharacterResult::Created:        message = "캐릭터를 만들었습니다"; break;
            case CreateCharacterResult::NicknameTaken:  message = "이미 사용 중인 닉네임입니다"; break;
            case CreateCharacterResult::SlotsFull:      message = "캐릭터 슬롯이 가득 찼습니다"; break;
            case CreateCharacterResult::UnknownSpecies: message = "알 수 없는 파트너입니다"; break;
            case CreateCharacterResult::NotAStarter:    message = "처음 고를 수 없는 포켓몬입니다"; break;
            case CreateCharacterResult::NotSupported:   message = "이 서버는 캐릭터 생성을 지원하지 않습니다"; break;
            case CreateCharacterResult::Error:          message = "서버 오류로 만들지 못했습니다"; break;
        }

        const bool ok = result == CreateCharacterResult::Created;
        if (ok) {
            spdlog::info("character created: {} (account {}, species {})", nick, accountId,
                         speciesId);
        } else {
            spdlog::info("character creation rejected for account {}: {}", accountId,
                         describe(result));
        }
        return std::pair{ok, message};
    });
}

// ------------------------------------------------------------- 캐릭터 선택

bool LoginHandler::handleSelectCharacter(TlsSession& session,
                                         const HeavenLogin::SelectCharacterRequest& request) {
    const std::uint64_t characterId = request.character_id();

    auto self = session.shared_from_this();
    const LoginContext* context = &context_;
    LoginHandler* handler = this;
    const std::uint64_t accountId = accountId_;

    const bool queued = context_.authQueue->submit([self, context, handler, accountId,
                                                    characterId] {
        // accountId 는 서버가 인증 후에 정한 값이다. 조회를 이걸로 좁히므로
        // 남의 캐릭터 번호를 보내도 찾히지 않는다.
        const auto character = context->characters->find(accountId, characterId);
        if (!character.has_value()) {
            spdlog::warn("account {} tried to select character {} which it does not own",
                         accountId, characterId);
            handler->finish(*self,
                            proto::encodeSelectCharacterFailure("캐릭터를 찾을 수 없습니다"));
            return;
        }

        // 서비스마다 audience 를 달리해 따로 서명한다. 한 장을 돌려쓰면
        // 필드 티켓으로 채팅 서버에 들어갈 수 있게 된다.
        const std::int64_t issued = proto::nowUnix();
        std::vector<proto::EndpointInfo> endpoints;
        endpoints.reserve(context->targets.size());

        for (const ServiceTarget& target : context->targets) {
            proto::TicketClaims claims;
            claims.issuer = context->issuer;
            claims.audience = target.service;
            claims.accountId = accountId;
            claims.characterId = character->id;
            claims.nickname = character->nickname;
            claims.issuedUnix = issued;
            claims.expiresUnix = issued + context->ticketTtlSeconds;

            try {
                endpoints.push_back({target.service, target.host, target.port,
                                     context->signer->sign(claims)});
            } catch (const std::exception& e) {
                spdlog::error("failed to sign the {} ticket for character {}: {}",
                              target.service, character->id, e.what());
                handler->finish(*self, proto::encodeSelectCharacterFailure(
                                           "서버 오류로 입장에 실패했습니다"));
                return;
            }
        }

        context->characters->touchPlayed(character->id);

        spdlog::info("entering as {} (character {}, account {}, {}) - {} ticket(s), valid {}s",
                     character->nickname, character->id, accountId, self->peer(),
                     endpoints.size(), context->ticketTtlSeconds);

        handler->finish(*self,
                        proto::encodeSelectCharacterSuccess(endpoints, character->nickname));
    });

    if (!queued) {
        spdlog::warn("{}: auth queue full, refusing character select", session.peer());
        finish(session, proto::encodeSelectCharacterFailure("서버가 혼잡합니다"));
    }
    return true;
}

// ------------------------------------------------------------- 캐릭터 삭제

bool LoginHandler::handleDeleteCharacter(TlsSession& session,
                                         const HeavenLogin::DeleteCharacterRequest& request) {
    const auto* confirm = request.confirm_nickname();
    if (confirm == nullptr || confirm->size() > proto::kMaxNicknameBytes) {
        resume(Stage::AwaitingSelection);
        session.send(proto::encodeCharacterList(false, "확인 입력이 올바르지 않습니다", {}));
        return true;
    }

    const LoginContext* context = &context_;
    const std::uint64_t accountId = accountId_;
    const std::uint64_t characterId = request.character_id();
    const std::string typed = confirm->str();

    return submitCharacterChange(session, [context, accountId, characterId, typed] {
        const data::DeleteResult result =
            context->characters->remove(accountId, characterId, typed);

        const char* message = nullptr;
        switch (result) {
            case data::DeleteResult::Deleted:      message = "캐릭터를 삭제했습니다"; break;
            case data::DeleteResult::NotFound:     message = "캐릭터를 찾을 수 없습니다"; break;
            case data::DeleteResult::NameMismatch: message = "닉네임이 일치하지 않습니다"; break;
            case data::DeleteResult::Nothing:      message = "지울 것이 없습니다"; break;
            case data::DeleteResult::NotUnlocked:  message = "해금하지 않은 포켓몬입니다"; break;
            case data::DeleteResult::NotSupported: message = "이 서버는 삭제를 지원하지 않습니다"; break;
            case data::DeleteResult::Error:        message = "서버 오류로 삭제하지 못했습니다"; break;
        }

        const bool ok = result == data::DeleteResult::Deleted;
        if (ok) {
            spdlog::info("character deleted: {} (character {}, account {})", typed, characterId,
                         accountId);
        } else {
            spdlog::info("character delete rejected for account {}: {}", accountId,
                         describe(result));
        }
        return std::pair{ok, message};
    });
}

// --------------------------------------------------------------- 파티 편집

bool LoginHandler::handleSetParty(TlsSession& session,
                                  const HeavenLogin::SetPartyRequest& request) {
    std::vector<std::uint16_t> dexNumbers;
    if (const auto* numbers = request.dex_numbers()) {
        // 상한을 먼저 본다. 저장소도 거르지만, 그전에 임의 길이 벡터를 만들지
        // 않는다 — 프레임 크기 제한 안이라도 굳이 받아 둘 이유가 없다.
        if (numbers->size() > data::kMaxPartySize) {
            resume(Stage::AwaitingSelection);
            session.send(proto::encodeCharacterList(false, "파티는 3마리까지입니다", {}));
            return true;
        }
        dexNumbers.reserve(numbers->size());
        for (const std::uint16_t dex : *numbers) {
            dexNumbers.push_back(dex);
        }
    }

    const LoginContext* context = &context_;
    const std::uint64_t accountId = accountId_;
    const std::uint64_t characterId = request.character_id();
    const std::uint16_t activeDex = request.active_dex();

    return submitCharacterChange(session, [context, accountId, characterId, dexNumbers, activeDex] {
        const data::PartyResult result =
            context->characters->setParty(accountId, characterId, dexNumbers, activeDex);

        const char* message = nullptr;
        switch (result) {
            case data::PartyResult::Ok:           message = "파티를 저장했습니다"; break;
            case data::PartyResult::NotFound:     message = "캐릭터를 찾을 수 없습니다"; break;
            case data::PartyResult::NotUnlocked:  message = "해금하지 않은 포켓몬입니다"; break;
            case data::PartyResult::TooMany:      message = "파티는 3마리까지입니다"; break;
            case data::PartyResult::Duplicate:    message = "같은 포켓몬을 두 번 넣을 수 없습니다"; break;
            case data::PartyResult::NotInParty:   message = "파티에 없는 포켓몬은 꺼낼 수 없습니다"; break;
            case data::PartyResult::NotSupported: message = "이 서버는 파티 편집을 지원하지 않습니다"; break;
            case data::PartyResult::Error:        message = "서버 오류로 저장하지 못했습니다"; break;
        }

        const bool ok = result == data::PartyResult::Ok;
        if (ok) {
            spdlog::info("party updated: character {} ({} members, active {})", characterId,
                         dexNumbers.size(), activeDex);
        }
        return std::pair{ok, message};
    });
}

// ------------------------------------------------------------- 파트너 방생

bool LoginHandler::handleReleasePartner(TlsSession& session,
                                        const HeavenLogin::ReleasePartnerRequest& request) {
    const LoginContext* context = &context_;
    const std::uint64_t accountId = accountId_;
    const std::uint64_t characterId = request.character_id();

    return submitCharacterChange(session, [context, accountId, characterId] {
        const data::DeleteResult result =
            context->characters->releasePartner(accountId, characterId);

        const char* message = nullptr;
        switch (result) {
            case data::DeleteResult::Deleted:      message = "파트너를 놓아주었습니다"; break;
            case data::DeleteResult::Nothing:      message = "놓아줄 파트너가 없습니다"; break;
            case data::DeleteResult::NotUnlocked:  message = "해금하지 않은 포켓몬입니다"; break;
            case data::DeleteResult::NotFound:     message = "캐릭터를 찾을 수 없습니다"; break;
            case data::DeleteResult::NameMismatch: message = "확인에 실패했습니다"; break;
            case data::DeleteResult::NotSupported: message = "이 서버는 방생을 지원하지 않습니다"; break;
            case data::DeleteResult::Error:        message = "서버 오류로 실패했습니다"; break;
        }

        const bool ok = result == data::DeleteResult::Deleted;
        if (ok) {
            spdlog::info("partner released: character {} (account {})", characterId, accountId);
        }
        return std::pair{ok, message};
    });
}

}  // namespace heaven::login
