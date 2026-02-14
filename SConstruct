import os
import sys

vars = Variables() # type: ignore
vars.Add('platform', 'Target platform to build for (linux, windows, macos)', 'linux')
vars.Add('target', 'Build target (debug, release)', 'debug')

env = Environment(variables=vars, ENV=os.environ) # type: ignore
target_platform = env['platform']
build_target = env['target']

# -------------------------------------------------------------------------
# NATIVE VS CROSS-COMPILE DETECTION
# -------------------------------------------------------------------------

if sys.platform.startswith('linux'):
    host_os = 'linux'
elif sys.platform == 'win32':
    host_os = 'windows'
elif sys.platform == 'darwin':
    host_os = 'macos'
else:
    host_os = 'linux'

is_native_build = (target_platform == host_os)
print(f"Building for: {target_platform} ({build_target}) | Host: {host_os} | Native: {is_native_build}")

# -------------------------------------------------------------------------
# DEPENDENCIES (LSP LIBRARY)
# -------------------------------------------------------------------------
# NOTE: You must have compiled the 'lsp' library for the target platform as well!
# Structure: extern/lsp-framework/build_linux, /build_windows, /build_macos

lsp_lib_path = os.path.join('extern', 'lsp-framework', f'build_{target_platform}')

env.Append(CPPPATH=[
    'src',
    'extern/lsp-framework',
    lsp_lib_path,
    os.path.join(lsp_lib_path, 'generated'),
    'extern/spdlog/include'
])

if target_platform == 'windows':
    env.Append(CPPPATH=[os.path.join(lsp_lib_path, 'generated')])

env.Append(LIBPATH=[lsp_lib_path]) 
env.Append(LIBS=['lsp'])


# -------------------------------------------------------------------------
# COMPILER CONFIGURATION
# -------------------------------------------------------------------------
if target_platform == 'windows' and is_native_build:
    env.Append(CXXFLAGS=['/std:c++20', '/W3', '/EHsc', '/nologo'])
    
    if build_target == 'release':
        env.Append(CXXFLAGS=['/O2'])
        env.Append(CPPDEFINES={'SPDLOG_ACTIVE_LEVEL': 'SPDLOG_LEVEL_INFO'})
    else:
        env.Append(CXXFLAGS=['/Zi', '/Od']) # Debug info, no optimization
        env.Append(CPPDEFINES={'SPDLOG_ACTIVE_LEVEL': 'SPDLOG_LEVEL_TRACE'})

else:

    env.Append(CXXFLAGS=['-std=c++20', '-Wall', '-Wextra'])

    if build_target == 'release':
        env.Append(CXXFLAGS=['-O2'])
        env.Append(CPPDEFINES={'SPDLOG_ACTIVE_LEVEL': 'SPDLOG_LEVEL_INFO'})
    else:
        env.Append(CXXFLAGS=['-g', '-O0'])
        env.Append(CPPDEFINES={'SPDLOG_ACTIVE_LEVEL': 'SPDLOG_LEVEL_TRACE'})

if target_platform == 'windows':
    
    if not is_native_build:
        env.Replace(CXX='x86_64-w64-mingw32-g++')
        env.Replace(AR='x86_64-w64-mingw32-gcc-ar')
        env.Replace(RANLIB='x86_64-w64-mingw32-gcc-ranlib')
        env.Append(LINKFLAGS=['-static', '-static-libgcc', '-static-libstdc++'])
    
    env.Append(LIBS=['ws2_32', 'shlwapi']) 
    env['PROGSUFFIX'] = '.exe'

elif target_platform == 'macos':
    
    macos_arch = ARGUMENTS.get('macos_arch', 'x86_64')
    
    if is_native_build:
        print(f"Native macOS Build - Using system compiler")
        env.Append(CXXFLAGS=['-arch', macos_arch])
        env.Append(LINKFLAGS=['-arch', macos_arch])
    else:
        
        print(f"Targeting macOS Architecture: {macos_arch} (Cross-Compile)")
        if macos_arch == 'x86_64':
            env.Replace(CXX='x86_64-apple-darwin23.5-clang++')
            env.Replace(AR='x86_64-apple-darwin23.5-ar')
            env.Append(CXXFLAGS=['-arch', 'x86_64'])
            env.Append(LINKFLAGS=['-arch', 'x86_64'])
        elif macos_arch == 'arm64':
            env.Replace(CXX='aarch64-apple-darwin23.5-clang++')
            env.Replace(AR='aarch64-apple-darwin23.5-ar')
            env.Append(CXXFLAGS=['-arch', 'arm64'])
            env.Append(LINKFLAGS=['-arch', 'arm64'])

    env.Append(CXXFLAGS=['-std=c++20', '-mmacosx-version-min=12.0'])
    env.Append(LINKFLAGS=['-mmacosx-version-min=12.0'])

elif target_platform == 'linux':
    env.Append(LIBS=['pthread'])


# -------------------------------------------------------------------------
# BUILD DIRECTORY SETUP
# -------------------------------------------------------------------------
build_dir = os.path.join('build', target_platform)
env.VariantDir(build_dir, 'src', duplicate=0)

# -------------------------------------------------------------------------
# COMPILATION DATABASE
# -------------------------------------------------------------------------
if target_platform == 'linux':
    env.Tool('compilation_db')
    env.CompilationDatabase('compile_commands.json')

# -------------------------------------------------------------------------
# SOURCE DISCOVERY
# -------------------------------------------------------------------------
sources = []

for root, dirs, files in os.walk('src'):
    for file in files:
        if file.endswith('.cpp'):
            rel_path = os.path.relpath(os.path.join(root, file), 'src')
            sources.append(os.path.join('src', rel_path))

# -------------------------------------------------------------------------
# BUILD TARGET
# -------------------------------------------------------------------------

if build_target == 'release':
    output_bin = os.path.join('bin', target_platform, 'release', f'gdshader_lsp_release_{target_platform}')
else:
    output_bin = os.path.join('bin', target_platform, 'debug', f'gdshader_lsp_debug_{target_platform}')

env.Program(target=output_bin, source=sources)