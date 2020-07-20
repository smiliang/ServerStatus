#ifndef MESSAGE_H
#define MESSAGE_H

#include <stdint.h>
#include <tgbot/tgbot.h>

enum
{
    MSG_CLIENT_UNKNOWN=0,
    MSG_CLIENT_ONLINE=1,
    MSG_CLIENT_OFFLINE=2,
    MSG_CLIENT_IPv4ONLINE=3,
    MSG_CLIENT_IPv4OFFLINE=4,
    MSG_CLIENT_IPv6ONLINE=5,
    MSG_CLIENT_IPv6OFFLINE=6,
    MSG_CLIENT_CPUOVERLOAD=7
};

class ClientObserver
{
private:
    char m_aUsername[128];
    bool m_Connected;
    bool m_Connected_last;
    bool m_Online4;
    bool m_Online4_last;
    bool m_Online6;
    bool m_Online6_last;
    
    int m_CPU;
    int m_CPUMax;
    int m_CPUMonitorTime;
    
    int m_msgType;
    double m_msgTime;
    double m_msgNum;
        
    void genMsg();
public:
    char m_aMsg[128];
    void setUsername(char *username);
    void setConnected(bool connected);
    void setIpv4Online(bool online);
    void setIpv6Online(bool online);
    void setCPU(int cpu);
    void shouldSendMsg();
    void resetMsg();
};

class MessageBot
{
private:
    TgBot::Bot *m_bot;
    int64_t m_chatId;
    bool setChatId(int64_t chatId);
    void readChatId();
    
public:
    void startBot();
    void sendMessage(char *msg);
};
#endif /* message_hpp */
