#include "PicaJoystick.h"
#include "Trace.h"
#include "GameSettings.h"
#include "../Platform/S3ECompat.h"
#include <cstring>
#include <cctype>
#include <string>

static JoystickData sJoystickData;

//======================================================================================================================
// Map an SDL device name to a bundled joystick preset. isRawJoystick is true for
// devices SDL did NOT recognise as a standard game controller - those are almost
// always R/C transmitters or sim dongles, which use the FrSky-style 4-axis layout.
static const char* PresetForDeviceName(const std::string& rawName, bool isRawJoystick)
{
    std::string n = rawName;
    for (char& c : n) c = (char)tolower((unsigned char)c);

    auto has = [&](const char* s) { return n.find(s) != std::string::npos; };

    // Explicit R/C transmitter / sim-controller families (whether SDL mapped them or not).
    if (has("frsky") || has("taranis") || has("radiomaster") || has("jumper") ||
        has("spektrum") || has("flysky") || has("opentx") || has("edgetx") ||
        has("radio") || has("transmitter") || has("rc ") || has("usb-sdr") ||
        has("simulator") || has("joystick control"))
        return "SystemSettings/Joystick/FrSky.xml";

    // PlayStation family.
    if (has("playstation") || has("dualshock") || has("dualsense") ||
        has("ps3") || has("ps4") || has("ps5") || has("sony") || has("wireless controller"))
        return "SystemSettings/Joystick/PlayStation.xml";

    // Xbox / XInput family.
    if (has("xbox") || has("x-box") || has("xinput") || has("microsoft"))
        return "SystemSettings/Joystick/XBox360.xml";

    // Unknown device: a raw joystick is most likely a transmitter; a recognised
    // game controller gets the standard 2-stick Xbox layout.
    return isRawJoystick ? "SystemSettings/Joystick/FrSky.xml"
                         : "SystemSettings/Joystick/XBox360.xml";
}

//======================================================================================================================
bool AutoConfigureController(GameSettings& gameSettings)
{
    Options& options = gameSettings.mOptions;
    if (!options.mAutoConfigureJoystick)
        return false;

    if (gamepadGetNumDevices() == 0)
        return false;

    // Prefer the currently-selected device index if it exists, else device 0.
    uint32 index = 0;
    if (options.mJoystickID >= 0 && (uint32)options.mJoystickID < gamepadGetNumDevices())
        index = (uint32)options.mJoystickID;

    std::string name = gamepadGetName(index);
    if (name.empty())
        return false;

    // Already auto-configured for this exact device? Leave it (respect the user's
    // subsequent manual tweaks).
    if (name == options.mAutoConfiguredJoystickDevice)
        return false;

    bool isRaw = gamepadIsRawJoystick(index);
    const char* preset = PresetForDeviceName(name, isRaw);

    TRACE("AutoConfigureController: device '%s' (raw=%d) -> preset %s", name.c_str(), (int)isRaw, preset);

    if (!gameSettings.mJoystickSettings.LoadFromFile(preset))
    {
        TRACE("AutoConfigureController: failed to load preset %s", preset);
        return false;
    }

    gameSettings.mJoystickSettings.mEnableJoystick = true;
    options.mJoystickID = (int)index;
    options.mAutoConfiguredJoystickDevice = name;
    return true;
}

//======================================================================================================================
JoystickData::JoystickData() 
{
    memset(this, 0, sizeof(*this));
}

//======================================================================================================================
bool JoystickAvailable()
{
    return gamepadAvailable() && gamepadGetNumDevices() > 0;
}

//======================================================================================================================
s3eResult GetJoystickStatus(JoystickData& joystick, int id)
{
    TRACE_FUNCTION_ONLY(2);

    if (!JoystickAvailable())
        return S3E_RESULT_ERROR;

    joystick = sJoystickData;

    return S3E_RESULT_SUCCESS;
}

//======================================================================================================================
void CalibrateJoystick()
{
    gamepadCalibrate();
}

//======================================================================================================================
void UpdateJoystick(int id)
{
    TRACE_FILE_IF(2) TRACE("Updating joystick %d\n", id);

    gamepadUpdate();

    sJoystickData = JoystickData();
    if (JoystickAvailable() && id >= 0 && id < (int) gamepadGetNumDevices())
    {
        TRACE_FILE_IF(2) TRACE("Copying gamepad results");
        // Map [-4096,4096] to [0,65535]
        uint32 numAxes = gamepadGetNumAxes(id);
        for (uint32 i = 0 ; i < numAxes && i < JoystickData::MAX_ANALOGUEINPUTS ; ++i)
            sJoystickData.mAnalogueInputs[i] = (long) ((4096 + gamepadGetAxis(id, i)) * 65535.0f/8192.0f);

        if (gamepadIsPointOfViewAvailable(id))
            sJoystickData.mPOVDirections[0] = gamepadGetPointOfViewAngle(id);
  
        uint32 buttons = gamepadGetButtons(id);
        for (uint32_t i = 0 ; i != JoystickData::MAX_BUTTONS ; ++i)
            sJoystickData.mButtons[i] = (buttons & 1 << i) ? 128 : 0;

#if 0
        char txt[64];
        sprintf(txt, "buttons = %u", buttons);
        IwGxPrintString(10, 10, txt);
#endif

        sprintf(sJoystickData.mName, "Joystick %d", id);
    }
    else
    {
        sprintf(sJoystickData.mName, "Joystick %d unavailable", id);
        TRACE_FILE_IF(2) TRACE("No gamepad available");
    }
}

//======================================================================================================================
void ResetJoystick()
{
    gamepadReset();
}

//======================================================================================================================
bool ShowJoystickInGame(const GameSettings& gameSettings)
{
    if (gameSettings.mOptions.mFrameworkSettings.isIOS())
        return false;

    // Note: Previously checked for Android API level < 11, but that's ancient (Android 3.0)
    // and no longer relevant - all supported Android devices are newer

    return true;
}

