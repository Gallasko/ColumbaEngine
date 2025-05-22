#include "stdafx.h"

#include "input.h"

#include <algorithm>

#include "../logger.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <SDL2/SDL.h>
// #include <SDL_opengl_glext.h>
// #include <GLES2/gl2.h>
// #include <GLFW/glfw3.h>
#else
	#ifdef __linux__
	#include <SDL2/SDL.h>
	#elif _WIN32
	#include <SDL.h>
	#endif
#endif

namespace pg
{
	namespace
	{
		static constexpr char const * DOM = "Input";

		//Helper function
		template <typename Instance>
		static inline bool CheckReleased(const Instance& instance)
		{
			return instance.state == Input::InputState::KEYRELEASED || instance.state == Input::InputState::MOUSERELEASE;
		}
	}

	Input::InputState Input::registerKeyInput(const SDL_Scancode& key, const Input::InputState& state)
	{
		LOG_THIS_MEMBER(DOM);

		const auto instance = KeyInstance{key, state};
		const auto it = findInputPos(instance, keyContainer);

		switch(state)
		{
			case Input::InputState::KEYPRESSED:
				if (it == -1)
				{
					keyContainer.push_back(instance);
					return Input::InputState::INPUTREGISTERED;
				}
				else
					return Input::InputState::INPUTERROR;

			break;

			case Input::InputState::KEYGRABBED:
				if (it != -1)
				{
					keyContainer.at(it).state = Input::InputState::KEYGRABBED;
					return Input::InputState::INPUTREGISTERED;
				}
				else
					return Input::InputState::INPUTERROR;

			break;

			case Input::InputState::KEYRELEASED:
				if (it != -1)
				{
					keyContainer.at(it).state = Input::InputState::KEYRELEASED;
					return Input::InputState::INPUTREGISTERED;
				}
				else
					return Input::InputState::INPUTERROR;
			break;

			default:
				return Input::InputState::INPUTERROR;
			break;
		}
	}

	Input::InputState Input::registerMouseInput(const MouseButton& button, const Input::InputState& state)
	{
		LOG_THIS_MEMBER(DOM);

		const auto it = findInputPos(button, buttonContainer);
		switch(state)
		{
			case Input::InputState::MOUSEPRESS:
			{
				if (it == -1)
				{
					buttonContainer.push_back(Input::ButtonInstance{button, Input::InputState::MOUSEPRESS});
					return Input::InputState::INPUTREGISTERED;
				}
				else
					return Input::InputState::INPUTERROR;
			}
			break;

			case Input::InputState::MOUSEGRABBED:
			{
				if (it != -1)
				{
					buttonContainer.at(it).state = Input::InputState::MOUSEGRABBED;
					return Input::InputState::INPUTREGISTERED;
				}
				else
					return Input::InputState::INPUTERROR;
			}
			break;

			case Input::InputState::MOUSERELEASE:
			{
				if (it != -1)
				{
					buttonContainer.at(it).state = Input::InputState::MOUSERELEASE;
					return Input::InputState::INPUTREGISTERED;
				}
				else
					return Input::InputState::INPUTERROR;
			}
			break;

			default:
			{
				return Input::InputState::INPUTERROR;
			}
			break;
		}
	}

	Input::InputState Input::registerMouseMove(const MousePos& mousePos, const MousePos& mouseDelta)
	{
		LOG_THIS_MEMBER(DOM);

		this->mousePos = mousePos;
		this->mouseDelta += mouseDelta;

		return Input::InputState::INPUTREGISTERED;
	}

	void Input::grabKey(const SDL_Scancode& key)
	{
		LOG_THIS_MEMBER(DOM);

		registerKeyInput(key, Input::InputState::KEYGRABBED);
	}

	void Input::grabMouse(const MouseButton& button)
	{
		LOG_THIS_MEMBER(DOM);

		registerMouseInput(button, Input::InputState::MOUSEGRABBED);
	}

	Input::InputState Input::keyState(const SDL_Scancode& key) const
	{
		LOG_THIS_MEMBER(DOM);

		const auto it = findInputPos(key, keyContainer);
		return (it != -1) ? keyContainer.at(it).state : Input::InputState::INPUTERROR;
	}

	bool Input::isKeyPressed(const SDL_Scancode& key) const
	{
		LOG_THIS_MEMBER(DOM);

		return keyState(key) == Input::InputState::KEYPRESSED;
	}

	bool Input::isKeyGrabbed(const SDL_Scancode& key) const
	{
		LOG_THIS_MEMBER(DOM);

		return keyState(key) == Input::InputState::KEYGRABBED;
	}

	bool Input::isKeyReleased(const SDL_Scancode& key) const
	{
		LOG_THIS_MEMBER(DOM);

		return keyState(key) == Input::InputState::KEYRELEASED;
	}

	Input::InputState Input::buttonState(const MouseButton& button) const
	{
		LOG_THIS_MEMBER(DOM);

		const auto it = findInputPos(button, buttonContainer);
		return (it != -1) ? buttonContainer.at(it).state : Input::InputState::INPUTERROR;
	}

	bool Input::isButtonPressed(const MouseButton& button) const
	{
		LOG_THIS_MEMBER(DOM);

		return buttonState(button) == Input::InputState::MOUSEPRESS;
	}

	bool Input::isButtonGrabbed(const MouseButton& button) const
	{
		LOG_THIS_MEMBER(DOM);

		return buttonState(button) == Input::InputState::MOUSEGRABBED;
	}

	bool Input::isButtonReleased(const MouseButton& button) const
	{
		LOG_THIS_MEMBER(DOM);

		return buttonState(button) == Input::InputState::MOUSERELEASE;
	}

	const MousePos& Input::getMousePos() const
	{
		LOG_THIS_MEMBER(DOM);

		return this->mousePos;
	}

	const MousePos& Input::getMouseDelta() const
	{
		LOG_THIS_MEMBER(DOM);

		return this->mouseDelta;
	}

	void Input::addGamepad(SDL_GameController *controller)
	{
		gamepadContainer.push_back(controller);
	}


	void Input::removeGamepad()
	{

	}

	void Input::clearGamepads()
	{
		for (const auto& gamepad : gamepadContainer)
		{
			SDL_GameControllerClose(gamepad);
		}

		gamepadContainer.clear();
	}

	void Input::updateInput(double deltaTime)
	{
		LOG_THIS_MEMBER(DOM);

		updateTime = deltaTime;

		this->mouseDelta.x = 0;
		this->mouseDelta.y = 0;

		// Remove old data
		const auto removeKey = std::remove_if(keyContainer.begin(), keyContainer.end(), &CheckReleased<KeyInstance>);
		keyContainer.erase(removeKey, keyContainer.end());

		const auto removeButton = std::remove_if(buttonContainer.begin(), buttonContainer.end(), &CheckReleased<ButtonInstance>);
		buttonContainer.erase(removeButton, buttonContainer.end());
	}

	void Input::reset()
	{
		std::vector<Input::KeyInstance>().swap(keyContainer);
		std::vector<Input::ButtonInstance>().swap(buttonContainer);
	}

	template <typename Container, typename Value>
	int Input::findInputPos(const Value& value, const Container& container) const
	{
		for (long long unsigned int i = 0; i < container.size(); i++)
			if(value == container.at(i).value)
				return i;

		return -1;
	}

}