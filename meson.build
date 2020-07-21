project(
	'cubewar',
	'cpp'
)

compiler = meson.get_compiler('cpp')

enet = compiler.find_library('enet')
fmt = compiler.find_library('fmt')
imgui = compiler.find_library('imgui')
freetype = compiler.find_library('freetype')
sdl2 = compiler.find_library('sdl2')
glew = compiler.find_library('glew')
opengl32 = compiler.find_library('opengl32')
winmm = compiler.find_library('winmm')
version = compiler.find_library('version')
setupapi = compiler.find_library('setupapi')
gdi32 = compiler.find_library('gdi32')
imm32 = compiler.find_library('imm32')
ole32 = compiler.find_library('ole32')
oleaut32 = compiler.find_library('oleaut32')
ws2_32 = compiler.find_library('ws2_32')
bullet_collision = compiler.find_library('bulletcollision')
bullet_dynamics = compiler.find_library('bulletdynamics')
bullet_inverse_dynamics = compiler.find_library('bulletinversedynamics')
bullet_linear_math = compiler.find_library('bulletlinearmath')
assimp = compiler.find_library('assimp')
irrxml = compiler.find_library('irrxml')
zlib = compiler.find_library('zlib')

executable(
	'cubewar',
	'gpu.cpp',
	'misc.cpp',
	'physics.cpp',
	'pov.cpp',
	'core.cpp',
	'sys.cpp',
	'textures.cpp',
	'local_player.cpp',
	'cfg.cpp',
	'meshes.cpp',
	'simplex.cpp',
	'materials.cpp',
	'sun.cpp',
	'net.cpp',
	'weapon.cpp',
	'scene.cpp',
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
