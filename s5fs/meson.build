project('s5fs', 'cpp', default_options : ['cpp_std=c++17'])

libfuse3 = dependency('fuse3', required : true)

s5fs_srcs = ['main.cpp', 's52k.cpp', 's52k.h']

executable('s5fs', s5fs_srcs, dependencies : libfuse3, install : true)
