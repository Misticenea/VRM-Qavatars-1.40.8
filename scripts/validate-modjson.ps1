$mod = "./mod.json"

# regenerate mod.json every time so changes to the template or qpm.json aren't left out
if (Test-Path -Path ".\mod.template.json") {
    & qpm qmod manifest
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }

    # qpm copies the version from mod.template.json as is, use qpm.json as the single source of the version
    $version = (Get-Content ./qpm.json -Raw | ConvertFrom-Json).info.version
    $modJson = Get-Content $mod -Raw | ConvertFrom-Json
    $modJson.version = $version
    $modJson | ConvertTo-Json -Depth 32 | Set-Content $mod
    Write-Output "Set mod.json version to $version from qpm.json"
}
elseif (-not (Test-Path -Path $mod)) {
    Write-Output "Error: mod.json and mod.template.json were not present"
    exit 1
}

Write-Output "Creating qmod from mod.json"

$psVersion = $PSVersionTable.PSVersion.Major
if ($psVersion -ge 6) {
    $schemaUrl = "https://raw.githubusercontent.com/Lauriethefish/QuestPatcher.QMod/main/QuestPatcher.QMod/Resources/qmod.schema.json"
    Invoke-WebRequest $schemaUrl -OutFile ./mod.schema.json

    $schema = "./mod.schema.json"
    $modJsonRaw = Get-Content $mod -Raw
    $modSchemaRaw = Get-Content $schema -Raw

    Remove-Item $schema

    Write-Output "Validating mod.json..."
    if (-not ($modJsonRaw | Test-Json -Schema $modSchemaRaw)) {
        Write-Output "Error: mod.json is not valid"
        exit 1
    }
}
else {
    Write-Output "Could not validate mod.json with schema: powershell version was too low (< 6)"
}
exit
