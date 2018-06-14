#include <mirrage/gui/gui.hpp>

#include <glm/gtx/norm.hpp>


namespace mirrage::gui {

	namespace {
		bool color_picker(nk_context* ctx, nk_color& c, int width, bool hasAlpha)
		{
			if(nk_combo_begin_color(ctx, c, nk_vec2(width, width))) {
				nk_layout_row_dynamic(ctx, 120, 1);
				c = nk_color_picker(ctx, c, hasAlpha ? NK_RGBA : NK_RGB);

				nk_layout_row_dynamic(ctx, 14, 1);
				c.r = static_cast<nk_byte>(nk_propertyi(ctx, "#R:", 0, c.r, 255, 1, 1));
				c.g = static_cast<nk_byte>(nk_propertyi(ctx, "#G:", 0, c.g, 255, 1, 1));
				c.b = static_cast<nk_byte>(nk_propertyi(ctx, "#B:", 0, c.b, 255, 1, 1));
				if(hasAlpha) {
					c.a = static_cast<nk_byte>(nk_propertyi(ctx, "#A:", 0, c.a, 255, 1, 1));
				}

				nk_combo_end(ctx);

				return true;
			}
			return false;
		}
	} // namespace

	bool color_picker(nk_context* ctx, util::Rgb& color, int width, float factor)
	{
		nk_color c = nk_rgb(static_cast<int>(color.r * 255 / factor),
		                    static_cast<int>(color.g * 255 / factor),
		                    static_cast<int>(color.b * 255 / factor));

		if(color_picker(ctx, c, width, false)) {
			auto newColor = util::Rgb{c.r / 255.f * factor, c.g / 255.f * factor, c.b / 255.f * factor};

			if(glm::length2(newColor - color) > 0.0001f) {
				color = newColor;
				return true;
			}
		}
		return false;
	}

	bool color_picker(nk_context* ctx, util::Rgba& color, int width, float factor)
	{
		nk_color c = nk_rgba(static_cast<int>(color.r * 255 / factor),
		                     static_cast<int>(color.g * 255 / factor),
		                     static_cast<int>(color.b * 255 / factor),
		                     static_cast<int>(color.a * 255 / factor));

		if(color_picker(ctx, c, width, true)) {
			auto newColor = util::Rgba{
			        c.r / 255.f * factor, c.g / 255.f * factor, c.b / 255.f * factor, c.a / 255.f * factor};

			if(glm::length2(newColor - color) > 0.0001f) {
				color = newColor;
				return true;
			}
		}
		return false;
	}
} // namespace mirrage::gui
