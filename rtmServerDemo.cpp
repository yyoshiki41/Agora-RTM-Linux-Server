#include <iostream>
#include <cstdlib>
#include <memory>
#include <vector>
#include <string>
#include <thread>
#include <sstream>
#include <unistd.h>
#include <pthread.h>
#include <map>
#include <algorithm>
#include <chrono>

#include "IAgoraRtmService.h"

using namespace std;

string APP_ID = "";

class RtmEventHandler: public agora::rtm::IRtmServiceEventHandler {
  public:
    RtmEventHandler() {}
    ~RtmEventHandler() {}

    virtual void onLoginSuccess() override {
        cout << "on login success" << endl;
    }

    virtual void onLoginFailure(agora::rtm::LOGIN_ERR_CODE errorCode) override {
        cout << "on login failure: errorCode = " << errorCode << endl;
    }

    virtual void onLogout(agora::rtm::LOGOUT_ERR_CODE errorCode) override {
        cout << "on login out" << endl;
    }

    virtual void onConnectionStateChanged(agora::rtm::CONNECTION_STATE state,
                        agora::rtm::CONNECTION_CHANGE_REASON reason) override {
        cout << "on connection state changed: state = " << state << endl;
    }

    virtual void onSendMessageResult(long long messageId,
                        agora::rtm::PEER_MESSAGE_ERR_CODE state) override {
        cout << "on send message messageId: " << messageId << " state: "
             << state << endl;
    }

    virtual void onMessageReceivedFromPeer(const char *peerId,
                        const agora::rtm::IMessage *message) override {
        cout << "on message received from peer: peerId = " << peerId
             << " message = " << message->getText() << endl;
    }

    virtual void onAddOrUpdateChannelAttributesResult(long long requestId,
                        agora::rtm::ATTRIBUTE_OPERATION_ERR errorCode) override {
        cout << "on add or update channel attributes: requestId = " << requestId
             << " errorCode = " << errorCode << endl;
    }
};

class ChannelEventHandler: public agora::rtm::IChannelEventHandler {
  public:
    ChannelEventHandler(string channel) {
        channel_ = channel;    
    }
    ~ChannelEventHandler() {}

    virtual void onJoinSuccess() override {
        cout << "on join channel success" << endl;
    }

    virtual void onJoinFailure(agora::rtm::JOIN_CHANNEL_ERR errorCode) override{
        cout << "on join channel failure: errorCode = " << errorCode << endl;
    }

    virtual void onLeave(agora::rtm::LEAVE_CHANNEL_ERR errorCode) override {
        cout << "on leave channel: errorCode = " << errorCode << endl;
    }

    virtual void onMessageReceived(const char* userId,
                        const agora::rtm::IMessage *msg) override {
        cout << "receive message from channel: " << channel_.c_str()
             << " user: " << userId << " message: " << msg->getText()
             << endl;
    }

    virtual void onMemberJoined(agora::rtm::IChannelMember *member) override {
        cout << "member: " << member->getUserId() << " joined channel: "
             << member->getChannelId() << endl;
    }

    virtual void onMemberLeft(agora::rtm::IChannelMember *member) override {
        cout << "member: " << member->getUserId() << " lefted channel: "
             << member->getChannelId() << endl;
    }

    virtual void onGetMembers(agora::rtm::IChannelMember **members,
                    int userCount,
                    agora::rtm::GET_MEMBERS_ERR errorCode) override {
        cout << "list all members for channel: " << channel_.c_str()
             << " total members num: " << userCount << endl;
        for (int i = 0; i < userCount; i++) {
            cout << "index[" << i << "]: " << members[i]->getUserId();
        }
    }

    virtual void onSendMessageResult(long long messageId,
                    agora::rtm::CHANNEL_MESSAGE_ERR_CODE state) override {
        cout << "send messageId: " << messageId << " state: " << state << endl;
    }

    private:
        string channel_;
};

class Demo {
  public:
    Demo() {
        eventHandler_.reset(new RtmEventHandler());
        agora::rtm::IRtmService* p_rs = agora::rtm::createRtmService();
        rtmService_.reset(p_rs, [](agora::rtm::IRtmService* p) {
            p->release();                                                           
        });                                                                         

        if (!rtmService_) {
            cout << "rtm service created failure!" << endl;
            exit(0);
        }

        if (rtmService_->initialize(APP_ID.c_str(), eventHandler_.get())) {
            cout << "rtm service initialize failure! appid invalid?" << endl;
            exit(0);
        }
    }
    ~Demo() {
        rtmService_->release();
    }

  public:
    bool login(std::string token, std::string userId) {
        if (rtmService_->login(token.c_str(), userId.c_str())) {
            cout << "login failed!" << endl;
            return false;
        }
        cout << "here" << endl;
        return true;
    }

