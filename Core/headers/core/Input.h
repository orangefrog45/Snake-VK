#pragma once
#include "events/EventManager.h"
#include "events/EventsCommon.h"

namespace ScriptInterface {
	class Input;
}

struct GLFWwindow;

namespace SNAKE {
	enum Key {
		// Alphabet keys
		A = 65,
		B = 66,
		C = 67,
		D = 68,
		E = 69,
		F = 70,
		G = 71,
		H = 72,
		I = 73,
		J = 74,
		K = 75,
		L = 76,
		M = 77,
		N = 78,
		O = 79,
		P = 80,
		Q = 81,
		R = 82,
		S = 83,
		T = 84,
		U = 85,
		V = 86,
		W = 87,
		X = 88,
		Y = 89,
		Z = 90,

		// Numeric keys
		Zero = 48,
		One = 49,
		Two = 50,
		Three = 51,
		Four = 52,
		Five = 53,
		Six = 54,
		Seven = 55,
		Eight = 56,
		Nine = 57,

		// Function keys
		F1 = 290,
		F2 = 291,
		F3 = 292,
		F4 = 293,
		F5 = 294,
		F6 = 295,
		F7 = 296,
		F8 = 297,
		F9 = 298,
		F10 = 299,
		F11 = 300,
		F12 = 301,

		// Special keys
		Space = 32,
		Enter = 257,
		Tab = 258,
		CapsLock = 280,
		Shift = 340,
		LeftControl = 341,
		RightControl = 345,
		Alt = 342,
		Escape = 256,
		Backspace = 259,
		Delete = 261,
		ArrowUp = 265,
		ArrowDown = 264,
		ArrowLeft = 263,
		ArrowRight = 262,
		PageUp = 266,
		PageDown = 267,
		Home = 268,
		End = 269,
		Insert = 260,
		Accent = 96,
	};



	class Input {
	public:
		enum MouseButton {
			LEFT_BUTTON = 0,
			RIGHT_BUTTON = 1,
			SCROLL = 2,
			NONE = 3,
		};

		enum MouseAction {
			UP = 0,
			DOWN = 1,
			MOVE = 2,
			TOGGLE_VISIBILITY,
		};

		enum InputType : uint8_t {
			RELEASE = 0, // GLFW_RELEASE
			PRESS = 1, // GLFW_PRESS
			HELD = 2 // GLFW_REPEAT
		};


		enum MouseEventType {
			RECEIVE, // Input received
			SET // State/window should be updated, e.g change cursor pos, listened for and handled in Window class
		};

		struct MouseEvent : public Event {
			MouseEvent(MouseEventType _event_type, MouseAction _action, MouseButton _button, glm::ivec2 _new_cursor_pos, glm::ivec2 _old_cursor_pos, std::any _data_payload = 0) :
				event_type(_event_type), mouse_action(_action), mouse_button(_button), mouse_pos_new(_new_cursor_pos), mouse_pos_old(_old_cursor_pos), data_payload(_data_payload) {};

			MouseEventType event_type;
			MouseAction mouse_action;
			MouseButton mouse_button;
			glm::ivec2 mouse_pos_new;
			glm::ivec2 mouse_pos_old;
			std::any data_payload;
		};


		struct KeyEvent : public Event {
			KeyEvent(unsigned _key, uint8_t _event_type) : key(_key), event_type(_event_type) {};

			unsigned int key;
			uint8_t event_type; // will be InputType enum, can't declare as such due to circular include
		};

		// Not case-sensitive
		// Returns true if key has been pressed this frame or is being held down
		bool IsKeyDown(Key key) {
			InputType state = m_key_states[key];
			return state != InputType::RELEASE;
		}

		// Not case-sensitive
		// Returns true if key has been pressed this frame or is being held down
		bool IsKeyDown(unsigned key) {
			InputType state = m_key_states[static_cast<Key>(std::toupper(key))];
			return (state != InputType::RELEASE);
		}

		// Not case-sensitive
		// Returns true only if key has been pressed this frame
		bool IsKeyPressed(Key key) {
			return m_key_states[key] == InputType::PRESS;
		}

		// Not case-sensitive
		// Returns true only if key has been pressed this frame
		bool IsKeyPressed(unsigned key) {
			return m_key_states[static_cast<Key>(std::toupper(key))] == InputType::PRESS;
		}


		bool IsMouseDown(MouseButton btn) {
			return m_mouse_states[btn] != InputType::RELEASE;
		}

		bool IsMouseClicked(MouseButton btn) {
			return m_mouse_states[btn] == InputType::PRESS;
		}

		bool IsMouseDown(unsigned btn) {
			return m_mouse_states[static_cast<MouseButton>(btn)] != InputType::RELEASE;
		}

		bool IsMouseClicked(unsigned btn) {
			return m_mouse_states[static_cast<MouseButton>(btn)] == InputType::PRESS;
		}

		glm::ivec2 GetMousePos() {
			return m_mouse_position;
		}


		glm::vec2 GetMouseDelta() {
			return m_mouse_position - m_last_mouse_position;
		}

		void OnUpdate(GLFWwindow* p_window);


		void KeyCallback(int key, [[maybe_unused]] int scancode, int action, [[maybe_unused]] int mods) {
			m_key_states[static_cast<Key>(key)] = static_cast<InputType>(action);
		}

		void MouseCallback(int button, int action, [[maybe_unused]] int mods) {
			auto& state = m_mouse_states[static_cast<MouseButton>(button)];
			if (action != MOVE) {
				state = static_cast<InputType>(action);
			}
		}

	private:
		glm::ivec2 m_mouse_position{ 0, 0 };
		glm::ivec2 m_last_mouse_position{ 0, 0 };

		std::unordered_map<Key, InputType> m_key_states;
		std::unordered_map<MouseButton, InputType> m_mouse_states;
	};
}
