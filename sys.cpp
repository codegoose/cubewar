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
#include <filesystem>
#include <vector>
#include <string.h>
#include <fmt/format.h>

#include "sys.h"

namespace cw::sys {
	bool enable_mouse_grab = false;
	std::vector<std::string> args;
	bool sdl_initialized = false;
	SDL_Window *sdl_window = 0;
	SDL_GLContext gl_context;
	NVGcontext *nvg_context = 0;
	ImGuiContext *imgui_context = 0;
	bool imgui_sdl_impl_initialized = false;
	bool imgui_opengl3_impl_initialized = false;
	bool enet_initialized = false;
	const uint64_t fixed_steps_per_second = 60;
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

namespace cw::core {
	void initialize();
	void shutdown();
	void on_fixed_step(const double &delta);
	void on_update(const double &delta, const double &interpolation);
	void on_relative_mouse_input(int x, int y);
	void on_imgui();
}

namespace cw::gpu {
	bool initialize();
	void shutdown();
	void generate_render_targets();
	extern glm::ivec2 render_target_size;;
	void render();
}

namespace cw::physics {
	void initialize();
	void shutdown();
}

std::filesystem::path cw::sys::bin_path() {
	return std::filesystem::path(args[0]).remove_filename();
}

bool cw::sys::tick() {
	SDL_Event os_event;
	bool quit_signal = false;
	assert(SDL_SetRelativeMouseMode(enable_mouse_grab ? SDL_TRUE : SDL_FALSE) == 0);
	while (SDL_PollEvent(&os_event)) {
		ImGui_ImplSDL2_ProcessEvent(&os_event);
		if (os_event.type == SDL_QUIT) quit_signal = true;
		else if (os_event.type == SDL_MOUSEMOTION) {
			if (enable_mouse_grab) core::on_relative_mouse_input(os_event.motion.xrel, os_event.motion.yrel);
		} else if (os_event.type == SDL_KEYDOWN) {
			if (os_event.key.keysym.sym == SDLK_F1 && os_event.key.repeat == 0) enable_mouse_grab = !enable_mouse_grab;
			if (os_event.key.keysym.sym == SDLK_F2 && os_event.key.repeat == 0) SDL_SetWindowFullscreen(sdl_window, SDL_GetWindowFlags(sdl_window) & SDL_WINDOW_FULLSCREEN ? 0 : SDL_WINDOW_FULLSCREEN);
			/*
			else if (os_event.key.keysym.sym == SDLK_w) player_binary_input_forward = true;
			else if (os_event.key.keysym.sym == SDLK_a) player_binary_input_left = true;
			else if (os_event.key.keysym.sym == SDLK_s) player_binary_input_backward = true;
			else if (os_event.key.keysym.sym == SDLK_d) player_binary_input_right = true;
			*/
		} else if (os_event.type == SDL_KEYUP) {
			/*
			if (os_event.key.keysym.sym == SDLK_w) player_binary_input_forward = false;
			else if (os_event.key.keysym.sym == SDLK_a) player_binary_input_left = false;
			else if (os_event.key.keysym.sym == SDLK_s) player_binary_input_backward = false;
			else if (os_event.key.keysym.sym == SDLK_d) player_binary_input_right = false;
			*/
		}
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
		core::on_fixed_step(fixed_step_time_delta);
		fixed_step_performance_counter += num_performance_counters_per_fixed_step;
		fixed_step_counter_remainder -= num_performance_counters_per_fixed_step;
		num_fixed_steps_this_i++;
		current_tick_iteration++;
		if (num_fixed_steps_this_i >= 10) {
			std::cout << "Fixed update is saturated. Accumulated " << fixed_step_counter_remainder << " on tick #" << current_tick_iteration << ". " << num_fixed_steps_this_i << " steps this frame." << std::endl;
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
		std::cout << "Interpolation not possible this frame." << std::endl;
	}
	else {
		const uint64_t progress_towards_next_fixed_step = (fixed_step_performance_counter + num_performance_counters_per_fixed_step) - performance_counter;
		interpolation_delta = 1.0 - (static_cast<double>(progress_towards_next_fixed_step) / static_cast<double>(num_performance_counters_per_fixed_step));
	}
	core::on_update(variable_time_delta, interpolation_delta);
	gpu::render();
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplSDL2_NewFrame(sdl_window);
	ImGui::NewFrame();
	core::on_imgui();
	//
	ImGui::Begin("Engine");
	ImGui::Text(fmt::format("Frame Delta: {}", variable_time_delta).c_str());
	ImGui::Text(fmt::format("Frame Time: {}", static_cast<int>(variable_time_delta * 1000.0)).c_str());
	ImGui::Text(fmt::format("Frames Per Second: {}", static_cast<int>(1.0 / variable_time_delta)).c_str());
	ImGui::Text(fmt::format("Fixed Steps Per Second: {}", fixed_steps_per_second).c_str());
	ImGui::Text(fmt::format("Performance Frequency: {}", performance_frequency).c_str());
	ImGui::Text(fmt::format("Last Performance Counter: {}", last_performance_counter).c_str());
	ImGui::Text(fmt::format("Variable Time Delta: {}", variable_time_delta).c_str());
	ImGui::Text(fmt::format("Fixed Step Time Delta: {}", fixed_step_time_delta).c_str());
	ImGui::Text(fmt::format("Performance Counters Per Fixed Step: {}", num_performance_counters_per_fixed_step).c_str());
	ImGui::Text(fmt::format("Fixed Step Counter Remainder: {}", fixed_step_counter_remainder).c_str());
	ImGui::Text(fmt::format("Optimal Performance: {}", is_performance_optimal ? "Yes" : "No").c_str());
	ImGui::Text(fmt::format("Tick: {}", current_tick_iteration).c_str());
	ImGui::End();
	//
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
		std::cout << "Cleaned up ImGui OpenGL implementation." << std::endl;
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

int main(int c, char **v) {
	for (int i = 0; i < c; i++) {
		std::cout << "v[" << i << "] -> \"" << v[i] << "\"" << std::endl;
		cw::sys::args.push_back(v[i]);
	}
	std::cout << "Working Area: \"" << std::filesystem::current_path().string() << "\"" << std::endl;
	std::cout << "Binary Path: \"" << cw::sys::bin_path().string() << "\"" << std::endl;
	if (SDL_Init(SDL_INIT_EVERYTHING) < 0) {
		std::cout << "Failed to initialize SDL2." << std::endl;
		return 1;
	} else std::cout << "SDL2 subsystems are ready." << std::endl;
	cw::sys::sdl_initialized = true;
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 5);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	cw::sys::sdl_window = SDL_CreateWindow("CubeWar", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 1024, 768, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN);
	if (!cw::sys::sdl_window) {
		cw::sys::kill();
		return 2;
	}
	cw::sys::gl_context = SDL_GL_CreateContext(cw::sys::sdl_window);
	if (!cw::sys::gl_context) {
		std::cout << "Failed to create OpenGL context." << std::endl;
		cw::sys::kill();
		return 3;
	}
	else std::cout << "OpenGL context created: " << glGetString(GL_VERSION) << " (" << glGetString(GL_RENDERER) << ")" << std::endl;
	static bool glew_init_already = false;
	if (!glew_init_already) {
		glewExperimental = true;
		if (glewInit()) {
			std::cout << "Failed to wrangle OpenGL extensions." << std::endl;
			cw::sys::kill();
			return 4;
		} else std::cout << "OpenGL extensions wrangled." << std::endl;
		glew_init_already = true;
	}
	cw::sys::nvg_context = nvgCreateGL3(NVG_STENCIL_STROKES | NVG_ANTIALIAS);
	if (!cw::sys::nvg_context) {
		std::cout << "Failed to create NanoVG context." << std::endl;
		cw::sys::kill();
		return 5;
	} else std::cout << "NanoVG context is ready." << std::endl;
	cw::sys::imgui_context = ImGui::CreateContext();
	if (!cw::sys::imgui_context) {
		std::cout << "Failed to create ImGui context." << std::endl;
		cw::sys::kill();
		return 6;
	}
	std::cout << "ImGui context is ready." << std::endl;
	if (!ImGui_ImplSDL2_InitForOpenGL(cw::sys::sdl_window, cw::sys::gl_context)) {
		cw::sys::imgui_sdl_impl_initialized = false;
		std::cout << "Failed to prepare ImGui context for SDL2 input." << std::endl;
		cw::sys::kill();
		return 7;
	}
	cw::sys::imgui_sdl_impl_initialized = true;
	if (!ImGui_ImplOpenGL3_Init("#version 130")) {
		cw::sys::imgui_opengl3_impl_initialized = false;
		std::cout << "Failed to prepare ImGui context for OpenGL rendering." << std::endl;
		cw::sys::kill();
		return 8;
	}
	cw::sys::imgui_opengl3_impl_initialized = true;
	ImGui::GetIO().Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\seguisb.ttf", 16);
	ImGui::GetIO().Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\seguisb.ttf", 20);
	ImGui::GetIO().Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\seguisb.ttf", 24);
	static std::string imgui_ini_filename = cw::sys::bin_path().string() + "imgui.ini";
	std::cout << "ImGui settings file: \"" << imgui_ini_filename << "\"" << std::endl;
	ImGui::GetIO().IniFilename = imgui_ini_filename.data();
	std::cout << "Integrated ImGui with SDL2 and OpenGL." << std::endl;
	if (enet_initialize() < 0) {
		cw::sys::enet_initialized = false;
		std::cout << "Failed to startup ENet." << std::endl;
		cw::sys::kill();
		return 9;
	}
	std::cout << "ENet is ready." << std::endl;
	cw::sys::enet_initialized = true;
	if (!cw::gpu::initialize()) {
		cw::sys::kill();
		return 10;
	}
	cw::physics::initialize();
	cw::core::initialize();
	SDL_ShowWindow(cw::sys::sdl_window);
	SDL_GL_SetSwapInterval(0);
	while (cw::sys::tick());
	SDL_HideWindow(cw::sys::sdl_window);
	cw::core::shutdown();
	cw::physics::shutdown();
	cw::gpu::shutdown();
	cw::sys::kill();
	return 0;
}
