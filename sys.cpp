#include <rang.hpp>
#include <iostream>
#define SDL_MAIN_HANDLED
#include <sdl.h>
#define GLEW_STATIC
#include <gl/glew.h>
#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_sdl.h>
#include <enet/enet.h>
#define NANOVG_GL3_IMPLEMENTATION
#include <nanovg.h>
#include <nanovg_gl.h>
#include <glm/vec2.hpp>

namespace cw::sys {
	bool sdl_initialized = false;
	SDL_Window *sdl_window = 0;
	SDL_GLContext gl_context;
	NVGcontext *nvg_context = 0;
	ImGuiContext *imgui_context = 0;
	bool imgui_sdl_impl_initialized = false;
	bool imgui_opengl3_impl_initialized = false;
	bool enet_initialized = false;
	const uint64_t fixed_steps_per_second = 20;
	uint64_t performance_frequency = 0;
	uint64_t last_performance_counter = 0;
	double variable_time_delta = 0;
	const double fixed_step_time_delta = 1. / fixed_steps_per_second;
	uint64_t num_performance_counters_per_fixed_step = 0;
	uint64_t fixed_step_counter_remainder = 0;
	uint64_t fixed_step_performance_counter = 0;
	bool is_performance_optimal = true;
	uint64_t current_tick_iteration = 0;
	double interpolation_delta = 0;
	bool tick();
	void kill();
}

using namespace cw::sys;

namespace cw::core {
	void initialize();
	void shutdown();
	void on_fixed_step(const double &delta);
	void on_update(const double &delta, const double &interpolation);
	void on_imgui();
}

namespace cw::gpu {
	bool initialize();
	void shutdown();
	void generate_render_targets();
	extern glm::ivec2 render_target_size;;
}

namespace cw::physics {
	void initialize();
	void shutdown();
}

