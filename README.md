# UnicodeRcon (ASE · ArkApi 플러그인)

ARK: Survival Evolved 전용 서버(ArkApi/AseApi)에서 **RCON 본문을 UTF-8로 처리**해 한글 등 유니코드
`Broadcast` / `ServerChat` / 명령이 깨지지 않게 하는 플러그인입니다. ASSM(ARK 서버 관리기)에서 분리한
독립 저장소이며, 빌드에 필요한 **ArkApi SDK가 함께 들어 있어** 따로 받을 것이 없습니다.

원본: [Michidu/Ark-Server-Plugins · UnicodeRcon](https://github.com/Michidu/Ark-Server-Plugins/tree/master/UnicodeRcon/UnicodeRcon)
(동작 원리는 같고, 포트 대신 RCON 패킷 구조로 판별하도록 고친 2026 빌드).

## 구성
```
UnicodeRcon.sln            Visual Studio 솔루션 (이 파일을 여세요)
UnicodeRcon/               소스(.cpp/.h) + 프로젝트(.vcxproj)
Configs/config.json        플러그인 설정 (서버의 ArkApi/Plugins/UnicodeRcon/ 에 복사)
Configs/PluginInfo.json    ArkApi 플러그인 정보
sdk/                       ArkApi SDK (Public 헤더, Private/Ark/Globals.h, lib/ArkApi.lib) — MIT, sdk/LICENSE.md
addon.json                 ASSM 애드온 설치기가 읽는 메타 정보 (ASSM 없이 쓰면 무시해도 됨)
```

## 빌드
1. Visual Studio 2019/2022 (C++ 데스크톱 개발)로 `UnicodeRcon.sln` 열기
2. 구성 **Release / x64** 선택 → 빌드
3. 결과: `out\UnicodeRcon.dll`

SDK 찾는 순서: 환경 변수 `ARKAPI_SDK` → 솔루션 옆 `sdk\` (이 저장소). `API/ARK/Ark.h`를 못 찾으면 빌드 시작 전에
이유를 알려 주고 멈춥니다. 서버에 설치된 ArkApi 버전과 SDK 버전을 맞추세요.

## 설치
서버의 `ShooterGame\Binaries\Win64\ArkApi\Plugins\UnicodeRcon\` 폴더에
`UnicodeRcon.dll`, `Configs\config.json`, `Configs\PluginInfo.json` 을 넣고 서버를 재시작합니다.

## 설정 (`config.json`)
| 키 | 기본값 | 설명 |
|---|---|---|
| `RconPort` | `0` | 0이면 포트와 무관하게 RCON 패킷 구조로 판별(권장). 일부 빌드는 소켓 포트를 엉뚱하게(예: 32330) 돌려주므로 0 유지 |
| `Verbose` | `false` | 서버 시작 때 훅 설치·포트 정보를 한 번 기록 |
| `LogCommands` | `false` | RCON 명령을 받을 때마다 한 줄 기록 (켜면 서버 로그가 많이 늘어남) |

훅 설치 **실패**는 설정과 무관하게 항상 로그에 남습니다(ArkApi가 해당 함수 오프셋을 모르는 경우).

## RCON 클라이언트 쪽
본문을 UTF-8로 보내고 받으세요. (ASSM은 기본으로 UTF-8을 씁니다.)

## 라이선스
- `sdk/` : MIT (`sdk/LICENSE.md`, ServersHub/Framework-ArkServerApi · Michidu)
- 플러그인 소스: 원본 Michidu/Ark-Server-Plugins 를 기반으로 수정 — 원본 저장소의 라이선스 표기를 따릅니다.
