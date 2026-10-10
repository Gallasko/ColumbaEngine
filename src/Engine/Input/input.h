#ifndef INPUT_H
#define INPUT_H

#include <atomic>
#include <vector>

#include <SDL_scancode.h>
#include <SDL_gamecontroller.h>

#include "Maths/geometry.h"

namespace pg
{
	typedef uint8_t MouseButton;

	class Input
	{
	public:
		enum class InputState
		{
			INPUTERROR,
			INPUTREGISTERED,
			KEYPRESSED,
			KEYGRABBED,
			KEYRELEASED,
			MOUSEPRESS,
			MOUSEGRABBED,
			MOUSERELEASE,
			GAMEPADPRESSED,
			GAMEPADRELEASED
		};

	private:
		template <typename T>
		struct InputInstance
		{
			T value;
			InputState state;

			inline bool operator==(const InputInstance &rhs) const
			{
				return this->value == rhs.value;
			}

			inline bool operator==(const T &rhs) const
			{
				return this->value == rhs;
			}

		};

		typedef InputInstance<SDL_Scancode> KeyInstance;
		typedef InputInstance<MouseButton> ButtonInstance;
		typedef InputInstance<SDL_GameControllerButton> GamepadButtonInstance;

	public:
		Input() {}

		Input::InputState registerKeyInput(const SDL_Scancode& key, const Input::InputState& state);
		Input::InputState registerMouseInput(const MouseButton& button, const Input::InputState& state);
		Input::InputState registerMouseMove(const Point2D& mousePos, const Point2D& mouseDelta);
		Input::InputState registerGamepadInput(const SDL_GameControllerButton& button, const Input::InputState& state);
		Input::InputState registerGamepadAxisMove(const MouseButton& button, const Input::InputState& state);

		void grabKey(const SDL_Scancode& key);
		void grabMouse(const MouseButton& button);

		Input::InputState keyState(const SDL_Scancode& key) const;
		bool isKeyPressed(const SDL_Scancode& key) const;
		bool isKeyGrabbed(const SDL_Scancode& key) const;
		bool isKeyReleased(const SDL_Scancode& key) const;

		Input::InputState buttonState(const MouseButton& button) const;
		bool isButtonPressed(const MouseButton& button) const;
		bool isButtonGrabbed(const MouseButton& button) const;
		bool isButtonReleased(const MouseButton& button) const;

		// Whether the button went down since the last call, even if it is up again by now. A press
		// and its release may both come between two reads of the state (a tap on a touch screen
		// lasts a few milliseconds): read through this, it is a click all the same.
		bool takePress(const MouseButton& button);

		// Todo
		Input::InputState gamepadButtonState(const MouseButton& button) const;
		bool isGamepadButtonPressed(const MouseButton& button) const;
		bool isGamepadButtonGrabbed(const MouseButton& button) const;

		// Todo gamepad axis

		const Point2D& getMousePos() const;
		const Point2D& getMouseDelta() const;

		void addGamepad(SDL_GameController *controller);
		void removeGamepad();

		void clearGamepads();

		void updateInput(double deltaTime);

		void reset();

	public:
		float updateTime = 1.0f;

	private:
		std::vector<Input::KeyInstance> keyContainer;
		std::vector<Input::ButtonInstance> buttonContainer;

		// One a button, set by the event and taken by who reads the clicks, which may be another thread
		static constexpr size_t NbLatchedButtons = 8;
		std::atomic<bool> pressLatch[NbLatchedButtons] = {};
		std::vector<SDL_GameController*> gamepadContainer;
		// Todo game pad button and axis holders
		Point2D mousePos;
		Point2D mouseDelta;

		template <typename Container, typename Value>
		int findInputPos(const Value& value, const Container& container) const;
	};
}

#endif //INPUT_H