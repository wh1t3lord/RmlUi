#include "../include/RendererExtensions.h"
#include <RmlUi/Core/Core.h>
#include <RmlUi/Core/Log.h>
#include <RmlUi/Core/Platform.h>
#include <RmlUi/Core/Traits.h>

#if defined RMLUI_RENDERER_GL2

	#if defined RMLUI_PLATFORM_WIN32
		#include <RmlUi_Include_Windows.h>
		#include <gl/Gl.h>
	#elif defined RMLUI_PLATFORM_MACOSX
		#include <AGL/agl.h>
		#include <OpenGL/gl.h>
		#include <OpenGL/glext.h>
	#elif defined RMLUI_PLATFORM_UNIX
		#include <GL/gl.h>
		#include <GL/glext.h>
		#include <GL/glx.h>
		#include <RmlUi_Include_Xlib.h>
	#endif

#elif defined RMLUI_RENDERER_GL3 && !defined RMLUI_PLATFORM_EMSCRIPTEN

	#include <RmlUi_Include_GL3.h>

#elif defined RMLUI_RENDERER_GL3 && defined RMLUI_PLATFORM_EMSCRIPTEN

	#include <GLES3/gl3.h>

#elif defined RMLUI_RENDERER_DX12

	#include <RmlUi_Renderer_DX12.h>

#endif

#if defined RMLUI_RENDERER_GL2 || defined RMLUI_RENDERER_GL3

RendererExtensions::Image RendererExtensions::CaptureScreen()
{
	int viewport[4] = {}; // x, y, width, height
	glGetIntegerv(GL_VIEWPORT, viewport);

		bool status = p_render_interface->CaptureScreen(img.width, img.height, img.num_components, img.row_pitch, p_image_data, image_data_size);
		RMLUI_ASSERT(status && "failed to make a screenshot (some variants why: driver failure, early calling, OS failure)");

		if (!status)
			return Image();

		img.data = Rml::UniquePtr<Rml::byte[]>(p_image_data);

		return img;
	}

	if (!result)
		return Image();

	return image;
}

#elif defined RMLUI_RENDERER_DX12

RendererExtensions::Image RendererExtensions::CaptureScreen()
{
	auto* render_interface = rmlui_static_cast<RenderInterface_DX12*>(Rml::GetRenderInterface());
	if (!render_interface)
	{
		Rml::Log::Message(Rml::Log::LT_ERROR, "Could not capture screen. RmlUi render interface not set, or unexpected type.");
		return {};
	}

	RendererExtensions::Image image;
	if (!render_interface->CaptureScreen(image.width, image.height, image.num_components, image.data))
	{
		Rml::Log::Message(Rml::Log::LT_ERROR, "The DirectX 12 renderer was unable to capture screen.");
		return {};
	}

	return image;
}

#else

RendererExtensions::Image RendererExtensions::CaptureScreen()
{
	return Image();
}

#endif
