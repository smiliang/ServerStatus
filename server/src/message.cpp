#include "message.h"
#include "system.h"
#include <cstring>
#include <cstdio>
#include <inttypes.h>

const char* c_botToken = "PLACE YOUR TOKEN HERE";
const char* c_chatPasswd = "PLACE YOUR PASSWD HERE";

const int offlineMsgDelay = 120;
const int msgInterval = 300;

void ClientObserver::setUsername(char *username)
{
    str_copy(m_aUsername, username, sizeof(m_aUsername));
}

void ClientObserver::setConnected(bool connected)
{
    m_Connected = connected;
}

void ClientObserver::setIpv4Online(bool online)
{
    m_Online4 = online;
}

void ClientObserver::setIpv6Online(bool online)
{
    m_Online6 = online;
}

void ClientObserver::setDisableIPv6Msg(bool disabled)
{
    m_disableIPv6Msg = disabled;
}

void ClientObserver::setCPU(int cpu)
{
    m_CPU = cpu;
    if (cpu > 89 && m_CPUMonitorTime == 0)
    {
        m_CPUMonitorTime = time_timestamp();//now
    }
    if (cpu > 89) {
        if (cpu > m_CPUMax) {
            m_CPUMax = cpu;
        }
    }
    else if(cpu < 90)
    {
        m_CPUMax = 0;
        m_CPUMonitorTime = 0;
    }
}

void ClientObserver::shouldSendMsg()
{
    int msgType = MSG_CLIENT_UNKNOWN;
    if (!m_Connected)
    {
        if (0 == m_lostConnectionTime)
        {
            m_lostConnectionTime = time_timestamp();
        }
        else if (time_timestamp() - m_lostConnectionTime > offlineMsgDelay)
        {//remind after 2 minutes to prevent continuous alert on unstable network
            msgType = MSG_CLIENT_OFFLINE;
        }
    }
    else if(m_Connected && m_lostConnectionTime > 0)
    {
        if (time_timestamp() - m_lostConnectionTime > offlineMsgDelay)
        {
            msgType = MSG_CLIENT_ONLINE;
        }
        m_lostConnectionTime = 0;
    }
    else if(!m_Online4)
    {
        if (0 == m_offline4Time)
        {
            m_offline4Time = time_timestamp();
        }
        else if(time_timestamp() - m_offline4Time > offlineMsgDelay)
        {
            msgType = MSG_CLIENT_IPv4OFFLINE;
        }
    }
    else if(m_Online4 && m_offline4Time > 0)
    {
        if (time_timestamp() - m_offline4Time > offlineMsgDelay)
        {
            msgType = MSG_CLIENT_IPv4ONLINE;
        }
        m_offline4Time = 0;
    }
    else if (!m_disableIPv6Msg && !m_Online6)
    {
        double now = time_timestamp();
        if (0 == m_offline6Time)
        {
            m_offline6Time = now;
        }
        else if(now - m_offline6Time > offlineMsgDelay && now - m_offline6Time < (offlineMsgDelay + 2*msgInterval + 10))
        {
            msgType = MSG_CLIENT_IPv6OFFLINE;
        }
    }
    else if(!m_disableIPv6Msg && m_Online6 && m_offline6Time > 0)
    {
        if (time_timestamp() - m_offline6Time > offlineMsgDelay)
        {
            msgType = MSG_CLIENT_IPv6ONLINE;
        }
        m_offline6Time = 0;
    }
    if (MSG_CLIENT_UNKNOWN == msgType && m_CPU > 89 && m_CPUMonitorTime > 0)
    {
        if (time_timestamp() - m_CPUMonitorTime > 180) {//Cpu heavy load over 180s.
            msgType = MSG_CLIENT_CPUOVERLOAD;
        }
    }

    if (MSG_CLIENT_UNKNOWN == msgType)
    {
        m_msgType = MSG_CLIENT_UNKNOWN;
        m_msgTime = 0;
        m_msgNum = 0;
    }
    else
    {
        if (msgType != m_msgType) {
            m_msgType = msgType;
            m_msgTime = time_timestamp();
            m_msgNum = 1;
            
            genMsg();
        }
        else {
            if (m_msgNum < 3)
            {//same msg limit to 3.
                int now = time_timestamp();
                if (now - m_msgTime > m_msgNum*msgInterval)
                {//remind every msgInterval*m_msgNum seconds.
                    m_msgTime = now;
                    ++m_msgNum;
                    
                    genMsg();
                }
            }
        }
    }
}

