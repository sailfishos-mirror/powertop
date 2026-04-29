test_lib_sources = [
  '../../src/lib.cpp',
  '../../src/test_framework.cpp',
  'test_read_file_content.cpp',
]

test_lib_exe = executable(
  'powertop-test-lib',
  test_lib_sources,
  include_directories: [
    include_directories('../..'),
    include_directories('../../src'),
    include_directories('..'),
  ],
  cpp_args: [
    '-DENABLE_TEST_FRAMEWORK',
    '-DHAVE_NO_PCI',
    '-DTEST_DATA_DIR="' + meson.current_source_dir() / 'data' + '"',
  ],
  dependencies: [ncursesw_dep],
)

test('lib: read_file_content', test_lib_exe)
