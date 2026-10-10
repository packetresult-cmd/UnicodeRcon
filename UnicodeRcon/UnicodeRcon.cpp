#include "UnicodeRcon.h"

// [송신] 응답 FString 을 UTF-8 패킷으로 직접 만들어 전송한다. (ARK 기본 ANSI 변환을 우회)
void Hook_RCONClientConnection_SendMessageW(RCONClientConnection* _this, int id, int type, FString* out_going_message)
{
	std::string msg = ArkApi::Tools::Utf8Encode(**out_going_message);
	const size_t len = msg.length();
	DWORD64 packet = reinterpret_cast<DWORD64>(FMemory::Malloc(len + 14));

	*(int*)packet = static_cast<int>(len + 10);
	*(int*)(packet + 4) = id;
	*(int*)(packet + 8) = type;

	memcpy((void*)(packet + 12), msg.data(), len);

	*(char*)(len + packet + 12) = 0;   // body NUL
	*(char*)(len + packet + 13) = 0;   // 패킷 끝 NUL

	auto socket = static_cast<FSocketBSD*>(_this->SocketField());

	int bytes_sent = 0;
	if (!socket->Send((char*)packet, static_cast<int>(len + 14), &bytes_sent))
		_this->IsClosedField() = true;

	FMemory::Free(reinterpret_cast<void*>(packet));

}

// [수신] RCON 소켓으로 들어온 명령 패킷(type==2)의 body 원본 바이트를 UTF-8로 디코드해 보관한다.
bool Hook_FSocketBSD_Recv(FSocketBSD* _this, char* Data, int BufferSize, int* BytesRead,
                          ESocketReceiveFlags::Type Flags)
{
	const bool ret = FSocketBSD_Recv_original(_this, Data, BufferSize, BytesRead, Flags);

	// 플레이어가 없으면 HandleNewPlayer 가 안 불려 포트를 못 잡으므로, 서버가 확실히 떠 있는 이 시점에 지연 로드.
	if (!loaded_rcon_port)
		LoadServerRconPort();

	const int nread = (BytesRead ? *BytesRead : 0);

	// 이 빌드에선 accepted RCON 소켓의 GetPortNo()가 리슨 포트가 아니라 엉뚱한 값(예: 32330)을 돌려줘
	// "포트로 RCON을 가려내는" 방식이 통하지 않는다(진단 로그로 확인). 따라서 포트 대신 **패킷 구조**로 판별한다.
	//   Source RCON: [size:4][id:4][type:4][body\0][\0]  → size>=10 이고 off+4+size<=nread 면 유효한 패킷.
	// config 로 RconPort 를 지정했고 그 포트와 일치하면 그것도 인정(엄격 모드). 지정 안 했으면(0) 구조만으로 판별.
	if (ret && Data && nread >= 14)
	{
		const bool port_ok = (config_rcon_port > 0) ? (_this->GetPortNo() == config_rcon_port) : true;
		if (port_ok)
		{
			int off = 0;
			while (off + 14 <= nread)
			{
				int size;  memcpy(&size, Data + off, 4);
				if (size < 10 || off + 4 + size > nread) break;   // 유효한 RCON 패킷 경계가 아님
				int packet_id;   memcpy(&packet_id,   Data + off + 4, 4);
				int packet_type; memcpy(&packet_type, Data + off + 8, 4);
				if (packet_type == 2)   // SERVERDATA_EXECCOMMAND
				{
					rcon_cmd_data = ArkApi::Tools::Utf8Decode(Data + off + 12).data();
					last_packet_id = packet_id;
					// 같은 패킷이 Recv 에서 두 번 보이는 빌드가 있어 id 중복은 한 번만 남긴다. 기본은 꺼짐(LogCommands).
					if (log_commands && packet_id != last_logged_id && Log::GetLog())
					{
						last_logged_id = packet_id;
						Log::GetLog()->info("[recv] RCON 명령 포착 id={} (port={}) → UTF-8 보관",
						                    packet_id, _this->GetPortNo());
					}
				}
				off += 4 + size;
			}
		}
	}

	return ret;
}

