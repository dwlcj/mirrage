/** The main menu ************************************************************
 *                                                                           *
 * Copyright (c) 2016 Florian Oetke                                          *
 *  This file is distributed under the MIT License                           *
 *  See LICENSE file for details.                                            *
\*****************************************************************************/

#pragma once

#include "meta_system.hpp"

#include <mirrage/gui/gui.hpp>
#include <mirrage/engine.hpp>
#include <mirrage/renderer/deferred_renderer.hpp>
#include <mirrage/utils/maybe.hpp>


namespace mirrage {

	class Test_screen : public Screen {
		public:
			Test_screen(Engine& engine);
			~Test_screen()noexcept;

		protected:
			void _update(util::Time delta_time)override;
			void _draw()override;

			void _on_enter(util::maybe<Screen&> prev) override;
			void _on_leave(util::maybe<Screen&> next) override;

			auto _prev_screen_policy()const noexcept -> Prev_screen_policy override {
				return Prev_screen_policy::discard;
			}

		private:
			util::Mailbox_collection _mailbox;
			
			Meta_system _meta_system;

			gui::Gui _gui;

			ecs::Entity_facet _camera;
			ecs::Entity_facet _sun;

			float _sun_elevation = 0.92;
			float _sun_azimuth = 1.22f;
			float _sun_color_temperature = 5600.f;

			glm::vec2 _look{};
			glm::vec3 _move{};
			bool _mouse_look = false;
			
			float _cam_yaw = 0;
			float _cam_pitch = 0;

			int _selected_preset = 0;

			void _set_preset(int);
			void _update_sun_position();

			void _draw_settings_window();
			void _draw_profiler_window();
	};

}