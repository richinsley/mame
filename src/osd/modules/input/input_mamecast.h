#ifndef INPUT_MAMECAST_H_
#define INPUT_MAMECAST_H_

#include "input_module.h"
#include "modules/osdmodule.h"

// MAME headers
#include "emu.h"
#include "uiinput.h"
#include "strconv.h"
#include "unicode.h"

// MAMEOS headers
#include "input_common.h"
#include "../lib/osdobj_common.h"

#include "modules/mamecast_joydev.pb.h"

#define MAX_MAMECAST_GAMEPADS 4
#define MAX_MAMECAST_BUTTONS 12
#define JOY_PROTO_BUFFER_LEN 17

static const char *const mamecast_button_names[] = {
	"A",
	"B",
	"X",
	"Y",
	"LB",
	"RB",
	"LT",
	"RT",
	"ADDRESS",
	"MENU",
	"LEFT_STICK_PRESSED",
	"RIGHT_STICK_PRESSED",
};

// Index	Button .pressed Code	    Button on Xbox	    Button on PlayStation
// 0	    gamepad.buttons[0].pressed	A	                X
// 1	    gamepad.buttons[1].pressed	B	                O
// 2	    gamepad.buttons[2].pressed	X	                Square
// 3	    gamepad.buttons[3].pressed	Y	                Triangle
// 4	    gamepad.buttons[4].pressed	LB	                L1
// 5	    gamepad.buttons[5].pressed	RB	                R1
// 6	    gamepad.buttons[6].pressed	LT	                L2
// 7	    gamepad.buttons[7].pressed	RT	                R2
// 8	    gamepad.buttons[8].pressed	Show Address Bar	Share
// 9	    gamepad.buttons[9].pressed	Show Menu	        Options
// 10	    gamepad.buttons[10].pressed	Left Stick Pressed	Left Stick Pressed
// 11	    gamepad.buttons[11].pressed	Right Stick Pressed	Right Stick Pressed
// 12	    gamepad.buttons[12].pressed	Directional Up	    Directional Up
// 13	    gamepad.buttons[13].pressed	Directional Down	Directional Down
// 14	    gamepad.buttons[14].pressed	Directional Left	Directional Left
// 15	    gamepad.buttons[15].pressed	Directional Right	Directional Right
// 16	    gamepad.buttons[16].pressed	Xbox Light-Up Logo	PlayStation Logo

class mamecast_joystick_device : public device_info
{
public:
	mamecast_joystick_device(running_machine &machine, const char *name, const char *id, input_module &module, uint32_t index);

	void poll() override;
	void reset() override;
	void configure();
	void update(mamecast::Gamepad_Update& msg);
private:
	uint32_t _index;
	uint8_t	_buttons[MAX_MAMECAST_BUTTONS];
	int32_t _hat_up;
	int32_t _hat_down;
	int32_t _hat_left;
	int32_t _hat_right;
	int32_t _x_axis;
	int32_t _y_axis;
	uint8_t _lastBuffer[JOY_PROTO_BUFFER_LEN];

	// we'll keep an internal keyboard state so we can translate select/start to keyboard equiliant of insert coin/player start
	int32_t _keys[MAX_KEYS];
private:
	std::mutex                         m_device_lock;
	bool                               m_configured;
};

class mamecast_keyboard_device : public device_info
{
public:
	mamecast_keyboard_device(running_machine &machine, const char *name, const char *id, input_module &module);

	void poll() override;
	void reset() override;
	void configure();
	void update(int32_t * keymap);
private:

private:
	std::mutex	m_device_lock;
	bool		m_configured;
	int32_t		m_state[MAX_KEYS];	
	int32_t		_oldstate[MAX_KEYS];
};
#endif