void ClientObserver::genMsg()
{
    if (MSG_CLIENT_UNKNOWN == m_msgType) {
        memset(m_aMsg, 0, sizeof(m_aMsg));
        return;
    }
    if (MSG_CLIENT_ONLINE == m_msgType)
    {
        str_format(m_aMsg, sizeof(m_aMsg), "Node %s is online now.", m_aUsername);
    }
    else if (MSG_CLIENT_OFFLINE == m_msgType)
    {
        str_format(m_aMsg, sizeof(m_aMsg), "Node %s is offline now.", m_aUsername);
    }
    else if (MSG_CLIENT_IPv4ONLINE == m_msgType)
    {
        str_format(m_aMsg, sizeof(m_aMsg), "Node %s IPv4 is online now.", m_aUsername);
    }
    else if (MSG_CLIENT_IPv4OFFLINE == m_msgType)
    {
        str_format(m_aMsg, sizeof(m_aMsg), "Node %s IPv4 is offline now.", m_aUsername);
    }
    else if (MSG_CLIENT_IPv6ONLINE == m_msgType)
    {
        str_format(m_aMsg, sizeof(m_aMsg), "Node %s IPv6 is online now.", m_aUsername);
    }
    else if (MSG_CLIENT_IPv6OFFLINE == m_msgType)
    {
        str_format(m_aMsg, sizeof(m_aMsg), "Node %s IPv6 is offline now.", m_aUsername);
    }
    else if (MSG_CLIENT_CPUOVERLOAD == m_msgType)
    {
        str_format(m_aMsg, sizeof(m_aMsg), "Node %s cpu overload, max cpu %d.", m_aUsername, m_CPUMax);
    }
    else
    {
        str_format(m_aMsg, sizeof(m_aMsg), "Node %s msgtype error %d.", m_aUsername, m_msgType);
    }
}

void ClientObserver::resetMsg()
{
    memset(m_aMsg, 0, sizeof(m_aMsg));
}

bool MessageBot::setChatId(int64_t chatId)
{
    m_chatId = chatId;
    
    constexpr const char *chatIdFileName = "chatId";
    IOHANDLE file = io_open(chatIdFileName, IOFLAG_WRITE);
    if (!file) {
        return false;
    }
    char aBuff[16];
    str_format(aBuff, sizeof(aBuff), "%" PRId64 "", chatId);
    io_write(file, aBuff, sizeof(aBuff));
    io_flush(file);
    io_close(file);
    return true;
}

void MessageBot::readChatId()
{
    constexpr const char *chatIdFileName = "chatId";
    IOHANDLE file = io_open(chatIdFileName, IOFLAG_READ);
    if (!file) {
        dbg_msg("Bot", "Couldn't open %s", chatIdFileName);
        return;
    }
    int fileSize = (int)io_length(file);
    char *pFileData = (char *)malloc(fileSize + 1);
    io_read(file, pFileData, fileSize);
    pFileData[fileSize] = 0;
    io_close(file);
    
    int64_t i;
    char c ;
    int scanned = sscanf(pFileData, "%" SCNd64 "%c", &i, &c);
    if (scanned == 1 && i > 0)
    {
        m_chatId = i;
        dbg_msg("Bot", "readChatId suc.");
    }
    else
    {
        dbg_msg("Bot", "readChatId failed. %c %d", c, i);
    }
    free(pFileData);
}

void MessageBot::startBot()
{
    readChatId();
    
    m_bot = new TgBot::Bot(c_botToken);
    m_bot->getEvents().onCommand("start", [&](TgBot::Message::Ptr message) {
        m_bot->getApi().sendMessage(message->chat->id, "Hi!");
    });
    m_bot->getEvents().onAnyMessage([&](TgBot::Message::Ptr message) {
        if (StringTools::startsWith(message->text, "/start")) {
            return;
        }
        if (StringTools::startsWith(message->text, "passwd")) {
            if (StringTools::endsWith(message->text, c_chatPasswd)) {
                if (setChatId(message->chat->id))
                {
                    m_bot->getApi().sendMessage(message->chat->id, "Connection succeeded.");
                }
                else {
                    m_bot->getApi().sendMessage(message->chat->id, "Temporary connection.");
                }
                return;
            }
        }
        if (m_chatId > 0 && message->chat->id == m_chatId) {
            m_bot->getApi().sendMessage(m_chatId, "The Connection is alive.");
        } else {
            m_bot->getApi().sendMessage(message->chat->id, "The bot is alive.");
        }
    });
    
    signal(SIGINT, [](int s) {
        dbg_msg("Bot", "SIGINT got %d\n", s);
        exit(0);
    });
    
    signal(SIGTERM, [](int s) {
        dbg_msg("Bot", "SIGINT got %d\n", s);
        exit(0);
    });

    try {
        dbg_msg("Bot username: %s\n", m_bot->getApi().getMe()->username.c_str());
        TgBot::TgLongPoll longPoll(*m_bot);
        while (true) {
            dbg_msg("Bot", "Long poll started \n");
            longPoll.start();
        }
    } catch (TgBot::TgException& e) {
        dbg_msg("Bot", "error: %s\n", e.what());
    }
}

void MessageBot::sendMessage(char *msg)
{
    if (m_chatId < 1) {
        dbg_msg("Bot", "chatId not exist, msg discard %s", msg);
        return;
    }
    m_bot->getApi().sendMessage(m_chatId, std::string(msg));
}
