# `main.cpp`

Contains a lot of code related to the operating system, loop timing, fixed updates and physics steps. SDL2, GLEW, ImGui, NanoVG, ENet, Bullet Physics, some primitives and shaders are initialized within `main()`.

## Global Variables

### Tick Frequency Settings

```
const uint64_t fixed_steps_per_second = 100;
uint64_t performance_frequency = 0;
uint64_t last_performance_counter = 0;
double variable_time_delta = 0;
const double fixed_step_time_delta = 1. / fixed_steps_per_second;
uint64_t num_performance_counters_per_fixed_step = 0;
uint64_t fixed_step_counter_remainder = 0;
bool is_performance_optimal = true;
```

These variables are only modified within `tick()`.

### Initialization State

```
bool sdl_initialized = false;
SDL_Window *sdl_window = 0;
SDL_GLContext gl_context;
NVGcontext *nvg_context = 0;
ImGuiContext *imgui_context = 0;
bool imgui_sdl_impl_initialized = false;
bool imgui_opengl3_impl_initialized = false;
bool enet_initialized = false;
```

These are set within ```main()``` and returned to the above specified defaults within ```kill()```.

### GPU Programs

```
std::map<std::string, GLuint> gpu_programs;
```

This map is initialized after all the other subsystems within ```main()```.  See ```make_programs_from_directory()``` for the logic.

### Physics State

```
btDefaultCollisionConfiguration *collision_configuration = 0;
btCollisionDispatcher *collision_dispatcher = 0;
btBroadphaseInterface *broadphase_interface = 0;
btSequentialImpulseConstraintSolver *constraint_solver = 0;
btDiscreteDynamicsWorld *dynamics_world = 0;
```

These are constructed within ```initialize_physics()``` and are cleaned up within ```cleanup_physics()```. Both of which are called within ```main()```.

### Configurations

```
const char *cfg_file = "settings.json";
nlohmann::json cfg;
```

```cfg``` is updated within ```load_cfg()``` and flushed to disk within ```flush_cfg()```.

### Rendering

```
glm::ivec2 render_target_size { 0, 0 };
GLuint primary_render_buffer = 0, primary_frame_buffer = 0;
GLuint deferred_surface_render_target = 0, deferred_position_render_target = 0, deferred_material_render_target = 0;
GLuint screen_quad_vertex_array = 0, screen_quad_vertex_buffer = 0;
```

The GL objects are constructed within ```make_screen_quad()``` which is called within ```main()``` and ```generate_render_targets()``` which is called within ```on_update()``` whenever the client area is resized. ```render_target_size``` is updated within ```on_update()``` and is referenced by ```generate_render_targets()```.

## Functions

### ```read_file()```

Return value is ```nullopt``` in case of error. Otherwise, a ```vector``` of ```char```.

### ```load_cfg()```

Pulls JSON structure from ```cfg_file```, overwriting ```cfg``` global. Does nothing if there was an error.

### ```flush_cfg()```

Dumps contents of the ```cfg``` to ```cfg_file```.

### ```cleanup_physics()```

Deletes all objects related to physics state.

### ```initialize_physics()```

Allocates new physics objects. Set ```dynamics_world``` gravity to ```0, -10, 0```.

### ```from_physics_world()```

Converts either ```btVector3``` or ```btQuaternion``` from Bullet's coordinate system to OpenGL's coordinate system and returns the respective GLM structure.

### ```to_physics_world()```

Converts either ```glm::vec3``` or ```glm::quat``` from OpenGL's coordinate system to Bullet's coordinate system and returns the respective Bullet structure.

### ```kill()```

Cleans up all initialization state variables and sets them to default values. Also calls ```flush_cfg()```.

### ```print_program_info_log()```

Prints diagnostic info related to ```glLinkProgram()``` to the terminal.

### ```make_program_from_shaders()```

Takes a ```vector``` of GL shader objects and attempts to link them into a program and return it. On error, calls ```print_program_info_log()``` and returns ```0```.

### ```print_shader_info_log()```

Prints diagnostic info related to ```glCompileShader()``` to the terminal.

### ```make_shader_from_file()```

Compiles the contents of a file into a GL shader object. The type of shader is determined by the extension ```.vs```, ```.gs.``` or ```.fs``` representing ```GL_VERTEX_SHADER```, ```GL_GEOMETRY_SHADER``` and ```GL_FRAGMENT_SHADER``` respectively. On error, returns ```0```. Calls ```print_shader_info_log()``` on compile error.

### ```map_file_names_and_extensions()```

This functions takes a list of files from a given directory and returns a map linking similarly named files to their various extensions.

Here is an example of a given directory containing some shader files:

```
main.vs
main.gs
main.fs
screen.vs
screen.fs
```

In the above case, the return value would be a map in which a key value of ```"main"``` would return a vector equal to ```{ ".vs", ".gs", ".fs" }```. Alternatively, a key value of ```"screen"``` would return ```{ ".vs", ".fs" }```. This function is used by ```make_programs_from_directory()``` to determine which source files are intended to be linked into the same program. Returns ```nullopt``` on error.

### ```make_programs_from_directory()```

Given a directory containing source code for GLSL shaders, they will be compiled into GL shader objects and linked into GL programs. Shader to Program relationships are determined by filename. See ```map_file_names_and_extensions()```. Returns a map linking common file names to resulting GL programs or ```nullopt``` on error.

### ```make_screen_quad()```

The resulting quad will cover the entire viewport and is used for fullscreen rendering effects. Associated global variables are ```screen_quad_vertex_array``` and ```screen_quad_vertex_buffer```.

### ```generate_render_targets()```

Generates the 2D textures that are used to store deferred rendering pipeline information. ```render_target_size``` is referenced and all textures will be equal to that size.

### ```on_fixed_step()```

Called by ```tick()``` after enough time has elapsed. Will always occur in intervals of ```fixed_step_time_delta```.

### ```on_update()```

Called every frame by ```tick()```.

### ```tick()```

Called as frequently as possible. Always calls ```on_update()``` and manages the rhythm of ```on_fixed_step()```.