    void logout() {
        rtmService_->logout();
        cout << "log out!" << endl;
    }

    void p2pChat(const std::string& dst) {
        string msg;
        while(true) {
            cout << "please input message you want to send, or input \"quit\" "
                 << "to leave p2pChat" << endl;
            getline(std::cin, msg);
            if (msg.compare("quit") == 0) {
                return;
            } else {
                sendMessageToPeer(dst, msg);
            }
        }
    }

    void groupChat(const std::string& channel) {
        string msg;
        channelEvent_.reset(new ChannelEventHandler(channel));
        agora::rtm::IChannel * channelHandler =
            rtmService_->createChannel(channel.c_str(), channelEvent_.get());
        if (!channelHandler) {
            cout << "create channel failed!" << endl;
        }
        channelHandler->join();
        while(true) {
            cout << "please input message you want to send, or input \"quit\" "
                 << " to leave groupChat, or input \"members\" to list members"
                 << endl;
            getline(std::cin, msg);
            if (msg.compare("quit") == 0) {
                channelHandler->leave();
                return;
            } else if (msg.compare("members") == 0) {
                channelHandler->getMembers();
            } else {
                sendMessageToChannel(channelHandler, msg);
            }
        }
    }

    void sendViewMessageToChannel(const std::string& channel, string &msg) {
        channelEvent_.reset(new ChannelEventHandler(channel));
        agora::rtm::IChannel * channelHandler =
            rtmService_->createChannel(channel.c_str(), channelEvent_.get());
        if (!channelHandler) {
            cout << "create channel failed!" << endl;
        }
        // NOTE: You do not have to join the specified channel to update its attributes.
        // channelHandler->join();
        // channelHandler->leave();
        agora::rtm::IRtmChannelAttribute* channelAttribute =
            rtmService_->createChannelAttribute();
        channelAttribute->setKey("views");
        channelAttribute->setValue(msg.c_str());
        const agora::rtm::IRtmChannelAttribute* a = channelAttribute;
        agora::rtm::ChannelAttributeOptions * channelAttributeOptions = new agora::rtm::ChannelAttributeOptions();
        channelAttributeOptions->enableNotificationToChannelMembers = true;
        const agora::rtm::ChannelAttributeOptions* o = channelAttributeOptions;

        const auto now = std::chrono::system_clock::now();
        long long requestId = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();

        int ret = rtmService_->addOrUpdateChannelAttributes(channel.c_str(), &a, 1, *o, requestId);
        if (ret) {
            cout << "adds or updates the attributes of a specified channel failed! return code: " << ret
                 << endl;
        }
        channelAttribute->release();
    }

    void sendMessageToPeer(std::string peerID, std::string msg) {
        agora::rtm::IMessage* rtmMessage = rtmService_->createMessage();
        rtmMessage->setText(msg.c_str());
        int ret = rtmService_->sendMessageToPeer(peerID.c_str(),
                                        rtmMessage);
        rtmMessage->release();
        if (ret) {
            cout << "send message to peer failed! return code: " << ret
                 << endl;
        }
    }

    void sendMessageToChannel(agora::rtm::IChannel * channelHandler,
                            string &msg) {
        agora::rtm::IMessage* rtmMessage = rtmService_->createMessage();
        rtmMessage->setText(msg.c_str());
        channelHandler->sendMessage(rtmMessage);
        rtmMessage->release();
    }

    private:
        std::unique_ptr<agora::rtm::IRtmServiceEventHandler> eventHandler_;
        std::unique_ptr<ChannelEventHandler> channelEvent_;
        std::shared_ptr<agora::rtm::IRtmService> rtmService_;
};

int main(int argc, const char * argv[]) {
    ::srand(::time(NULL));
    if (argc != 5) {
        cout << "Usage: ./rtmServerDemo <userId> <token> <channel> <msg>" << endl;
        exit(-1);
    }
    string userId = argv[1];
    string token = argv[2];
    string channel = argv[3];
    string msg = argv[4];

    APP_ID = std::getenv("APP_ID");

    std::vector<std::unique_ptr<Demo>> DemoList;
    std::unique_ptr<Demo> tmp;
    tmp.reset(new Demo());
    DemoList.push_back(std::move(tmp));

    int index = 1;
    while(true) {
        bool ret = DemoList[index-1]->login(token, userId);
        std::this_thread::sleep_for(std::chrono::seconds(1));
        if (!ret) {
            continue;
        }

        DemoList[index-1]->sendViewMessageToChannel(channel, msg);
        break;
    }
    DemoList[index-1]->logout();

    exit(0);
}
