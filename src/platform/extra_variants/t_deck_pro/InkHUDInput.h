#pragma once

#include "configuration.h"

#if defined(T_DECK_PRO) && defined(MESHTASTIC_INCLUDE_INKHUD)

#include "PowerFSM.h"
#include "graphics/niche/InkHUD/InkHUD.h"
#include "graphics/niche/InkHUD/Persistence.h"
#include "graphics/niche/InkHUD/SystemApplet.h"
#include "input/TouchScreenImpl1.h"
#include "input/kbI2cBase.h"
#include "modules/SystemCommandsModule.h"
#include "sleep.h"
#include <cstring>
#ifdef T_DECK_MAX
#include "graphics/niche/InkHUD/Applets/Max/LockApplet.h"
#include "graphics/niche/InkHUD/Applets/Max/MaxUi.h"
#endif

void attachTDeckProTouchInterrupt();

#ifdef T_DECK_MAX
enum class TDeckMaxKeyEvent : uint8_t { TAP, DOUBLE, HOLD };
#endif

class TDeckProInkHUDInput : public Observer<const InputEvent *>
{
  public:
    void begin()
    {
        observe(touchScreenImpl1);
        observe(&keyboard);
        sleepObserver.observe(&notifyDeepSleep);
        lightSleepObserver.observe(&notifyLightSleep);
        wakeObserver.observe(&notifyLightSleepEnd);
    }

#ifdef T_DECK_MAX
    // The three touch keys under the screen of the T-Deck Max, counted left to right:
    // 0 = heart, 1 = speech bubble, 2 = arrow. Outside menus they are fixed shortcuts:
    //   heart:  friends (favourites map; recent nodes if the map is not enabled)
    //   bubble: chat (channel 0, pressed again: direct messages)
    //   arrow:  next screen
    //   heart held for 1.5 s (or tapped twice): lock / unlock the screen
    // With the menu open: heart / arrow move up / down, bubble chooses. While typing: heart cancels, bubble sends.
    // The menu itself opens from the icon in the top right corner (see Events::onTouchTap).
    void touchKey(uint8_t id, TDeckMaxKeyEvent event)
    {
        auto *hud = NicheGraphics::InkHUD::InkHUD::getInstance();
        powerFSM.trigger(EVENT_INPUT);

        // Locked: only the unlock gesture does anything
        auto *lockApplet = hud->getSystemApplet("Lock");
        if (lockApplet && lockApplet->isForeground()) {
            if (id == 0 && event != TDeckMaxKeyEvent::TAP)
                NicheGraphics::InkHUD::LockApplet::unlock(hud);
            return;
        }

        bool typing = false;
        bool systemHandlingInput = false;
        for (auto *applet : hud->systemApplets) {
            typing |= applet->handleFreeText;
            systemHandlingInput |= applet->handleInput;
        }

        if (id == 0 && event != TDeckMaxKeyEvent::TAP && !typing) {
            NicheGraphics::InkHUD::LockApplet::lock(hud);
            return;
        }
        if (event == TDeckMaxKeyEvent::HOLD)
            return;

        switch (id) {
        case 0:
            if (typing)
                hud->freeTextCancel();
            else if (systemHandlingInput)
                hud->touchNavUp();
            else if (!hud->showApplet(MaxUi::A_FAVORITES))
                hud->showApplet(MaxUi::A_RECENTS);
            break;
        case 1:
            if (typing)
                hud->freeTextDone();
            else if (systemHandlingInput)
                hud->longpress();
            else {
                tDeckMaxUnread = 0;
                auto *active = hud->getActiveApplet();
                const bool onChannel0 =
                    hud->userApplets.size() > MaxUi::A_CHANNEL0 && active == hud->userApplets.at(MaxUi::A_CHANNEL0);
                if (!hud->showApplet(onChannel0 ? MaxUi::A_DMS : MaxUi::A_CHANNEL0))
                    hud->showApplet(MaxUi::A_CHANNEL0);
            }
            break;
        case 2:
            if (typing)
                break;
            if (systemHandlingInput)
                hud->touchNavDown();
            else
                hud->nextApplet();
            break;
        default:
            break;
        }
    }
#endif