// [처리] 원본 ProcessRCONPacket 호출 직전에 Body 를 올바르게 디코드한 문자열로 덮어쓴다.
void Hook_RCONClientConnection_ProcessRCONPacket(RCONClientConnection* _this, RCONPacket* packet, UWorld* in_world)
{
	const bool authed = _this->IsAuthenticatedField();
	const bool matched = (last_packet_id == packet->Id);

	if (authed && matched)
	{
		packet->Body = rcon_cmd_data;
		rcon_cmd_data.Empty();
		last_packet_id = -1;
	}

	RCONClientConnection_ProcessRCONPacket_original(_this, packet, in_world);
}

// 첫 플레이어 처리 시점에도 포트를 확실히 읽어 둔다(원본 동작 유지).
bool _cdecl Hook_AShooterGameMode_HandleNewPlayer(AShooterGameMode* _this, AShooterPlayerController* Player,
                                                  UPrimalPlayerData* PlayerData, AShooterCharacter* PlayerCharacter,
                                                  bool bIsFromLogin)
{
	if (!loaded_rcon_port)
		LoadServerRconPort();
	return AShooterGameMode_HandleNewPlayer_original(_this, Player, PlayerData, PlayerCharacter, bIsFromLogin);
}

// SetHook 결과를 로그로 남기는 헬퍼 (실패하면 ArkApi 로그에 "... does not exist" 도 함께 찍힘).
template <typename T>
static void SetHookLogged(const char* name, LPVOID detour, T** original)
{
	const bool ok = ArkApi::GetHooks().SetHook(name, detour, original);
	if (Log::GetLog())
	{
		if (!ok)       Log::GetLog()->error("[hook] {} → 설치 실패! (ArkApi가 이 함수 오프셋을 모름 → 이 훅은 동작하지 않음)", name);
		else if (verbose) Log::GetLog()->info("[hook] {} → 설치 성공", name);
	}
}

void Load()
{
	Log::Get().Init("UnicodeRcon");
	LoadConfig();

	if (!loaded_rcon_port)
		LoadServerRconPort();

	SetHookLogged("RCONClientConnection.SendMessageW",
	              &Hook_RCONClientConnection_SendMessageW, &RCONClientConnection_SendMessageW_original);
	SetHookLogged("FSocketBSD.Recv",
	              &Hook_FSocketBSD_Recv, &FSocketBSD_Recv_original);
	SetHookLogged("RCONClientConnection.ProcessRCONPacket",
	              &Hook_RCONClientConnection_ProcessRCONPacket, &RCONClientConnection_ProcessRCONPacket_original);
	SetHookLogged("AShooterGameMode.HandleNewPlayer_Implementation",
	              &Hook_AShooterGameMode_HandleNewPlayer, &AShooterGameMode_HandleNewPlayer_original);

	if (Log::GetLog())
		Log::GetLog()->info("UnicodeRcon loaded. rcon_port={} (config={}, verbose={})",
		                    rconport, config_rcon_port, verbose);
}

void Destroy()
{
	ArkApi::GetHooks().DisableHook("RCONClientConnection.SendMessageW", &Hook_RCONClientConnection_SendMessageW);
	ArkApi::GetHooks().DisableHook("FSocketBSD.Recv", &Hook_FSocketBSD_Recv);
	ArkApi::GetHooks().DisableHook("RCONClientConnection.ProcessRCONPacket",
	                               &Hook_RCONClientConnection_ProcessRCONPacket);
	ArkApi::GetHooks().DisableHook("AShooterGameMode.HandleNewPlayer_Implementation",
	                               &Hook_AShooterGameMode_HandleNewPlayer);
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
		Load();
		break;
	case DLL_PROCESS_DETACH:
		Destroy();
		break;
	}
	return TRUE;
}
