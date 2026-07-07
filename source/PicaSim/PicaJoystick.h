#include "../Platform/S3ECompat.h"

struct JoystickData 
{
    JoystickData();
    enum {MAX_ANALOGUEINPUTS=8, MAX_POVDIRECTIONS=4, MAX_BUTTONS=32};
    long mAnalogueInputs[MAX_ANALOGUEINPUTS];
    unsigned long mPOVDirections[MAX_POVDIRECTIONS];
    unsigned char mButtons[MAX_BUTTONS];
    char mName[512];
};

bool JoystickAvailable();

s3eResult GetJoystickStatus(JoystickData& joystick, int id);

void CalibrateJoystick();

void UpdateJoystick(int id);

void ResetJoystick();

bool ShowJoystickInGame(const struct GameSettings& gameSettings);

// Auto-detect a connected game controller / R/C transmitter and, if one is
// present and hasn't already been auto-configured this device, load a matching
// joystick preset (by SDL device name), enable joystick input and select the
// device. Respects manual setup: once a device has been auto-configured it is
// left alone (so the user's tweaks persist) until a different device appears.
// Controlled by GameSettings::mOptions.mAutoConfigureJoystick (default on).
// Returns true if it changed the settings this call.
bool AutoConfigureController(class GameSettings& gameSettings);
