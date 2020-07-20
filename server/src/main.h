#ifndef MAIN_H
#define MAIN_H

#include <stdint.h>
#include "server.h"
#include "message.h"

class CConfig
{
public:
	bool m_Verbose;
	char m_aConfigFile[1024];
	char m_aWebDir[1024];
	char m_aTemplateFile[1024];
	char m_aJSONFile[1024];
	char m_aBindAddr[256];
	int m_Port;

	CConfig();
};

class CMain
{
	CConfig m_Config;
	CServer m_Server;
    MessageBot m_msgBot;

	struct CClient
	{
		bool m_Active;
		bool m_Disabled;
		bool m_Connected;
		int m_ClientNetID;
		int m_ClientNetType;
		char m_aUsername[128];
		char m_aName[128];
		char m_aType[128];
		char m_aHost[128];
		char m_aLocation[128];
		char m_aPassword[128];

		int64 m_TimeConnected;
		int64 m_LastUpdate;

		struct CStats
		{
			bool m_Online4;
			bool m_Online6;
			int64_t m_Uptime;
			double m_Load;
			int64_t m_NetworkRx;
			int64_t m_NetworkTx;
			int64_t m_NetworkIn;
			int64_t m_NetworkOut;
			int64_t m_MemTotal;
			int64_t m_MemUsed;
			int64_t m_SwapTotal;
			int64_t m_SwapUsed;
			int64_t m_HDDTotal;
			int64_t m_HDDUsed;
			double m_CPU;
			char m_aCustom[512];
			// Options
			bool m_Pong;
		} m_Stats;
        
        ClientObserver m_clientObserver;
	} m_aClients[NET_MAX_CLIENTS];

	struct CJSONUpdateThreadData
	{
		CClient *pClients;
		CConfig *pConfig;
		MessageBot *pMsgBot;
		volatile short m_ReloadRequired;
	} m_JSONUpdateThreadData;

	static void JSONUpdateThread(void *pUser);
    static void StartBotThread(void *pBot);
public:
	CMain(CConfig Config);

	void OnNewClient(int ClienNettID, int ClientID);
	void OnDelClient(int ClientNetID);
	int HandleMessage(int ClientNetID, char *pMessage);
	int ReadConfig();
	int Run();

	CClient *Client(int ClientID) { return &m_aClients[ClientID]; }
	CClient *ClientNet(int ClientNetID);
	const CConfig *Config() const { return &m_Config; }
	int ClientNetToClient(int ClientNetID);
};


#endif
