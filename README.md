# DashAuth

A work-in-progress mod that aims to make developers' lives easier for implementing GD message based authentication.

example usage:
```c++
#include <Geode/Geode.hpp>
#include <rooot.dashauth/include/dashauth.hpp>

using namespace geode::prelude;

using namespace dashauth;

#include <Geode/modify/MenuLayer.hpp>
class $modify(MyMenuLayer, MenuLayer) {
	bool init() {
		if (!MenuLayer::init()) {
			return false;
		}

		auto myButton = CCMenuItemSpriteExtra::create(
			CCSprite::createWithSpriteFrameName("GJ_likeBtn_001.png"),
			this,
			menu_selector(MyMenuLayer::onMyButton)
		);

		auto menu = this->getChildByID("bottom-menu");
		menu->addChild(myButton);

		myButton->setID("my-button"_spr);

		menu->updateLayout();

		return true;
	}

	void onMyButton(CCObject*) {
		DashAuthRequest().getToken(Mod::get(), "http://localhost:61475/api/v1")->except([]() {
			log::info("failed to get token :c");
			FLAlertLayer::create("DashAuth Error", "Failed to get token. why? idfk lmfao", "OK")->show();
		})->then([](std::string const& token) {
			log::info("got token!! {} :3", token);
			FLAlertLayer::create("meow", fmt::format("{}", token), "bomb brazil")->show();
		});
	}
};
```
