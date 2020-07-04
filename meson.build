project(
	'cubewar',
	'cpp',
	default_options: [ 'includedir=include' ]
)

include_dir = include_directories('include')
library_dir = meson.source_root() + '\\library'
message('Searching "' + library_dir + '" for libraries.')
compiler = meson.get_compiler('cpp')

enet = compiler.find_library('enet', dirs: library_dir)
fmt = compiler.find_library('fmt', dirs: library_dir)
imgui = compiler.find_library('imgui', dirs: library_dir)
freetype = compiler.find_library('freetype', dirs: library_dir)
nanovg = compiler.find_library('nanovg', dirs: library_dir)
sdl2 = compiler.find_library('sdl2', dirs: library_dir)
glew = compiler.find_library('glew', dirs: library_dir)
opengl32 = compiler.find_library('opengl32', dirs: library_dir)
winmm = compiler.find_library('winmm', dirs: library_dir)
version = compiler.find_library('version', dirs: library_dir)
setupapi = compiler.find_library('setupapi', dirs: library_dir)
gdi32 = compiler.find_library('gdi32', dirs: library_dir)
imm32 = compiler.find_library('imm32', dirs: library_dir)
ole32 = compiler.find_library('ole32', dirs: library_dir)
oleaut32 = compiler.find_library('oleaut32', dirs: library_dir)
ws2_32 = compiler.find_library('ws2_32', dirs: library_dir)
bullet_collision = compiler.find_library('bulletcollision', dirs: library_dir)
bullet_dynamics = compiler.find_library('bulletdynamics', dirs: library_dir)
bullet_inverse_dynamics = compiler.find_library('bulletinversedynamics', dirs: library_dir)
bullet_linear_math = compiler.find_library('bulletlinearmath', dirs: library_dir)
assimp = compiler.find_library('assimp', dirs: library_dir)
irrxml = compiler.find_library('irrxml', dirs: library_dir)
zlib = compiler.find_library('zlib', dirs: library_dir)

executable(
	'cubewar',
	'main.cpp',
	include_directories : include_dir,
	dependencies : [
		sdl2,
		winmm,
		version,
		setupapi,
		imm32,
		ole32,
		oleaut32,
		glew,
		opengl32,
		gdi32,
		nanovg,
		imgui,
		fmt,
		enet,
		ws2_32,
		freetype,
		bullet_collision,
		bullet_dynamics,
		bullet_inverse_dynamics,
		bullet_linear_math,
		assimp,
		irrxml,
		zlib
	],
	override_options: 'cpp_std=c++17'
)