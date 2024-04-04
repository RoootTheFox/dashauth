#pragma once

#include "dashauth.hpp"
#include <Geode/utils/web.hpp>

using namespace geode::prelude;

#ifdef GEODE_IS_WINDOWS
    #ifdef DASHAUTH_EXPORTING
        #define DASHAUTH_DLL __declspec(dllexport)
    #else
        #define DASHAUTH_DLL __declspec(dllimport)
    #endif
#else
    #define DASHAUTH_DLL
#endif

namespace dashauth {
    class DASHAUTH_DLL SentDashAuthRequest {
        public:
        SentDashAuthRequest();
        ~SentDashAuthRequest();
        void then(std::function<void(std::string const&)>);
        [[nodiscard]]
        SentDashAuthRequest* except(std::function<void()>);

        void initialize(geode::Mod* mod, std::string server_url);
        private:

        std::function<void(std::string const&)> m_then_callback;
        std::function<void()> m_except_callback;
        geode::Mod* m_mod;
        std::string m_server_url;
    };

    class DASHAUTH_DLL DashAuthRequest {
        public:
        DashAuthRequest();
        ~DashAuthRequest();
        [[nodiscard]]
        SentDashAuthRequest* getToken(geode::Mod* mod, const std::string& server_url);

        friend class SentDashAuthRequest;
    };
};