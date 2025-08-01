/*
 * This source file is part of RmlUi, the HTML/CSS Interface Middleware
 *
 * For the latest information, see http://github.com/mikke89/RmlUi
 *
 * Copyright (c) 2008-2010 CodePoint Ltd, Shift Technology Ltd
 * Copyright (c) 2019-2025 The RmlUi Team, and contributors
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

#include "RmlUi_Backend.h"
#include "RmlUi_Platform_SDL.h"
#include "RmlUi_Renderer_DX12.h"
#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/Core.h>
#include <RmlUi/Core/FileInterface.h>
#include <RmlUi/Core/Log.h>

#if SDL_MAJOR_VERSION == 2 && !defined(_WIN32)
	#error "Only the Vulkan SDL backend is supported."
#endif

/**
    Global data used by this backend.

    Lifetime governed by the calls to Backend::Initialize() and Backend::Shutdown().
 */
struct BackendData {
	SystemInterface_SDL system_interface;
	RenderInterface_DX12* render_interface = nullptr;
	Backend::RmlRenderInitInfo* p_default_pointer = nullptr;
	SDL_Window* window = nullptr;

	bool running = true;
};
static Rml::UniquePtr<BackendData> data;

bool Backend::Initialize(const char* window_name, int width, int height, bool allow_resize, RmlRenderInitInfo* p_info)
{
	RMLUI_ASSERT(!data);
	if (p_info)
	{
		p_info = nullptr;
	}
#if SDL_MAJOR_VERSION >= 3
	if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS))
		return false;
#else
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_TIMER) != 0)
		return false;
#endif

	// Submit click events when focusing the window.
	SDL_SetHint(SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH, "1");

#if SDL_MAJOR_VERSION >= 3
	const float window_size_scale = SDL_GetDisplayContentScale(SDL_GetPrimaryDisplay());
	SDL_PropertiesID props = SDL_CreateProperties();
	SDL_SetStringProperty(props, SDL_PROP_WINDOW_CREATE_TITLE_STRING, window_name);
	SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_X_NUMBER, SDL_WINDOWPOS_CENTERED);
	SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_Y_NUMBER, SDL_WINDOWPOS_CENTERED);
	SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, int(width * window_size_scale));
	SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER, int(height * window_size_scale));
	SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_VULKAN_BOOLEAN, false);
	SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_RESIZABLE_BOOLEAN, allow_resize);
	SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_HIGH_PIXEL_DENSITY_BOOLEAN, true);
	SDL_Window* window = SDL_CreateWindowWithProperties(props);
	SDL_DestroyProperties(props);
#else
	const Uint32 window_flags = ((allow_resize ? SDL_WINDOW_RESIZABLE : 0));
	SDL_Window* window = SDL_CreateWindow(window_name, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width, height, window_flags);
	// SDL2 implicitly activates text input on window creation. Turn it off for now, it will be activated again e.g. when focusing a text input field.
	SDL_StopTextInput();
#endif

	if (!window)
	{
		Rml::Log::Message(Rml::Log::LT_ERROR, "SDL error on create window: %s", SDL_GetError());
		return false;
	}

	data = Rml::MakeUnique<BackendData>();
	data->window = window;

	if (!p_info)
	{
		HWND window_handle{};
#if SDL_MAJOR_VERSION == 2
		SDL_SysWMinfo wmInfo;
		SDL_VERSION(&wmInfo.version);
		if (!SDL_GetWindowWMInfo(window, &wmInfo))
		{
			// Handle error (e.g., print SDL_GetError() and exit)
			const char* error = SDL_GetError();
			SDL_Log("SDL_GetWindowWMInfo failed: %s", error);
			// Handle failure appropriately
			return false;
		}

		if (wnInfo.info.win.window == nullptr)
		{
			SDL_Log("invalid win32 handle! failed to initialize renderer dx12 on sdl!");
			// Handle failure appropriately
			return false;
		}

		window_handle = wmInfo.info.win.window;
#elif SDL_MAJOR_VERSION == 3
		SDL_PropertiesID props = SDL_GetWindowProperties(window);

		if (props == 0)
		{
			const char* error = SDL_GetError();
			SDL_Log("SDL_GetWindowProperties failed: [%s]. Failed to initialize renderer dx12 on sdl", error);
			return false;
		}

		void* p_not_casted_hwnd = SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr);

		if (p_not_casted_hwnd == nullptr)
		{
			const char* error = SDL_GetError();
			SDL_Log("SDL_GetPointerProperty failed to obtained win32 hwnd: [%s]. Failed to initialize renderer dx12 on sdl", error);
			return false;
		}

		window_handle = reinterpret_cast<HWND>(p_not_casted_hwnd);
#endif

		p_info = new RmlRenderInitInfo(window_handle, true);
		p_info->Get_Settings().vsync = false;

		RMLUI_ASSERT(data->p_default_pointer == nullptr && "must be not initialized! probably you forgot to call backend::shutdown");
		data->p_default_pointer = p_info;
	}

	data->render_interface = RmlDX12::Initialize(nullptr, p_info);

	if (!data->render_interface)
	{
#ifdef _WIN32
		MessageBoxA(NULL, "failed to create render interface dx12", "FATAL", 0);
#endif
		SDL_DestroyWindow(data->window);
		data.reset();
		SDL_Quit();
		Rml::Log::Message(Rml::Log::LT_ERROR, "Failed to initialize Vulkan render interface");
		return false;
	}

	data->system_interface.SetWindow(window);
	data->render_interface->SetViewport(width, height);

	return true;
}

