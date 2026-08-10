#pragma once

// argon2id 비밀번호 해싱.
//
// 결과는 해시 하나가 아니라 자기 설명적인 인코딩 문자열이다.
//   $argon2id$v=19$m=19456,t=2,p=1$<salt-b64>$<hash-b64>
// 솔트와 파라미터가 안에 들어 있으므로 별도 컬럼이 필요 없고,
// 나중에 파라미터를 올려도 기존 행이 그대로 검증된다.
//
// 주의: 설계상 느리다(수십 ms). IOCP 워커 스레드에서 직접 호출하면
// 그 워커가 통째로 멈춘다. 반드시 별도 인증 스레드에서 돌릴 것.

#include <string>
#include <string_view>

namespace heaven::data {

// 새 비밀번호를 인코딩 문자열로 만든다. accounts.password_hash 에 그대로 넣는다.
// 실패하면 예외를 던진다.
std::string hashPassword(std::string_view password);

// 인코딩 문자열에서 솔트와 파라미터를 읽어 검증한다.
// 비교는 상수 시간이며, 잘못된 형식이면 그냥 false 를 반환한다.
bool verifyPassword(std::string_view password, const std::string& encoded);

}  // namespace heaven::data
