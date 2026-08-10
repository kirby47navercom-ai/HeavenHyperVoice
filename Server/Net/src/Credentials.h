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

namespace heaven::net {

// 자격증명 관리자에 보이는 이름.
inline constexpr const char* kDbCredentialTarget = "HeavenHyperVoice/db";
inline constexpr const char* kRedisCredentialTarget = "HeavenHyperVoice/redis";

// 없으면 nullopt.
std::optional<std::string> readStoredPassword(const std::string& target);

// 덮어쓴다. 실패하면 사유를 담아 예외를 던진다.
void storePassword(const std::string& target, const std::string& password);

// 저장된 항목을 지운다. 없으면 false.
bool erasePassword(const std::string& target);

// --- 명령줄 도구. 프롬프트로 받아 저장하고 종료 코드를 돌려준다 ---
//
// 비밀번호를 인자로 받지 않는 이유는 위와 같다. 콘솔에서 직접 받아야
// 프로세스 목록에도 셸 히스토리에도 남지 않는다.

// 화면에 남기지 않고 한 줄 읽는다.
std::string readHiddenLine();

// label 은 "database" 처럼 프롬프트에 쓸 이름이다.
int storePasswordInteractive(const std::string& target, const char* label);
int erasePasswordAndReport(const std::string& target, const char* label);

}  // namespace heaven::net
