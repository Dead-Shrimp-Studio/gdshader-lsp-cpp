import os

vars = Variables()
vars.Add('platform', 'Target platform to build for (linux, windows, macos)', 'linux')
vars.Add('target', 'Build target (debug, release)', 'debug')

env = Environment(variables=vars, ENV=os.environ)
target_platform = env['platform']
build_target = env['target']

print(f"Building for platform: {target_platform} ({build_target})")

# -------------------------------------------------------------------------
# DEPENDENCIES (LSP LIBRARY)
# -------------------------------------------------------------------------
# NOTE: You must have compiled the 'lsp' library for the target platform as well!
# We assume you have a folder structure like: extern/lsp-framework/build/linux, /windows, etc.
# If you only have one flat 'build' folder, you will need to recompile the lib every time you switch platform.

lsp_lib_path = os.path.join('extern', 'lsp-framework', 'build')
# Optional: Try to find a platform specific subdir if you organize it that way
if os.path.exists(os.path.join(lsp_lib_path, target_platform)):
    lsp_lib_path = os.path.join(lsp_lib_path, target_platform)

env.Append(CPPPATH=[
    'src',
    'extern/lsp-framework',
    lsp_lib_path
])

env.Append(LIBPATH=[lsp_lib_path]) 
env.Append(LIBS=['lsp'])


# -------------------------------------------------------------------------
# COMPILER CONFIGURATION
# -------------------------------------------------------------------------
env.Append(CXXFLAGS=['-std=c++20', '-Wall', '-Wextra'])

if build_target == 'release':
    env.Append(CXXFLAGS=['-O2'])
else:
    env.Append(CXXFLAGS=['-g', '-O0'])

# Platform Specific Settings
if target_platform == 'windows':
    # Requires: sudo apt install mingw-w64
    env.Replace(CXX='x86_64-w64-mingw32-g++')
    env.Replace(AR='x86_64-w64-mingw32-gcc-ar')
    env.Replace(RANLIB='x86_64-w64-mingw32-gcc-ranlib')
    
    env['PROGSUFFIX'] = '.exe'
    
    # Windows Sockets and static linking to avoid dependency hell
    env.Append(LIBS=['ws2_32', 'shlwapi']) 
    env.Append(LINKFLAGS=['-static', '-static-libgcc', '-static-libstdc++'])

elif target_platform == 'macos':
    
    vars.Add('macos_arch', 'Target architecture (x86_64, arm64)', 'x86_64')
    env = Environment(variables=vars, ENV=os.environ)
    macos_arch = env.get('macos_arch', 'x86_64')

    print(f"Targeting macOS Architecture: {macos_arch}")

    # Use the explicit compiler versions we verified working (Darwin 23.5 / macOS 14)
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
    
    # Force C++20 and newer macOS target (matches what we did in CMake)
    env.Append(CXXFLAGS=['-std=c++20', '-mmacosx-version-min=12.0'])
    env.Append(LINKFLAGS=['-mmacosx-version-min=12.0'])

elif target_platform == 'linux':
    # Default GCC/Clang on host
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
# Walk the actual 'src' directory to find files
for root, dirs, files in os.walk('src'):
    for file in files:
        if file.endswith('.cpp'):
            # Get path relative to 'src' (e.g., "server/gdshader_server.cpp")
            rel_path = os.path.relpath(os.path.join(root, file), 'src')
            
            # Tell SCons to compile the 'build' version of this file
            # (e.g., "build/server/gdshader_server.cpp")
            sources.append(os.path.join('src', rel_path))

# -------------------------------------------------------------------------
# BUILD TARGET
# -------------------------------------------------------------------------
# Output the executable inside 'build/'
output_bin = os.path.join('bin', target_platform, 'gdshader-lsp')

env.Program(target=output_bin, source=sources)