bool cw::sys::tick() {
	SDL_Event os_event;
	bool quit_signal = false;
	while (SDL_PollEvent(&os_event)) {
		ImGui_ImplSDL2_ProcessEvent(&os_event);
		if (os_event.type == SDL_QUIT) quit_signal = true;
		/*
		else if (os_event.type == SDL_MOUSEMOTION) {
			if (SDL_GetRelativeMouseMode() != SDL_TRUE) continue;
			// cw::pov::orientation += glm::fvec2(os_event.motion.xrel, os_event.motion.yrel) * 0.1f;
		}
		else if (os_event.type == SDL_KEYDOWN) {
			if (os_event.key.keysym.sym == SDLK_LCTRL) {
				if (SDL_GetRelativeMouseMode() == SDL_TRUE) SDL_SetRelativeMouseMode(SDL_FALSE);
				else SDL_SetRelativeMouseMode(SDL_TRUE);
			} else if (os_event.key.keysym.sym == SDLK_w) player_binary_input_forward = true;
			else if (os_event.key.keysym.sym == SDLK_a) player_binary_input_left = true;
			else if (os_event.key.keysym.sym == SDLK_s) player_binary_input_backward = true;
			else if (os_event.key.keysym.sym == SDLK_d) player_binary_input_right = true;
		} else if (os_event.type == SDL_KEYUP) {
			if (os_event.key.keysym.sym == SDLK_w) player_binary_input_forward = false;
			else if (os_event.key.keysym.sym == SDLK_a) player_binary_input_left = false;
			else if (os_event.key.keysym.sym == SDLK_s) player_binary_input_backward = false;
			else if (os_event.key.keysym.sym == SDLK_d) player_binary_input_right = false;
		}
		*/
	}
	int w, h;
	SDL_GetWindowSize(sdl_window, &w, &h);
	if (!(gpu::render_target_size.x == w && gpu::render_target_size.y == h)) {
		gpu::render_target_size = { w, h };
		gpu::generate_render_targets();
	}
	if (!performance_frequency) {
		performance_frequency = SDL_GetPerformanceFrequency();
		num_performance_counters_per_fixed_step = performance_frequency / fixed_steps_per_second;
		last_performance_counter = SDL_GetPerformanceCounter();
		fixed_step_performance_counter = last_performance_counter;
		return true;
	}
	const uint64_t performance_counter = SDL_GetPerformanceCounter();
	const uint64_t elapsed_performance_counter = performance_counter - last_performance_counter;
	fixed_step_counter_remainder += elapsed_performance_counter;
	uint32_t num_fixed_steps_this_i = 0;
	while (fixed_step_counter_remainder >= num_performance_counters_per_fixed_step) {
		cw::core::on_fixed_step(fixed_step_time_delta);
		fixed_step_performance_counter += num_performance_counters_per_fixed_step;
		fixed_step_counter_remainder -= num_performance_counters_per_fixed_step;
		num_fixed_steps_this_i++;
		current_tick_iteration++;
		if (num_fixed_steps_this_i >= 10) {
			std::cout << rang::bg::yellow << rang::fg::black;
			std::cout << "Fixed update is saturated. Accumulated " << fixed_step_counter_remainder << " on tick #" << current_tick_iteration << ". " << num_fixed_steps_this_i << " steps this frame.";
			std::cout << rang::bg::reset << rang::fg::reset << std::endl;
			break;
		}
	}
	is_performance_optimal = num_fixed_steps_this_i <= 1;
	last_performance_counter = performance_counter;
	variable_time_delta = static_cast<double>(elapsed_performance_counter) / performance_frequency;
	interpolation_delta = 0;
	const uint64_t next_fixed_step_expected_counter = fixed_step_performance_counter + num_performance_counters_per_fixed_step;
	if (performance_counter >= next_fixed_step_expected_counter) {
		interpolation_delta = 1;
		std::cout << rang::fg::yellow << "Interpolation not possible this frame." << rang::fg::reset << std::endl;
	}
	else {
		const uint64_t progress_towards_next_fixed_step = (fixed_step_performance_counter + num_performance_counters_per_fixed_step) - performance_counter;
		interpolation_delta = 1.0 - (static_cast<double>(progress_towards_next_fixed_step) / static_cast<double>(num_performance_counters_per_fixed_step));
	}
	cw::core::on_update(variable_time_delta, interpolation_delta);
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplSDL2_NewFrame(sdl_window);
	ImGui::NewFrame();
	cw::core::on_imgui();
	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
	SDL_GL_SwapWindow(sdl_window);
	return !quit_signal;
}

void cw::sys::kill() {
	if (sdl_window) SDL_HideWindow(sdl_window);
	if (enet_initialized) {
		enet_deinitialize();
		enet_initialized = false;
		std::cout << "Shutdown ENet." << std::endl;
	}
	if (imgui_opengl3_impl_initialized) {
		ImGui_ImplOpenGL3_Shutdown();
		std::cout << "Cleaned up ImGui OpenGL implemenation." << std::endl;
	}
	if (imgui_sdl_impl_initialized) {
		ImGui_ImplSDL2_Shutdown();
		std::cout << "Cleaned up ImGui SDL2 implementation." << std::endl;
	}
	if (imgui_context) {
		ImGui::DestroyContext(imgui_context);
		std::cout << "Destroyed ImGui context." << std::endl;
	}
	if (nvg_context) {
		nvgDeleteGL3(nvg_context);
		std::cout << "Destroyed NanoVG context." << std::endl;
	}
	if (gl_context) {
		SDL_GL_DeleteContext(gl_context);
		std::cout << "Destroyed SDL2 OpenGL context." << std::endl;
	}
	if (sdl_window) {
		SDL_DestroyWindow(sdl_window);
		std::cout << "Destroyed SDL2 window." << std::endl;
	}
	imgui_opengl3_impl_initialized = false;
	imgui_sdl_impl_initialized = false;
	imgui_context = 0;
	nvg_context = 0;
	gl_context = 0;
	sdl_window = 0;
	if (sdl_initialized) {
		SDL_Quit();
		sdl_initialized = false;
	}
}

