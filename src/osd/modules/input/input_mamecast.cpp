#include "input_mamecast.h"
#include "modules/socketpipe.h"
#include <iostream>
#include "../lib/osdobj_common.h"
#include "modules/osdwindow.h"

// we'll pass select and start from the gamepad into the keyboard device as thier corresponding key code
// ergo, we'll need to top level pointer to the singular keybaord device
static mamecast_keyboard_device * _mamecast_keyboard = nullptr;

class keyboard_input_mamecast : public input_module_base
{
public:
	keyboard_input_mamecast()
		: 	input_module_base(OSD_KEYBOARDINPUT_PROVIDER, "mamecast"),
			_keyboardDevice(nullptr)
		{

		}

	int init(const osd_options &options) override { 
		// osd_socket_pipe::Instance().setDataCB(this);
		input_module_base::init(options);
		return 0; 
	}
	void input_init(running_machine &machine) override {
		_keyboardDevice = devicelist()->create_device<mamecast_keyboard_device>(machine, "Mamecst Keyboard 1", "Mamecst Keyboard 1", *this);
		_keyboardDevice->configure();
		_mamecast_keyboard = _keyboardDevice;
	};

	void poll_if_necessary(running_machine &machine) override {
		_keyboardDevice->poll();
    };
	
	bool should_poll_devices(running_machine& machine) override
	{
		return true;
	}

	// void onData(uint8_t * buffer, size_t len) {
	// 	mamecast::Gamepad_Update msg;
	// 	msg.ParseFromArray(buffer, len);
	// 	_gamepads[msg.joy_id()]->update(msg);
	// }
private:
	mamecast_keyboard_device * _keyboardDevice;
};

MODULE_DEFINITION(KEYBOARD_MAMECAST, keyboard_input_mamecast)

class mouse_input_mamecast : public input_module
{
public:
	mouse_input_mamecast()
		: input_module(OSD_MOUSEINPUT_PROVIDER, "mamecast") {}
	int init(const osd_options &options) override { return 0; }
	void input_init(running_machine &machine) override {};
	void poll_if_necessary(running_machine &machine) override {};
	void pause() override {};
	void resume() override {};
};

MODULE_DEFINITION(MOUSE_MAMECAST, mouse_input_mamecast)

class lightgun_input_mamecast : public input_module
{
public:
	lightgun_input_mamecast()
		: input_module(OSD_LIGHTGUNINPUT_PROVIDER, "mamecast") {}
	int init(const osd_options &options) override { return 0; }
	void input_init(running_machine &machine) override {};
	void poll_if_necessary(running_machine &machine) override {};
	void pause() override {};
	void resume() override {};
};

MODULE_DEFINITION(LIGHTGUN_MAMECAST, lightgun_input_mamecast)

class joystick_input_mamecast : public input_module_base, public ISocketPipeDataCB
{
public:
	joystick_input_mamecast()
		: input_module_base(OSD_JOYSTICKINPUT_PROVIDER, "mamecast") {

		}

	int init(const osd_options &options) override { 
		osd_socket_pipe::Instance().setDataCB(this);
		input_module_base::init(options);
		return 0; 
	}
	void input_init(running_machine &machine) override {
		for(uint32_t i = 0; i < MAX_MAMECAST_GAMEPADS; i++)
		{
			// allocate and link in a new device for each player
			char device_name[32];
			snprintf(device_name, sizeof(device_name), "Mamecast Player %u", i + 1);
			_gamepads[i] = devicelist()->create_device<mamecast_joystick_device>(machine, device_name, device_name, *this, i);

			// Configure each gamepad to add buttons and Axes, etc.
			_gamepads[i]->configure();
		}
	};

	void poll_if_necessary(running_machine &machine) override {
        for(int i = 0; i < MAX_MAMECAST_GAMEPADS; i++)
		{
			_gamepads[i]->poll();
		}
    };
	
	bool should_poll_devices(running_machine& machine) override
	{
		// return sdl_event_manager::instance().has_focus() && input_enabled();
		return true;
	}

	void onData(uint8_t * buffer, size_t len) {
		mamecast::Gamepad_Update msg;
		msg.ParseFromArray(buffer, len);
		_gamepads[msg.joy_id()]->update(msg);
	}
private:
	mamecast_joystick_device * _gamepads[MAX_MAMECAST_GAMEPADS];
};

MODULE_DEFINITION(JOYSTICK_MAMECAST, joystick_input_mamecast)

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// mamecast_joystick_device impl
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

mamecast_joystick_device::mamecast_joystick_device(running_machine &machine, const char *name, const char *id, input_module &module, uint32_t index)
	: device_info(machine, name, id, DEVICE_CLASS_JOYSTICK, module),
	_index(index)
{
	reset();
}

