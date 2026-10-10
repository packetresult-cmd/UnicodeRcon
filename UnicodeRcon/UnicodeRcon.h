#pragma once
//
// UnicodeRcon (2026 빌드) — ASE 서버 유니코드(한글) RCON 지원 ArkApi 플러그인
//
// 원본: Michidu/Ark-Server-Plugins · UnicodeRcon
//       https://github.com/Michidu/Ark-Server-Plugins/tree/master/UnicodeRcon/UnicodeRcon
//
// 반복 로그 없음: 명령 포착 로그는 LogCommands, 시작 시 훅·포트 정보는 Verbose 일 때만 남깁니다(훅 실패는 항상).
// 로직은 원본과 동일하되, 어디서 끊기는지 로그로 드러나게 했습니다:
//   1) 네 개 훅의 SetHook() 반환값(성공/실패)을 각각 로그로 남김
//      → ArkApi가 해당 함수 오프셋을 모르면 SetHook 이 false 를 돌려주고 조용히 안 걸립니다(기존 버전의 함정).
//   2) 감지된 RCON 포트 값을 로그로 남김(0이면 감지 실패).
//   3) Recv 로 들어온 패킷 중 "RCON처럼 보이는"(type 2/3) 것의 실제 포트를 (상한 두고) 로그 → 실제 RCON 포트 확인.
//   4) ProcessRCONPacket / SendMessageW 통과 여부 로그.
// config.json 으로 RconPort 수동 지정과 Verbose on/off 가능.
//
#include <windows.h>

#include <fstream>
#include <string>

#include <API/ARK/Ark.h>
#include <json.hpp>

#pragma comment(lib, "ArkApi.lib")

DECLARE_HOOK(RCONClientConnection_SendMessageW, void, RCONClientConnection*, int, int, FString*);
DECLARE_HOOK(RCONClientConnection_ProcessRCONPacket, void, RCONClientConnection*, RCONPacket*, UWorld*);
DECLARE_HOOK(FSocketBSD_Recv, bool, FSocketBSD*, char*, int, int*, ESocketReceiveFlags::Type);
DECLARE_HOOK(AShooterGameMode_HandleNewPlayer, bool, AShooterGameMode*, AShooterPlayerController*, UPrimalPlayerData*,
             AShooterCharacter*, bool);

inline bool loaded_rcon_port = false;
inline FString rcon_cmd_data;
inline int last_packet_id = -1;
inline int last_logged_id = -1;   // LogCommands 중복 방지용
inline int rconport = 0;
inline int config_rcon_port = 0;   // config.json 의 RconPort (0이면 자동 감지)
inline bool verbose = false;       // config.json 의 Verbose (시작 시 포트 정보 로그, 기본 꺼짐)
inline bool log_commands = false;  // config.json 의 LogCommands (명령 포착마다 한 줄, 기본 꺼짐 — 서버 로그가 도배됨)

// config.json ( .../ArkApi/Plugins/UnicodeRcon/config.json ) 읽기. 없으면 자동 감지 + 로그 최소.
inline void LoadConfig()
{
	try
	{
		const std::string path = ArkApi::Tools::GetCurrentDir() +
			"/ArkApi/Plugins/UnicodeRcon/config.json";
		std::ifstream file(path);
		if (!file.is_open())
			return;
		nlohmann::json cfg;
		file >> cfg;
		file.close();
		config_rcon_port = cfg.value("RconPort", 0);
		verbose = cfg.value("Verbose", false);
		log_commands = cfg.value("LogCommands", false);
	}
	catch (...) { config_rcon_port = 0; verbose = false; log_commands = false; }
}

// 실행 중 서버의 RCON 소켓에서 실제 포트를 읽는다. config 값이 있으면 그걸 우선.
inline void LoadServerRconPort()
{
	if (config_rcon_port > 0)
	{
		rconport = config_rcon_port;
		loaded_rcon_port = true;
		if (Log::GetLog()) Log::GetLog()->info("UnicodeRcon: RCON 포트 = {} (config.json 지정)", rconport);
		return;
	}
	auto* gm = ArkApi::GetApiUtils().GetShooterGameMode();
	if (gm && gm->RCONSocketField())
	{
		// 현재 SDK에서 URCONServer::SocketField() 는 FSocket& 를 돌려주므로 주소(&)를 취해 FSocketBSD* 로 캐스팅.
		rconport = static_cast<FSocketBSD*>(&gm->RCONSocketField()->SocketField())->GetPortNo();
		loaded_rcon_port = true;
		if (Log::GetLog()) Log::GetLog()->info("UnicodeRcon: RCON 포트 자동 감지 = {}", rconport);
	}
	else if (Log::GetLog())
	{
		Log::GetLog()->warn("UnicodeRcon: 아직 ShooterGameMode/RCONSocket 없음 — 포트 감지 보류");
	}
}
