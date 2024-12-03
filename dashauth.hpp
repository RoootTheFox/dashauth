#pragma once

#include "dashauth.hpp"
#include <Geode/utils/web.hpp>
#include <vector>

using namespace geode::prelude;

namespace dashauth {
    enum AuthProgress {
        NONE,
        REQUESTING_CHALLENGE,
        SENDING_MESSAGE,
        GETTING_TOKEN,
        // auth is done at this point, rest is cleanup :3
        DONE_GETTING_SENT_MESSAGES,
        DONE_DELETING_MESSAGES
    };

    class SentDashAuthRequest {
        public:
        SentDashAuthRequest() {
            geode::log::info("what are you doing");
        };
        ~SentDashAuthRequest() {};
        void then(std::function<void(std::string const&)> callback) {
            this->m_then_callback = callback;

            // then is also responsible for calling the callback
            log::info("requesting auth challenge for {} (base {})", this->m_mod->getID(), this->m_server_url);

            auto token = geode::Mod::get()->getSavedValue<std::string>(fmt::format("dashauth_token_{}", this->m_mod->getID()));

            if (!token.empty()) {
                callback(token);
                return;
            }

            auto account_manager = GJAccountManager::sharedState();

            std::thread request_thread([this, account_manager] {
                auto req = geode::utils::web::WebRequest();
                req.timeout(std::chrono::seconds(6));

                this->m_listener.bind([this, account_manager] (web::WebTask::Event* e) {
                    if (web::WebResponse* value = e->getValue()) {
                        geode::log::info("got response uwu");
                        // The request finished!
                        switch (this->m_progress) {
                            case REQUESTING_CHALLENGE: {
                                geode::log::info("nyaaaaa !!");
                                //geode::log::info("nyaaaaaaaa!! {}", value);

                                auto json = value->json();
                                if (!json.isOk()) {
                                    auto message = fmt::format("failed to get challenge: {}", json.err());
                                    log::error("{}", message);
                                    this->m_except_callback(message);
                                    return;
                                }
                                auto response = json.unwrap();

                                // my own servers are non-spec compliant lmao
                                if (response.contains("success") && response["success"].asBool().unwrapOr(false)) {
                                    this->m_state_account_id = response["data"]["bot_account_id"].as<int>().unwrapOr(0);
                                    m_state_challenge = response["data"]["challenge"].asString().unwrapOr("");
                                    m_state_challenge_id = response["data"]["id"].asString().unwrapOr("");
                                } else {
                                    m_state_account_id = response["bot_account_id"].as<int>().unwrapOr(0);
                                    m_state_challenge = response["challenge"].asString().unwrapOr("");
                                    m_state_challenge_id = response["id"].asString().unwrapOr("");
                                }

                                // send private message to bot account
                                geode::log::debug("sending auth to bot {} with challenge {}", m_state_account_id, m_state_challenge);
                                this->m_progress = SENDING_MESSAGE;
                                // lambda moment
                                auto self = this;

                                auto req = web::WebRequest();
                                req.timeout(std::chrono::seconds(6));
                                req.bodyString(fmt::format("{}{}{}{}{}{}{}{}{}{}{}{}{}",
                                    "gameVersion=21",
                                    "&binaryVersion=35", // idk lmao
                                    "&secret=Wmfd2893gb7",
                                    "&gjp2=", account_manager->m_GJP2,
                                    "&accountID=", std::to_string(account_manager->m_accountID),
                                    "&toAccountID=", std::to_string(m_state_account_id),
                                    "&subject=", cocos2d::ZipUtils::base64URLEncode(fmt::format("auth-{}", m_state_challenge)),
                                    // body: "meow, delete me if you see this :3 (this is used for authentication)"
                                    "&body=XFFdQh0RUFdZVEVRElhUEV1UFUheQRJGVFQURl1YQhQIBhEZQFpcQhFdQRVEQlFWFVdeRhJUREVcV1tFWFdTQVheWhs=",
                                    "&secret=Wmfd2893gb7"));
                                req.userAgent("");
                                req.header("Content-Type", "application/x-www-form-urlencoded");

                                this->m_listener.setFilter(req.post("https://www.boomlings.com/database/uploadGJMessage20.php"));
                                break;
                            }
                            case SENDING_MESSAGE: {
                                auto text = value->string();
                                if (!text.isOk()) {
                                    auto message = fmt::format("failed to send gd message: {}", text.err());
                                    log::error("{}", message);
                                    this->m_except_callback(message);
                                    return;
                                }
                                auto response = text.unwrap();

                                if (response == "-1") {
                                    auto message = "failed to send gd message";
                                    log::error("{}", message);
                                    this->m_except_callback(message);
                                    return;
                                }

                                geode::log::info("message sent! {}", response);
                                this->m_progress = GETTING_TOKEN;

                                auto req = web::WebRequest();
                                req.timeout(std::chrono::seconds(12));
                                this->m_listener.setFilter(req.get(fmt::format("{}/challenge_complete/{}", this->m_server_url, m_state_challenge_id))); // TODO: DO NOT HARDCODE THIS !!!
                                break;
                            }
                            case GETTING_TOKEN: {
                                auto json = value->json();
                                if (!json.isOk()) {
                                    auto message = fmt::format("failed to get challenge: {}", json.err());
                                    log::error("{}", message);
                                    this->m_except_callback(message);
                                    return;
                                }
                                auto response = json.unwrap();

                                log::info("GOT API ACCESS TOKEN (REAL) (NOT FAKE): {}", response);

                                std::string token = "";
                                if (response.contains("success") && response["success"].asBool()) {
                                    token = response["data"].asString().unwrapOr("");
                                } else {
                                    token = "uh oh";
                                }

                                geode::Mod::get()->setSavedValue<std::string>(fmt::format("dashauth_token_{}", this->m_mod->getID()), token);
                                // this is safe because this runs on the gd thread (? does it anymore? idk)
                                this->m_then_callback(token);

                                // we'll keep running and clean up our mess like a good maid ^-^ :3
                                auto req = web::WebRequest();
                                req.timeout(std::chrono::seconds(6));
                                req.bodyString(fmt::format("{}{}{}{}{}{}",
                                                "secret=Wmfd2893gb7",
                                                "&gjp2=", account_manager->m_GJP2,
                                                "&accountID=", std::to_string(account_manager->m_accountID),
                                                "&getSent=1"));
                                req.userAgent("");
                                req.header("Content-Type", "application/x-www-form-urlencoded");
                                this->m_progress = DONE_GETTING_SENT_MESSAGES;
                                this->m_listener.setFilter(req.post("https://www.boomlings.com/database/getGJMessages20.php"));
                                break;
                            }
                            case DONE_GETTING_SENT_MESSAGES: {
                                auto text = value->string();
                                if (!text.isOk()) {
                                    log::error("failed to get gd messages: {}", text.err());
                                    return;
                                }

                                auto response = text.unwrap();
                                if (response == "-1") {
                                    log::error("failed to get gd messages: -1");
                                    return;
                                }

                                std::string res = response;
                                std::vector<std::string> messages_str;
                                std::string tmp;
                                std::stringstream res_ss(res);
                                while (getline(res_ss, tmp, '|')) {
                                    messages_str.push_back(tmp);
                                }

                                std::vector<std::string> messages_to_delete {};
                                for (auto message : messages_str) {
                                    std::vector<std::string> message_parts;
                                    std::string tmp;
                                    std::stringstream message_ss(message);
                                    while (getline(message_ss, tmp, ':')) {
                                        message_parts.push_back(tmp);
                                    }

                                    std::map<std::string, std::string> message_map;
                                    int counter = 0;
                                    for (auto part : message_parts) {
                                        counter++;
                                        if (counter % 2 == 0) {
                                            message_map[message_parts.at(counter - 2)] = part;
                                        }
                                    }
                                    if (stoi(message_map["2"]) == m_state_account_id) {
                                        log::info("found challenge account id: {}", m_state_account_id);
                                        messages_to_delete.push_back(message_map["1"]);
                                    }
                                }

                                if (messages_to_delete.empty()) {
                                    log::info("no messages to delete (???)");
                                    return;
                                }

                                std::string delete_str = "";
                                for (auto message_id : messages_to_delete) {
                                    delete_str += message_id + ",";
                                }
                                delete_str.pop_back();
                                log::info("deleting messages: {}", delete_str);

                                auto req = web::WebRequest();
                                req.timeout(std::chrono::seconds(6));
                                req.bodyString(fmt::format("{}{}{}{}{}{}{}{}",
                                                    "secret=Wmfd2893gb7",
                                                    "&gjp2=", account_manager->m_GJP2,
                                                    "&accountID=", std::to_string(account_manager->m_accountID),
                                                    "&messages=", delete_str,
                                                    "&isSender=1"));
                                req.userAgent("");
                                req.header("Content-Type", "application/x-www-form-urlencoded");
                                this->m_progress = DONE_DELETING_MESSAGES;
                                this->m_listener.setFilter(req.post("https://www.boomlings.com/database/deleteGJMessages20.php"));
                                break;
                            }
                            case DONE_DELETING_MESSAGES: {
                                // no checks !! :3
                                log::info("cleaned up messages like a good girl >w<");
                            }
                        }

                    } else if (web::WebProgress* progress = e->getProgress()) {
                        // The request is still in progress...
                        //log::info("pwogwess uwu");
                        //log::info("pwogwess >w< {} | up {}", progress.downloadProgress().unwrapOr(-1), progress.uploadProgress().unwrapOr(-1));
                    } else if (e->isCancelled()) {
                        // our request was cancelled
                        this->m_except_callback("request was cancelled");
                    }
                });
                this->m_progress = REQUESTING_CHALLENGE;
                this->m_listener.setFilter(req.get(fmt::format("{}/request_challenge/{}", this->m_server_url, account_manager->m_accountID)));
            });
            request_thread.detach();
        };

        [[nodiscard]]
        SentDashAuthRequest* except(std::function<void(std::string const&)> callback) {
            this->m_except_callback = callback;
            return this;
        }

        void initialize(geode::Mod* mod, std::string server_url) {
            this->m_mod = mod;
            this->m_server_url = server_url;
        }
        private:

        std::function<void(std::string const&)> m_then_callback;
        std::function<void(std::string const&)> m_except_callback;
        geode::Mod* m_mod;
        std::string m_server_url;

        // state stuff
        int m_state_account_id = 0;
        std::string m_state_challenge = "";
        std::string m_state_challenge_id = "";

        EventListener<web::WebTask> m_listener;
        AuthProgress m_progress = NONE;
    };

    class DashAuthRequest {
        public:
        DashAuthRequest() {
            geode::log::info("constructed auth request >~<");
        };
        ~DashAuthRequest() {
        };
        [[nodiscard]]
        SentDashAuthRequest* getToken(geode::Mod* mod, const std::string& server_url) {
            auto request = new SentDashAuthRequest();
            request->initialize(mod, server_url);
            return request;
        }

        friend class SentDashAuthRequest;
    };
};