  protected:
    int onNotify(const InputEvent *event) override
    {
        auto *hud = NicheGraphics::InkHUD::InkHUD::getInstance();
        auto &settings = hud->persistence->settings;
        const bool touch = event->source && strcmp(event->source, "touchscreen1") == 0;
        const bool wakeOnly = !touch && hud->isTouchTimedOut();
        powerFSM.trigger(EVENT_INPUT);
        if (wakeOnly)
            return 0;
        settings.joystick.alignment = (4 - settings.rotation) % 4;
        bool typing = false;
        bool systemHandlingInput = false;
        for (auto *applet : hud->systemApplets) {
            typing |= applet->handleFreeText;
            systemHandlingInput |= applet->handleInput;
        }

        auto input = event->inputEvent;
        const input_broker_event directions[] = {INPUT_BROKER_UP, INPUT_BROKER_RIGHT, INPUT_BROKER_DOWN, INPUT_BROKER_LEFT};
        for (uint8_t i = 0; touch && i < 4; ++i) {
            if (input == directions[i]) {
                input = directions[(i + 4 - settings.rotation % 4) % 4];
                break;
            }
        }

        switch (input) {
        case INPUT_BROKER_ALT_PRESS:
            tDeckProToggleBacklights();
            break;
        case INPUT_BROKER_USER_PRESS:
        case INPUT_BROKER_SELECT:
            if (touch) {
                uint16_t x = event->touchX;
                uint16_t y = event->touchY;
                if (x >= EINK_WIDTH || y >= EINK_HEIGHT)
                    return 0;
                rotateTouch(x, y, settings.rotation);
                if (event->inputEvent == INPUT_BROKER_USER_PRESS)
                    hud->touchTap(x, y);
                else
                    hud->touchLongPress(x, y);
            } else if (typing)
                hud->freeTextDone();
            else
                hud->longpress();
            break;
        case INPUT_BROKER_BACK:
            if (typing)
                hud->freeText('\b');
            else
                hud->exitShort();
            break;
        case INPUT_BROKER_CANCEL:
            if (typing)
                hud->freeTextCancel();
            else
                hud->exitShort();
            break;
        case INPUT_BROKER_UP:
            hud->touchNavUp();
            break;
        case INPUT_BROKER_DOWN:
            hud->touchNavDown();
            break;
        case INPUT_BROKER_LEFT:
            if (systemHandlingInput)
                hud->touchNavUp();
            else
                hud->prevApplet();
            break;
        case INPUT_BROKER_RIGHT:
            if (systemHandlingInput)
                hud->touchNavDown();
            else
                hud->nextApplet();
            break;
        case INPUT_BROKER_ANYKEY:
            if (event->kbchar == INPUT_BROKER_MSG_TAB)
                hud->openAppSwitcher();
            else if (event->kbchar == INPUT_BROKER_GPS_TOGGLE || event->kbchar == INPUT_BROKER_SEND_PING) {
                InputEvent command = *event;
                command.inputEvent = static_cast<input_broker_event>(event->kbchar);
                systemCommands.handleInputEvent(&command);
            } else if (event->kbchar == INPUT_BROKER_MSG_MUTE_TOGGLE)
                systemCommands.handleInputEvent(event);
            else if (typing && event->kbchar >= 0x20 && event->kbchar < 0x7F)
                hud->freeText(event->kbchar);
            break;
        default:
            break;
        }
        return 0;
    }

  private:
    SystemCommandsModule systemCommands;
    KbI2cBase keyboard{"TDeckProKeyboard"};
    CallbackObserver<TDeckProInkHUDInput, void *> sleepObserver{this, &TDeckProInkHUDInput::beforeSleep};
    CallbackObserver<TDeckProInkHUDInput, void *> lightSleepObserver{this, &TDeckProInkHUDInput::beforeLightSleep};
    CallbackObserver<TDeckProInkHUDInput, esp_sleep_wakeup_cause_t> wakeObserver{this, &TDeckProInkHUDInput::afterLightSleep};

    int beforeLightSleep(void *)
    {
        tDeckProUpdateBacklights(true);
        detachInterrupt(CST328_PIN_INT);
        return 0;
    }

    int afterLightSleep(esp_sleep_wakeup_cause_t)
    {
        tDeckProUpdateBacklights();
        attachTDeckProTouchInterrupt();
        return 0;
    }

    int beforeSleep(void *)
    {
        tDeckProUpdateBacklights(true);
#ifdef PIN_VIBRATION
        digitalWrite(PIN_VIBRATION, LOW);
#endif
        return 0;
    }

    static void rotateTouch(uint16_t &x, uint16_t &y, uint8_t rotation)
    {
        const uint16_t rawX = x;
        const uint16_t rawY = y;
        switch (rotation % 4) {
        case 1:
            x = rawY;
            y = EINK_WIDTH - 1 - rawX;
            break;
        case 2:
            x = EINK_WIDTH - 1 - rawX;
            y = EINK_HEIGHT - 1 - rawY;
            break;
        case 3:
            x = EINK_HEIGHT - 1 - rawY;
            y = rawX;
            break;
        }
    }
};

#endif