void Backend::Shutdown()
{
	RMLUI_ASSERT(data);
	RMLUI_ASSERT(data->render_interface);

	if (data->p_default_pointer)
	{
		delete data->p_default_pointer;
		data->p_default_pointer = nullptr;
	}

	RmlDX12::Shutdown(data->render_interface);

	if (data->render_interface)
	{
		delete data->render_interface;	
		data->render_interface=nullptr;
	}

	SDL_DestroyWindow(data->window);

	data.reset();

	SDL_Quit();
}

Rml::SystemInterface* Backend::GetSystemInterface()
{
	RMLUI_ASSERT(data);
	return &data->system_interface;
}

Rml::RenderInterface* Backend::GetRenderInterface()
{
	RMLUI_ASSERT(data);
	return data->render_interface;
}

static bool WaitForValidSwapchain()
{
#if SDL_MAJOR_VERSION >= 3
	constexpr auto event_quit = SDL_EVENT_QUIT;
#else
	constexpr auto event_quit = SDL_QUIT;
#endif

	bool result = true;

	// In some situations the swapchain may become invalid, such as when the window is minimized. In this state the renderer cannot accept any render
	// calls. Since we don't have full control over the main loop here we may risk calls to Context::Render if we were to return. Instead, we keep the
	// application inside this loop until we are able to recreate the swapchain and render again.
	while (!data->render_interface->IsSwapchainValid())
	{
		SDL_Event ev;
		while (SDL_PollEvent(&ev))
		{
			if (ev.type == event_quit)
			{
				// Restore the window so that we can recreate the swapchain, and then properly release all resource and shutdown cleanly.
				SDL_RestoreWindow(data->window);
				result = false;
			}
		}
		SDL_Delay(10);
		data->render_interface->RecreateSwapchain();
	}

	return result;
}

bool Backend::ProcessEvents(Rml::Context* context, KeyDownCallback key_down_callback, bool power_save)
{
	RMLUI_ASSERT(data && context);

#if SDL_MAJOR_VERSION >= 3
	#define RMLSDL_WINDOW_EVENTS_BEGIN
	#define RMLSDL_WINDOW_EVENTS_END
	auto GetKey = [](const SDL_Event& event) { return event.key.key; };
	auto GetDisplayScale = []() { return SDL_GetWindowDisplayScale(data->window); };
	constexpr auto event_quit = SDL_EVENT_QUIT;
	constexpr auto event_key_down = SDL_EVENT_KEY_DOWN;
	constexpr auto event_window_size_changed = SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED;
	bool has_event = false;
#else
	#define RMLSDL_WINDOW_EVENTS_BEGIN \
	case SDL_WINDOWEVENT:              \
	{                                  \
		switch (ev.window.event)       \
		{
	#define RMLSDL_WINDOW_EVENTS_END \
		}                            \
		}                            \
		break;
	auto GetKey = [](const SDL_Event& event) { return event.key.keysym.sym; };
	auto GetDisplayScale = []() { return 1.f; };
	constexpr auto event_quit = SDL_QUIT;
	constexpr auto event_key_down = SDL_KEYDOWN;
	constexpr auto event_window_size_changed = SDL_WINDOWEVENT_SIZE_CHANGED;
	int has_event = 0;
#endif

	bool result = data->running;
	data->running = true;

	SDL_Event ev;
	if (power_save)
		has_event = SDL_WaitEventTimeout(&ev, static_cast<int>(Rml::Math::Min(context->GetNextUpdateDelay(), 10.0) * 1000));
	else
		has_event = SDL_PollEvent(&ev);

	while (has_event)
	{
		bool propagate_event = true;
		switch (ev.type)
		{
		case event_quit:
		{
			propagate_event = false;
			result = false;
		}
		break;
		case event_key_down:
		{
			propagate_event = false;
			const Rml::Input::KeyIdentifier key = RmlSDL::ConvertKey(GetKey(ev));
			const int key_modifier = RmlSDL::GetKeyModifierState();
			const float native_dp_ratio = GetDisplayScale();

			// See if we have any global shortcuts that take priority over the context.
			if (key_down_callback && !key_down_callback(context, key, key_modifier, native_dp_ratio, true))
				break;
			// Otherwise, hand the event over to the context by calling the input handler as normal.
			if (!RmlSDL::InputEventHandler(context, data->window, ev))
				break;
			// The key was not consumed by the context either, try keyboard shortcuts of lower priority.
			if (key_down_callback && !key_down_callback(context, key, key_modifier, native_dp_ratio, false))
				break;
		}
		break;

			RMLSDL_WINDOW_EVENTS_BEGIN

		case event_window_size_changed:
		{
			Rml::Vector2i dimensions = {ev.window.data1, ev.window.data2};
			data->render_interface->SetViewport(dimensions.x, dimensions.y);
		}
		break;

			RMLSDL_WINDOW_EVENTS_END

		default: break;
		}

		if (propagate_event)
			RmlSDL::InputEventHandler(context, data->window, ev);

		has_event = SDL_PollEvent(&ev);
	}

	if (!WaitForValidSwapchain())
		result = false;

	return result;
}

void Backend::RequestExit()
{
	RMLUI_ASSERT(data);
	data->running = false;
}

void Backend::BeginFrame()
{
	RMLUI_ASSERT(data);
	RMLUI_ASSERT(data->render_interface);
	data->render_interface->BeginFrame();
	data->render_interface->Clear();
}

void Backend::PresentFrame()
{
	RMLUI_ASSERT(data);
	RMLUI_ASSERT(data->render_interface);
	data->render_interface->EndFrame();
}
