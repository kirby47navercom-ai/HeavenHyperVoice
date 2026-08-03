#pragma once

// Windows 자격증명 관리자에 DB 비밀번호를 보관한다.
//
// 저장된 값은 DPAPI 로 사용자 계정 키에 묶여 암호화된다. 같은 사용자만 읽을 수
// 있고, 다른 계정이나 다른 머신으로 파일을 복사해도 복호화되지 않는다.
//
// 이걸 쓰면 런처를 더블클릭해도 환경변수 설정 없이 DB 에 붙을 수 있다.
// 대안들의 문제:
//   - 명령줄 인자 : 프로세스 목록에서 다른 프로세스가 다 본다
//   - 설정 파일   : 평문. 실수로 커밋될 수 있다
//   - 환경변수    : 평문(레지스트리). 모든 자식 프로세스가 상속한다

#include <optional>
#include <string>

namespace heaven::login {

// 자격증명 관리자에 보이는 이름.
inline constexpr const char* kDbCredentialTarget = "HeavenHyperVoice/db";

// 없으면 nullopt.
std::optional<std::string> readStoredPassword(const std::string& target);

// 덮어쓴다. 실패하면 사유를 담아 예외를 던진다.
void storePassword(const std::string& target, const std::string& password);

// 저장된 항목을 지운다. 없으면 false.
bool erasePassword(const std::string& target);

}  // namespace heaven::login
