project('squarehole', 'c', 'cpp',
  version : '0.1',
  license : 'GPL-3.0-or-later',
  meson_version : '>=1.10.0')

fs = import('fs')

# These arguments are only used to build the shared library
# not the executables that use the library.
lib_args = ['-DBUILDING_SQUAREHOLE']

# GEGL operation shared library
glib = dependency('glib-2.0', required : true)
gegl = dependency('gegl-0.4', required : true)
libavutil = dependency('libavutil', required: true)
libavcodec = dependency('libavcodec', required: true)
libswresample = dependency('libswresample', required: true)
dependencies = [glib, gegl, libavutil, libavcodec, libswresample]

# Unfortunately you can't just pull in airwin2rack as a meson wrap or a
# subproject because meson's CMake integration gets a bit to eager and messes
# up the syntax of CMakeRC generated cpp files. So, the solution is just to
# include airwin2rack as a git submodule outside of the subprojects directory
# and manually shell out to CMake as seen below.
prog_cmake = find_program('cmake')
airwin2rack_build = custom_target(
  'airwin2rack_build',
  output : 'airwin2rack',
  command : [
    prog_cmake,
    '-S@SOURCE_ROOT@' / 'lib' / 'airwin2rack',
    '-B@BUILD_ROOT@' / 'airwin2rack',
  ],
)
airwin2rack_target = custom_target(
  'libairwin-registry.a',
  output : 'libairwin-registry.a',
  build_subdir : 'airwin2rack',
  depends : airwin2rack_build,
  command : [
    prog_cmake,
    '--build', '@BUILD_ROOT@' / 'airwin2rack',
    '--target', 'airwin-registry',
  ],
)

src_files = files([
  'src' / 'squarehole.c',
  'src' / 'audio.c',
  'src' / 'airwin.cpp',
])
inc = include_directories('src', 'lib' / 'airwin2rack' / 'src')

shlib = shared_library('squarehole', src_files, airwin2rack_target,
  c_args : lib_args,
  dependencies : dependencies,
  include_directories : inc,
  name_prefix : '',
)

# Make this library usable as a Meson subproject.
squarehole_dep = declare_dependency(
  include_directories: include_directories('.'),
  link_with : shlib)

# Unit tests
unity_subproject = subproject('unity')
unity_dependency = unity_subproject.get_variable('unity_dep')
unity_gen_runner = unity_subproject.get_variable('gen_test_runner')

foreach src_file : src_files
  test_name = 'test_' + fs.name(src_file)
  test_file = 'test' / test_name
  if fs.exists(test_file)
    test_exec = executable(test_name,
      sources : [
        src_file,
        test_file,
        unity_gen_runner.process(test_file)
      ],
      include_directories : inc,
      dependencies : [unity_dependency, dependencies],
    )
    test(test_name, test_exec)
  endif
endforeach
