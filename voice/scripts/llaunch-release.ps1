#
# generate, compile and run exe files in Release mode
#
function getExePathFromCMakeLists() {
    $content = Get-Content -Raw -Path "./CMakeLists.txt"
    $exeName = "MetasequoiaVoiceInput" # Default
    if ($content -match 'set\(MY_EXECUTABLE_NAME\s+\"([^\"]+)\"\)') {
        $exeName = $matches[1]
    }
    return "./build/bin/Release/$exeName.exe"
}

$currentDirectory = Get-Location
$cmakeListsPath = Join-Path -Path $currentDirectory -ChildPath "CMakeLists.txt"

if (-not (Test-Path $cmakeListsPath)) {
    Write-Host "No CMakeLists.txt in current directory, please check." -ForegroundColor Red
    return
}

Write-Host "Start generating and compiling in RELEASE mode..." -ForegroundColor Cyan

$buildFolderPath = ".\build"
if (-not (Test-Path $buildFolderPath)) {
    New-Item -ItemType Directory -Path $buildFolderPath | Out-Null
}

# Generate using preset
cmake --preset=default-release

if ($LASTEXITCODE -eq 0) {
    # Build using Release configuration
    cmake --build build --config Release
    
    if ($LASTEXITCODE -eq 0) {
        $exePath = getExePathFromCMakeLists
        if (Test-Path $exePath) {
            Write-Host "Build successful. Starting application..." -ForegroundColor Green
            Write-Host "=================================================="
            & $exePath
        } else {
            Write-Host "Error: Executable not found at $exePath" -ForegroundColor Red
        }
    } else {
        Write-Host "Build failed." -ForegroundColor Red
    }
} else {
    Write-Host "Generation failed." -ForegroundColor Red
}