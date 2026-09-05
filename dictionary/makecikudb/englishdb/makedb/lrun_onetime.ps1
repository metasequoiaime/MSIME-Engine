$ErrorActionPreference = "Stop"

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path

function Invoke-PythonScript {
    param([Parameter(Mandatory = $true)][string]$Path)

    python $Path
    if ($LASTEXITCODE -ne 0) {
        throw "Python script failed with exit code ${LASTEXITCODE}: $Path"
    }
}

Invoke-PythonScript (Join-Path $scriptDir "create_db_and_table.py")
Invoke-PythonScript (Join-Path $scriptDir "insert_data.py")
Invoke-PythonScript (Join-Path $scriptDir "verify_db.py")