int main() {
	if (SDL_Init(SDL_INIT_EVERYTHING) < 0) {
		std::cout << rang::fg::red << "Failed to initialize SDL2." << rang::fg::reset << std::endl;
		return 1;
	} else std::cout << "SDL2 subsystems are ready." << std::endl;
	sdl_initialized = true;
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 5);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	sdl_window = SDL_CreateWindow("CubeWar", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 1024, 768, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN);
	if (!sdl_window) {
		kill();
		return 2;
	}
	gl_context = SDL_GL_CreateContext(sdl_window);
	if (!gl_context) {
		std::cout << rang::fg::red << "Failed to create OpenGL context." << rang::fg::reset << std::endl;
		kill();
		return 3;
	}
	else std::cout << "OpenGL context created: " << glGetString(GL_VERSION) << " (" << glGetString(GL_RENDERER) << ")" << std::endl;
	static bool glew_init_already = false;
	if (!glew_init_already) {
		glewExperimental = true;
		if (glewInit()) {
			std::cout << "Failed to wrangle OpenGL extensions." << std::endl;
			kill();
			return 4;
		} else std::cout << "OpenGL extensions wrangled." << std::endl;
		glew_init_already = true;
	}
	nvg_context = nvgCreateGL3(NVG_STENCIL_STROKES | NVG_ANTIALIAS);
	if (!nvg_context) {
		std::cout << rang::fg::red << "Failed to create NanoVG context." << rang::fg::reset << std::endl;
		kill();
		return 5;
	} else std::cout << "NanoVG context is ready." << std::endl;
	imgui_context = ImGui::CreateContext();
	if (!imgui_context) {
		std::cout << rang::fg::red << "Failed to create ImGui context." << rang::fg::reset << std::endl;
		kill();
		return 6;
	}
	std::cout << "ImGui context is ready." << std::endl;
	if (!ImGui_ImplSDL2_InitForOpenGL(sdl_window, gl_context)) {
		imgui_sdl_impl_initialized = false;
		std::cout << rang::fg::red << "Failed to prepare ImGui context for SDL2 input." << rang::fg::reset << std::endl;
		kill();
		return 7;
	}
	imgui_sdl_impl_initialized = true;
	if (!ImGui_ImplOpenGL3_Init("#version 130")) {
		imgui_opengl3_impl_initialized = false;
		std::cout << rang::fg::red << "Failed to prepare ImGui context for OpenGL rendering." << rang::fg::reset << std::endl;
		kill();
		return 8;
	}
	imgui_opengl3_impl_initialized = true;
	ImGui::GetIO().Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\seguisb.ttf", 16);
	ImGui::GetIO().Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\seguisb.ttf", 20);
	ImGui::GetIO().Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\seguisb.ttf", 24);
	std::cout << "Integrated ImGui with SDL2 and OpenGL." << std::endl;
	if (enet_initialize() < 0) {
		enet_initialized = false;
		std::cout << rang::fg::red << "Failed to startup ENet." << rang::fg::reset << std::endl;
		kill();
		return 9;
	}
	std::cout << "ENet is ready." << std::endl;
	enet_initialized = true;
	if (!cw::gpu::initialize()) {
		kill();
		return 10;
	}
	cw::physics::initialize();
	cw::core::initialize();
	SDL_ShowWindow(sdl_window);
	if (SDL_GL_SetSwapInterval(-1) != 0) {
		std::cout << rang::fg::yellow << "Immediate late buffer swaps are not supported. Using regular sync." << rang::fg::reset << std::endl;
		SDL_GL_SetSwapInterval(1);
	}
	while (tick());
	SDL_HideWindow(sdl_window);
	// shutdown_game_components();
	cw::core::shutdown();
	cw::physics::shutdown();
	cw::gpu::shutdown();
	kill();
	return 0;
}