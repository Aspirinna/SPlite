param(
    [string]$Configuration = "Release",
    [string]$SteamworksSdkDir = "",
    [string]$MSBuildPath = ""
)

$ErrorActionPreference = "Stop"
$repositoryRoot = Split-Path -Parent $PSScriptRoot
$packageRoot = Join-Path $repositoryRoot "dist\SPlite"

if (-not $MSBuildPath) {
    $msbuildCommand = Get-Command msbuild.exe -ErrorAction SilentlyContinue
    if ($msbuildCommand) {
        $MSBuildPath = $msbuildCommand.Source
    }
    else {
        $vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
        if (Test-Path -LiteralPath $vswhere) {
            $MSBuildPath = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild -find "MSBuild\**\Bin\MSBuild.exe" | Select-Object -First 1
        }
    }
}
if (-not $MSBuildPath -or -not (Test-Path -LiteralPath $MSBuildPath)) {
    throw "未找到 MSBuild。请通过 -MSBuildPath 传入完整路径。"
}

$buildArguments = @(
    (Join-Path $repositoryRoot "SPlite.vcxproj"),
    "/t:Build",
    "/p:Configuration=$Configuration",
    "/p:Platform=x64",
    "/m:1",
    "/v:minimal",
    "/nologo"
)
if ($SteamworksSdkDir) {
    $buildArguments += "/p:SteamworksSdkDir=$SteamworksSdkDir"
}

& $MSBuildPath @buildArguments
if ($LASTEXITCODE -ne 0) {
    Write-Warning "全量构建失败，尝试规避 MSVC 14.51 中文源码前端缺陷。"
    $recoveryArguments = $buildArguments
    & $MSBuildPath @recoveryArguments
}
if ($LASTEXITCODE -ne 0) {
    & $MSBuildPath @recoveryArguments
}
if ($LASTEXITCODE -ne 0) { throw "主程序在恢复构建后仍然失败。" }

$testProject = Join-Path $repositoryRoot "SPlite.Tests.vcxproj"
& $MSBuildPath $testProject /t:Build /p:Configuration=Release /p:Platform=x64 /m:1 /v:minimal /nologo
if ($LASTEXITCODE -ne 0) {
    & $MSBuildPath $testProject /t:Build /p:Configuration=Release /p:Platform=x64 /m:1 /v:minimal /nologo
}
if ($LASTEXITCODE -ne 0) { throw "测试程序编译失败。" }

& (Join-Path $repositoryRoot "x64\Release\SPlite.Tests.exe")
if ($LASTEXITCODE -ne 0) { throw "核心测试失败。" }

if (Test-Path -LiteralPath $packageRoot) {
    Remove-Item -LiteralPath $packageRoot -Recurse -Force
}
New-Item -ItemType Directory -Path (Join-Path $packageRoot "assets") -Force | Out-Null
New-Item -ItemType Directory -Path (Join-Path $packageRoot "shaders") -Force | Out-Null

Copy-Item -LiteralPath (Join-Path $repositoryRoot "x64\$Configuration\SPlite.exe") -Destination $packageRoot
Copy-Item -Path (Join-Path $repositoryRoot "assets\*") -Destination (Join-Path $packageRoot "assets") -Recurse
Copy-Item -Path (Join-Path $repositoryRoot "src\shaders\*") -Destination (Join-Path $packageRoot "shaders") -Recurse

$steamDll = Join-Path $repositoryRoot "x64\$Configuration\steam_api64.dll"
if (Test-Path -LiteralPath $steamDll) {
    Copy-Item -LiteralPath $steamDll -Destination $packageRoot
}

Write-Host "发布包已生成：$packageRoot"
