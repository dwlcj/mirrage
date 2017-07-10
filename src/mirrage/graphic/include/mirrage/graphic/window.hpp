/** Window & OpenGL-Context creation + management ****************************
 *                                                                           *
 * Copyright (c) 2016 Florian Oetke                                          *
 *  This file is distributed under the MIT License                           *
 *  See LICENSE file for details.                                            *
\*****************************************************************************/

#pragma once

#include <mirrage/graphic/settings.hpp>

#include <mirrage/utils/template_utils.hpp>
#include <mirrage/utils/units.hpp>

#include <vulkan/vulkan.hpp>

#include <vector>
#include <memory>
#include <string>


typedef struct SDL_Window SDL_Window;


namespace lux {
	namespace asset{
		class Asset_manager;
	}

namespace graphic {

	class Window : public util::Registered<Window, Context> {
		public:
			Window(Context& context, std::string name, std::string title, int display, int width, int height,
			       Fullscreen fullscreen);
			~Window();

			void on_present();
			auto surface() {return _surface.get();}
			auto window_handle() {return _window.get();}

			auto name()const noexcept -> auto& {
				return _name;
			}
			void title(const std::string& title);

			/// tries to change the window dimensions (true on success).
			/// Sets width and height to the closest supported values
			bool dimensions(int& width, int& height, Fullscreen fullscreen);

			auto width()const noexcept{return _width;}
			auto height()const noexcept{return _height;}
			auto viewport()const noexcept {return glm::vec4(0.f, 0.f, width(), height());}

		private:
			std::string _name;
			std::string _title;
			int         _display;
			int         _width, _height;
			Fullscreen  _fullscreen;

			std::unique_ptr<SDL_Window,void(*)(SDL_Window*)> _window;
			vk::UniqueSurfaceKHR                             _surface;

			double _frame_start_time = 0;
			float _delta_time_smoothed = 0;
			float _cpu_delta_time_smoothed = 0;
			float _time_since_last_FPS_output = 0;

			void _update_fps_timer(double present_started);
	};

}
}

