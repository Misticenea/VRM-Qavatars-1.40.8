Param(
    [Parameter(Mandatory=$false)]
    [Switch] $clean,

    [Parameter(Mandatory=$false)]
    [Switch] $diag,

    [Parameter(Mandatory=$false)]
    [Switch] $help
)

if ($help -eq $true) {
    Write-Output "`"Build`" - Copiles your mod into a `".so`" or a `".a`" library"
    Write-Output "`n-- Arguments --`n"

    Write-Output "-Clean `t`t Deletes the `"build`" folder, so that the entire library is rebuilt"
    Write-Output "-Diag `t`t Logs avatar posing diagnostics every second while in game"

    exit
}

# if user specified clean, remove all build files
if ($clean.IsPresent) {
    if (Test-Path -Path "build") {
        remove-item build -R
    }
}


if (($clean.IsPresent) -or (-not (Test-Path -Path "build"))) {
    new-item -Path build -ItemType Directory
} 

# always pass the option so a previous diagnostics build doesn't stay cached
$diagOption = if ($diag.IsPresent) { "ON" } else { "OFF" }

& cmake -G "Ninja" -DCMAKE_BUILD_TYPE="RelWithDebInfo" -DVRMQ_DIAG="$diagOption" -B build
& cmake --build ./build