void mamecast_joystick_device::poll()
{
	std::lock_guard<std::mutex> scope_lock(m_device_lock);
	for(int button_index = 0; button_index < MAX_MAMECAST_BUTTONS; button_index++)
	{
		_buttons[button_index] = _lastBuffer[button_index] ? 0xff : 0x00;
	}

	_y_axis = _x_axis = 0;
	
	if(_lastBuffer[12])
	{
		_y_axis = -32768;
	} 
	else if(_lastBuffer[13])
	{
		_y_axis = 32767;
	}

	if(_lastBuffer[14])
	{
		_x_axis = -32768;
	} 
	else if(_lastBuffer[15])
	{
		_x_axis = 32767;
	}

	_hat_up = _lastBuffer[12] ? 0x80 : 0;
	_hat_down = _lastBuffer[13] ? 0x80 : 0;
	_hat_left = _lastBuffer[14] ? 0x80 : 0;
	_hat_right = _lastBuffer[15] ? 0x80 : 0;

}

void mamecast_joystick_device::reset()
{
	std::lock_guard<std::mutex> scope_lock(m_device_lock);
	memset(_lastBuffer, 0, JOY_PROTO_BUFFER_LEN);
	memset(_keys, 0, MAX_KEYS * sizeof(int32_t));
	_x_axis = 0;
	_y_axis = 0;
}

void mamecast_joystick_device::update(mamecast::Gamepad_Update& msg)
{
	std::lock_guard<std::mutex> scope_lock(m_device_lock);
	memcpy((void*)_lastBuffer, (void*)msg.buttons().c_str(), JOY_PROTO_BUFFER_LEN);

	// http://www.philipstorr.id.au/pcbook/book3/scancode.htm
	// https://docs.mamedev.org/usingmame/defaultkeys.html#default-arcade-game-controls
	// pass on the select and start to keyboard handler

	int select = 0;
	int start = 0;
	if(_index == 0)
	{
		// Player 1
		// coin		'5' = 0x2e
		// start	'1' = 0x16
		select = ITEM_ID_5;
		start = ITEM_ID_1;
	} 
	else if(_index == 1)
	{
		// coin		'6' = 0x36
		// start	'2' = 0x1e
		select = ITEM_ID_6;
		start = ITEM_ID_2;
	}
	else if(_index == 2)
	{
		// coin		'7' = 0x3d
		// start	'3' = 0x26
		select = ITEM_ID_7;
		start = ITEM_ID_3;
	}
	else if(_index == 3)
	{
		// coin		'8' = 0x3e
		// start	'4' = 0x25
		select = ITEM_ID_8;
		start = ITEM_ID_4;
	}
	_keys[select] = msg.buttons()[8] ? 0x80 : 0x00;
	_keys[start] = msg.buttons()[9] ? 0x80 : 0x00;
	_mamecast_keyboard->update(_keys);
}

void mamecast_joystick_device::configure()
{
	std::lock_guard<std::mutex> scope_lock(m_device_lock);

	// populate buttons - TODO mame doesn't have a discrete concept of player buttons, just ITEM_ID_BUTTON1 - ITEM_ID_BUTTON32
	// it does have ITEM_ID_START and 
	for (int butnum = 0; butnum < MAX_MAMECAST_BUTTONS; butnum++)
	{
		_buttons[butnum] = 0;
		device()->add_item(
			mamecast_button_names[butnum],
			static_cast<input_item_id>(ITEM_ID_BUTTON1 + butnum),
			generic_button_get_state<uint8_t>,
			&_buttons[butnum]);
	}

	device()->add_item(
		"LSX",
		ITEM_ID_XAXIS,
		generic_axis_get_state<int32_t>,
		&_x_axis);

	device()->add_item(
		"LSY",
		ITEM_ID_YAXIS,
		generic_axis_get_state<int32_t>,
		&_y_axis);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// mamecast_keyboard_device impl
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

mamecast_keyboard_device::mamecast_keyboard_device(running_machine &machine, const char *name, const char *id, input_module &module)
	: device_info(machine, name, id, DEVICE_CLASS_KEYBOARD, module)
{
	reset();
}

void mamecast_keyboard_device::poll()
{
	std::lock_guard<std::mutex> scope_lock(m_device_lock);
	memcpy((void*)m_state, (void*)_oldstate, MAX_KEYS * sizeof(int32_t));
}

void mamecast_keyboard_device::reset()
{
	std::lock_guard<std::mutex> scope_lock(m_device_lock);
	memset((void*)m_state, 0, MAX_KEYS * sizeof(int32_t));
	memset((void*)_oldstate, 0, MAX_KEYS * sizeof(int32_t));
}

void mamecast_keyboard_device::update(int32_t *keymap)
{
	std::lock_guard<std::mutex> scope_lock(m_device_lock);
	memcpy((void*)_oldstate, (void*)keymap, MAX_KEYS * sizeof(int32_t));
}

void mamecast_keyboard_device::configure()
{
	std::lock_guard<std::mutex> scope_lock(m_device_lock);

	// populate it
	for (int keynum = (int)ITEM_ID_0; keynum <= (int)ITEM_ID_9; keynum++)
	{
		input_item_id itemid = (input_item_id)keynum;

		// generate the default / modified name
		char defname[20];
		snprintf(defname, sizeof(defname) - 1, "Key (%d)", keynum);

		device()->add_item(defname, itemid, generic_button_get_state<std::int32_t>, &m_state[keynum]);
	}
}
