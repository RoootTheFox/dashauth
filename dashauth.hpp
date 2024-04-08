#pragma once

#include "dashauth.hpp"
#include <Geode/utils/web.hpp>
#include <vector>

using namespace geode::prelude;

namespace dashauth {
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
                geode::utils::web::AsyncWebRequest()
                    .timeout(std::chrono::seconds(6))
                    .fetch(fmt::format("{}/request_challenge/{}", this->m_server_url, account_manager->m_accountID))
                    .json()
                    .then([this, account_manager](matjson::Value const& response) {
                        geode::log::info("nyaaaaaaaa!! {}", response);
                        int account_id = 0;
                        std::string challenge = "";
                        std::string challenge_id = "";

                        // my own servers are non-spec compliant lmao
                        if (response.contains("success") && response["success"].as_bool()) {
                            account_id = response["data"]["bot_account_id"].as_int();
                            challenge = response["data"]["challenge"].as_string();
                            challenge_id = response["data"]["id"].as_string();
                        } else {
                            account_id = response["bot_account_id"].as_int();
                            challenge = response["challenge"].as_string();
                            challenge_id = response["id"].as_string();
                        }

                        // send private message to bot account
                        geode::log::debug("sending auth to bot {} with challenge {}", account_id, challenge);

                        // lambda moment
                        auto self = this;

                        geode::utils::web::AsyncWebRequest()
                            .bodyRaw(fmt::format("{}{}{}{}{}{}{}{}{}{}{}{}{}",
                                "gameVersion=21",
                                "&binaryVersion=35", // idk lmao
                                "&secret=Wmfd2893gb7",
                                "&gjp2=", account_manager->m_GJP2,
                                "&accountID=", std::to_string(account_manager->m_accountID),
                                "&toAccountID=", std::to_string(account_id),
                                "&subject=", cocos2d::ZipUtils::base64URLEncode(fmt::format("auth-{}", challenge)),
                                // body: "meow, delete me if you see this :3 (this is used for authentication)"
                                "&body=XFFdQh0RUFdZVEVRElhUEV1UFUheQRJGVFQURl1YQhQIBhEZQFpcQhFdQRVEQlFWFVdeRhJUREVcV1tFWFdTQVheWhs=",
                                "&secret=Wmfd2893gb7"))
                            .timeout(std::chrono::seconds(6))
                            .userAgent("")
                            .header("Content-Type", "application/x-www-form-urlencoded")
                            .post("https://www.boomlings.com/database/uploadGJMessage20.php")
                            .text()
                            .then([self, challenge_id, account_id, account_manager](std::string const& response) {
                                if (response == "-1") {
                                    auto message = "failed to send gd message";
                                    log::error("{}", message);
                                    self->m_except_callback(message);
                                    return;
                                }
                                geode::log::info("message sent! {}", response);

                                web::AsyncWebRequest()
                                    .timeout(std::chrono::seconds(12))
                                    .fetch(fmt::format("{}/challenge_complete/{}", self->m_server_url, challenge_id)) // TODO: DO NOT HARDCODE THIS !!!
                                    .json()
                                    .then([self, account_id, account_manager](matjson::Value const& response) {
                                        log::info("GOT API ACCESS TOKEN (REAL) (NOT FAKE): {}", response);

                                        std::string token = "";
                                        if (response.contains("success") && response["success"].as_bool()) {
                                            token = response["data"].as_string();
                                        } else {
                                            token = "uh oh";
                                        }

                                        geode::Mod::get()->setSavedValue<std::string>(fmt::format("dashauth_token_{}", self->m_mod->getID()), token);
                                        // this is safe because this runs on the gd thread
                                        self->m_then_callback(token);

                                        // delete challenge message
                                        geode::utils::web::AsyncWebRequest()
                                            .bodyRaw(fmt::format("{}{}{}{}{}{}",
                                                "secret=Wmfd2893gb7",
                                                "&gjp2=", account_manager->m_GJP2,
                                                "&accountID=", std::to_string(account_manager->m_accountID),
                                                "&getSent=1"))
                                            .timeout(std::chrono::seconds(6))
                                            .userAgent("")
                                            .header("Content-Type", "application/x-www-form-urlencoded")
                                            .post("https://www.boomlings.com/database/getGJMessages20.php")
                                            .text()
                                            .then([account_manager, account_id](std::string const& response) {
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
                                                    if (stoi(message_map["2"]) == account_id) {
                                                        log::info("found challenge account id: {}", account_id);
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
                                                geode::utils::web::AsyncWebRequest()
                                                    .bodyRaw(fmt::format("{}{}{}{}{}{}{}{}",
                                                        "secret=Wmfd2893gb7",
                                                        "&gjp2=", account_manager->m_GJP2,
                                                        "&accountID=", std::to_string(account_manager->m_accountID),
                                                        "&messages=", delete_str,
                                                        "&isSender=1"))
                                                    .timeout(std::chrono::seconds(6))
                                                    .userAgent("")
                                                    .header("Content-Type", "application/x-www-form-urlencoded")
                                                    .post("https://www.boomlings.com/database/deleteGJMessages20.php")
                                                    .text()
                                                    .then([](std::string const& response) {
                                                        log::info("deleted messages: {}", response);
                                                    }).expect([](std::string const& error) {
                                                        log::error("failed to delete messages: {}", error);
                                                    });
                                            });
                                    }).expect([self](std::string const& error) {
                                        auto message = fmt::format("failed to get api access token :( {}", error);
                                        log::error("{}", message);
                                        self->m_except_callback(message);
                                        return;
                                    });
                                })
                                .expect([self](std::string const& error) {
                                    auto message = fmt::format("failed to send gd message: {}", error);
                                    log::error("{}", message);
                                    self->m_except_callback(message);
                                    return;
                                });
                    }).expect([this](std::string const& error) {
                        auto message = fmt::format("failed to get challenge: {}", error);
                        log::error("{}", message);
                        this->m_except_callback(message);
                        return;
                    });
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